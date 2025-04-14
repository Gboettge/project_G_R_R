// NimBLE Client - Scan

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "sdkconfig.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
char *TAG = "BLE Client Scan";
uint8_t ble_addr_type;

struct ble_addr {
    uint8_t type;          // Address type (public or random)
    uint8_t val[6];       // Address value (6 bytes)
};

ble_addr_t address =
{
    .type = 0,
    .val = { 0x40, 0x32, 0x04, 0x07, 0x3B, 0xD2 },
};

struct ble_gap_conn_params conn_params;


void ble_app_scan(void);

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    
    struct ble_addr my_device_addr;

    // Initialize the address type (0 for public, 1 for random)
    my_device_addr.type = 0; // Assuming 0 is for public address

    // Initialize the address value (example address)
    uint8_t example_address[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    memcpy(my_device_addr.val, example_address, sizeof(my_device_addr.val));

    // const char* mcfuck = "BLE-RILLE";
    struct ble_hs_adv_fields fields;

    switch (event->type)
    {
    // NimBLE event discovery
    case BLE_GAP_EVENT_DISC:
        // ESP_LOGI("GAP", "GAP EVENT DISCOVERY");
        ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (fields.name_len > 0)
        {
            printf("Name: %.*s\n", fields.name_len, fields.name);
            // if (strcmp((char *)fields.name, mcfuck) == 0) 
            // {
            //     printf("Gurkan hittad\n");
            //     // Store the server address
            //     // my_device_addr = event->disc.addr;
            //     // Initiate connection
            //     ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &address, BLE_HS_FOREVER, &conn_params, ble_gap_event, NULL);
            // }
            // if (strcmp((char *)fields.name, "BLE-RILLE") == 0) // Replace with your server's name
            // {
            //     printf("Gurkan hittad\n");
            //     // Store the server address
            //     // my_device_addr = event->disc.addr;
            //     // Initiate connection
            //     ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &address, BLE_HS_FOREVER, &conn_params, ble_gap_event, NULL);
            // }
        }
        break;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            ESP_LOGI(TAG, "Connected to server");
            // You can now start reading/writing characteristics here
        }
        else
        {
            ESP_LOGI(TAG, "Failed to connect to server");
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected from server");
        break;

    default:
        break;
    }
    return 0;
}

void ble_app_scan(void)
{
    printf("Start scanning ...\n");

    struct ble_gap_disc_params disc_params;
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    ble_gap_disc(ble_addr_type, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
}

// The application
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    ble_app_scan();                          
}

// The infinite task
void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

void app_main()
{
    struct server_add;
    nvs_flash_init();                               // 1 - Initialize NVS flash using
    esp_nimble_hci_init();           // 2 - Initialize ESP controller
    nimble_port_init();                             // 3 - Initialize the controller stack
    ble_svc_gap_device_name_set("BLE-Scan-Client"); // 4 - Set device name characteristic
    ble_svc_gap_init();                             // 4 - Initialize GAP service
    ble_hs_cfg.sync_cb = ble_app_on_sync;           // 5 - Set application
    nimble_port_freertos_init(host_task);           // 6 - Set infinite task
}