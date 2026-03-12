from esphome import codegen as cg
from esphome.components import display, select
from esphome.components.ledc import output as ledc_output
from esphome.components.font import Font as EspFont
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISPLAY_ID,
    CONF_ENTITY_ID,
    CONF_ID,
    CONF_FREQUENCY,
    CONF_DISABLED_BY_DEFAULT,
    CONF_NAME,
    CONF_OUTPUT,
    CONF_MODE,
    CONF_INVERTED,
    CONF_NUMBER,
    CONF_PIN,
)
from esphome.core import ID
from esphome.components.esp32 import gpio as esp32_gpio

m5dial_ns = cg.esphome_ns.namespace("m5dial_thermostat")
api_ns = cg.esphome_ns.namespace("api")

M5DialThermostat = m5dial_ns.class_(
    "M5DialThermostat", cg.Component, api_ns.class_("CustomAPIDevice")
)
UnitSelect = m5dial_ns.class_("UnitSelect", select.Select, cg.Component)

DEPENDENCIES = ["api"]
AUTO_LOAD = ["select", "output", "ledc", "font"]
CODEOWNERS = ["@jamesshannon"]

CONF_ACTIVE_BRIGHTNESS = "active_brightness"
CONF_IDLE_BRIGHTNESS = "idle_brightness"
CONF_IDLE_TIMEOUT = "idle_timeout"
CONF_ENABLE_SOUNDS = "enable_sounds"
CONF_COMMS_TIMEOUT = "comms_timeout"
CONF_FONT_MODE_ID = "font_mode_id"
CONF_FONT_SETPOINT_ID = "font_setpoint_id"
CONF_FONT_TEMP_ID = "font_temp_id"
CONF_FONT_ERROR_ID = "font_error_id"
CONF_BACKLIGHT_ID = "backlight_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5DialThermostat),
        cv.Required(CONF_ENTITY_ID): cv.string,
        cv.Required(CONF_DISPLAY_ID): cv.use_id(display.Display),
        cv.Optional(CONF_ACTIVE_BRIGHTNESS, default=180): cv.int_range(min=0, max=255),
        cv.Optional(CONF_IDLE_BRIGHTNESS, default=50): cv.int_range(min=0, max=255),
        cv.Optional(CONF_IDLE_TIMEOUT, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ENABLE_SOUNDS, default=True): cv.boolean,
        cv.Optional(CONF_COMMS_TIMEOUT, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_FONT_MODE_ID): cv.use_id(EspFont),
        cv.Optional(CONF_FONT_SETPOINT_ID): cv.use_id(EspFont),
        cv.Optional(CONF_FONT_TEMP_ID): cv.use_id(EspFont),
        cv.Optional(CONF_FONT_ERROR_ID): cv.use_id(EspFont),
        cv.GenerateID(CONF_BACKLIGHT_ID): cv.declare_id(ledc_output.LEDCOutput),
    }
).extend(cv.COMPONENT_SCHEMA)


def _normalize_pin(value):
    if isinstance(value, str):
        if value.upper().startswith("GPIO"):
            return int(value[4:])
        return int(value)
    return int(value)


async def _create_ledc_output(pin, component_id):
    # NOTE: pin is validated manually for LEDCOutput output pin generation.
    pin_id = ID(f"{component_id}_pin", is_declaration=True, type=esp32_gpio.ESP32InternalGPIOPin)
    ledc_config = {
        CONF_ID: component_id,
        CONF_PIN: {
            CONF_ID: pin_id,
            CONF_NUMBER: _normalize_pin(pin),
            CONF_INVERTED: False,
            CONF_MODE: {
                CONF_OUTPUT: True,
            },
        },
        CONF_FREQUENCY: 1000.0,
    }
    await ledc_output.to_code(ledc_config)
    return component_id


async def _create_ledc_backlight(backlight_id):
    await _create_ledc_output("GPIO9", backlight_id)
    return backlight_id


async def _create_buzzer(owner_id):
    del owner_id
    return None


async def _create_unit_select(owner_id, thermostat):
    unit_id = ID(f"{owner_id}_unit_select", is_declaration=True, type=UnitSelect)
    unit_config = {
        cv.GenerateID(): unit_id,
        CONF_NAME: "Unit",
        CONF_DISABLED_BY_DEFAULT: False,
    }
    unit = await select.new_select(unit_config, options=["celsius", "fahrenheit"])
    cg.add(unit.set_parent(thermostat))
    return unit


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))

    display_obj = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(display_obj))

    backlight_id = await _create_ledc_backlight(config[CONF_BACKLIGHT_ID])
    cg.add(var.set_backlight(await cg.get_variable(backlight_id)))

    await _create_buzzer(str(config[CONF_ID]))

    if CONF_FONT_MODE_ID in config:
        cg.add(var.set_font_mode(await cg.get_variable(config[CONF_FONT_MODE_ID])))
    if CONF_FONT_SETPOINT_ID in config:
        cg.add(var.set_font_setpoint(await cg.get_variable(config[CONF_FONT_SETPOINT_ID])))
    if CONF_FONT_TEMP_ID in config:
        cg.add(var.set_font_temp(await cg.get_variable(config[CONF_FONT_TEMP_ID])))
    if CONF_FONT_ERROR_ID in config:
        cg.add(var.set_font_error(await cg.get_variable(config[CONF_FONT_ERROR_ID])))

    unit_select = await _create_unit_select(str(config[CONF_ID]), var)
    cg.add(var.set_unit_select(unit_select))

    cg.add(var.set_active_brightness(config[CONF_ACTIVE_BRIGHTNESS]))
    cg.add(var.set_idle_brightness(config[CONF_IDLE_BRIGHTNESS]))
    cg.add(var.set_idle_timeout(config[CONF_IDLE_TIMEOUT]))
    cg.add(var.set_comms_timeout(config[CONF_COMMS_TIMEOUT]))
    cg.add(var.set_enable_sounds(config[CONF_ENABLE_SOUNDS]))
