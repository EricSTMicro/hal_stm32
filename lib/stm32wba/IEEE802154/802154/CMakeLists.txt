# Copyright (c) 2025 STMicroelectronics
#
# SPDX-License-Identifier: Apache-2.0


zephyr_include_directories(STM32_WPAN/Target)
zephyr_include_directories(Core/Inc)
zephyr_include_directories(System/Config/Log)
zephyr_include_directories(System/Config/Debug_GPIO)

if(CONFIG_ZIGBEE_STM32WBA)
	zephyr_include_directories(Core/Inc/zigbee)
	
	zephyr_sources(STM32_WPAN/Target/zigbee_plat.c)
	zephyr_sources(STM32_WPAN/Target/zigbee_sys_if.c)
elseif(CONFIG_IEEE802154_STM32WBA)
	zephyr_include_directories(Core/Inc/802154)
endif()

zephyr_sources(STM32_WPAN/Target/linklayer_plat.c)
zephyr_sources(STM32_WPAN/Target/ll_sys_if.c)