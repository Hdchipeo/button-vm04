/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* This is the button that is used for toggling the power */
#define BUTTON_1          GPIO_NUM_0
#define BUTTON_2          GPIO_NUM_3
#define BUTTON_ACTIVE_LEVEL  0

void app_button_start();
void read_nvs(bool *ota_enabled);
void write_nvs(bool ota_enabled);
