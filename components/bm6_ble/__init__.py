import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

# Namespace and Class definitions
bm6_ble_ns = cg.esphome_ns.namespace("bm6_ble")
BM6Hub = bm6_ble_ns.class_("BM6Hub", cg.Component, ble_client.BLEClientNode)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BM6Hub),
    # Ensure you are passing the ble_client_id in your YAML
    cv.Required("ble_client_id"): cv.use_id(ble_client.BLEClient),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # This is the "Magic" line from your working bm2_ble code
    # It forces 'BM6Hub *bm6_1' in the global scope
    var = cg.new_Pvariable(config[CONF_ID])
    
    # Standard component registration
    await cg.register_component(var, config)
    
    # BLE Node registration matching the working bm2_ble pattern
    ble = await cg.get_variable(config["ble_client_id"])
    cg.add(ble.register_ble_node(var))