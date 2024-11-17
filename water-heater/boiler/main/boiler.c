/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include <stdio.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "time.h"
#include "math.h"
#include "my_wifi.h"

esp_err_t wifi_connect(void);
void display_chart(uint8_t *points, int num_points, int current, int hours, float temp, int thres, int mode);

static const char *TAG = "boiler";

#define EXAMPLE_ONEWIRE_BUS_GPIO    26
#define EXAMPLE_ONEWIRE_MAX_DS18B20 2

#define NUM_POINTS 320

typedef struct interval {
    int from;
    int to;
    int temp;
    char* desc;
} interval_t;

static RTC_DATA_ATTR uint8_t temp[NUM_POINTS] = {};
static RTC_DATA_ATTR uint16_t count = 0;
static RTC_DATA_ATTR uint8_t level = 0;
static const uint16_t temp_low = 40;
//static const interval_t intervals[] =  {  { 430, 530, 45 },
//                                          { 1400, 1600, 45},
//                                          { 600, 1900, 23},
//                                          { 1900, 2100, 40} };
static const interval_t intervals[] =  {
      { 900, 1700, 35, "Day" },
      { 1440, 1501, 45, "Af1" },
      { 1500, 1531, 50, "Af2" },
      { 1530, 1600, 55, "Af3" },
      { 1900, 2101, 42, "Eve"},
      { 2100, 2131, 45, "Ev1"},
      { 2130, 2230, 40, "Ev2" },
      { 400, 431, 42, "Mo1" },
      { 430, 510, 50, "Mo2"} };


static void example_deep_sleep_register_ext0_wakeup(void)
{
    const int ext_wakeup_pin_0 = 25;
    printf("Enabling EXT0 wakeup on pin GPIO%d\n", ext_wakeup_pin_0);
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(ext_wakeup_pin_0, 1));

    // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
    // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
    // No need to keep that power domain explicitly, unlike EXT1.
    ESP_ERROR_CHECK(rtc_gpio_pullup_dis(ext_wakeup_pin_0));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_en(ext_wakeup_pin_0));
}

static float get_single_temp(ds18b20_device_handle_t h)
{
    float temperature;
    ESP_ERROR_CHECK(ds18b20_get_temperature(h, &temperature));
    ESP_LOGI(TAG, "temperature read from DS18B20[%d]: %.2fC", 0, temperature);
    return temperature;
}

static float get_temperature(ds18b20_device_handle_t h)
{
    float t1 = get_single_temp(h);
    float t2 = get_single_temp(h);
    const int retry_count = 5;
    int retry = 0;

    while (fabsf(t1 -t2) > 1.5) {
        t1 = t2;
        t2 = get_single_temp(h);
        if (++retry > retry_count) {
            return 42.0;
        }
    }
    return (t1+t2)/2;
}

#define GPIO_OUT 14
#define LED_OUT 5
#define LED_OUT1 0
#define LED_OUT2 2
#define LED_OUT3 4

void app_main(void)
{
    int wakeup_reason = esp_sleep_get_wakeup_cause();
    printf("Wakeup cause: %d\n", wakeup_reason);

    time_t now;
    struct tm timeinfo;
    time(&now);
//    localtime_r(&now, &timeinfo);
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).


    gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(GPIO_OUT) | BIT64(LED_OUT) | BIT64(LED_OUT1) | BIT64(LED_OUT2) | BIT64(LED_OUT3),
    };
    gpio_config(&io_conf);

//    while (1) {
//        gpio_set_level(GPIO_OUT, 0);
//        vTaskDelay(pdMS_TO_TICKS(1000));
//        gpio_set_level(GPIO_OUT, 1);
//        vTaskDelay(pdMS_TO_TICKS(1000));
//    }
//    gpio_hold_dis(GPIO_OUT);
//    gpio_set_level(GPIO_OUT, level);
    gpio_set_level(LED_OUT, 1);
    gpio_set_level(LED_OUT1, 0);
    gpio_set_level(LED_OUT2, 0);
    gpio_set_level(LED_OUT3, 0);
//    gpio_hold_en(GPIO_OUT);
    gpio_hold_en(LED_OUT);
    gpio_hold_en(LED_OUT1);
    gpio_hold_en(LED_OUT2);
    gpio_hold_en(LED_OUT3);
    gpio_deep_sleep_hold_en();
    if (timeinfo.tm_year < (2016 - 1900)) {
        level = 0;
        gpio_hold_dis(LED_OUT);
        gpio_set_level(LED_OUT, 0);
        gpio_hold_dis(GPIO_OUT);
        gpio_set_level(GPIO_OUT, 0);
        gpio_hold_en(GPIO_OUT);
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        if (wifi_connect() != ESP_OK) {
            esp_deep_sleep(1000000LL * 10);
        }
        esp_deep_sleep(1000000LL * 1);
    }
    int hours = timeinfo.tm_hour*100 + timeinfo.tm_min;
    ESP_LOGE(TAG, "Time [%02d:%02d]", hours/100, hours%100);

    // install new 1-wire bus
    onewire_bus_handle_t bus;
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = EXAMPLE_ONEWIRE_BUS_GPIO,
    };
    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10, // 1byte ROM command + 8byte ROM number + 1byte device command
    };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));
    ESP_LOGI(TAG, "1-Wire bus installed on GPIO%d", EXAMPLE_ONEWIRE_BUS_GPIO);

