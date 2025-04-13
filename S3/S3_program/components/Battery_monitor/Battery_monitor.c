#include "Battery_monitor.h"

#define BATT_HYSTERESIS 50 //50 mV är najs då det är det som är stegen mellan procentsatserna

const static char *TAG = "BATTERY MONITOR";

static esp_err_t ADC_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle); // inspo från ESP-DOCS


//  *************** TO-DO ***************
// 
//  kolla curve fitting till S3
//  kolla vilken ADC-unit som lirar med radiokommunikation
//  implementera och gör om för task & sleep-kompabilitet
//
//  *************************************
batt_handle battery_init(const gpio_num_t pin)
{
    batt_handle battery = (batt_handle)pvPortMalloc(sizeof(batt_conf));
    if (!battery)
    {
        printf("Malloc Failed\n");
        exit(1);
    }
    battery->pin = pin;
    battery->state = BATT_IN_RANGE;
    battery->state_prev = BATT_IN_RANGE;
    battery->charge = BATT_CHRG_CALC;
    battery->charge_prev = BATT_CHRG_100;
    battery->measure_time = 0;
    battery->voltage = 0;
    battery->voltage_prev = 0;
    battery->task_handle = NULL;
    battery->ADC_handle = NULL;
    battery->CAL_handle = NULL;

    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_config, &battery->ADC_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(battery->ADC_handle, pin, &chan_config));

    ADC_calibration_init(ADC_UNIT_1, pin, chan_config.atten, &battery->CAL_handle); //endast CURVE_FITTING på C6
    return battery;
}

static void battery_charge_calc(batt_handle battery)
{
    // värden för 18650 Li-ion i 2S konfig med spänningsdelning R1: 300K R2: 100K, ger säkert spann mellan 1.5V(jag har satt 1.6) till 2.1V i Vout. och ca 20 MIKROamp sink.
    if(battery->voltage <= 1600 && battery->voltage > 500)
    {
        battery->charge = BATT_CHRG_0;
        battery->state = BATT_TOO_LOW; // kan vara fel batteri tex AA

    }
    else if(battery->voltage <= 1600 && battery->voltage < 500)
    {
        battery->charge = BATT_CRHG_UNKNOWN;
        battery->state = BATT_NOT_CONNECTED;
    }
    else if(battery->voltage > 1650  && battery->voltage < 1700)
    {
        battery->charge = BATT_CHRG_10;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 1700  && battery->voltage < 1750)
    {
        battery->charge = BATT_CHRG_20;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 1750  && battery->voltage < 1800)
    {
        battery->charge = BATT_CHRG_30;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 1780  && battery->voltage < 1850)
    {
        battery->charge = BATT_CHRG_40;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 1850 && battery->voltage < 1900)
    {
        battery->charge = BATT_CHRG_50;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 1900 && battery->voltage < 1950)
    {
        battery->charge = BATT_CHRG_60;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 1950 && battery->voltage < 2000)
    {
        battery->charge = BATT_CHRG_70;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 2000 && battery->voltage < 2050)
    {
        battery->charge = BATT_CHRG_80;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 2050 && battery->voltage < 2100)
    {
        battery->charge = BATT_CHRG_90;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 2050 && battery->voltage < 2100)
    {
        battery->charge = BATT_CHRG_100;
        battery->state = BATT_IN_RANGE;
    }
    else if(battery->voltage > 2500)
    {
        battery->charge = BATT_CRHG_UNKNOWN;
        battery->state = BATT_TOO_HIGH;
    }

}

void battery_update(batt_handle battery) 
{
    TickType_t current_time = xTaskGetTickCount();


    if(current_time - battery->measure_time > pdMS_TO_TICKS(10 * 1000)) //ta measurement var 10e sekund
    {
        battery->measure_time = current_time;
        adc_oneshot_get_calibrated_result(battery->ADC_handle,battery->CAL_handle, battery->pin, &battery->voltage);
        printf("%d\n", battery->voltage);
        if (battery->voltage < battery->voltage_prev - BATT_HYSTERESIS || //om nya värdet är +/- det gamla med hysteres så updaterar vi state.
            battery->voltage > battery->voltage_prev + BATT_HYSTERESIS)
        {
            battery_charge_calc(battery);
            battery->voltage_prev = battery->voltage;
        }
    
        if(battery->state != battery->state_prev || battery->charge != battery->charge_prev)
        {
            battery->state_prev = battery->state;
            battery->charge_prev = battery->charge;
            // printf("Battery state or charge changed: %dmV\n", battery->voltage);
            if(battery->state == BATT_NOT_CONNECTED)
            {
                // printf("Battery not connected or broken circuit. Voltage: %d mV\n", battery->voltage);
        
            }
            else if(battery->charge == BATT_CHRG_0 ||
                    battery->charge == BATT_CRHG_UNKNOWN ||
                    battery->state == BATT_TOO_LOW||
                    battery->state == BATT_TOO_HIGH)
            {
                // printf("CHECK BATTERY. Voltage: %d mV\n", battery->voltage);
            }
        }

    }


}

batt_state battery_get_state(const batt_handle battery)
{
    return battery->state;
}
batt_state battery_get_state_prev(const batt_handle battery)
{
    return battery->state_prev;
}

batt_charge battery_get_charge(const batt_handle battery)
{
    return battery->charge;
}

batt_charge battery_get_charge_prev(const batt_handle battery)
{
    return battery->charge_prev;
}


static esp_err_t ADC_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) // inspo från ESP-DOCS
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return ret;
}