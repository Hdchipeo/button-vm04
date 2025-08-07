#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "esp_now_hub.h"
#include "app_priv.h"

static const char *TAG = "espnow";

static QueueHandle_t s_espnow_queue = NULL;

static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//static uint8_t ir_device_mac_addr[ESP_NOW_ETH_ALEN] = { 0xe4, 0x65, 0xb8, 0x11, 0x7f, 0x9d };
static uint8_t ir_device_mac_addr[ESP_NOW_ETH_ALEN] = {0xa0, 0xa3, 0xb3, 0xfe, 0xeb, 0x7d};
static uint8_t hmi_device_mac_addr[ESP_NOW_ETH_ALEN] = {0x78, 0x1c, 0x3c, 0x2b, 0xbb, 0x80};

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
}

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    example_espnow_event_t evt;
    example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL)
    {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t *mac_addr = recv_info->src_addr;
    uint8_t *des_addr = recv_info->des_addr;

    if (mac_addr == NULL || data == NULL || len <= 0)
    {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    if (IS_BROADCAST_ADDR(des_addr))
    {
        /* If added a peer with encryption before, the receive packets may be
         * encrypted as peer-to-peer message or unencrypted over the broadcast channel.
         * Users can check the destination address to distinguish it.
         */
        ESP_LOGD(TAG, "Receive broadcast ESPNOW data");
    }
    else
    {
        ESP_LOGD(TAG, "Receive unicast ESPNOW data");
    }

    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL)
    {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}

static void handle_received_data(example_espnow_event_recv_cb_t *recv_cb)
{
    if (recv_cb == NULL || recv_cb->data == NULL || recv_cb->data_len <= 0)
    {
        ESP_LOGE(TAG, "Received data is NULL or invalid length");
        return;
    }
    button_data_t *button_data = (button_data_t *)recv_cb->data;

    ESP_LOGI(TAG, "Received state: %d", button_data->state);
}

static void espnow_task(void *pvParameter)
{
    example_espnow_event_t evt;

    while (xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE)
    {
        switch (evt.id)
        {
        case EXAMPLE_ESPNOW_SEND_CB:
        {
            break;
        }
        case EXAMPLE_ESPNOW_RECV_CB:
        {
            example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
            handle_received_data(recv_cb);
            free(recv_cb->data);
            break;
        }
        default:
            ESP_LOGE(TAG, "Callback type error: %d", evt.id);
            break;
        }
    }
}

void espnow_add_peer(uint8_t *mac_addr, bool encrypt)
{
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(TAG, "Malloc peer information fail");
        return;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = encrypt;
    memcpy(peer->peer_addr, mac_addr, ESP_NOW_ETH_ALEN);

    esp_err_t ret = esp_now_add_peer(peer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Add peer failed: %s", esp_err_to_name(ret));
    }

    free(peer);
}

static esp_err_t espnow_init(void)
{

    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    if (s_espnow_queue == NULL)
    {
        ESP_LOGE(TAG, "Create queue fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK(esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL));
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    espnow_add_peer(broadcast_mac, false);
    espnow_add_peer(ir_device_mac_addr, false);
    espnow_add_peer(hmi_device_mac_addr, false);

    xTaskCreate(espnow_task, "esp_now_task", 2048, NULL, 4, NULL);

    return ESP_OK;
}

void espnow_send_button_command(button_data_t *button_data)
{
    if (button_data == NULL)
    {
        ESP_LOGE(TAG, "Button data is NULL");
        return;
    }

    uint8_t command[sizeof(button_data_t)];
    memcpy(command, button_data, sizeof(button_data_t));
    size_t command_length = sizeof(button_data_t);

    esp_err_t ret = esp_now_send(ir_device_mac_addr, command, command_length);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Send command failed: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Command sent successfully");
    }
}

void send_data_to_ir_device(const char *command)
{
    button_data_t btn_data = {0};
    strncpy(btn_data.cmd, command, sizeof(btn_data.cmd) - 1);
    btn_data.cmd[sizeof(btn_data.cmd) - 1] = '\0'; // Ensure null termination
    espnow_send_button_command(&btn_data);
}

static void espnow_deinit()
{
    vQueueDelete(s_espnow_queue);
    s_espnow_queue = NULL;
    esp_now_deinit();
}

void app_espnow_start(void)
{
    wifi_init();
    espnow_init();
}
void app_espnow_stop(void)
{
    espnow_deinit();
}