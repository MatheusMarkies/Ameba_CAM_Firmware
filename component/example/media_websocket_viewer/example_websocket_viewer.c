/******************************************************************************
* example_websocket_viewer.c
* Copyright(c) 2007 - 2021 Realtek Corporation. All rights reserved.
******************************************************************************/
#include "mmf2_link.h"
#include "mmf2_siso.h"
#include "module_video.h"
#include "module_web_viewer.h"
#include "mmf2_pro2_video_config.h"
#include "log_service.h"
#include "sensor.h"

#include "bt_config_app_main.h"
#include "bt_config_app_task.h"
#include "bt_config_wifi.h"
#include "atcmd_bt.h"
#include "platform_opts_bt.h"

#include "wifi_conf.h"
#include "lwip_netconf.h"

/*****************************************************************************
* ISP channel : 0
* Video type  : H264/HEVC
*****************************************************************************/

#define V1_CHANNEL 0
#define V1_BPS 4*1024*1024
#define V1_RCMODE 2 // 1: CBR, 2: VBR

#define VIDEO_TYPE VIDEO_H264
#define VIDEO_CODEC AV_CODEC_ID_H264

extern int  bt_config_app_init(void);
extern void bt_config_app_deinit(void);

static mm_context_t *video_v1_ctx			= NULL;
static mm_context_t *wview_v1_ctx			= NULL;
static mm_siso_t *siso_video_wview_v1		= NULL;

static video_params_t video_v1_params = {
	.stream_id = V1_CHANNEL,
	.type = VIDEO_TYPE,
	.bps = V1_BPS,
	.rc_mode = V1_RCMODE,
	.use_static_addr = 1,
	.level = VCENC_H264_LEVEL_4,
	.profile = VCENC_H264_MAIN_PROFILE,
	.cavlc = 1 
};

static uint8_t video_started = 0;

void clear_wifi_config(void)
{
	fATW0((void *)"0");
	fATW1((void *)"0");
}

static int start_video_streaming(void)
{
	if (video_started) {
		printf("Video streaming already started\r\n");
		return 0;
	}

	printf("Starting video streaming...\r\n");

	video_v1_params.resolution = VIDEO_FHD;
	video_v1_params.width = 1280;
	video_v1_params.height = 720;
	video_v1_params.fps = 20;
	video_v1_params.gop = 20;

#if (USE_UPDATED_VIDEO_HEAP == 0)
	int voe_heap_size = video_voe_presetting(1, video_v1_params.width, video_v1_params.height, V1_BPS, 0,
						0, 0, 0, 0, 0,
						0, 0, 0, 0, 0,
						0, 0, 0);
#else
	int voe_heap_size = video_voe_presetting_by_params(&video_v1_params, 0, NULL, 0, NULL, 0, NULL);
#endif
	printf("voe heap size = %d\r\n", voe_heap_size);

	video_v1_ctx = mm_module_open(&video_module);
	if (video_v1_ctx) {
		mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_PARAMS, (int)&video_v1_params);
		mm_module_ctrl(video_v1_ctx, MM_CMD_SET_QUEUE_LEN, video_v1_params.fps * 3);
		mm_module_ctrl(video_v1_ctx, MM_CMD_INIT_QUEUE_ITEMS, MMQI_FLAG_DYNAMIC);
	} else {
		printf("video open fail\r\n");
		return -1;
	}

	encode_rc_parm_t rc_parm;
	rc_parm.minQp = 15;
	rc_parm.maxQp = 40;
	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_SET_RCPARAM, (int)&rc_parm);

	wview_v1_ctx = mm_module_open(&websocket_viewer_module);
	if (wview_v1_ctx) {
		printf("web viewer open success\r\n");
	} else {
		printf("web viewer open fail\r\n");
		goto video_fail;
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
		printf("siso_video_wview_v1 open fail\r\n");
		goto video_fail;
	}

	mm_module_ctrl(video_v1_ctx, CMD_VIDEO_APPLY, V1_CHANNEL);
	mm_module_ctrl(wview_v1_ctx, CMD_WEB_VIEWER_APPLY, 1);

	video_started = 1;
	printf("Video streaming started successfully!\r\n");
	return 0;

