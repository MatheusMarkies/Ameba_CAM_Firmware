#include "platform_stdlib.h"
#include "atcmd_bt.h"
#include "platform_opts_bt.h"
#include "wifi_conf.h"
#include "lwip_netconf.h"
#include <string.h>

#if defined(CONFIG_BT_CONFIG) && CONFIG_BT_CONFIG

extern int  bt_config_app_init(void);
extern void bt_config_app_deinit(void);

static char received_ssid[33] = {0};
static char received_password[65] = {0};
static bool wifi_credentials_received = false;

void bt_config_callback(uint8_t *data, uint16_t len)
{
    printf("[BT_CONFIG] Dados recebidos: %.*s (len=%d)\r\n", len, data, len);

    char *separator = strchr((char*)data, ':');
    
    if (separator != NULL) {
        // Formato: "SSID:senha"
        int ssid_len = separator - (char*)data;
        int pass_len = len - ssid_len - 1;
        
        if (ssid_len > 0 && ssid_len < 33) {
            memset(received_ssid, 0, sizeof(received_ssid));
            memcpy(received_ssid, data, ssid_len);
            received_ssid[ssid_len] = '\0';
            
            if (pass_len > 0 && pass_len < 65) {
                memset(received_password, 0, sizeof(received_password));
                memcpy(received_password, separator + 1, pass_len);
                received_password[pass_len] = '\0';
            }
            
            wifi_credentials_received = true;
            printf("[BT_CONFIG] SSID: %s\r\n", received_ssid);
            printf("[BT_CONFIG] Password: %s\r\n", received_password);
        }
    } else {
        printf("[BT_CONFIG] Formato inválido. Use: SSID:senha\r\n");
    }
}

int connect_to_wifi_bt(void) 
{
    if (!wifi_credentials_received) {
        printf("[BT_CONFIG] Nenhuma credencial recebida ainda\r\n");
        return -1;
    }
    
    rtw_network_info_t connect_params;
    memset(&connect_params, 0, sizeof(rtw_network_info_t));
    
    int ssid_len = strlen(received_ssid);
    connect_params.ssid.len = (unsigned char)ssid_len;
    memcpy(connect_params.ssid.val, received_ssid, ssid_len);
    
    if (received_password[0] != '\0') {
        int pass_len = strlen(received_password);
        connect_params.password = (unsigned char *)received_password;
        connect_params.password_len = pass_len;
        connect_params.security_type = RTW_SECURITY_WPA2_AES_PSK;
    } else {
        connect_params.security_type = RTW_SECURITY_OPEN;
        connect_params.password = NULL;
        connect_params.password_len = 0;
    }
    
    printf("[BT_CONFIG] Tentando conectar ao WiFi...\r\n");
    int result = wifi_connect(&connect_params, 1);
    
    if (result == RTW_SUCCESS) {
        vTaskDelay(500);
        
        char *ip_addr = LwIP_GetIP(0);
        printf("[BT_CONFIG] WiFi conectado! IP: %s\r\n", ip_addr);

        char response[128];
        snprintf(response, sizeof(response), "OK:IP=%s", ip_addr);
        bt_config_send_data((uint8_t*)response, strlen(response));

        vTaskDelay(100); 
        
        char *gateway = LwIP_GetGW(0);
        char *netmask = LwIP_GetMASK(0);
        
        snprintf(response, sizeof(response), "INFO:GW=%s,MASK=%s", gateway, netmask);
        bt_config_send_data((uint8_t*)response, strlen(response));
        
    } else {
        printf("[BT_CONFIG] Falha ao conectar WiFi\r\n");
        char response[] = "ERROR:Falha na conexao WiFi";
        bt_config_send_data((uint8_t*)response, strlen(response));
    }
    
    return result;
}

void bt_config_send_data(uint8_t *data, uint16_t len)
{

    printf("[BT_CONFIG] Enviando: %.*s\r\n", len, data);

}

void bt_config_start_user(void)
{
    if (bt_command_type(BT_COMMAND_STACK)) {
        printf("[BT_CONFIG] ERROR: command type error\r\n");
        return;
    }

    printf("[BT_CONFIG] Iniciando...\r\n");

    bt_config_app_init();
    set_bt_cmd_type(CONFIG_BIT | STACK_BIT);
    
    printf("[BT_CONFIG] Aguardando credenciais via Bluetooth...\r\n");
    printf("[BT_CONFIG] Envie no formato: SSID:senha\r\n");
}

void bt_config_stop_user(void)
{
    if (!bt_command_type(BT_COMMAND_CONFIG)) {
        printf("[BT_CONFIG] ERROR: command type error\r\n");
        return;
    }

    printf("[BT_CONFIG] Parando...\r\n");
    bt_config_app_deinit();
    set_bt_cmd_type(0);
    
    memset(received_ssid, 0, sizeof(received_ssid));
    memset(received_password, 0, sizeof(received_password));
    wifi_credentials_received = false;
}

bool bt_config_has_credentials(void)
{
    return wifi_credentials_received;
}

const char* bt_config_get_ssid(void)
{
    return received_ssid;
}

const char* bt_config_get_password(void)
{
    return received_password;
}

#endif