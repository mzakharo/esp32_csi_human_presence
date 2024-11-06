#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "string.h"
#include "esp_mac.h"
#include "rom/ets_sys.h"
#include "mdet.h"
#include <complex.h>

#define WIFI_SSID     CONFIG_ESP_WIFI_SSID
#define WIFI_PASS     CONFIG_ESP_WIFI_PASSWORD

#define THRESH  0.60f

static const char *TAG = "CSI";
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static uint8_t ap_bssid[6] = {0};  // Global variable to store AP's BSSID
static uint8_t sta_mac[6] = {0};


static CSIMagnitudeDetector* detector;


// Wi-Fi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
         wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        
        // Store the BSSID of the connected AP
        memcpy(ap_bssid, event->bssid, sizeof(ap_bssid));

        ESP_LOGI(TAG, "Connected to AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    }
}
#define PERIOD_MS 100
// CSI data callback
static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    if (!info || !info->buf) {
        ESP_LOGW(TAG, "<%s> wifi_csi_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
        return;
    }

   static uint8_t null_mac[6] = {0};
   if (memcmp(info->mac, null_mac, 6)) {
        return;
    }
    if (memcmp(info->dmac, sta_mac, 6)) {
        return;
    }
    static uint32_t prev_timestamp = 0;
    static int s_count = 0;
    const wifi_pkt_rx_ctrl_t *rx_ctrl = &info->rx_ctrl;

    if (rx_ctrl->timestamp < prev_timestamp)
        prev_timestamp = rx_ctrl->timestamp;        
    if (rx_ctrl->timestamp - prev_timestamp < (PERIOD_MS-25) *1000)
        return;
    prev_timestamp = rx_ctrl->timestamp;

    ets_printf("CSI_DATA,%d," MACSTR ",%d,%d,%d,%d,%d,%d,%d",
            s_count++, MAC2STR(ap_bssid), rx_ctrl->rssi, rx_ctrl->rate,
            rx_ctrl->noise_floor,  rx_ctrl->channel,
            rx_ctrl->timestamp, rx_ctrl->sig_len, rx_ctrl->rx_state);
    #if CONFIG_SOC_WIFI_HE_SUPPORT
    char he = 1;
    #else
    char he = 0;
    #endif
    ets_printf(",%d,%d,\"[%d", info->len, he, info->buf[0]);

    for (int i = 1; i < info->len; i++) {
        ets_printf(",%d", info->buf[i]);
    }

    ets_printf("]\"\n");
    float confidence;
    float magnitude_data[26];
    int j = 0;
    #if CONFIG_SOC_WIFI_HE_SUPPORT
        int offset = 12;
    #else
        int offset = 2;
    #endif
    for (int i = offset; i < offset + 52; i=i+2) {  //we reject upper 26 subcarriers since data there is not stable
        float complex z = (float)info->buf[i] + (float)info->buf[i+1]*I;
        magnitude_data[j] = cabsf(z);
        j++;
    }
    int is_present = detect_presence(detector, magnitude_data, &confidence);
        
    if (is_present) {
        printf("Human presence detected! (Confidence: %.2f. thresh: %.2f, var: %.2f cor: %.2f)\n", confidence, THRESH, detector->var, detector->cor);
    } else {
        printf("No presence detected. (Confidence: %.2f.  thresh: %.2f, var: %.2f cor: %.2f)\n", confidence, THRESH, detector->var, detector->cor);
    }
}

// Initialize CSI collection
void wifi_init_csi() {
  /**< default config */
    #if CONFIG_SOC_WIFI_HE_SUPPORT
    wifi_csi_config_t  csi_config = {
        .enable = true,                    /**< enable to acquire CSI */
        .acquire_csi_legacy = true,        /**< enable to acquire L-LTF when receiving a 11g PPDU */
        .acquire_csi_ht20 = false,         /**< enable to acquire HT-LTF when receiving an HT20 PPDU */
        .acquire_csi_ht40 = false,         /**< enable to acquire HT-LTF when receiving an HT40 PPDU */
        .acquire_csi_su = false,            /**< enable to acquire HE-LTF when receiving an HE20 SU PPDU */
        .acquire_csi_mu = false,           /**< enable to acquire HE-LTF when receiving an HE20 MU PPDU */
        .acquire_csi_dcm = false,           /**< enable to acquire HE-LTF when receiving an HE20 DCM applied PPDU */
        .acquire_csi_beamformed = false,    /**< enable to acquire HE-LTF when receiving an HE20 Beamformed applied PPDU */
        .acquire_csi_he_stbc = 0,       /**< when receiving an STBC applied HE PPDU,
                                                    0- acquire the complete HE-LTF1
                                                    1- acquire the complete HE-LTF2
                                                    2- sample evenly among the HE-LTF1 and HE-LTF2 */
        .val_scale_cfg = 0,             /**< value 0-3 */
        .dump_ack_en = true,               /**< enable to dump 802.11 ACK frame, default disabled */
    };
    #else
    wifi_csi_config_t csi_config = {
        .lltf_en           = true,
        .htltf_en          = false,
        .stbc_htltf2_en    = false,
        .ltf_merge_en      = false,
        .channel_filter_en = false,
        .manu_scale        = false,
        .shift             = false,
        .dump_ack_en       = true,
    };
    #endif    
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, sta_mac));
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));
}

// Function to transmit a raw Wi-Fi frame
void send_raw_frame() {

    wifi_ap_record_t ap_info  = {0};
    esp_wifi_sta_get_ap_info(&ap_info);

   typedef struct {
    uint8_t frame_control[2];
    uint16_t duration;
    uint8_t destination_address[6];
    uint8_t source_address[6];
    uint8_t broadcast_address[6];
    uint16_t sequence_control;
} __attribute__((packed)) wifi_null_data_t;

    wifi_null_data_t null_data = {
        .frame_control       = {0x48, 0x01},
        .duration            = 0x0000,
        .sequence_control    = 0x0000,
    };

    memcpy(null_data.destination_address, ap_info.bssid, 6);
    memcpy(null_data.broadcast_address, ap_info.bssid, 6);
    memcpy(null_data.source_address, sta_mac, 6);

    // Transmit the raw frame
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA,  &null_data, sizeof(null_data), true);
    if (err == ESP_OK) {
        //ESP_LOGI(TAG, "Raw frame sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send raw frame: %d", err);
    }
}

// Wi-Fi initialization in station mode
void wifi_init_sta() {
    wifi_event_group = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    ESP_ERROR_CHECK(esp_event_loop_create_default()); 
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, &instance_any_id);

    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, &instance_any_id);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    detector = create_detector(WINDOW_SIZE, THRESH);
    wifi_init_csi();

}

// Task to send raw frames periodically
void raw_frame_task(void *pvParameters) {
    ESP_ERROR_CHECK(esp_wifi_config_80211_tx_rate(WIFI_IF_STA, WIFI_PHY_RATE_6M));
    while (1) {
        send_raw_frame();
        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
    }
}

void app_main() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    // Create task to send raw frames periodically
    xTaskCreate(raw_frame_task, "raw_frame_task", 4096, NULL, 5, NULL);
}