video_fail:
	if (video_v1_ctx) {
		mm_module_close(video_v1_ctx);
		video_v1_ctx = NULL;
	}
	if (wview_v1_ctx) {
		mm_module_close(wview_v1_ctx);
		wview_v1_ctx = NULL;
	}
	return -1;
}

int connection_by_bt = 2;

void kill_bt(){
	bt_config_send_msg(0);
	vTaskDelay(1000);
    printf("[APP] Desligando o Bluetooth para liberar a antena...\r\n");
    printf("[APP] Bluetooth desligado.\r\n");

	printf("Waiting for network stability...\r\n");

	vTaskDelay(2000);
}

void on_wifi_connected(void)
{
	if(connection_by_bt == 1){
		//kill_bt();
	}

	uint8_t *ip = LwIP_GetIP(0);
	printf("\r\n========================================\r\n");
	printf("WiFi Connected Successfully!\r\n");
	printf("IP Address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
	printf("========================================\r\n");

	printf("Waiting for network stability...\r\n");
	vTaskDelay(2000);

	printf("Starting video streaming...\r\n");
	if (start_video_streaming() == 0) {
		printf("\r\n========================================\r\n");
		printf("WebSocket Viewer Available!\r\n");
		printf("Open: http://%d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
		printf("========================================\r\n");
	} else {
		printf("ERROR: Failed to start video streaming!\r\n");
	}
}

void clear_wifi_autoreconnect(){
	wifi_config_autoreconnect(0, 0, 0);
	printf("Auto-reconnect disabled\r\n");

	printf("[APP] Forçando desligamento total do WiFi...\r\n");
	clear_wifi_config();
	wifi_disconnect();
	vTaskDelay(2000);
	printf("[APP] WiFi desligado. Iniciando Bluetooth...\r\n");
}

static void mmf2_video_websocket_viewer(void *param)
{
	printf("\r\n");
	printf("========================================\r\n");
	printf("  AMB82-Mini BT WiFi Config + Video\r\n");
	printf("  Version: 1.0\r\n");
	printf("========================================\r\n");

	//clear_wifi_autoreconnect();
	if(connection_by_bt == 2){
	vTaskDelay(15000);
	if ((wifi_get_join_status() == RTW_JOINSTATUS_SUCCESS) && 
	    (*(u32 *)LwIP_GetIP(0) != IP_ADDR_INVALID)) {
		connection_by_bt = 0;
		uint8_t *ip = LwIP_GetIP(0);
		printf("WARNING: WiFi already connected!\r\n");
		printf("Current IP: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
		vTaskDelay(5000);
		on_wifi_connected();
		
	} else {
		printf("No WiFi connection found\r\n");
		printf("Waiting for WiFi configuration via Bluetooth...\r\n");
		printf("Use BLE app to send: SSID:PASSWORD\r\n");
		printf("========================================\r\n");

		printf("[BT_CONFIG] Iniciando...\r\n");
		connection_by_bt = 1;
		
		int ret = bt_config_app_init();

		if (ret == 0) {
			printf("[BT_CONFIG] BT Stack initialized successfully!\r\n");
		} else {
			printf("[BT_CONFIG] ERROR: Failed to initialize!\r\n");
		}
		
		printf("[BT_CONFIG] Aguardando credenciais via Bluetooth...\r\n");
		printf("[BT_CONFIG] Envie no formato: SSID:senha\r\n");
	}	
	while(1) {
		vTaskDelay(10000);
	}
}

}

void example_websocket_viewer(void)
{
	if (xTaskCreate(mmf2_video_websocket_viewer, 
	                ((const char *)"mmf2_video_websocket_viewer"), 
	                16384, 
	                NULL, 
	                tskIDLE_PRIORITY + 2,
	                NULL) != pdPASS) {
		printf("\r\n%s xTaskCreate failed", __FUNCTION__);
	}
}