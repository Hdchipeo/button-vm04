/*  Temperature Sensor demo implementation using RGB LED and timer

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <sdkconfig.h>
#include <string.h>

#include "app_priv.h"
#include "esp_now_hub.h"

#include "driver/gpio.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_now_hub.h"

extern bool ota_enabled;

static const char *TAG = "app_button";

void read_nvs(bool *ota_enabled)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }

    size_t size = sizeof(bool);
    err = nvs_get_blob(my_handle, "ota_enabled", ota_enabled, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "ota_enabled not found, using default: false");
        *ota_enabled = false;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading ota_enabled: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Read ota_enabled = %s", *ota_enabled ? "true" : "false");
    }

    nvs_close(my_handle);
}

void write_nvs(bool ota_enabled)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(my_handle, "ota_enabled", &ota_enabled, sizeof(bool));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error writing ota_enabled: %s", esp_err_to_name(err));
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "NVS written");

    nvs_close(my_handle);
}

static void tap_btn_cb(void *handle, void *usr_data)
{

    ESP_LOGI(TAG, "Button tapped");
   send_data_to_ir_device(WHITE_SCREEN_CMD);
}

static void double_tap_btn_cb(void *handle, void *usr_data)
{
    ESP_LOGI(TAG, "Double tapped");
    send_data_to_ir_device(RESET_SCREEN_CMD);
}

static void long_press_btn_cb(void *handle, void *usr_data)
{
    ESP_LOGI(TAG, "Button long pressed");
    ESP_LOGI(TAG, "Restarting and updating firmware...");
    ota_enabled = true;
    write_nvs(ota_enabled);
    esp_restart();
}

void app_button_start()
{

    ESP_LOGI(TAG, "Initializing button...");

    button_config_t btn_cfg_1 = {
        .short_press_time = 180,
        .long_press_time = 3000,
    };

    button_config_t btn_cfg_2 = {
        .short_press_time = 180,
        .long_press_time = 3000,
    };

    button_gpio_config_t gpio_cfg_1 = {
        .active_level = BUTTON_ACTIVE_LEVEL,
        .gpio_num = BUTTON_1,
    };
    button_gpio_config_t gpio_cfg_2 = {
        .active_level = BUTTON_ACTIVE_LEVEL,
        .gpio_num = BUTTON_2,
    };

    button_handle_t btn1 = NULL;
    button_handle_t btn2 = NULL;

    // Initialize the first button
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg_1, &gpio_cfg_1, &btn1);
    if (ret != ESP_OK || btn1 == NULL) {
        ESP_LOGE(TAG, "Failed to create button 1, ret=%s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Button 1 initialized");
    }
    //Initialize the second button
    ret = iot_button_new_gpio_device(&btn_cfg_2, &gpio_cfg_2, &btn2);
    if (ret != ESP_OK || btn2 == NULL) {
        ESP_LOGE(TAG, "Failed to create button 2, ret=%s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Button 2 initialized");
    }

    // for (int i = 0; i < 2; i++) {
    //     gpio_cfg.gpio_num = list_gpio[i];
    //     esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn[i]);
    //     if (ret != ESP_OK || btn[i] == NULL) {
    //         ESP_LOGE(TAG, "Failed to create button[%d], ret=%s", i, esp_err_to_name(ret));
    //         continue;
    //     }
    //     ESP_LOGI(TAG, "Button[%d] initialized", i);
    // }

    if (btn1) {
        iot_button_register_cb(btn1, BUTTON_SINGLE_CLICK, NULL, tap_btn_cb, NULL);
        iot_button_register_cb(btn1, BUTTON_DOUBLE_CLICK, NULL, double_tap_btn_cb, NULL);
        iot_button_register_cb(btn1, BUTTON_LONG_PRESS_START, NULL, long_press_btn_cb, NULL);
    }

    if (btn2) {
        iot_button_register_cb(btn2, BUTTON_SINGLE_CLICK, NULL, tap_btn_cb, NULL);
        iot_button_register_cb(btn2, BUTTON_DOUBLE_CLICK, NULL, double_tap_btn_cb, NULL);
        iot_button_register_cb(btn2, BUTTON_LONG_PRESS_START, NULL, long_press_btn_cb, NULL);
    }
    
}
