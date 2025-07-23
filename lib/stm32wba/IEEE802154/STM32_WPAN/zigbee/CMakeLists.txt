# Copyright (c) 2025 STMicroelectronics
#
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/zcl)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/zcl/general)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/zgp)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/stack/port/mac)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/stack/port/stm32wba)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/STM32_WPAN/Target)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/STM32_WPAN/App/inc)

add_library(st_cluster_lib STATIC IMPORTED GLOBAL)
add_library(st_zigbee_lib STATIC IMPORTED GLOBAL)

set_target_properties(st_cluster_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../zephyr/blobs/stm32wba/lib/ZigbeeClusters.a)

if(DEFINED CONFIG_ZIGBEE_STM32WPAN_DEVICE_TYPE)

    if(CONFIG_ZIGBEE_STM32WPAN_DEVICE_TYPE STREQUAL "FFD")
        set_target_properties(st_zigbee_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../zephyr/blobs/stm32wba/lib/ZigBeeProR23_FFD.a)
    elseif(CONFIG_ZIGBEE_STM32WPAN_DEVICE_TYPE STREQUAL "RFD")
        set_target_properties(st_zigbee_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../zephyr/blobs/stm32wba/lib/ZigBeeProR23_RFD.a)
    else()
        message(FATAL_ERROR "Unknown ZigBee device type, the only supported device types are FFD and RFD")
    endif()

else()
    # The default type is FFD
    set_target_properties(st_zigbee_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../zephyr/blobs/stm32wba/lib/ZigBeeProR23_FFD.a)
endif()


target_link_libraries(app PUBLIC st_zigbee_lib)
target_link_libraries(app PUBLIC st_cluster_lib)


