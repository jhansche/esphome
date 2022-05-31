import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, ble_client, time
from esphome.const import (
    CONF_HEAT_MODE,
    CONF_ID,
    CONF_RECEIVE_TIMEOUT,
    CONF_TIME_ID,
)
from . import (
    BEDJET_CLIENT_COMPAT_SCHEMA,
    BLE_CLIENT_COMPAT_SCHEMA,
    CONF_BEDJET_ID,
    register_bedjet_child,
)

CODEOWNERS = ["@jhansche"]
DEPENDENCIES = ["ble_client"]

bedjet_ns = cg.esphome_ns.namespace("bedjet")
Bedjet = bedjet_ns.class_(
    "Bedjet", climate.Climate, ble_client.BLEClientNode, cg.PollingComponent
)
BedjetHeatMode = bedjet_ns.enum("BedjetHeatMode")
BEDJET_HEAT_MODES = {
    "heat": BedjetHeatMode.HEAT_MODE_HEAT,
    "extended": BedjetHeatMode.HEAT_MODE_EXTENDED,
}

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Bedjet),
            cv.Optional(CONF_HEAT_MODE, default="heat"): cv.enum(
                BEDJET_HEAT_MODES, lower=True
            ),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="0s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(BLE_CLIENT_COMPAT_SCHEMA)
    .extend(BEDJET_CLIENT_COMPAT_SCHEMA)
    .extend(cv.polling_component_schema("30s")),
    # TODO: compat layer
    cv.has_at_most_one_key(ble_client.CONF_BLE_CLIENT_ID, CONF_BEDJET_ID),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    # TODO: compat layer
    if ble_client.CONF_BLE_CLIENT_ID in config:
        # TODO: this is deprecated, replaced by bedjet_id
        # await ble_client.register_ble_node(var, config)
        yield
    elif CONF_BEDJET_ID in config:
        await register_bedjet_child(var, config)

    cg.add(var.set_heating_mode(config[CONF_HEAT_MODE]))

    # TODO: move to parent
    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time_id(time_))
    if CONF_RECEIVE_TIMEOUT in config:
        cg.add(var.set_status_timeout(config[CONF_RECEIVE_TIMEOUT]))
