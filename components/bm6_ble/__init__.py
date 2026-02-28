import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

AUTO_LOAD = ["ble_client", "sensor", "binary_sensor"]
MULTI_CONF = True

bm6_ble_ns = cg.esphome_ns.namespace("bm6_ble")

# FIX: cg.Component MUST come first for the ID to be declared as a pointer in C++
BM6Hub = bm6_ble_ns.class_("BM6Hub", cg.Component, ble_client.BLEClientNode)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BM6Hub),
}).extend(ble_client.BLE_CLIENT_SCHEMA).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # This now correctly generates: bm6_ble::BM6Hub *bm6_1 = new bm6_ble::BM6Hub();
    var = cg.new_variable(config[CONF_ID], BM6Hub.new())
    
    # Await the registrations for 2026.2.x compatibility
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)