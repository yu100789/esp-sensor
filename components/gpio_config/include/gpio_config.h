#ifndef __GPIO_CONFIG_H__
#define __GPIO_CONFIG_H__

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "globaldefines.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifi_mqtt_config.h"

#define GPIO_OUTPUT_IO_0 WIFI_LED
#define GPIO_OUTPUT_IO_1 RLED
#define GPIO_OUTPUT_IO_2 BLED
#define GPIO_OUTPUT_IO_3 BLED1

#define GPIO_OUTPUT_PIN_SEL                                                                                                                          \
    ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1) | (1ULL << GPIO_OUTPUT_IO_2) | (1ULL << GPIO_OUTPUT_IO_3))

#define GPIO_INPUT_IO_0 CLOSE_POWER
#define GPIO_INPUT_IO_1 SMARTLINK

#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1))

#define ESP_INTR_FLAG_DEFAULT 0
void gpio_pin_init(void);
#endif