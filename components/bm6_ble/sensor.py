import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_TYPE, UNIT_VOLT, UNIT_CELSIUS, UNIT_PERCENT,
    DEVICE_CLASS_VOLTAGE, DEVICE_CLASS_TEMPERATURE, DEVICE_CLASS_BATTERY,
    STATE_CLASS_MEASUREMENT,
)
from . import bm6_ble_ns, BM6Hub

CONF_BM6_BLE_ID = "bm6_ble_id"

# cv.typed_schema returns a plain callable, not a schema object, so .extend()
# cannot be used on it. Instead, embed the hub ID into every type schema via
# this helper so cv.typed_schema receives complete, self-contained schemas.
def _sensor_schema(base):
    return base.extend({
        cv.GenerateID(CONF_BM6_BLE_ID): cv.use_id(BM6Hub),
    })

# Each type carries its own fixed unit, device class, icon and precision so
# none of these need to be specified in YAML — they are applied automatically.
TYPES = {
    "VOLTAGE": _sensor_schema(sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=2,
        icon="mdi:car-battery",
    )),
    "TEMPERATURE": _sensor_schema(sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon="mdi:thermometer",
    )),
    "BATTERY_LEVEL": _sensor_schema(sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon="mdi:battery",
    )),
}

CONFIG_SCHEMA = cv.typed_schema(TYPES, key=CONF_TYPE, upper=True)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_BM6_BLE_ID])
    sens = await sensor.new_sensor(config)

    if config[CONF_TYPE] == "VOLTAGE":
        cg.add(hub.set_voltage_sensor(sens))
    elif config[CONF_TYPE] == "TEMPERATURE":
        cg.add(hub.set_temperature_sensor(sens))
    elif config[CONF_TYPE] == "BATTERY_LEVEL":
        cg.add(hub.set_level_sensor(sens))