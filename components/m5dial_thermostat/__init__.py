from esphome import cg
from esphome.components import display, font, ledc, rtttl, select
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISPLAY_ID,
    CONF_ENTITY_ID,
    CONF_ID,
    CONF_OUTPUT,
    CONF_PIN,
)
from esphome.core import ID

m5dial_ns = cg.esphome_ns.namespace("m5dial_thermostat")
api_ns = cg.esphome_ns.namespace("api")

M5DialThermostat = m5dial_ns.class_(
    "M5DialThermostat", cg.Component, api_ns.class_("CustomAPIDevice")
)
UnitSelect = m5dial_ns.class_("UnitSelect", select.Select, cg.Component)

DEPENDENCIES = ["api"]
CODEOWNERS = ["@yourname"]

CONF_ACTIVE_BRIGHTNESS = "active_brightness"
CONF_IDLE_BRIGHTNESS = "idle_brightness"
CONF_IDLE_TIMEOUT = "idle_timeout"
CONF_ENABLE_SOUNDS = "enable_sounds"
CONF_COMMS_TIMEOUT = "comms_timeout"

FONT_FILE = "gfonts://Roboto"
FONT_GLYPHSET = "GF_Latin_Kernel"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5DialThermostat),
        cv.Required(CONF_ENTITY_ID): cv.string,
        cv.Required(CONF_DISPLAY_ID): cv.use_id(display.Display),
        cv.Optional(CONF_ACTIVE_BRIGHTNESS, default=255): cv.int_range(min=0, max=255),
        cv.Optional(CONF_IDLE_BRIGHTNESS, default=50): cv.int_range(min=0, max=255),
        cv.Optional(CONF_IDLE_TIMEOUT, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ENABLE_SOUNDS, default=True): cv.boolean,
        cv.Optional(CONF_COMMS_TIMEOUT, default="30s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def _create_ledc_output(pin, component_id):
    ledc_config = {CONF_ID: component_id, CONF_PIN: pin}
    await ledc.to_code(ledc_config)
    return component_id


async def _create_ledc_backlight(owner_id):
    backlight_id = ID(f"{owner_id}_backlight", is_declaration=True, type=ledc.LEDCOutput)
    pin = await cg.gpio_pin_expression("GPIO9")
    await _create_ledc_output(pin, backlight_id)
    return backlight_id


async def _create_buzzer(owner_id):
    buzzer_output_id = ID(
        f"{owner_id}_buzzer_output", is_declaration=True, type=ledc.LEDCOutput
    )
    pin = await cg.gpio_pin_expression("GPIO3")
    await _create_ledc_output(pin, buzzer_output_id)

    rtttl_id = ID(f"{owner_id}_buzzer_tone", is_declaration=True, type=rtttl.Rtttl)
    rtttl_config = {
        CONF_ID: rtttl_id,
        CONF_OUTPUT: buzzer_output_id,
    }
    await rtttl.to_code(rtttl_config)
    return rtttl_id


async def _create_font(owner_id, suffix, size):
    font_id = ID(f"{owner_id}_{suffix}", is_declaration=True, type=font.Font)
    raw_data_id = ID(
        f"{owner_id}_{suffix}_raw_data", is_declaration=True, type=cg.uint8
    )
    raw_glyph_id = ID(
        f"{owner_id}_{suffix}_raw_glyph", is_declaration=True, type=font.Glyph
    )
    font_config = {
        CONF_ID: font_id,
        font.CONF_FILE: FONT_FILE,
        font.CONF_SIZE: size,
        font.CONF_BPP: 4,
        font.CONF_GLYPHSETS: [FONT_GLYPHSET],
        font.CONF_GLYPHS: [
            c
            for c in dict.fromkeys(
                " ?°0123456789./-CFHeatingCoolFanIdleDryAutoSetdeg"
            )
        ],
        font.CONF_RAW_DATA_ID: raw_data_id,
        font.CONF_RAW_GLYPH_ID: raw_glyph_id,
    }
    await font.to_code(font_config)
    return font_id


async def _create_unit_select(owner_id, thermostat):
    unit_id = ID(f"{owner_id}_unit_select", is_declaration=True, type=UnitSelect)
    unit_config = {
        cv.GenerateID(): unit_id,
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

    owner_id = str(config[CONF_ID])

    backlight_id = await _create_ledc_backlight(owner_id)
    cg.add(var.set_backlight(await cg.get_variable(backlight_id)))

    rtttl_id = await _create_buzzer(owner_id)
    cg.add(var.set_rtttl(await cg.get_variable(rtttl_id)))

    font_mode_id = await _create_font(owner_id, "font_mode", 16)
    font_setpoint_id = await _create_font(owner_id, "font_setpoint", 20)
    font_temp_id = await _create_font(owner_id, "font_temp", 48)
    font_error_id = await _create_font(owner_id, "font_error", 72)

    cg.add(var.set_font_mode(await cg.get_variable(font_mode_id)))
    cg.add(var.set_font_setpoint(await cg.get_variable(font_setpoint_id)))
    cg.add(var.set_font_temp(await cg.get_variable(font_temp_id)))
    cg.add(var.set_font_error(await cg.get_variable(font_error_id)))

    unit_select = await _create_unit_select(owner_id, var)
    cg.add(var.set_unit_select(unit_select))

    cg.add(var.set_active_brightness(config[CONF_ACTIVE_BRIGHTNESS]))
    cg.add(var.set_idle_brightness(config[CONF_IDLE_BRIGHTNESS]))
    cg.add(var.set_idle_timeout(config[CONF_IDLE_TIMEOUT]))
    cg.add(var.set_comms_timeout(config[CONF_COMMS_TIMEOUT]))
    cg.add(var.set_enable_sounds(config[CONF_ENABLE_SOUNDS]))
