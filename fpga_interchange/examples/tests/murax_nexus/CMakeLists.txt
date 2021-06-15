add_interchange_group_test(
    name murax_nexus
    family ${family}
    board_list lifcl40evn
    tcl run_nexus.tcl
    sources murax.v
    techmap ../../remap_nexus.v
    skip_dcp
)

