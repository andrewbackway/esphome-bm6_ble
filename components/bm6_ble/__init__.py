import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

AUTO_LOAD = ["ble_client", "sensor", "binary_sensor"]
MULTI_CONF = True


# Define the namespace
bm6_ble_ns = cg.esphome_ns.namespace("bm6_ble")

# CRITICAL: Inheriting from cg.Component FIRST is what tells ESPHome 
# to declare this as a pointer (*) in main.cpp
BM6Hub = bm6_ble_ns.class_("BM6Hub", cg.Component, ble_client.BLEClientNode)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BM6Hub),
}).extend(ble_client.BLE_CLIENT_SCHEMA).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # This creates the pointer and the 'new' assignment
    var = cg.new_variable(config[CONF_ID], BM6Hub.new())
    
    # We MUST await these in 2026.2.x to avoid the "coroutine never awaited" error
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)