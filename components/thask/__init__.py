"""tHAsk: the tasks of a tHAsk list or smart view on a tile and a card, and the open ones counted in the top bar.

The two sensors of plugin.yaml (the top bar's list) come in here; register_plugin() reads the id, version, tile memory
and the screen's texts from the manifest and translations/ beside this folder (docs/FIRMWARE_API.md).
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

CONF_OPEN = "open"
CONF_OVERDUE = "overdue"

thask_ns = cg.esphome_ns.namespace("thask")
Thask = thask_ns.class_("Thask", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Thask),
        cv.Required(CONF_OPEN): cv.use_id(sensor.Sensor),
        cv.Required(CONF_OVERDUE): cv.use_id(sensor.Sensor),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_open(await cg.get_variable(config[CONF_OPEN])))
    cg.add(var.set_overdue(await cg.get_variable(config[CONF_OVERDUE])))
