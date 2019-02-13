#ifndef __NATIVE_OTA_H__
#define __NATIVE_OTA_H__
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "wifi_mqtt_config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_https_ota.h"
void ota_start();
#endif 
