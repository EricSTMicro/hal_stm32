# Copyright (c) 2025 STMicroelectronics
#
# SPDX-License-Identifier: Apache-2.0

zephyr_compile_definitions( -DMAC )

zephyr_include_directories(Common/WPAN/Interfaces)
zephyr_include_directories(Common/WPAN/Modules)
zephyr_include_directories(Common/WPAN/Modules/Flash)
zephyr_include_directories(Common/WPAN/Modules/RTDebug)
zephyr_include_directories(Common/WPAN/Modules/Log)
zephyr_include_directories(Common/WPAN/Modules/BasicAES)
zephyr_include_directories(Common/WPAN/Modules/MemoryManager)
zephyr_include_directories(Common/Utilities/)
zephyr_include_directories(Common/Utilities/misc)
zephyr_include_directories(Common/Utilities/tim_serv)
zephyr_include_directories(Common/Utilities/trace/adv_trace)
zephyr_include_directories(mac/inc)
zephyr_include_directories(STM32_WPAN)
zephyr_include_directories(STM32_WPAN/Target)
zephyr_include_directories(802154/Core/inc)
zephyr_include_directories(802154/System/Config/Log)
zephyr_include_directories(802154/System/Config/Debug_GPIO)



zephyr_sources(Common/WPAN/Interfaces/hw_pka.c)
zephyr_sources(Common/WPAN/Interfaces/hw_pka_p256.c)
zephyr_sources(Common/WPAN/Modules/scm.c)
zephyr_sources(Common/WPAN/Modules/stm_list.c)
zephyr_sources(Common/WPAN/Modules/power_table.c)
zephyr_sources(Common/WPAN/Modules/RTDebug/RTDebug.c)

if(CONFIG_FLASH)
  zephyr_sources(Common/WPAN/Modules/Flash/flash_manager.c)
  zephyr_sources(Common/WPAN/Modules/Flash/flash_driver.c)
  zephyr_sources(Common/WPAN/Modules/Flash/rf_timing_synchro.c)
endif()


zephyr_sources(Common/WPAN/Modules/BasicAES/baes_ccm.c)
zephyr_sources(Common/WPAN/Modules/BasicAES/baes_cmac.c)
zephyr_sources(Common/WPAN/Modules/BasicAES/baes_ecb.c)
zephyr_sources(Common/WPAN/Modules/MemoryManager/advanced_memory_manager.c)
zephyr_sources(Common/WPAN/Modules/MemoryManager/stm32_mm.c)

add_library(st_link_lib STATIC IMPORTED GLOBAL)
set_target_properties(st_link_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../../zephyr/blobs/stm32wba/lib/WBA6_LinkLayer15_4.a)
target_link_libraries(app PUBLIC st_link_lib)

add_subdirectory(STM32_WPAN)
add_subdirectory_ifdef(CONFIG_NET_L2_CUSTOM_IEEE802154_STM32WBA mac)
add_subdirectory_ifdef(CONFIG_IEEE802154_STM32WBA 802154)

