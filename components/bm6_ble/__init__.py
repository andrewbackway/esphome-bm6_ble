import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

AUTO_LOAD = ["sensor", "binary_sensor"]
DEPENDENCIES = ["esp32_ble_tracker", "ble_client"]
CODEOWNERS = ["@andrewbackway"]

# Namespace and Class definitions
bm6_ble_ns = cg.esphome_ns.namespace("bm6_ble")
BM6Hub = bm6_ble_ns.class_("BM6Hub", cg.Component, ble_client.BLEClientNode)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BM6Hub),
    cv.Required("ble_client_id"): cv.use_id(ble_client.BLEClient),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    ble = await cg.get_variable(config["ble_client_id"])
    cg.add(ble.register_ble_node(var))
