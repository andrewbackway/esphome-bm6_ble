import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_TYPE, DEVICE_CLASS_PROBLEM, DEVICE_CLASS_BATTERY_CHARGING
from . import bm6_ble_ns, BM6Hub

CONF_BM6_BLE_ID = "bm6_ble_id"

TYPES = {
    "LOW_VOLTAGE": binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
        icon="mdi:battery-alert",
    ),
    "CHARGING": binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_BATTERY_CHARGING,
        icon="mdi:battery-charging",
    ),
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_BM6_BLE_ID): cv.use_id(BM6Hub),
    cv.Required(CONF_TYPE): cv.enum(TYPES, upper=True),
}).extend(binary_sensor.binary_sensor_schema())

async def to_code(config):
    hub = await cg.get_variable(config[CONF_BM6_BLE_ID])
    sens = await binary_sensor.new_binary_sensor(config)
    
    if config[CONF_TYPE] == "LOW_VOLTAGE":
        cg.add(hub.set_low_voltage_binary(sens))
    elif config[CONF_TYPE] == "CHARGING":
        cg.add(hub.set_charging_binary(sens))