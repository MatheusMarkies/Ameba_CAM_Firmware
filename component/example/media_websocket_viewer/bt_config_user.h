#pragma once

#include <stdbool.h>
#include <stdint.h>

void bt_config_start_user(void);

void bt_config_stop_user(void);

void bt_config_callback(uint8_t *data, uint16_t len);

int connect_to_wifi_bt(void);

void bt_config_send_data(uint8_t *data, uint16_t len);

bool bt_config_has_credentials(void);

const char* bt_config_get_ssid(void);

const char* bt_config_get_password(void);