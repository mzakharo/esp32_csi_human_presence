
/**
 * @author Mikhail Zakharov <mzakharo@gmail.com>
 */

#pragma once

#include "esphome.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/wifi/wifi_component.h"


namespace esphome {
namespace wifi_csi {

class CsiSensor : public PollingComponent, public binary_sensor::BinarySensor
{
public:
    CsiSensor();
    ~CsiSensor();

    float get_setup_priority() const override;
    void dump_config();

    void set_timing(int pollingInterval);
    void set_threshold(float threshold);

    void update() override;

protected:
    int m_pollingInterval;  ///< polling interval in ms (default: 100)
    float m_threshold;    ///< threshold at which confidence becomes motion
};

} // namespace wifi_csi
} // namespace esphome
