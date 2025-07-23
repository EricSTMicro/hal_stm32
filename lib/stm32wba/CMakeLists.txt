# Copyright (c) 2025 STMicroelectronics
#
# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_BT_STM32WBA ble)
add_subdirectory_ifdef(CONFIG_IEEE802154_STM32WBA IEEE802154)

