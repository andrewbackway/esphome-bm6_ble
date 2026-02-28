import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TYPE, UNIT_VOLT, UNIT_CELSIUS, UNIT_PERCENT,
    DEVICE_CLASS_VOLTAGE, DEVICE_CLASS_TEMPERATURE, DEVICE_CLASS_BATTERY,
    STATE_CLASS_MEASUREMENT, CONF_ICON
)
from . import bm6_ble_ns, BM6Hub

CONF_BM6_BLE_ID = "bm6_ble_id"

TYPES = {
    "VOLTAGE": sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
        icon="mdi:flash",
    ),
    "TEMPERATURE": sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon="mdi:thermometer",
    ),
    "BATTERY_LEVEL": sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon="mdi:battery",
    ),
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_BM6_BLE_ID): cv.use_id(BM6Hub),
    cv.Required(CONF_TYPE): cv.enum(TYPES, upper=True),
}).extend(sensor.sensor_schema())

async def to_code(config):
    hub = await cg.get_variable(config[CONF_BM6_BLE_ID])
    sens = await sensor.new_sensor(config)
    
    if config[CONF_TYPE] == "VOLTAGE":
        cg.add(hub.set_voltage_sensor(sens))
    elif config[CONF_TYPE] == "TEMPERATURE":
        cg.add(hub.set_temperature_sensor(sens))
    elif config[CONF_TYPE] == "BATTERY_LEVEL":
        cg.add(hub.set_level_sensor(sens))