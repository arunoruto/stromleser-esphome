import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

DEPENDENCIES = ['wifi']

shelly_emulator_ns = cg.esphome_ns.namespace('shelly_emulator')
ShellyEmulator = shelly_emulator_ns.class_('ShellyEmulator', cg.Component)

CONF_PHASE_COUNT = 'phase_count'
CONF_POWER_OFFSET = 'power_offset'
CONF_UDP_PORT = 'udp_port'
CONF_POWER_SENSOR = 'power_sensor'
CONF_ENERGY_CONSUMED_SENSOR = 'energy_consumed_sensor'
CONF_ENERGY_DELIVERED_SENSOR = 'energy_delivered_sensor'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ShellyEmulator),
    cv.Optional(CONF_PHASE_COUNT, default=1): cv.int_range(1, 3),
    cv.Optional(CONF_POWER_OFFSET, default=0.0): cv.float_,
    cv.Optional(CONF_UDP_PORT, default=2220): cv.port,
    cv.Required(CONF_POWER_SENSOR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_ENERGY_CONSUMED_SENSOR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_ENERGY_DELIVERED_SENSOR): cv.use_id(sensor.Sensor),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_phase_count(config[CONF_PHASE_COUNT]))
    cg.add(var.set_power_offset(config[CONF_POWER_OFFSET]))
    cg.add(var.set_udp_port(config[CONF_UDP_PORT]))

    power_sens = await cg.get_variable(config[CONF_POWER_SENSOR])
    cg.add(var.set_power_sensor(power_sens))

    energy_cons_sens = await cg.get_variable(config[CONF_ENERGY_CONSUMED_SENSOR])
    cg.add(var.set_energy_consumed_sensor(energy_cons_sens))

    energy_del_sens = await cg.get_variable(config[CONF_ENERGY_DELIVERED_SENSOR])
    cg.add(var.set_energy_delivered_sensor(energy_del_sens))
