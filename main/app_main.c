#include "app_main.h"

static void MyTask(void* parm)
{
    static char fuck[200],timestamp[200];
    uint32_t adc_pin32, adc_pin34, adc_pin35, ADC_PIN25;
    static char topic[200];
    sprintf(topic, "WIFI/DNSEN51-0/0/%s/TX/sensors", get_macAddress());
    while (1) {
        get_time(timestamp);
        xEventGroupWaitBits(getEventGroup(),BIT0,true,false,pdMS_TO_TICKS(15000));
        adc_pin32 = get_adc_value(32, false); // set false to return Voltage not the raw
        adc_pin34 = get_adc_value(34, false);
        adc_pin35 = get_adc_value(35, false);
        ADC_PIN25 = get_adc_value(25, false);
        sprintf(fuck,
            "{\"Temperature\":\"%.1f\",\"Humidity\":\"%d\",\"NO2\":\"%d\",\"ALL\":\"%d\",\"NH4\":\"%d\",\"CO\":\"%d\",\"Alarm\":\"%d\","
            "\"timestamp\":"
            "\"%s\"}",
            get_temp(), get_hum(), adc_pin32, adc_pin34, adc_pin35, ADC_PIN25, gpio_get_level(CLOSE_POWER), timestamp);
        mqtt_send(fuck, topic, 1);
        // vTaskDelay(pdMS_TO_TICKS(15000));
    }
    vTaskDelete(NULL);
}
void app_main()
{
    ledc_init();
    gpio_pin_init();
    adc_initConfig();
    wifi_init();
    if(gpio_get_level(CLOSE_POWER)){
        ESP_LOGI(tag,"Battery mode");
        gpio_set_level(BLED, 1);
    }
    else{
        ESP_LOGI(tag,"AC mode");
        gpio_set_level(BLED, 0);
    }
    ledc_toggle(1500);
    dht11_enable();
    xTaskCreate(MyTask, "mytask", 1024 * 8, NULL, 0, NULL);
    vTaskDelete(NULL);
}