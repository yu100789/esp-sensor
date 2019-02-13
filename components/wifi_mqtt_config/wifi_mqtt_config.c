#include "wifi_mqtt_config.h"

static const char* TAG = "wifi";

esp_mqtt_client_handle_t user_client = 0;
wifi_config_t* wifi_config;
static wifi_state_t wifistate = 0;
static char mac_buf[6];
static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static int s_retry_num = 0;
static TaskHandle_t smartconfig_example_task_handle = NULL;
static char setvalue[200],alarmcontrol[200],lastwillTopic[200],ota_info[100];

static void mqtt_app_start(void);
static void smartconfig_example_task(void* parm);

void changeWifiState(uint8_t state) { wifistate = state; }
void judgement(int data_len, const char* event_data, int topic_len)
{
    if (topic_len == strlen(OTA_TOPIC)) {
        float old, new;
        old = strtof(OTA_VERSION, NULL);
        new = strtof(event_data, NULL);
        ESP_LOGI(TAG, "current version: %.1f,   available version: %.1f", old, new);
        if (new > old)
            ota_start();
    }else if(topic_len == strlen(setvalue)){
        cJSON* json_data = cJSON_Parse(event_data);
        int NO2 = atoi(cJSON_GetObjectItem(json_data, "NO2")->valuestring);
        int ALL = atoi(cJSON_GetObjectItem(json_data, "ALL")->valuestring);
        int NH4 = atoi(cJSON_GetObjectItem(json_data, "NH4")->valuestring);
        int CO = atoi(cJSON_GetObjectItem(json_data, "CO")->valuestring);
        setVoltageLimit(NO2,ALL,NH4,CO);
        cJSON_Delete(json_data);
    }else if(topic_len == strlen(alarmcontrol)){
        cJSON* json_data = cJSON_Parse(event_data);
        int ALARMCONTROL = atoi(cJSON_GetObjectItem(json_data, "ALARMCONTROL")->valuestring);
        setAlarmControl(ALARMCONTROL);
        cJSON_Delete(json_data);
    }else{
        ESP_LOGE(TAG, "Find nothing match with topic");
    }
    ESP_LOGI(TAG, "free memory : %d Bytes", esp_get_free_heap_size());
}
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    // your_context_t *context = event->context;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        wifistate = WIFI_MQTT_CONNECTED;
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_publish(event->client,lastwillTopic,"online",6,1,1);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_mqtt_client_subscribe(event->client, OTA_TOPIC, 2);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_mqtt_client_subscribe(event->client, setvalue, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_mqtt_client_subscribe(event->client, alarmcontrol, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        sprintf(ota_info, "WIFI/DNSEN51-0/0/%s/TX/version", get_macAddress());
        esp_mqtt_client_publish(event->client,ota_info,OTA_VERSION,strlen(OTA_VERSION),1,1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        wifistate = WIFI_CONNECTED;
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_TOPIC\n %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "MQTT_EVENT_DATA\n %.*s", event->data_len, event->data);
        judgement(event->data_len, event->data, event->topic_len);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGE(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    }
    return ESP_OK;
}
void mqtt_send(const char* data, const char* topic, int retain)
{
    if (wifistate == WIFI_MQTT_CONNECTED) {
        ESP_LOGI(TAG, "SEND CONTENT : %s\ntopic : %s", data, topic);
        esp_mqtt_client_publish(user_client, topic, data, strlen(data), 0, retain);
    } else {
        ESP_LOGE(TAG, "WIFI NOT CONNECT %s", data);
    }
}
static esp_err_t wifi_event_handler(void* ctx, system_event_t* event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_AP_START:
        start_tests();
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d", MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR "leave, AID=%d", MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_START:
        if(ESP_ERR_WIFI_SSID == esp_wifi_connect()){
            if (smartconfig_example_task_handle == NULL){
                xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 1024 * 4, NULL, 6, &smartconfig_example_task_handle);
            }
        }
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        wifistate = WIFI_CONNECTED;
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        mqtt_app_start();
        if (ENABLE_HTTP_SERVER)
            start_tests(); // httpserver
        xTaskCreate(sntp_initialize, "sntp_initialize", 1024 * 8, NULL, 0, NULL);
        s_retry_num = 0;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        wifistate = WIFI_DISCONNECT;
        ESP_LOGE(TAG, "DISCONNECT REASON : %d", event->event_info.disconnected.reason);
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_connect();
        if (s_retry_num < 5) {
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            if (smartconfig_example_task_handle != NULL){
                vTaskDelete(smartconfig_example_task_handle);
                smartconfig_example_task_handle = NULL;
            }
            xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 1024 * 4, NULL, 6, &smartconfig_example_task_handle);
            s_retry_num = 0;
        }
        if (user_client != NULL) {
            esp_mqtt_client_destroy(user_client);
            user_client = NULL;
        }
        ESP_LOGE(TAG, "connect to the AP fail\n");
        break;
    default:
        break;
    }
    return ESP_OK;
}
static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .host = MQTT_HOST_IP,
        .port = MQTT_HOST_PORT,
        .username = MQTT_ID,
        .password = MQTT_PW,
        .keepalive = 180,
        .disable_auto_reconnect = false,
        .lwt_topic = lastwillTopic,                  /*!< LWT (Last Will and Testament) message topic (NULL by default) */
        .lwt_msg = "offline",                    /*!< LWT message (NULL by default) */
        .lwt_qos = 2,                            /*!< LWT message qos */
        .lwt_retain = true,                         /*!< LWT retained message flag */
        .lwt_msg_len = 7,   
        .event_handle = mqtt_event_handler,
        .task_prio = 10,
    };
    user_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(user_client);
}
static void sc_callback(smartconfig_status_t status, void* pdata)
{
    switch (status) {
    case SC_STATUS_WAIT:
        ESP_LOGI(TAG, "SC_STATUS_WAIT");
        break;
    case SC_STATUS_FIND_CHANNEL:
        wifistate = SMARTCONFIGING; //   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
        ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
        break;
    case SC_STATUS_GETTING_SSID_PSWD:
        wifistate = SMARTCONFIG_GETTING;
        ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
        break;
    case SC_STATUS_LINK:
        ESP_LOGI(TAG, "SC_STATUS_LINK");
        wifi_config = (wifi_config_t*)pdata;
        ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_STA, WIFI_BAND));
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SC_STATUS_LINK_OVER:
        ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
        if (pdata != NULL) {
            uint8_t phone_ip[4] = { 0 };
            memcpy(phone_ip, (uint8_t*)pdata, 4);
            ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
        }
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
        break;
    default:
        break;
    }
}

