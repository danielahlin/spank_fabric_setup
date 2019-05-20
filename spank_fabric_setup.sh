#!/bin/bash -x

CMD="$(basename $0)"

# Uncomment for debugging
set -x
exec 1>/tmp/${CMD}.$$.log 2>&1
env

# Exit handler 
#
# Assumption is that regular cleanup in this handler are taken on failure-conditions
exit_handler()
{
    res=$?
    # If normal exit - then just exit
    if [[ $? == 0 ]]; then
        return
    fi

    exit $res
}


# Ignore if this is called without any span_fabric environment variable set
if [[ -z "$SPANK_FABRIC_PARTITION_CREATE_ADHOC" &&
      -z "$SPANK_FABRIC_PARTITION_JOIN" &&
      -z "$SPANK_FABRIC_BIND_MPI" ]]; then
    exit 0;
fi

# Kerberos things
KEYTAB=/etc/slurm-krb5.keytab
KINIT=/usr/heimdal/bin/kinit
HOST_PRINCIPAL=host/$(hostname)

# Read configuration
SFS_CONF=/etc/spank_fabric_setup/spank-fabric-setup-cfg.sh
if [[ -r "${SFS_CONF}" ]]; then
    source "${SFS_CONF}"
fi

EXPANDEDNODES=$(scontrol show hostname ${SLURM_JOB_NODELIST})
NODES=${EXPANDEDNODES//$'\n'/,}
# Count nodes in two steps
NNODES=${NODES//[^,]/}
NNODES=$(((${#NODES} != 0) + ${#NNODES}))

# Create partitions
declare -A TAGS
declare -A PKEYS
if [[ -n "${SPANK_FABRIC_PARTITION_CREATE_ADHOC}" ]]; then    
    for SPEC in ${SPANK_FABRIC_PARTITION_CREATE_ADHOC//,/ }; do
	TAG="${SPEC#*:}"
	INTERFACEID="${SPEC%:*}"
	MEMBERIDS=$(${SFS_COLLECT_NODE_IDS} -n ${NODES} -i ${INTERFACEID} -s)
	PKEY=$(${SFS_ADD_PARTITION} -i ${INTERFACEID} -m ${MEMBERIDS//$'\n'/,} -p "${SLURM_JOB_ID}-${TAG}")
	PKEYS[$TAG]=$PKEY
	if [[ -z "${TAGS[$TAG]}" ]]; then
	    TAGS[$TAG]=$NODES
	else
	    TAGS[$TAG]="${TAGS[$TAG]},$NODES"
	fi
    done
fi

function printIPv4
{
    ADDR=$1
    printf '%d.%d.%d.%d\n' $((255 & ADDR >> 24)) $((255 & ADDR >> 16)) $((255 & ADDR >> 8)) $((255 & ADDR >> 0))
}

# FIXME
DEV=ib0
HOSTDIR=/tmp/spank_fabric_setup-${SLURM_JOB_ID}
mkdir -p "${HOSTDIR}/etc"
mkdir -p "${HOSTDIR}/etc/sysconfig/network-scripts-addons"
mkdir -p "${HOSTDIR}/etc/udev/rules.d"

# Create a file containing IPv4 addresses and hostnames
NETID=$((10 << 24))
for TAG in ${!TAGS[*]}; do
    echo "# Subnet for TAG $TAG starting at $(printIPv4 $NETID)"
    NODES=${TAGS[$TAG]}
    echo $(printIPv4 $NETID) $TAG-net $TAG-net.pdc.kth.se
    HOSTID=1
    for NODE in ${NODES//,/ }; do
	echo $(printIPv4 $((HOSTID + NETID))) $NODE-$TAG $NODE-$TAG.pdc.kth.se
	INVMASK=$((INVMASK | HOSTID))
	HOSTID=$((HOSTID + 1))
    done
    INVMASK=$((INVMASK | HOSTID))
    MASK=$((INVMASK ^ (2**32-1)))
    echo ${DEV}_${TAG} >> ${HOSTDIR}/interfaces
    echo "# End of subnet for TAG ${TAG} netmask $(printIPv4 ${MASK})"
    printf -v MASKEDPKEY "%04x" $((0x8000 | ${PKEYS[$TAG]}))
    echo 'ACTION=="add", SUBSYSTEM=="net", DEVPATH=="/devices/virtual/net/'${DEV}.${MASKEDPKEY}'" NAME="'${DEV}_${TAG}'"' >> "${HOSTDIR}/etc/udev/rules.d/01-spank_fabric_setup_ifnames.rules"
    # Note that script below needs some fixes in the ifup-ib script to work
    # 1. Remove the constraint that the pkey'd interface have to be named REALDDEV.PKEY
    # 2. Add a sleep between creation of child-device and the check to
    # see if it has been created    
    cat > "${HOSTDIR}/etc/sysconfig/network-scripts-addons/ifcfg-${DEV}_${TAG}" <<EOF
NAME=\${BASH_SOURCE##*-}
PHYSDEV=\${NAME%_*}
DEVICE=\${NAME}
PKEY=yes
PKEY_ID=${PKEYS[$TAG]}
TAG=\${NAME##*_}
BOOTPROTO=static
TYPE=ib
NM_CONTROLLED=no
IPADDR=\$(getent ahostsv4 \${HOSTNAME%%.*}-\${TAG}.\${HOSTNAME#*.} | sed -n '1 s/ .*//p;q' )
NETMASK=$(printIPv4 $MASK)
EOF
    # Next subnet:
    NETID=$((NETID + INVMASK + 1))
    echo
done > "${HOSTDIR}/etc/hosts"

# FIXME below pdcp should use equivalent of ssh -n...
# Copy out configuration files
env PATH=/bin:/usr/bin:/sbin:/usr/sbin $KINIT "--keytab=$KEYTAB" $HOST_PRINCIPAL pdcp -l root -rw "${SLURM_JOB_NODELIST}" "${HOSTDIR}" /tmp < /dev/null

# Setup names 
env PATH=/bin:/usr/bin:/sbin:/usr/sbin $KINIT "--keytab=$KEYTAB" $HOST_PRINCIPAL pdsh -l root -w "${SLURM_JOB_NODELIST}" "cat /etc/hosts >> ${HOSTDIR}/etc/hosts && mount -o bind {${HOSTDIR},}/etc/hosts"  < /dev/null

# Setup network config
env PATH=/bin:/usr/bin:/sbin:/usr/sbin $KINIT "--keytab=$KEYTAB" $HOST_PRINCIPAL pdsh -l root -w "${SLURM_JOB_NODELIST}" "cp -r {,${HOSTDIR}}/etc/sysconfig/network-scripts && cp ${HOSTDIR}/etc/sysconfig/network-scripts-addons/* ${HOSTDIR}/etc/sysconfig/network-scripts && mount -o bind {${HOSTDIR},}/etc/sysconfig/network-scripts; mount -o bind {${HOSTDIR},}/etc/udev/rules.d/01-spank_fabric_setup_ifnames.rules && udevadm control -R"  < /dev/null

# Bring up interfaces
env PATH=/bin:/usr/bin:/sbin:/usr/sbin $KINIT "--keytab=$KEYTAB" $HOST_PRINCIPAL pdsh -l root -w "${SLURM_JOB_NODELIST}" "for a in \$(cat '${HOSTDIR}/interfaces'); do ifup \$a; done" < /dev/null

#if [[ -n ${SPANK_PKEY_FABRIC_PARTITION_JOIN} ]]; then
#fi
#if [[ -n ${SPANK_PKEY_FABRIC_BIND_MPI ]]; then
#fi
       
