/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

static const char *TAG = "example";

#define EXAMPLE_ONEWIRE_BUS_GPIO    26
#define EXAMPLE_ONEWIRE_MAX_DS18B20 2

#define NUM_POINTS 320
#define TEMP_MIN 25
#define TEMP_MAX 65
static RTC_DATA_ATTR uint8_t temp[NUM_POINTS];
static RTC_DATA_ATTR uint16_t count = 0;
static RTC_DATA_ATTR uint8_t level = 0;


const int ext_wakeup_pin_0 = 25;

void example_deep_sleep_register_ext0_wakeup(void)
{
    printf("Enabling EXT0 wakeup on pin GPIO%d\n", ext_wakeup_pin_0);
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(ext_wakeup_pin_0, 1));

    // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
    // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
    // No need to keep that power domain explicitly, unlike EXT1.
    ESP_ERROR_CHECK(rtc_gpio_pullup_dis(ext_wakeup_pin_0));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_en(ext_wakeup_pin_0));
}



void display(void) {

    lv_display_t *d = bsp_display_start();
    bsp_display_rotate(d, LV_DISPLAY_ROTATION_90);

    ESP_LOGI("example", "Display LVGL animation");
    bsp_display_lock(0);
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Temperature Over Time");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_t *chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 320, 180);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 20);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);   /*Show lines and points too*/

    static lv_style_t style_points;
    lv_style_init(&style_points);
    lv_style_set_width(&style_points, 1);   // Adjust point width (size)
    lv_style_set_height(&style_points, 1);  // Adjust point height (size)

    // Set the chart's y-axis range to the temperature scale (25 to 65 degrees)
//    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 200);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, NUM_POINTS);

    // Add a data series to the chart
    lv_chart_series_t *temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED),
                                                         LV_CHART_AXIS_PRIMARY_Y);
//    lv_chart_series_t *ser2 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_point_count(chart, NUM_POINTS);
    lv_obj_add_style(chart, &style_points, 0);

    // Set up the chart to display points from the temp array
    for (int i = count; i < NUM_POINTS; i++) {
        lv_chart_set_next_value(chart, temp_series, temp[i]);
//        lv_chart_set_next_value(chart, ser2, (i + 25) % 25 + 10);
    }
    for (int i = 0; i < count; i++) {
        lv_chart_set_next_value(chart, temp_series, temp[i]);
//        lv_chart_set_next_value(chart, ser2, (i + 25) % 25 + 10);
    }

    // Refresh the chart to display data
    lv_chart_refresh(chart);


    bsp_display_unlock();
    bsp_display_backlight_on();
    vTaskDelay(pdMS_TO_TICKS(10000));
    bsp_display_backlight_off();

}
#define GPIO_OUT 14
#define LED_OUT 5
#define LED_OUT1 0
#define LED_OUT2 2
#define LED_OUT3 4

void app_main(void)
{
    int ret = esp_sleep_get_wakeup_cause();
    printf("Wakeup cause: %d\n", ret);
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
    gpio_hold_dis(GPIO_OUT);
    gpio_set_level(GPIO_OUT, level);
    gpio_set_level(LED_OUT, 1);
    gpio_set_level(LED_OUT1, 0);
    gpio_set_level(LED_OUT2, 0);
    gpio_set_level(LED_OUT3, 0);
    gpio_hold_en(GPIO_OUT);
    gpio_hold_en(LED_OUT);
    gpio_hold_en(LED_OUT1);
    gpio_hold_en(LED_OUT2);
    gpio_hold_en(LED_OUT3);
    gpio_deep_sleep_hold_en();
    if (ret == ESP_SLEEP_WAKEUP_EXT0) {
        gpio_hold_dis(LED_OUT);
        gpio_set_level(LED_OUT, 0);
        display();
        esp_deep_sleep(1000000LL * 1);
//        vTaskDelay(pdMS_TO_TICKS(10000));
//        gpio_set_level(LED_OUT, 1);
//        gpio_hold_en(LED_OUT);
    }


//    for (int i = 0; i < NUM_POINTS; i++) {
//        temp[i] = i;
//    }

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

    int ds18b20_device_num = 0;
    ds18b20_device_handle_t ds18b20s[EXAMPLE_ONEWIRE_MAX_DS18B20];
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
    ESP_LOGI(TAG, "Searching done, %d DS18B20 device(s) found", ds18b20_device_num);

    // set resolution for all DS18B20s
    for (int i = 0; i < ds18b20_device_num; i++) {
        // set resolution
        ESP_ERROR_CHECK(ds18b20_set_resolution(ds18b20s[i], DS18B20_RESOLUTION_12B));
    }

    // get temperature from sensors one by one
    float temperature;
    ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds18b20s[0]));
    ESP_ERROR_CHECK(ds18b20_get_temperature(ds18b20s[0], &temperature));
    ESP_LOGI(TAG, "temperature read from DS18B20[%d]: %.2fC", 0, temperature);
    temp[count] = (temperature/70.0) * 256;
    ESP_LOGE(TAG, "TEMP %d %d", count, temp[count]);
    ++count;
    count %= NUM_POINTS;
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", 10);
    level = count % 2;
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", level);
    gpio_deep_sleep_hold_en();
    example_deep_sleep_register_ext0_wakeup();
    esp_deep_sleep(1000000LL * 10);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(200));

        for (int i = 0; i < ds18b20_device_num; i ++) {
            ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds18b20s[i]));
            ESP_ERROR_CHECK(ds18b20_get_temperature(ds18b20s[i], &temperature));
            ESP_LOGI(TAG, "temperature read from DS18B20[%d]: %.2fC", i, temperature);
        }
    }
}
