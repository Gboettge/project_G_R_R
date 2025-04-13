#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

typedef enum {
    BATT_NOT_CONNECTED = -1,
    BATT_TOO_LOW,
    BATT_IN_RANGE,
    BATT_TOO_HIGH,

}batt_state;

typedef enum {
    BATT_CRHG_UNKNOWN = -1,
    BATT_CHRG_0,
    BATT_CHRG_10,
    BATT_CHRG_20,
    BATT_CHRG_30,
    BATT_CHRG_40,
    BATT_CHRG_50,
    BATT_CHRG_60,
    BATT_CHRG_70,
    BATT_CHRG_80,
    BATT_CHRG_90,
    BATT_CHRG_100,
    BATT_CHRG_CALC,
    
}batt_charge;


typedef struct{
    gpio_num_t pin;
    batt_state state;
    batt_state state_prev;
    batt_charge charge;
    batt_charge charge_prev;
    int voltage; // mV
    int voltage_prev; // mV
    TickType_t measure_time;
    adc_oneshot_unit_handle_t ADC_handle;
    adc_cali_handle_t CAL_handle;
    TaskHandle_t task_handle; // ev. task
}batt_conf;

typedef batt_conf* batt_handle;

batt_handle battery_init(const gpio_num_t pin);

void battery_update(batt_handle battery);

batt_state battery_get_state(const batt_handle battery);

batt_state battery_get_state_prev(const batt_handle battery);

batt_charge battery_get_charge(const batt_handle battery);

batt_charge battery_get_charge_prev(const batt_handle battery);