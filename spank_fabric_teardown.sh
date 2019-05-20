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

if [[ -n "${SPANK_FABRIC_PARTITION_CREATE_ADHOC}" ]]; then    
    for SPEC in ${SPANK_FABRIC_PARTITION_CREATE_ADHOC//,/ }; do
	TAG="${SPEC#*:}"
	"$SFS_REMOVE_PARTITION" -p "${SLURM_JOB_ID}-${TAG}"
    done
fi

HOSTDIR=/tmp/spank_fabric_setup-${SLURM_JOB_ID}

env PATH=/bin:/usr/bin:/sbin:/usr/sbin $KINIT "--keytab=$KEYTAB" $HOST_PRINCIPAL pdsh -l root -w "${SLURM_JOB_NODELIST}" "for a in \$(cat '${HOSTDIR}/interfaces'); do ifdown \$a; done; umount '/etc/hosts' '/etc/sysconfig/network-scripts' '/etc/udev/rules.d/01-spank_fabric_setup_ifnames.rules' && rm -rf '${HOSTDIR}'; udevadm control -R"  < /dev/null

#if [[ -n ${SPANK_PKEY_FABRIC_PARTITION_JOIN} ]]; then
#fi
#if [[ -n ${SPANK_PKEY_FABRIC_BIND_MPI ]]; then
#fi
       
     
