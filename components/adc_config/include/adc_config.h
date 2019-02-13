#ifndef __ADC_CONFIG_H__
#define __ADC_CONFIG_H__
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "wifi_mqtt_config.h"
#include "globaldefines.h"
#include "ledc_config.h"
#define DEFAULT_VREF 1100 // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES 64 // Multisampling

typedef struct adc_info {
    adc_channel_t channel;
    adc_atten_t atten;
    esp_adc_cal_characteristics_t * chars;
    uint32_t voltage;
    uint32_t raw;
} adc_info_t;

uint32_t get_adc_value(int adc_pin,bool raw_type);
void setVoltageLimit(int NO2,int ALL,int NH4,int CO);
void setAlarmControl(int data);
void adc_initConfig(void);
EventGroupHandle_t getEventGroup(void);
#endif