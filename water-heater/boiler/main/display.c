#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"

void display_show(char* text)
{
    static bool init = false;
    static lv_obj_t *label;
    if (!init) {
        init = true;
        ESP_LOGI("example", "Display LVGL animation");
        lv_display_t *d = bsp_display_start();
        bsp_display_lock(0);
        bsp_display_rotate(d, LV_DISPLAY_ROTATION_90);

        lv_obj_t *scr = lv_disp_get_scr_act(NULL);

        label = lv_label_create(scr);
        lv_label_set_text(label, text);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
        bsp_display_unlock();
        bsp_display_backlight_on();
    } else {
        bsp_display_lock(0);
        lv_label_set_text(label, text); // Update the text
        bsp_display_unlock();
    }
}

void display_chart(uint8_t *points, int num_points, int current, int hours, float temp, int thres, int mode)
{

    lv_display_t *d = bsp_display_start();
    bsp_display_lock(0);
    bsp_display_rotate(d, LV_DISPLAY_ROTATION_90);
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    lv_obj_t *label = lv_label_create(scr);
    char *text;
    asprintf(&text, "[%02d:%02d] %.2f~C [%d]%d", hours/100, hours%100, temp, mode, thres);
    lv_label_set_text(label, text);
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
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 40, 140);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, num_points);

    // Add a data series to the chart
    lv_chart_series_t *temp_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED),
                                                         LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_series_t *ser2 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_point_count(chart, num_points);
    lv_obj_add_style(chart, &style_points, 0);

    // Set up the chart to display points from the temp array
    for (int i = current; i < num_points; i++) {
        lv_chart_set_next_value(chart, temp_series, 0x7F & points[i]);
        lv_chart_set_next_value(chart, ser2, (0x80 & points[i])?100:50 );
    }
    for (int i = 0; i < current; i++) {
        lv_chart_set_next_value(chart, temp_series, 0x7F & points[i]);
        lv_chart_set_next_value(chart, ser2, (0x80 & points[i])?100:50 );
    }

    // Refresh the chart to display data
    lv_chart_refresh(chart);


    bsp_display_unlock();
    bsp_display_backlight_on();
    vTaskDelay(pdMS_TO_TICKS(10000));
    free(text);
    bsp_display_backlight_off();
}
