##
# @brief CSI binary sensor motion detector
#
# @author Mikhail Zakharov <mzakharo@gmail.com>
##

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_TIMING,
    CONF_THRESHOLD,
    CONF_ID,
)

CODEOWNERS = ["@mzakharo"]
wifi_csi_ns = cg.esphome_ns.namespace("wifi_csi")


CsiSensor = wifi_csi_ns.class_(
    "CsiSensor", binary_sensor.BinarySensor, cg.PollingComponent
)


CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(CsiSensor)
    .extend(cv.COMPONENT_SCHEMA)
    .extend({
        cv.Optional(CONF_TIMING): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_THRESHOLD): cv.zero_to_one_float,
    })
)



async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    if CONF_TIMING in config:
        cg.add(var.set_timing(config[CONF_TIMING]))
    if CONF_THRESHOLD in config:
        cg.add(var.set_threshold(config[CONF_THRESHOLD]))
    await cg.register_component(var, config)
