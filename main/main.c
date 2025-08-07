#include <stdio.h>
#include "app_priv.h"
#include "esp_now_hub.h"
#include "esp_log.h"
#include "ota.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_main";

bool ota_enabled = false;

void app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "Initializing NVS...");

    read_nvs(&ota_enabled);

    app_button_start();
    ESP_LOGI(TAG, "Button device started, waiting for button presses...");

    if (ota_enabled)
    {
        ota_enabled = false;
        write_nvs(ota_enabled);
        app_ota_start();
        ESP_LOGI(TAG, "OTA device started, waiting for OTA...");
    }
    else
    {
        app_espnow_start();
    }

    ESP_LOGI(TAG, "ESP-NOW hub started, waiting for messages...");
}