//    FF1161811E64FF28

    int ds18b20_device_num = 0;
    ds18b20_device_handle_t ds18b20s[EXAMPLE_ONEWIRE_MAX_DS18B20];
#if ONEWIRE_AUTODETECT
    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next_onewire_device;
    esp_err_t search_result = ESP_OK;

    // create 1-wire device iterator, which is used for device search
    ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
    ESP_LOGI(TAG, "Device iterator created, start searching...");
    do {
        search_result = onewire_device_iter_get_next(iter, &next_onewire_device);
        if (search_result == ESP_OK) { // found a new device, let's check if we can upgrade it to a DS18B20
            ds18b20_config_t ds_cfg = {};
            if (ds18b20_new_device(&next_onewire_device, &ds_cfg, &ds18b20s[ds18b20_device_num]) == ESP_OK) {
                ESP_LOGI(TAG, "Found a DS18B20[%d], address: %016llX", ds18b20_device_num, next_onewire_device.address);
                ds18b20_device_num++;
                if (ds18b20_device_num >= EXAMPLE_ONEWIRE_MAX_DS18B20) {
                    ESP_LOGI(TAG, "Max DS18B20 number reached, stop searching...");
                    break;
                }
            } else {
                ESP_LOGI(TAG, "Found an unknown device, address: %016llX", next_onewire_device.address);
            }
        }
    } while (search_result != ESP_ERR_NOT_FOUND);
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));
#else
    ds18b20_config_t cfg = {};
    onewire_device_t dev = { .address = 0xFF1161811E64FF28, .bus = bus };
    ds18b20_device_num = (ds18b20_new_device(&dev, &cfg, &ds18b20s[0]) == ESP_OK &&
                          ds18b20_trigger_temperature_conversion(ds18b20s[0]) == ESP_OK  ) ? 1 : 0;

#endif
    ESP_LOGI(TAG, "Searching done, %d DS18B20 device(s) found", ds18b20_device_num);
    float temperature;
    if (ds18b20_device_num == 0) {
        temperature = 42.0;
        ESP_LOGI(TAG, "temperature not available, using %.2fC", temperature);
    } else {
        // set resolution
        ESP_ERROR_CHECK(ds18b20_set_resolution(ds18b20s[0], DS18B20_RESOLUTION_12B));
        // get temperature
        temperature = get_temperature(ds18b20s[0]);
    }
    ESP_LOGI(TAG, "Using temperature %.2f~C", temperature);

    int temp_threshold = temp_low;
    int mode;
    for (mode = 0; mode<9; ++mode) {
        if (hours > intervals[mode].from && hours < intervals[mode].to) {
            temp_threshold = intervals[mode].temp;
            ESP_LOGI(TAG, "Found mode %d", mode);;
            break;
        }
    }
    ESP_LOGI(TAG, "Using temp threshold %d", temp_threshold);
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        gpio_hold_dis(LED_OUT);
        gpio_set_level(LED_OUT, 0);
        display_chart(temp, NUM_POINTS, count, hours, temperature, temp_threshold, mode);
        esp_deep_sleep(1000000LL * 1);
    }
    temp[count] = temperature*2;
    ESP_LOGE(TAG, "TEMP %d %d", count, temp[count]);

    if (temperature < temp_threshold && level == 0)  {
        level = 1;
        ESP_LOGI(TAG, "Switching ON: %f < %f", temperature, 1.0*temp_threshold);
        gpio_hold_dis(GPIO_OUT);
        gpio_set_level(GPIO_OUT, 1);
        gpio_hold_en(GPIO_OUT);
        gpio_deep_sleep_hold_en();
    } else if (temperature > (temp_threshold + 1) && level == 1) {
        level = 0;
        ESP_LOGI(TAG, "Switching OFF: %f ~> %f", temperature, 1.0*temp_threshold);
        gpio_hold_dis(GPIO_OUT);
        gpio_set_level(GPIO_OUT, 0);
        gpio_hold_en(GPIO_OUT);
        gpio_deep_sleep_hold_en();
    }
    temp[count] |= level ? 0x80: 0;
    ++count;
    count %= NUM_POINTS;

    gpio_deep_sleep_hold_en();
    example_deep_sleep_register_ext0_wakeup();
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", 60);
    esp_deep_sleep(1000000LL * 60);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(200));

        for (int i = 0; i < ds18b20_device_num; i ++) {
            ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds18b20s[i]));
            ESP_ERROR_CHECK(ds18b20_get_temperature(ds18b20s[i], &temperature));
            ESP_LOGI(TAG, "temperature read from DS18B20[%d]: %.2fC", i, temperature);
        }
    }
}
