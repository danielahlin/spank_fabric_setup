#
export SFS_CONF_DIR=/etc/spank_fabric_setup
export SFS_ADD_PARTITION="${SFS_CONF_DIR}/scripts/fsm-add-partition"
export SFS_REMOVE_PARTITION="${SFS_CONF_DIR}/scripts/fsm-remove-partition"
export SFS_JOIN_PARTITION="${SFS_CONF_DIR}/scripts/fsm-join-partition"

# Parameters to the COLLECT_NODE_ID subscript
export SFS_COLLECT_NODE_IDS="${SFS_CONF_DIR}/scripts/collect-node-ids"
export SFS_COLLECT_NODE_IDS_MAP="${SFS_CONF_DIR}/node-id-map"

# Parameters to the SFS_*_PARTITION*fsm subscripts
export SFS_FSM_ADD_PARTITION_BASEURL="http://localhost:9876"

# Scripts to setup and  usage of partition on the nodes
# Script for add-hoc add
export SFS_NODE_ADD="${SFS_CONF_DIR}/scripts/node-setup-add"
# Script for joining existing partition
export SFS_NODE_JOIN="${SFS_CONF_DIR}/scripts/node-setup-join"
# Script for leaving partition
export SFS_NODE_LEAVE="${SFS_CONF_DIR}/scripts/node-setup-leave"

# Defaults
export SFS_FABRIC_DEFAULT_INTERFACEID=mlx4_0-1

# DNS defaults
export SFS_FABRIC_DOMAIN=example.net
