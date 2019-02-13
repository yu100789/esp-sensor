/* DHT11 temperature sensor library
   Usage:
   		Set DHT PIN using  setDHTPin(pin) command
   		getFtemp(); this returns temperature in F
   Sam Johnston 
   October 2016
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef DHT11_H_  
#define DHT11_H_
#include <string.h>

#include <openssl/ssl.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>

#include <driver/rmt.h>
#include <soc/rmt_reg.h>

#include <nvs_flash.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>
void dht11_enable(void);
float get_temp();
int get_hum();
#endif