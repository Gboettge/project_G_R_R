#include <stdio.h>
#include "Bluedroid_central_client.h"

uint16_t client_conn = 0;

esp_gatt_if_t client_interface = 0;

static uint8_t received_data = 0x00;

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
void handle_service_search_complete(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *p_data);
void handle_notify_registration(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *p_data);
void handle_notification(esp_ble_gattc_cb_param_t *p_data);
void handle_write_descriptor(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *p_data);

void ble_run()
{
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // #if CONFIG_EXAMPLE_CI_PIPELINE_ID
    // memcpy(remote_device_name, esp_bluedroid_get_example_name(), sizeof(remote_device_name));
    // #endif
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s gap register failed, error code = %x", __func__, ret);
        return;
    }

    // register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb); // här börjar helvetet
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s gattc register failed, error code = %x", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret)
    {
        ESP_LOGE(GATTC_TAG, "%s gattc app register failed, error code = %x", __func__, ret);
    }
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret)
    {
        ESP_LOGE(GATTC_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
}

void ble_send()
{
    uint8_t data = 0x06;
    esp_ble_gattc_write_char(client_interface,
                             gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                             gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                             sizeof(data),
                             &data,
                             ESP_GATT_WRITE_TYPE_RSP,
                             ESP_GATT_AUTH_REQ_NONE);
}

esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = REMOTE_SERVICE_UUID,
    },
};

esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = REMOTE_NOTIFY_CHAR_UUID,
    },
};

esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
    },
};

esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(GATTC_TAG, "GATT client register, status %d, app_id %d, gattc_if %d", param->reg.status, param->reg.app_id, gattc_if);
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret)
        {
            ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GATTC_CONNECT_EVT:
    {
        ESP_LOGI(GATTC_TAG, "Connected, conn_id %d, remote " ESP_BD_ADDR_STR "", p_data->connect.conn_id,
                 ESP_BD_ADDR_HEX(p_data->connect.remote_bda));
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
        if (mtu_ret)
        {
            ESP_LOGE(GATTC_TAG, "Config MTU error, error code = %x", mtu_ret);
        }
        break;
    }
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK)
        {
            ESP_LOGE(GATTC_TAG, "Open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "Open successfully, MTU %u", p_data->open.mtu);
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(GATTC_TAG, "Service discover failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "Service discover complete, conn_id %d", param->dis_srvc_cmpl.conn_id);

        client_conn = param->dis_srvc_cmpl.conn_id; // richards jävla påhitt

        esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, &remote_filter_service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        ESP_LOGI(GATTC_TAG, "MTU exchange, status %d, MTU %d", param->cfg_mtu.status, param->cfg_mtu.mtu);
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
    {
        ESP_LOGI(GATTC_TAG, "Service search result, conn_id = %x, is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(GATTC_TAG, "start handle %d, end handle %d, current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID)
        {
            ESP_LOGI(GATTC_TAG, "Service found");
            get_server = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
            ESP_LOGI(GATTC_TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(GATTC_TAG, "Service search failed, status %x", p_data->search_cmpl.status);
            break;
        }
        if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE)
        {
            ESP_LOGI(GATTC_TAG, "Get service information from remote device");
        }
        else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH)
        {
            ESP_LOGI(GATTC_TAG, "Get service information from flash");
        }
        else
        {
            ESP_LOGI(GATTC_TAG, "Unknown service source");
        }
        ESP_LOGI(GATTC_TAG, "Service search complete");
        if (get_server)
        {
            handle_service_search_complete(gattc_if, p_data);
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    {
        handle_notify_registration(gattc_if, p_data);
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        handle_notification(p_data);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        handle_write_descriptor(gattc_if, p_data);
        break;
    case ESP_GATTC_SRVC_CHG_EVT:
    {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "Service change from " ESP_BD_ADDR_STR "", ESP_BD_ADDR_HEX(bda));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK)
        {
            ESP_LOGE(GATTC_TAG, "Characteristic write failed, status %x)", p_data->write.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "Characteristic write successfully");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        connect = false;
        get_server = false;
        ESP_LOGI(GATTC_TAG, "Disconnected, remote " ESP_BD_ADDR_STR ", reason 0x%02x",
                 ESP_BD_ADDR_HEX(p_data->disconnect.remote_bda), p_data->disconnect.reason);
        break;
    default:
        break;
    }
}

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
    {
        // the unit of the duration is second
        uint32_t duration = 30;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        // scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTC_TAG, "Scanning start failed, status %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "Scanning start successfully");

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt)
        {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            adv_name = esp_ble_resolve_adv_data_by_type(scan_result->scan_rst.ble_adv,
                                                        scan_result->scan_rst.adv_data_len + scan_result->scan_rst.scan_rsp_len,
                                                        ESP_BLE_AD_TYPE_NAME_CMPL,
                                                        &adv_name_len);
            ESP_LOGI(GATTC_TAG, "Scan result, device " ESP_BD_ADDR_STR ", name len %u", ESP_BD_ADDR_HEX(scan_result->scan_rst.bda), adv_name_len);
            ESP_LOG_BUFFER_CHAR(GATTC_TAG, adv_name, adv_name_len);

#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
            if (scan_result->scan_rst.adv_data_len > 0)
            {
                ESP_LOGI(GATTC_TAG, "adv data:");
                ESP_LOG_BUFFER_HEX(GATTC_TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
            }
            if (scan_result->scan_rst.scan_rsp_len > 0)
            {
                ESP_LOGI(GATTC_TAG, "scan resp:");
                ESP_LOG_BUFFER_HEX(GATTC_TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
            }
#endif

            if (adv_name != NULL)
            {
                if (strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0)
                {
                    // Note: If there are multiple devices with the same device name, the device may connect to an unintended one.
                    // It is recommended to change the default device name to ensure it is unique.
                    ESP_LOGI(GATTC_TAG, "Device found %s", remote_device_name);
                    if (connect == false)
                    {
                        connect = true;
                        ESP_LOGI(GATTC_TAG, "Connect to the remote device");
                        esp_ble_gap_stop_scanning();
                        esp_ble_gatt_creat_conn_params_t creat_conn_params = {0};
                        memcpy(&creat_conn_params.remote_bda, scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
                        creat_conn_params.remote_addr_type = scan_result->scan_rst.ble_addr_type;
                        creat_conn_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
                        creat_conn_params.is_direct = true;
                        creat_conn_params.is_aux = false;
                        creat_conn_params.phy_mask = 0x0;
                        esp_ble_gattc_enh_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
                                               &creat_conn_params);
                    }
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTC_TAG, "Scanning stop failed, status %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "Scanning stop successfully");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTC_TAG, "Advertising stop failed, status %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "Advertising stop successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(GATTC_TAG, "Connection params update, status %d, conn_int %d, latency %d, timeout %d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        ESP_LOGI(GATTC_TAG, "Packet length update, status %d, rx %d, tx %d",
                 param->pkt_data_length_cmpl.status,
                 param->pkt_data_length_cmpl.params.rx_len,
                 param->pkt_data_length_cmpl.params.tx_len);
        break;
    default:
        break;
    }
}

void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            // lägg
            //  client_if = gattc_if;
            client_interface = gattc_if;
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        }
        else
        {
            ESP_LOGI(GATTC_TAG, "reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gattc_if == gl_profile_tab[idx].gattc_if)
            {
                if (gl_profile_tab[idx].gattc_cb)
                {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

void handle_service_search_complete(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *p_data)
{
    uint16_t count = 0;
    esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
                                                            p_data->search_cmpl.conn_id,
                                                            ESP_GATT_DB_CHARACTERISTIC,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                            INVALID_HANDLE,
                                                            &count);
    if (status != ESP_GATT_OK)
    {
        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
        return;
    }

    if (count > 0)
    {
        char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
        if (!char_elem_result)
        {
            ESP_LOGE(GATTC_TAG, "gattc no mem");
            return;
        }
        else
        {
            status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                    p_data->search_cmpl.conn_id,
                                                    gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                    gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                    remote_filter_char_uuid,
                                                    char_elem_result,
                                                    &count);
            if (status != ESP_GATT_OK)
            {
                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                free(char_elem_result);
                char_elem_result = NULL;
                return;
            }

            /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
            if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY))
            {
                gl_profile_tab[PROFILE_A_APP_ID].char_handle = char_elem_result[0].char_handle;
                esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[0].char_handle);
            }
        }
        /* free char_elem_result */
        free(char_elem_result);
    }
    else
    {
        ESP_LOGE(GATTC_TAG, "no char found");
    }
}

void handle_notify_registration(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *p_data)
{
    if (p_data->reg_for_notify.status != ESP_GATT_OK)
    {
        ESP_LOGE(GATTC_TAG, "Notification register failed, status %d", p_data->reg_for_notify.status);
    }
    else
    {
        ESP_LOGI(GATTC_TAG, "Notification register successfully");
        uint16_t count = 0;
        uint16_t notify_en = 1;
        esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                    ESP_GATT_DB_DESCRIPTOR,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                                                    &count);
        if (ret_status != ESP_GATT_OK)
        {
            ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
            return;
        }
        if (count > 0)
        {
            descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
            if (!descr_elem_result)
            {
                ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
                return;
            }
            else
            {
                ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                    p_data->reg_for_notify.handle,
                                                                    notify_descr_uuid,
                                                                    descr_elem_result,
                                                                    &count);
                if (ret_status != ESP_GATT_OK)
                {
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                    free(descr_elem_result);
                    descr_elem_result = NULL;
                    return;
                }
                /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
                {
                    ret_status = esp_ble_gattc_write_char_descr(gattc_if,
                                                                gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                descr_elem_result[0].handle,
                                                                sizeof(notify_en),
                                                                (uint8_t *)&notify_en,
                                                                ESP_GATT_WRITE_TYPE_RSP,
                                                                ESP_GATT_AUTH_REQ_NONE);
                }

                if (ret_status != ESP_GATT_OK)
                {
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_write_char_descr error");
                }

                /* free descr_elem_result */
                free(descr_elem_result);
            }
        }
        else
        {
            ESP_LOGE(GATTC_TAG, "decsr not found");
        }
    }
}

void handle_notification(esp_ble_gattc_cb_param_t *p_data)
{
    if (p_data->notify.is_notify)
    {
        ESP_LOGI(GATTC_TAG, "Notification received");
    }
    else
    {
        ESP_LOGI(GATTC_TAG, "Indication received");
    }
    ESP_LOG_BUFFER_HEX(FROM_S3, p_data->notify.value, p_data->notify.value_len);
    received_data = p_data->notify.value[0];
}

void handle_write_descriptor(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *p_data)
{
    if (p_data->write.status != ESP_GATT_OK)
    {
        ESP_LOGE(GATTC_TAG, "Descriptor write failed, status %x", p_data->write.status);
        return;
    }
    ESP_LOGI(GATTC_TAG, "Descriptor write successfully");
    uint8_t write_char_data[35];
    for (int i = 0; i < sizeof(write_char_data); ++i)
    {
        write_char_data[i] = 0xff;
    }
    esp_ble_gattc_write_char(gattc_if,
                             gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                             gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                             sizeof(write_char_data),
                             write_char_data,
                             ESP_GATT_WRITE_TYPE_RSP,
                             ESP_GATT_AUTH_REQ_NONE);
}

uint8_t get_received_data()
{
    return received_data;
}

void clear_received_data()
{
    received_data = 0x00;
}