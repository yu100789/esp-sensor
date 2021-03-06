#include "adc_config.h"
static const char* TAG = "ADC";
static adc_info_t ADC_PIN25 = { .channel = ADC2_CHANNEL_8, .atten = ADC_ATTEN_11db, .chars = NULL, .voltage = 0, .raw = 0 };
static adc_info_t ADC_PIN35 = { .channel = ADC1_CHANNEL_7, .atten = ADC_ATTEN_11db, .chars = NULL, .voltage = 0, .raw = 0 };
static adc_info_t ADC_PIN34 = { .channel = ADC1_CHANNEL_6, .atten = ADC_ATTEN_11db, .chars = NULL, .voltage = 0, .raw = 0 };
static adc_info_t ADC_PIN32 = { .channel = ADC1_CHANNEL_4, .atten = ADC_ATTEN_11db, .chars = NULL, .voltage = 0, .raw = 0 };
static const adc_unit_t unit = ADC_UNIT_1;
static uint32_t limit_voltage_NO2 = 1000;
static uint32_t limit_voltage_ALL = 2200;
static uint32_t limit_voltage_NH4 = 1000;
static uint32_t limit_voltage_CO = 1000;
static uint16_t ALARMCONTROL = 1;
static EventGroupHandle_t adcEventGroup;

static void check_efuse()
{
    // Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    // Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}
static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

static void AdcMainTask(void* parm)
{
    /* Attempt to create the event group. */
    adcEventGroup = xEventGroupCreate();
    adc_power_on();
    // Check if Two Point or Vref are burned into eFuse
    check_efuse();
    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_PIN32.channel, ADC_PIN32.atten);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_PIN34.channel, ADC_PIN34.atten);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_PIN35.channel, ADC_PIN35.atten);
    adc2_config_channel_atten(ADC_PIN25.channel, ADC_PIN25.atten);

    // Characterize ADC
    ADC_PIN32.chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    ADC_PIN34.chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    ADC_PIN35.chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    ADC_PIN25.chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, ADC_PIN32.atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, ADC_PIN32.chars);
    print_char_val_type(val_type);
    esp_adc_cal_characterize(unit, ADC_PIN34.atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, ADC_PIN34.chars);
    esp_adc_cal_characterize(unit, ADC_PIN35.atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, ADC_PIN35.chars);
    esp_adc_cal_characterize(ADC_UNIT_2, ADC_PIN25.atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, ADC_PIN25.chars);
    // Continuously sample ADC1
    while (1) {
        // Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            ADC_PIN32.raw += adc1_get_raw(ADC_PIN32.channel);
            ADC_PIN34.raw += adc1_get_raw(ADC_PIN34.channel);
            ADC_PIN35.raw += adc1_get_raw(ADC_PIN35.channel);
            adc2_get_raw(ADC_PIN25.channel, ADC_WIDTH_BIT_12, (int*)&ADC_PIN25.raw);
        }
        ADC_PIN32.raw /= NO_OF_SAMPLES;
        ADC_PIN34.raw /= NO_OF_SAMPLES;
        ADC_PIN35.raw /= NO_OF_SAMPLES;
        ADC_PIN25.raw /= NO_OF_SAMPLES;
        // Convert adc_reading to voltage in mV
        ADC_PIN32.voltage = esp_adc_cal_raw_to_voltage(ADC_PIN32.raw, ADC_PIN32.chars);
        ADC_PIN34.voltage = esp_adc_cal_raw_to_voltage(ADC_PIN34.raw, ADC_PIN34.chars);
        ADC_PIN35.voltage = esp_adc_cal_raw_to_voltage(ADC_PIN35.raw, ADC_PIN35.chars);
        ADC_PIN25.voltage = esp_adc_cal_raw_to_voltage(ADC_PIN25.raw, ADC_PIN25.chars);
        if ((ADC_PIN32.voltage > limit_voltage_NO2 || ADC_PIN34.voltage > limit_voltage_ALL || ADC_PIN35.voltage > limit_voltage_NH4
            || ADC_PIN25.voltage > limit_voltage_CO) && ALARMCONTROL==1) {
            /* Set bit 0 in xEventGroup. */
            xEventGroupSetBits(adcEventGroup,BIT0);/* The bits being set. */
            gpio_set_level(RLED, 1);
            ledc_toggle(1500);
        } else {
            xEventGroupClearBits(adcEventGroup,BIT0);/* The bits being set. */
            gpio_set_level(RLED, 0);
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
        // ESP_LOGI(TAG,"Raw:     %d  \t%d  \t%d  \t%d\n", adc_reading_0, adc_reading_1, adc_reading_2, adc_reading_3);
        ESP_LOGI(TAG,"\nadc32 %dmv\nadc34 %dmV\nadc35 %dmv\nadc25 %dmv\n", ADC_PIN32.voltage, ADC_PIN34.voltage, ADC_PIN35.voltage, ADC_PIN25.voltage);
    }
}
uint32_t get_adc_value(int adc_pin, bool raw_type)
{
    switch (adc_pin) {
    case 32:
        if (!raw_type)
            return ADC_PIN32.voltage;
        else
            return ADC_PIN32.raw;
        break;
    case 34:
        if (!raw_type)
            return ADC_PIN34.voltage;
        else
            return ADC_PIN34.raw;
        break;
    case 35:
        if (!raw_type)
            return ADC_PIN35.voltage;
        else
            return ADC_PIN35.raw;
        break;
    case 25:
        if (!raw_type)
            return ADC_PIN25.voltage;
        else
            return ADC_PIN25.raw;
        break;
    default:
        break;
    }
    return 0;
}
void setVoltageLimit(int NO2,int ALL,int NH4,int CO)
{
    if(NO2!=0)
        limit_voltage_NO2 = NO2;
    if(ALL!=0)
        limit_voltage_ALL = ALL;
    if(NH4!=0)
        limit_voltage_NH4 = NH4;
    if(CO!=0)
        limit_voltage_CO = CO;
    ESP_LOGI(TAG,"Setting limit NO2:%d,ALL:%d,NH4:%d,CO:%d",limit_voltage_NO2,limit_voltage_ALL,limit_voltage_NH4,limit_voltage_CO);
}
void setAlarmControl(int data)
{
    ALARMCONTROL = data;
    ESP_LOGI(TAG,"Alarm control now is %d",ALARMCONTROL);
}
void adc_initConfig(void)
{
    xTaskCreate(AdcMainTask, "adc_task", 1024 * 8, NULL, 0, NULL);
}
EventGroupHandle_t getEventGroup(void)
{
    return adcEventGroup;
}