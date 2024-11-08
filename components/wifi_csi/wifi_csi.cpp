
/**

 * @author Mikhail Zakharov <mzakharo@gmail.com>
 */

#include "wifi_csi.h"

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "mdet.h"
#include "esp_err.h"
#include <complex.h>
static const char *const TAG = "wifi_csi";
static uint8_t sta_mac[6] = {0};

#define PERIOD_MS 100
static CSIMagnitudeDetector* detector;
static float confidence;


// CSI data callback
static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    if (!info || !info->buf) {
        //ESP_LOGW(TAG, "<%s> wifi_csi_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
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

    float magnitude_data[26];
    int j = 0;
    #if CONFIG_SOC_WIFI_HE_SUPPORT
        int offset = 12;
    #else
        int offset = 2;
    #endif
    for (int i = offset; i < offset + 52; i=i+2) {  //we reject upper 26 subcarriers since data there is not stable
        std::complex<float> z((float)info->buf[i], (float)info->buf[i+1]);
        magnitude_data[j] = std::abs(z);
        j++;
    }
    detect_presence(detector, magnitude_data, &confidence);
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

    wifi_ap_record_t ap_info;
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
        printf("Failed to send raw frame: %d", err);
    }
}


// Task to send raw frames periodically
void raw_frame_task(void *pvParameters) {
    ESP_ERROR_CHECK(esp_wifi_config_80211_tx_rate(WIFI_IF_STA, WIFI_PHY_RATE_6M));
    while (1) {
        send_raw_frame();
        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
    }
}

esphome::wifi_csi::CsiSensor::CsiSensor()
: PollingComponent()
, binary_sensor::BinarySensor()
, m_pollingInterval(100)
, m_threshold(0.50)
{
    set_update_interval(m_pollingInterval);
    this->set_device_class("motion");    
    detector = create_detector(WINDOW_SIZE, m_threshold);
    
}

esphome::wifi_csi::CsiSensor::~CsiSensor()
{
        //TODO: implement me
}

float esphome::wifi_csi::CsiSensor::get_setup_priority() const
{
    return esphome::setup_priority::AFTER_WIFI;
}

void esphome::wifi_csi::CsiSensor::dump_config()
{
    ESP_LOGCONFIG(TAG, "Wifi CSI:");
    ESP_LOGCONFIG(TAG, "polling interval: %dms", m_pollingInterval);
    ESP_LOGCONFIG(TAG, "threshold: %.2f", m_threshold);
}

void esphome::wifi_csi::CsiSensor::set_timing(int pollingInterval)
{
    m_pollingInterval = pollingInterval;
    set_update_interval(pollingInterval);
}

void esphome::wifi_csi::CsiSensor::set_threshold(float threshold)
{
    m_threshold = threshold;
}


void esphome::wifi_csi::CsiSensor::update() {

    static bool initialized = false;
    if (!initialized) {
        wifi_init_csi();
        // Create task to send raw frames periodically
        xTaskCreate(raw_frame_task, "raw_frame_task", 4096, NULL, 5, NULL);
        initialized = true;
    }
    
    bool motion = (confidence >= m_threshold) ? true : false;
    publish_state(motion);

    // log periodically
    static time_t last_t = 0;
    time_t now_t;
    time(&now_t);
    if (difftime(now_t, last_t) > 0.5) {
        ESP_LOGD(TAG, "Confidence: %.2f, thresh: %.2f, var: %.2f cor: %.2f", confidence, m_threshold, detector->var, detector->cor);
        last_t = now_t;
    }     
}
