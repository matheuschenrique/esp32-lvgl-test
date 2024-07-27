/*
 * SDA - 23
 * SCL - 18
 * CS - 15
 * DC - 5
 * RESET - 4
 * SNTP inspired by: https://github.com/vinothkannan369/ESP32/blob/main/SNTP/set_clk.c
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "lvgl_app.h"

#include <dht.h>

lv_obj_t *temp_arc;
lv_obj_t *humidity_arc;
lv_obj_t *temp_text;
lv_obj_t *humidity_text;
lv_obj_t *clock_text;

SemaphoreHandle_t xGuiSemaphore;

/*********************
 *      DEFINES
 *********************/
#define SENSOR_TYPE DHT_TYPE_DHT11

#define WIFI_SSID      "MATHEUS"
#define WIFI_PASS      "33236839"

static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define SNTP_TASK_BIT      BIT2
static const char *TAG = "wifi station";

char current_date_time[100];

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void dht_read(void *pvParameters);
static void sntp_task(void *pvParameters);
static void wifi_task(void *pvParameters);
static void wifi_config();
static void set_time();

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {

    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 1, NULL, 1);
    xTaskCreate(wifi_task, "wifi", 4096, NULL, 1, NULL);
    xTaskCreate(dht_read, "dht", configMINIMAL_STACK_SIZE * 3, NULL, 1, NULL);
    xTaskCreate(sntp_task, "sntp", configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL);
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
       // if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
		//{
            esp_wifi_connect();
           // s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        //} else {
        //    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        //s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT | SNTP_TASK_BIT);
    }
}

void wifi_config() {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_DISCONNECTED,
                    &event_handler,
                    NULL,
                    NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    vTaskDelay(200 / portTICK_PERIOD_MS);

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

void get_datetime(char *date_time){
	char strftime_buf[64];
	time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Set timezone to Indian Standard Time
    setenv("TZ", "UTC+03:00", 1);
    tzset();
    localtime_r(&now, &timeinfo);

    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    strcpy(date_time,strftime_buf);
}

static void initialize_sntp(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
    esp_sntp_init();
}

static void obtain_time(void)
{
    initialize_sntp();
    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
}

void set_time()  {

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
}

void sntp_task(void *pvParameters) {
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           SNTP_TASK_BIT, 
                                           pdTRUE,
                                           pdFALSE,
                                           portMAX_DELAY);
    if (bits & SNTP_TASK_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected. Starting SNTP initialization.");
        while(1) {
            get_datetime(current_date_time);
            printf("current d/t = %s\n", current_date_time);
            char buf[10];
            snprintf(buf, sizeof(buf), "%.8s", current_date_time+11);
            if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                lv_label_set_text(clock_text, buf);
                xSemaphoreGive(xGuiSemaphore);
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

void dht_read(void *pvParameters) {
    float temp, hum;

    while(1) {
        if (dht_read_float_data(SENSOR_TYPE, GPIO_NUM_19, &hum, &temp) == ESP_OK) {
            printf("HUM: %.2f TEMP: %.2f\n", hum, temp);
            if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
                // lv_arc_set_value(temp_arc, (int16_t)map(temp, -20, 50, 0, 100));
                lv_arc_set_value(temp_arc, (int16_t)(temp));
                char buf[10];
                snprintf(buf, sizeof(buf), "%.2f%s", temp, degree_symbol);
                lv_label_set_text(temp_text, buf);

                lv_arc_set_value(humidity_arc, (int16_t)(hum));
                snprintf(buf, sizeof(buf), "%.2f%%", hum);
                lv_label_set_text(humidity_text, buf);
                xSemaphoreGive(xGuiSemaphore);
            }
            
        } else {
            printf("ERROR READING SENSOR\n");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void wifi_task(void *pvParameters) {
    wifi_config();
    vTaskDelay(200 / portTICK_PERIOD_MS);
    set_time();
    vTaskDelete(NULL);
}