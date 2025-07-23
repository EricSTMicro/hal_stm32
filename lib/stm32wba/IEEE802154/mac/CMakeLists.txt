zephyr_compile_definitions( -DMAC_LAYER )

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

zephyr_include_directories(${ZEPHYR_BASE}/subsys/net/l2/ieee802154/)
zephyr_include_directories(${ZEPHYR_BASE}/../modules/openthread/platform)
zephyr_include_directories(${ZEPHYR_BASE}/../modules/lib/openthread/include/)
zephyr_include_directories(${ZEPHYR_BASE}/../modules/hal/stm32/)
zephyr_include_directories(${ZEPHYR_BASE}/include/)
zephyr_include_directories(${ZEPHYR_BASE}/drivers/ieee802154/)

add_library(st_mac_lib STATIC IMPORTED GLOBAL)
add_library(st_mac_wrapper_lib STATIC IMPORTED GLOBAL)

set_target_properties(st_mac_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../../zephyr/blobs/stm32wba/lib/WBA6_Mac15_4.a)
set_target_properties(st_mac_wrapper_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../../zephyr/blobs/stm32wba/lib/wba_mac_lib.a)

zephyr_sources(src/mac_port.c)

target_link_libraries(app PUBLIC st_mac_lib) 
target_link_libraries(app PUBLIC st_mac_wrapper_lib)
