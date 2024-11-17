/*
    LilyGo Ink Screen Series u8g2Fonts Test
        - Created by Lewis he
*/
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include <WiFi.h>
#include "time.h"

#define GPIO_OUT ((gpio_num_t)33)
#define LED_OUT ((gpio_num_t)32)


RTC_DATA_ATTR static uint16_t temp[250] = { 0 };
RTC_DATA_ATTR static int temp_i = 0;
RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static int status = 0;

static const char *TAG = "boiler";



typedef struct interval {
    uint16_t from;
    uint16_t to;
    uint16_t limit;
    char*    desc;
} interval_t;

RTC_DATA_ATTR static uint16_t temp_low = 40;
// RTC_DATA_ATTR static uint16_t temps[] = { 20, 40, 35, 40 };
// RTC_DATA_ATTR static uint16_t temps[] = { 20, 40, 45, 40 };
// const char* desc[] = {"Default", "Day", "Cheap", "Eve", "Pre"};
RTC_DATA_ATTR static interval_t intervals[] =  {  { 700, 1800, 35, "Day" },
                                                  { 1440, 1501, 45, "PrePreCheap" },
                                                  { 1500, 1531, 50, "PreCheap" },
                                                  { 1530, 1600, 55, "Cheap" },
                                                  { 1900, 2101, 42, "PreEve"},
                                                  { 2100, 2131, 45, "Evening"},
                                                  { 2130, 2230, 40, "PostEve" },
                                                  { 400, 431, 42, "PreMorn" },
                                                  { 430, 510, 50, "Morning"} };





// According to the board, cancel the corresponding macro definition
#define LILYGO_T5_V213
// #define LILYGO_T5_V22
// #define LILYGO_T5_V24
// #define LILYGO_T5_V28
// #define LILYGO_T5_V102
// #define LILYGO_T5_V266
// #define LILYGO_EPD_DISPLAY_102
// #define LILYGO_EPD_DISPLAY_154

#include "driver/rtc_io.h"
#include <DS18B20.h>

#include <boards.h>
#include <GxEPD.h>

#if defined(LILYGO_T5_V102) || defined(LILYGO_EPD_DISPLAY_102)
#include <GxGDGDEW0102T4/GxGDGDEW0102T4.h> //1.02" b/w
#elif defined(LILYGO_T5_V266)
#include <GxDEPG0266BN/GxDEPG0266BN.h>    // 2.66" b/w   form DKE GROUP
#elif defined(LILYGO_T5_V213)
#include <GxDEPG0213BN/GxDEPG0213BN.h>    // 2.13" b/w  form DKE GROUP
#else
// #include <GxGDGDEW0102T4/GxGDGDEW0102T4.h> //1.02" b/w
// #include <GxGDEW0154Z04/GxGDEW0154Z04.h>  // 1.54" b/w/r 200x200
// #include <GxGDEW0154Z17/GxGDEW0154Z17.h>  // 1.54" b/w/r 152x152
// #include <GxGDEH0154D67/GxGDEH0154D67.h>  // 1.54" b/w
// #include <GxDEPG0150BN/GxDEPG0150BN.h>    // 1.51" b/w   form DKE GROUP
// #include <GxDEPG0266BN/GxDEPG0266BN.h>    // 2.66" b/w   form DKE GROUP
// #include <GxDEPG0290R/GxDEPG0290R.h>      // 2.9" b/w/r  form DKE GROUP
// #include <GxDEPG0290B/GxDEPG0290B.h>      // 2.9" b/w    form DKE GROUP
// #include <GxGDEW029Z10/GxGDEW029Z10.h>    // 2.9" b/w/r  form GoodDisplay
// #include <GxGDEW0213Z16/GxGDEW0213Z16.h>  // 2.13" b/w/r form GoodDisplay
// #include <GxGDE0213B1/GxGDE0213B1.h>      // 2.13" b/w  old panel , form GoodDisplay
// #include <GxGDEH0213B72/GxGDEH0213B72.h>  // 2.13" b/w  old panel , form GoodDisplay
// #include <GxGDEH0213B73/GxGDEH0213B73.h>  // 2.13" b/w  old panel , form GoodDisplay
// #include <GxGDEM0213B74/GxGDEM0213B74.h>  // 2.13" b/w  form GoodDisplay 4-color
// #include <GxGDEW0213M21/GxGDEW0213M21.h>  // 2.13"  b/w Ultra wide temperature , form GoodDisplay
// #include <GxDEPG0213BN/GxDEPG0213BN.h>    // 2.13" b/w  form DKE GROUP
// #include <GxGDEW027W3/GxGDEW027W3.h>      // 2.7" b/w   form GoodDisplay
// #include <GxGDEW027C44/GxGDEW027C44.h>    // 2.7" b/w/r form GoodDisplay
// #include <GxGDEH029A1/GxGDEH029A1.h>      // 2.9" b/w   form GoodDisplay
// #include <GxDEPG0750BN/GxDEPG0750BN.h>    // 7.5" b/w   form DKE GROUP
#endif

#include GxEPD_BitmapExamples
// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <OneWire.h>


GxIO_Class io(SPI,  EPD_CS, EPD_DC,  EPD_RSET);
GxEPD_Class display(io, EPD_RSET, EPD_BUSY);
DS18B20 ds(27);

const char* ssid       = "xxxxxxxxxxxxx";
const char* password   = "yyyyyyyyyyyyy";
// const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer1 = "time.windows.com";

const char* time_zone = "CET-1CEST";

static void try_wifi()
{
  configTzTime(time_zone, ntpServer1);
  //connect to WiFi
  Serial.println("Connecting to access point");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED!");
  Serial.println("\nGetting time...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)){
      delay(1500);
      Serial.print(".");
  }

}

