import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

AUTO_LOAD = ["ble_client", "sensor", "binary_sensor"]
MULTI_CONF = True

bm6_ble_ns = cg.esphome_ns.namespace("bm6_ble")
BM6Hub = bm6_ble_ns.class_("BM6Hub", ble_client.BLEClientNode, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BM6Hub),
}).extend(ble_client.BLE_CLIENT_SCHEMA).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # Use new_variable instead of new_PVar
    var = cg.new_variable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)