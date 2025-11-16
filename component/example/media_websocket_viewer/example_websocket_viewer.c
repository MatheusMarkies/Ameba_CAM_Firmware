/******************************************************************************
* example_websocket_viewer.c
* Copyright(c) 2007 - 2021 Realtek Corporation. All rights reserved.
*
******************************************************************************/
#include "mmf2_link.h"
#include "mmf2_siso.h"
#include "module_video.h"
#include "module_web_viewer.h"
#include "mmf2_pro2_video_config.h"
#include "log_service.h"
#include "sensor.h"

#include "wifi_conf.h"
#include "lwip_netconf.h"
#include <string.h>

/*****************************************************************************
* ISP channel : 0
* Video type  : H264/HEVC
*****************************************************************************/

#define V1_CHANNEL 0
#define V1_BPS 6*1024*1024
#define V1_RCMODE 2 // 1: CBR, 2: VBR

#define VIDEO_TYPE VIDEO_H264
#define VIDEO_CODEC AV_CODEC_ID_H264

static mm_context_t *video_v1_ctx = NULL;
static mm_context_t *wview_v1_ctx = NULL;
static mm_siso_t *siso_video_wview_v1 = NULL;

static video_params_t video_v1_params = {
    .stream_id = V1_CHANNEL,
    .type = VIDEO_TYPE,
    .bps = V1_BPS,
    .rc_mode = V1_RCMODE,
    .use_static_addr = 1,
    .level = VCENC_H264_LEVEL_4,
    .profile = VCENC_H264_MAIN_PROFILE ,
    .cavlc = 1 
};

static int connect_to_wifi_direct(const char *ssid_str, const char *password_str)
{
    rtw_network_info_t connect_params;
    memset(&connect_params, 0, sizeof(rtw_network_info_t));
    
    int ssid_len = strlen(ssid_str);
    connect_params.ssid.len = (unsigned char)ssid_len;
    memcpy(connect_params.ssid.val, ssid_str, ssid_len);

    if (password_str != NULL && password_str[0] != '\0') {
        int pass_len = strlen(password_str);
        connect_params.password = (unsigned char *)password_str;
        connect_params.password_len = pass_len;
        connect_params.security_type = RTW_SECURITY_WPA2_AES_PSK;
    } else {
        connect_params.security_type = RTW_SECURITY_OPEN;
        connect_params.password = NULL;
        connect_params.password_len = 0;
    }

    return wifi_connect(&connect_params, 1);
}

static void wifi_init_direct(void)
{
    printf("[WIFI] Modo direto ativado\r\n");
    
    while (true) {
        vTaskDelay(200);
        int status = connect_to_wifi_direct("2.4G_Kitnet1", "67984523");

        if (status == RTW_SUCCESS) {
            printf("[WIFI] Conectado!\r\n");
            printf("[WIFI] IP: %s\r\n", LwIP_GetIP(0));
            break;
        } else {
            printf("[WIFI] Falha ao conectar\r\n");
        }
    }
}

static void mmf2_video_websocket_viewer(void *param)
{
    // Configuração de vídeo
    video_v1_params.resolution = VIDEO_FHD;
    video_v1_params.width = 1920;
    video_v1_params.height = 1080;
    video_v1_params.fps = 30;
    video_v1_params.gop = 30;
    
#if (USE_UPDATED_VIDEO_HEAP == 0)
    int voe_heap_size = video_voe_presetting(1, video_v1_params.width, video_v1_params.height, V1_BPS, 0,
                        0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0,
                        0, 0, 0);
#else
    int voe_heap_size = video_voe_presetting_by_params(&video_v1_params, 0, NULL, 0, NULL, 0, NULL);
#endif
    
    printf("\r\n voe heap size = %d\r\n", voe_heap_size);
    
    video_v1_ctx = mm_module_open(&video_module);
    if (video_v1_ctx) {
        mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
        mm_module_ctrl(video_v1_ctx, MM_CMD_SET_QUEUE_LEN, video_v1_params.fps * 3);
        mm_module_ctrl(video_v1_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);

    } else {
        rt_printf("video open fail\n\r");
        goto mmf2_video_web_viewer_fail;
    }

    encode_rc_parm_t rc_parm;
    rc_parm.minQp = 15;
    rc_parm.maxQp = 26;
    mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_RCPARAM, (int)&rc_parm);

    wview_v1_ctx = mm_module_open(&websocket_viewer_module);
    if (wview_v1_ctx) {
        rt_printf("web viewer open success\n\r");
    } else {
        rt_printf("web viewer open fail\n\r");
        goto mmf2_video_web_viewer_fail;
    }
    
    siso_video_wview_v1 = siso_create();
    if (siso_video_wview_v1) {
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
        siso_ctrl(siso_video_wview_v1, MMIC_CMD_SET_SECURE_CONTEXT, 1, 0);
#endif
        siso_ctrl(siso_video_wview_v1, MMIC_CMD_ADD_INPUT, (uint32_t)video_v1_ctx, 0);
        siso_ctrl(siso_video_wview_v1, MMIC_CMD_ADD_OUTPUT, (uint32_t)wview_v1_ctx, 0);
        siso_start(siso_video_wview_v1);
    } else {
        rt_printf("siso_video_wview_v1 open fail\n\r");
        goto mmf2_video_web_viewer_fail;
    }
    
    mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);
    mm_module_ctrl(wview_v1_ctx, CMD_WEB_VIEWER_APPLY, 1);

    mmf2_video_web_viewer_fail:
    vTaskDelete(NULL);
    return;
}

void example_websocket_viewer(void)
{
    if (xTaskCreate(mmf2_video_websocket_viewer, ((const char *)"mmf2_video_websocket_viewer"), 
                   8192, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
        printf("\n\r%s xTaskCreate(mmf2_video_websocket_viewer) failed", __FUNCTION__);
    }
}