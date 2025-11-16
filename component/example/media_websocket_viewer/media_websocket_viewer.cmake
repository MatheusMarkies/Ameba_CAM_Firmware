### add lib ###
list(
    APPEND app_example_lib
)

### add flags ###
list(
    APPEND app_example_flags
    -DCONFIG_BT=1
    -DCONFIG_BT_CONFIG=1
    -DCONFIG_BT_PERIPHERAL=1
    -DCONFIG_BT_CENTRAL=0
    -DCONFIG_FTL_ENABLED
    -DCONFIG_EXAMPLE_WLAN_FAST_CONNECT=0
    
)

### add header files ###
list(
    APPEND app_example_inc_path
    "${sdk_root}/project/realtek_amebapro2_v0_example/src/mmfv2_video_example"
)

### add source file ###
list(
    APPEND app_example_sources
    app_example.c
    example_websocket_viewer.c
)

list(TRANSFORM app_example_sources PREPEND ${CMAKE_CURRENT_LIST_DIR}/)