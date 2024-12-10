#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "my_wifi.h"

void display_show(char* text);

static const char *TAG = "wifi";
static EventGroupHandle_t s_wifi_event_group;



static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG,"connect to the AP fail");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, 1);
    }
}


esp_err_t wifi_connect(bool quick)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    wifi_config_t wifi_config = {
            .sta = {
                    .ssid = WIFI_SSID,
                    .password = WIFI_PASSWORD,

            },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    int retry = 0;
    char *text;
    const int retry_count = quick ? 40 : 120;
    EventBits_t bits = 0;
    while (bits == 0) {
        asprintf(&text, "connecting to wifi... %d", retry++);
        display_show(text);
        free(text);
        bits = xEventGroupWaitBits(s_wifi_event_group,1,pdTRUE,pdTRUE, pdMS_TO_TICKS(500));
        if (retry >= retry_count) {
            return ESP_FAIL;
        }
    }

    if (bits & 1) {
        ESP_LOGI(TAG, "Initializing SNTP");
        display_show("updating system time!");
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2, ESP_SNTP_SERVER_LIST("time.windows.com", "pool.ntp.org" ) );
        esp_netif_sntp_init(&config);
        retry = 0;
        while (esp_netif_sntp_sync_wait(500 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT) {
            asprintf(&text, "waiting for SNTP... %d", retry++);
            display_show(text);
            free(text);
            ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
            if (retry > retry_count) {
                return ESP_FAIL;
            }
        }
        time_t now = 0;
        struct tm timeinfo = { 0 };
        time(&now);
        localtime_r(&now, &timeinfo);
        char strftime_buf[64];
        // Set timezone to Eastern Standard Time and print local time
        setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
        tzset();
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        asprintf(&text, "%s", strftime_buf);
        display_show(text);
        vTaskDelay(pdMS_TO_TICKS(1000));
        free(text);
    }
    return ESP_OK;

}