static void blink_task(void *ctx)
{
    gpio_hold_dis(LED_OUT);
    while (1) {
        gpio_set_level(LED_OUT, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_OUT, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    vTaskDelete(NULL);
}


void setup(void)
{
    Serial.begin(115200);

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(GPIO_OUT) | BIT64(LED_OUT);

    gpio_config(&io_conf);
    gpio_hold_dis(LED_OUT);
    gpio_set_level(LED_OUT, 0);
    gpio_hold_en(LED_OUT);
    gpio_deep_sleep_hold_en();
    // gpio_hold_dis(GPIO_OUT);
    // gpio_set_level(GPIO_OUT, status);
    // gpio_hold_en(GPIO_OUT);
    // gpio_deep_sleep_hold_en();


    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).

    if (timeinfo.tm_year < (2016 - 1900)) {
        status = 0;
        gpio_hold_dis(GPIO_OUT);
        gpio_set_level(GPIO_OUT, 0);
        gpio_hold_en(GPIO_OUT);
        gpio_deep_sleep_hold_en();
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        xTaskCreate(blink_task, "blink", 1024, NULL, 5, NULL);
        try_wifi();
        status = 0;
        // esp_deep_sleep(1000000LL); // sleep for one second
    }


#if defined(LILYGO_EPD_DISPLAY_102)
    pinMode(EPD_POWER_ENABLE, OUTPUT);
    digitalWrite(EPD_POWER_ENABLE, HIGH);
#endif /*LILYGO_EPD_DISPLAY_102*/
#if defined(LILYGO_T5_V102)
    pinMode(POWER_ENABLE, OUTPUT);
    digitalWrite(POWER_ENABLE, HIGH);
#endif /*LILYGO_T5_V102*/

    SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI);
    display.init(); // enable diagnostic output on Serial

    // LilyGo_logo();
    // Serial.println("setup done");
}

static int temp_to_pixel(int temp)
{
  return 120 - 120.0*((double)temp/16.0 - 20)/50;
  // return 120 - 120.0*(temp - 18)/10;
}

void loop()
{
    static int i = 0;
    display.setRotation(1);
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(0, 20);
    // display.printf("%d\n", i++);
    // int x = (20 + i) % 200;
    // int y = (20+ i) % 100;
    double temperature = ds.getTempC();
    temp[temp_i] = temperature*16;
    temp[temp_i] |= status ? 0x8000: 0;
    temp_i = (temp_i+1)%250;
    Serial.printf("temp_i %d\n", temp_i);
    configTzTime(time_zone, ntpServer1);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      esp_restart();
    }


    int hours = timeinfo.tm_hour*100 + timeinfo.tm_min;
    // display.drawRect(x, y, 60+x, 60+y, GxEPD_BLACK);

    uint16_t temp_threshold = temp_low;
    int mode = -1;
    for (int i=0; i<9; ++i) {
        if (hours > intervals[i].from && hours < intervals[i].to) {
            temp_threshold = intervals[i].limit;
            mode = i;
            Serial.printf("Found mode %d\n", i);
            // break;
        }
    }
    Serial.printf("Using temp threshold %d\n", temp_threshold);
    Serial.printf("%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
    Serial.printf("Temp: %2.2f\n", temperature);

    // temperature = (temp_i & 1)? 15 : 50;
    if (temperature < 10 || temperature > 70) {
      temperature = 43;
    }
    Serial.printf("Temp after: %2.2f\n", temperature);
    int change = 0;
    {
    if (temperature < temp_threshold && status == 0)  {
        status = 1;
        change = 1;
        Serial.printf("Switching ON: %f < %f\n", temperature, 1.0*temp_threshold);
        gpio_hold_dis(GPIO_OUT);
        gpio_set_level(GPIO_OUT, 1);
        gpio_hold_en(GPIO_OUT);
        gpio_deep_sleep_hold_en();
    } else if (temperature > (temp_threshold + 1) && status == 1) {
        status = 0;
        change = 1;
        Serial.printf("Switching OFF: %f > %f\n", temperature, 1.0*temp_threshold);
        gpio_hold_dis(GPIO_OUT);
        gpio_set_level(GPIO_OUT, 0);
        gpio_hold_en(GPIO_OUT);
        gpio_deep_sleep_hold_en();
    }
    }
    Serial.printf("%s\n%02d [%s]", mode>=0 ? intervals[mode].desc : "Default", temp_threshold, status?"ON":"OFF");
    

    if (0) { // {temp_i % 8 == 0 || change) {
      for (int i=temp_i; i<250; ++i) {
        if (temp[i] & 0x8000) {
          display.drawLine(i - temp_i, 120, i - temp_i, temp_to_pixel(temp[i]&0x7FFF), GxEPD_BLACK);
        } else {
          display.drawPixel(i - temp_i, temp_to_pixel(temp[i]&0x7FFF), GxEPD_BLACK);
        }
      }
      for (int i=0; i<temp_i; ++i) {
        if (temp[i] & 0x8000) {
          display.drawLine(249 - temp_i + i, 120, 249 - temp_i + i, temp_to_pixel(temp[i]&0x7FFF), GxEPD_BLACK);
        } else {
          display.drawPixel(249 - temp_i + i, temp_to_pixel(temp[i]&0x7FFF), GxEPD_BLACK);
        }
      }
      display.printf("%2.2f\n", temperature);
      display.printf("%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
      if (mode >= 0) {
        display.printf("%s\n%02d [%s]", intervals[mode].desc, temp_threshold, status?"ON":"OFF");
      }
      display.update();
      display.powerDown();
    }


    esp_deep_sleep(60*1000000LL); // sleep for one minute
    // esp_deep_sleep(10*1000000LL); // sleep for one minute
}
