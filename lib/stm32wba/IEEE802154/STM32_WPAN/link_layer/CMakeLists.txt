
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
zephyr_include_directories(ll_cmd_lib/inc)
zephyr_include_directories(ll_cmd_lib/inc/_40nm_reg_files)
zephyr_include_directories(ll_cmd_lib/inc/ot_inc)
zephyr_include_directories(ll_cmd_lib/config/ieee_15_4_basic)
zephyr_include_directories(ll_sys/inc)


zephyr_sources(ll_sys/src/ll_sys_cs.c)
zephyr_sources(ll_sys/src/ll_sys_dp_slp.c)
zephyr_sources(ll_sys/src/ll_sys_intf.c)
zephyr_sources(ll_sys/src/ll_sys_startup.c)