static void smartconfig_example_task(void* parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_stop());
    ESP_ERROR_CHECK(esp_esptouch_set_timeout(60));
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    ESP_ERROR_CHECK(esp_smartconfig_start(sc_callback));
    while (1) {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, pdMS_TO_TICKS(100000));
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            smartconfig_example_task_handle = NULL;
            vTaskDelete(NULL);
        }
        if (uxBits == 0){
            ESP_LOGI(TAG,"SmartLink Timeout");
            esp_restart();
        }
    }
}
static void wifi_state_event(void* parm)
{
    ESP_ERROR_CHECK(gpio_set_direction(WIFI_LED, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(WIFI_LED, GPIO_PULLUP_ONLY));
    while (1) {
        switch (wifistate) {
        case WIFI_DISCONNECT:
            vTaskDelay(pdMS_TO_TICKS(3000));
            gpio_set_level(WIFI_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(300));
            break;
        case WIFI_CONNECTED:
            vTaskDelay(pdMS_TO_TICKS(1000));
            gpio_set_level(WIFI_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        case WIFI_MQTT_CONNECTED:
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(WIFI_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        case SMARTCONFIGING:
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(WIFI_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case SMARTCONFIG_GETTING:
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(WIFI_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            break;
        case OTA_RUNNING:
            vTaskDelay(pdMS_TO_TICKS(25));
            gpio_set_level(WIFI_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(25));
            break;
        default:
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }
        gpio_set_level(WIFI_LED, 0);
    }
}
void wifi_init(void)
{
    sprintf(setvalue, "WIFI/DNSEN51-0/0/%s/RX/setvalue", get_macAddress());
    sprintf(alarmcontrol, "WIFI/DNSEN51-0/0/%s/RX/alarmcontrol", get_macAddress());
    sprintf(lastwillTopic, "WIFI/DNSEN51-0/0/%s/TX/lastwill", get_macAddress());
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
    nvs_flash_init();
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    esp_read_mac((uint8_t*)mac_buf, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "factory set MAC address: %02x:%02x:%02x:%02x:%02x:%02x", mac_buf[0], mac_buf[1], mac_buf[2], mac_buf[3], mac_buf[4], mac_buf[5]);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_STA, WIFI_BAND));
    xTaskCreate(wifi_state_event, "wifi_state", 1024, NULL, 0, NULL);
}
const char* get_macAddress(void)
{
    static char mac[13];
    esp_read_mac((uint8_t*)mac_buf, ESP_MAC_WIFI_STA);
    sprintf(mac, "%02x%02x%02x%02x%02x%02x", mac_buf[0], mac_buf[1], mac_buf[2], mac_buf[3], mac_buf[4], mac_buf[5]);
    return mac;
}
void smartlinkStart(void)
{
    // if(wifistate == WIFI_CONNECTED || wifistate == WIFI_MQTT_CONNECTED){
        s_retry_num = 10;
        ESP_ERROR_CHECK(esp_wifi_disconnect());
    // }
}