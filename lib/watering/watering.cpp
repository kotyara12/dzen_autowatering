#include "watering.h"
#include "strings.h"
#include "math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include "project_config.h"
#include "def_consts.h"
#include "def_alarm.h"
#include "esp_timer.h"
#include "mbcontroller.h"
#include "rLog.h"
#include "rTypes.h"
#include "rStrings.h"
#include "reStates.h"
#include "reEvents.h"
#include "reMqtt.h"
#include "reEsp32.h"
#include "reWiFi.h"
#if CONFIG_TELEGRAM_ENABLE
#include "reTgSend.h"
#endif // CONFIG_TELEGRAM_ENABLE
#if CONFIG_DATASEND_ENABLE
#include "reDataSend.h"
#endif // CONFIG_DATASEND_ENABLE
#if defined(CONFIG_ELTARIFFS_ENABLED) && CONFIG_ELTARIFFS_ENABLED
#include "reElTariffs.h"
#endif // CONFIG_ELTARIFFS_ENABLED
#include "reLed.h" 
#include "reLoadCtrl.h"
#include "reGpio.h"

static const char* logTAG   = "WTRС";
static const char* wateringTaskName = "watering";
static uint32_t _sensorsReadInterval = CONFIG_WATERING_TASK_CYCLE / 1000;
static bool _sensorsNeedStore = false;
static TaskHandle_t _wateringTask;
static EventGroupHandle_t _wateringFlags = nullptr;

// Низкий уровень воды
#define WATER_LEVEL_CHANGED   BIT0
#define WATER_LEVEL_LOW       BIT1

// Перелив
#define WATER_LEAK_IN1        BIT2
#define WATER_LEAK_IN2        BIT3
#define WATER_LEAK_IN3        BIT4

// Временные события
#define TIME_MINUTE_EVENT     BIT8

// Есть изменения на любом из входов
#define FORCED_CONTROL        (WATER_LEVEL_CHANGED | TIME_MINUTE_EVENT)

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------ Параметры ------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static paramsGroupHandle_t pgSensors;
static paramsGroupHandle_t pgIntervals;
static paramsGroupHandle_t pgWatering;

static void sensorsInitParameters()
{
  // ------------------------------------------------------------------------------------------
  // Группы
  // ------------------------------------------------------------------------------------------
  pgSensors = paramsRegisterGroup(nullptr, 
    CONFIG_SENSOR_PGROUP_ROOT_KEY, CONFIG_SENSOR_PGROUP_ROOT_TOPIC, CONFIG_SENSOR_PGROUP_ROOT_FRIENDLY);
  pgIntervals = paramsRegisterGroup(pgSensors, 
    CONFIG_SENSOR_PGROUP_INTERVALS_KEY, CONFIG_SENSOR_PGROUP_INTERVALS_TOPIC, CONFIG_SENSOR_PGROUP_INTERVALS_FRIENDLY);
  pgWatering = paramsRegisterGroup(nullptr, 
    CONFIG_WATERING_KEY, CONFIG_WATERING_TOPIC, CONFIG_WATERING_FRIENDLY);

  // ------------------------------------------------------------------------------------------
  // Интервалы
  // ------------------------------------------------------------------------------------------
  // Период чтения данных с сенсоров
  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgIntervals,
    CONFIG_SENSOR_PARAM_INTERVAL_READ_KEY, CONFIG_SENSOR_PARAM_INTERVAL_READ_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&_sensorsReadInterval);

  // Период публикации данных с сенсоров на MQTT
  paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgIntervals,
    CONFIG_SENSOR_PARAM_INTERVAL_MQTT_KEY, CONFIG_SENSOR_PARAM_INTERVAL_MQTT_FRIENDLY,
    CONFIG_MQTT_PARAMS_QOS, (void*)&iMqttPubInterval);

  #if CONFIG_OPENMON_ENABLE
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgIntervals,
      CONFIG_SENSOR_PARAM_INTERVAL_OPENMON_KEY, CONFIG_SENSOR_PARAM_INTERVAL_OPENMON_FRIENDLY,
      CONFIG_MQTT_PARAMS_QOS, (void*)&iOpenMonInterval);
  #endif // CONFIG_OPENMON_ENABLE

  #if CONFIG_NARODMON_ENABLE
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgIntervals,
      CONFIG_SENSOR_PARAM_INTERVAL_NARODMON_KEY, CONFIG_SENSOR_PARAM_INTERVAL_NARODMON_FRIENDLY,
      CONFIG_MQTT_PARAMS_QOS, (void*)&iNarodMonInterval);
  #endif // CONFIG_NARODMON_ENABLE

  #if CONFIG_THINGSPEAK_ENABLE
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgIntervals,
      CONFIG_SENSOR_PARAM_INTERVAL_THINGSPEAK_KEY, CONFIG_SENSOR_PARAM_INTERVAL_THINGSPEAK_FRIENDLY,
      CONFIG_MQTT_PARAMS_QOS, (void*)&iThingSpeakInterval);
  #endif // CONFIG_THINGSPEAK_ENABLE

  // ------------------------------------------------------------------------------------------
  // Полив
  // ------------------------------------------------------------------------------------------
  if (pgWatering) {
    // Уведомления о включении и выключении в TG
    paramsSetLimitsU8(
      paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgWatering, 
        CONFIG_NOTIFY_WATERING_KEY, CONFIG_NOTIFY_WATERING_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&wateringNotify),
      (uint8_t)NOTIFY_OFF, (uint8_t)NOTIFY_SOUND);
    paramsSetLimitsU8(
      paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgWatering, 
        CONFIG_NOTIFY_WATERLEAK_KEY, CONFIG_NOTIFY_WATERLEAK_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&waterleakNotify),
      (uint8_t)NOTIFY_OFF, (uint8_t)NOTIFY_SOUND);
    paramsSetLimitsU8(
      paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgWatering, 
        CONFIG_NOTIFY_WATERLVL_KEY, CONFIG_NOTIFY_WATERLVL_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&waterlevelNotify),
      (uint8_t)NOTIFY_OFF, (uint8_t)NOTIFY_SOUND);

    // Датчики уровня и протечки
    paramsSetLimitsU8(
      paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgWatering, 
        CONFIG_WATERLVL_ENABLED_KEY, CONFIG_WATERLVL_ENABLED_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&waterlevelSensorEnabled),
      0, 1);
    paramsSetLimitsU8(
      paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgWatering, 
        CONFIG_WATERLEAK_ENABLED1_KEY, CONFIG_WATERLEAK_ENABLED1_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&waterleakSensorEnabled1),
      0, 1);
    paramsSetLimitsU8(
      paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgWatering, 
        CONFIG_WATERLEAK_ENABLED2_KEY, CONFIG_WATERLEAK_ENABLED2_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&waterleakSensorEnabled2),
      0, 1);
    paramsSetLimitsU8(
      paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgWatering, 
        CONFIG_WATERLEAK_ENABLED3_KEY, CONFIG_WATERLEAK_ENABLED3_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&waterleakSensorEnabled3),
      0, 1);
    // Количество циклов подтверждения устранения утечки
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgWatering, 
        CONFIG_WATERLEAK_DEBOUNCE_KEY, CONFIG_WATERLEAK_DEBOUNCE_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&waterleakDebounceCount);

    // Режим управления поливом
    paramsSetLimitsU8(
      paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U8, nullptr, pgWatering, 
        CONFIG_MODE_KEY, CONFIG_MODE_FRIENDLY, 
        CONFIG_MQTT_PARAMS_QOS, (void*)&wateringMode),
      (uint8_t)WATERING_OFF, (uint8_t)WATERING_SENSORS);
    // Расписание работы полива
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_TIMESPAN, nullptr, pgWatering, 
      CONFIG_TIMESPAN_KEY, CONFIG_TIMESPAN_FRIENDLY, 
      CONFIG_MQTT_PARAMS_QOS, (void*)&wateringTimespan);
    // Влажность почвы
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgWatering, 
      CONFIG_WATERING_MST_MIN_KEY, CONFIG_WATERING_MST_MIN_FRIENDLY, 
      CONFIG_MQTT_PARAMS_QOS, (void*)&wateringMoistureMin);
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgWatering, 
      CONFIG_WATERING_MST_MAX_KEY, CONFIG_WATERING_MST_MAX_FRIENDLY, 
      CONFIG_MQTT_PARAMS_QOS, (void*)&wateringMoistureMax);
    // Температура почвы
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgWatering, 
      CONFIG_WATERING_ST_MIN_KEY, CONFIG_WATERING_ST_MIN_FRIENDLY, 
      CONFIG_MQTT_PARAMS_QOS, (void*)&wateringSoilTempMin);
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_FLOAT, nullptr, pgWatering, 
      CONFIG_WATERING_ST_MAX_KEY, CONFIG_WATERING_ST_MAX_FRIENDLY, 
      CONFIG_MQTT_PARAMS_QOS, (void*)&wateringSoilTempMax);
    // Максимальное время полива
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgWatering, 
      CONFIG_WATERING_MAX_DUR_KEY, CONFIG_WATERING_MAX_DUR_FRIENDLY, 
      CONFIG_MQTT_PARAMS_QOS, (void*)&wateringMoistureMaxDuration);
    // Время работы для одного цикла
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgWatering, 
      CONFIG_WATERING_CYC_TIME_KEY, CONFIG_WATERING_CYC_TIME_FRIENDLY, 
      CONFIG_MQTT_PARAMS_QOS, (void*)&wateringMoistureCycleTime);
    // Интервал повторения циклов
    paramsRegisterValue(OPT_KIND_PARAMETER, OPT_TYPE_U32, nullptr, pgWatering, 
      CONFIG_WATERING_CYC_INTV_KEY, CONFIG_WATERING_CYC_INTV_FRIENDLY, 
      CONFIG_MQTT_PARAMS_QOS, (void*)&wateringMoistureCycleInterval);
  };
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------- Сенсоры -------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static bool sensorsPublish(rSensor *sensor, char* topic, char* payload, const bool free_topic, const bool free_payload)
{
  return mqttPublish(topic, payload, CONFIG_MQTT_SENSORS_QOS, CONFIG_MQTT_SENSORS_RETAINED, free_topic, free_payload);
}

static void sensorsMqttTopicsCreate(bool primary)
{
  sensorSoil.topicsCreate(primary);
  sensorIndoor.topicsCreate(primary);
  sensorHeating.topicsCreate(primary);
}

static void sensorsMqttTopicsFree()
{
  sensorSoil.topicsFree();
  sensorIndoor.topicsFree();
  sensorHeating.topicsFree();
}

static void relaysStoreData();
static void sensorsStoreData()
{
  rlog_i(logTAG, "Store sensors data");

  sensorSoil.nvsStoreExtremums(SENSOR_SOIL_KEY); 
  sensorIndoor.nvsStoreExtremums(SENSOR_INDOOR_KEY); 
  sensorHeating.nvsStoreExtremums(SENSOR_HEATING_KEY); 

  relaysStoreData();
}

static void sensorsInitModbus()
{
  rlog_i(logTAG, "Modbus initialization");
  // Инициализация RS485 и Modbus
  RE_OK_CHECK_EVENT(mbc_master_init(MB_PORT_SERIAL_MASTER, &_modbus), return);
  // Configure Modbus
  mb_communication_info_t comm;
  memset(&comm, 0, sizeof(comm));
  comm.mode = MB_MODE_RTU;
  comm.port = SENSOR_MODBUS_PORT;
  comm.baudrate = SENSOR_MODBUS_SPEED;
  comm.parity = UART_PARITY_DISABLE;
  RE_OK_CHECK_EVENT(mbc_master_setup((void*)&comm), return);
  // Set UART pins
  RE_OK_CHECK_EVENT(uart_set_pin(SENSOR_MODBUS_PORT, SENSOR_MODBUS_PIN_TXD, SENSOR_MODBUS_PIN_RXD, SENSOR_MODBUS_PIN_RTS, SENSOR_MODBUS_PIN_CTS), return);
  // Start Modbus
  RE_OK_CHECK_EVENT(mbc_master_start(), return);
  // Set UART mode
  RE_OK_CHECK_EVENT(uart_set_mode(SENSOR_MODBUS_PORT, UART_MODE_RS485_HALF_DUPLEX), return);
}

static void sensorsInitSensors()
{
  // Почва
  static rTemperatureItem siSoilTemp(nullptr, CONFIG_SENSOR_TEMP_NAME, CONFIG_FORMAT_TEMP_UNIT,
    SENSOR_SOIL_FILTER_MODE, SENSOR_SOIL_FILTER_SIZE, 
    CONFIG_FORMAT_TEMP_VALUE, CONFIG_FORMAT_TEMP_STRING,
    #if CONFIG_SENSOR_TIMESTAMP_ENABLE
      CONFIG_FORMAT_TIMESTAMP_L, 
    #endif // CONFIG_SENSOR_TIMESTAMP_ENABLE
    #if CONFIG_SENSOR_TIMESTRING_ENABLE  
      CONFIG_FORMAT_TIMESTAMP_S, CONFIG_FORMAT_TSVALUE
    #endif // CONFIG_SENSOR_TIMESTRING_ENABLE
  );
  static rSensorItem siSoilMois(nullptr, CONFIG_SENSOR_MOISTURE_NAME, 
    SENSOR_SOIL_FILTER_MODE, SENSOR_SOIL_FILTER_SIZE, 
    CONFIG_FORMAT_MOISTURE_VALUE, CONFIG_FORMAT_MOISTURE_STRING,
    #if CONFIG_SENSOR_TIMESTAMP_ENABLE
      CONFIG_FORMAT_TIMESTAMP_L, 
    #endif // CONFIG_SENSOR_TIMESTAMP_ENABLE
    #if CONFIG_SENSOR_TIMESTRING_ENABLE  
      CONFIG_FORMAT_TIMESTAMP_S, CONFIG_FORMAT_TSVALUE
    #endif // CONFIG_SENSOR_TIMESTRING_ENABLE
  );
  sensorSoil.initExtItems(SENSOR_SOIL_NAME, SENSOR_SOIL_TOPIC, false,
    _modbus, SENSOR_SOIL_ADDRESS, SENSOR_SOIL_TYPE,
    &siSoilTemp, &siSoilMois, nullptr, nullptr,
    1000, SENSOR_SOIL_ERRORS_LIMIT, nullptr, sensorsPublish);
  sensorSoil.registerParameters(pgSensors, SENSOR_SOIL_KEY, SENSOR_SOIL_TOPIC, SENSOR_SOIL_NAME);
  sensorSoil.nvsRestoreExtremums(SENSOR_SOIL_KEY);

  // Комната
  static rTemperatureItem siIndoorTemp(nullptr, CONFIG_SENSOR_TEMP_NAME, CONFIG_FORMAT_TEMP_UNIT,
    SENSOR_INDOOR_FILTER_MODE, SENSOR_INDOOR_FILTER_SIZE, 
    CONFIG_FORMAT_TEMP_VALUE, CONFIG_FORMAT_TEMP_STRING,
    #if CONFIG_SENSOR_TIMESTAMP_ENABLE
      CONFIG_FORMAT_TIMESTAMP_L, 
    #endif // CONFIG_SENSOR_TIMESTAMP_ENABLE
    #if CONFIG_SENSOR_TIMESTRING_ENABLE  
      CONFIG_FORMAT_TIMESTAMP_S, CONFIG_FORMAT_TSVALUE
    #endif // CONFIG_SENSOR_TIMESTRING_ENABLE
  );
  static rSensorItem siIndoorHum(nullptr, CONFIG_SENSOR_HUMIDITY_NAME, 
    SENSOR_INDOOR_FILTER_MODE, SENSOR_INDOOR_FILTER_SIZE, 
    CONFIG_FORMAT_HUMIDITY_VALUE, CONFIG_FORMAT_HUMIDITY_STRING,
    #if CONFIG_SENSOR_TIMESTAMP_ENABLE
      CONFIG_FORMAT_TIMESTAMP_L, 
    #endif // CONFIG_SENSOR_TIMESTAMP_ENABLE
    #if CONFIG_SENSOR_TIMESTRING_ENABLE  
      CONFIG_FORMAT_TIMESTAMP_S, CONFIG_FORMAT_TSVALUE
    #endif // CONFIG_SENSOR_TIMESTRING_ENABLE
  );
  sensorIndoor.initExtItems(SENSOR_INDOOR_NAME, SENSOR_INDOOR_TOPIC, false,
    SENSOR_INDOOR_BUS, HTU2X_RES_RH12_TEMP14, true,
    &siIndoorHum, &siIndoorTemp,
    3000, SENSOR_INDOOR_ERRORS_LIMIT, nullptr, sensorsPublish);
  sensorIndoor.registerParameters(pgSensors, SENSOR_INDOOR_KEY, SENSOR_INDOOR_TOPIC, SENSOR_INDOOR_NAME);
  sensorIndoor.nvsRestoreExtremums(SENSOR_INDOOR_KEY);

  // Батареи отопления
  static rTemperatureItem siHeatingTemp(nullptr, CONFIG_SENSOR_TEMP_NAME, CONFIG_FORMAT_TEMP_UNIT,
    SENSOR_HEATING_FILTER_MODE, SENSOR_HEATING_FILTER_SIZE, 
    CONFIG_FORMAT_TEMP_VALUE, CONFIG_FORMAT_TEMP_STRING,
    #if CONFIG_SENSOR_TIMESTAMP_ENABLE
      CONFIG_FORMAT_TIMESTAMP_L, 
    #endif // CONFIG_SENSOR_TIMESTAMP_ENABLE
    #if CONFIG_SENSOR_TIMESTRING_ENABLE  
      CONFIG_FORMAT_TIMESTAMP_S, CONFIG_FORMAT_TSVALUE
    #endif // CONFIG_SENSOR_TIMESTRING_ENABLE
  );
  sensorHeating.initExtItems(SENSOR_HEATING_NAME, SENSOR_HEATING_TOPIC, false,
    (gpio_num_t)SENSOR_HEATING_PIN, ONEWIRE_NONE, SENSOR_HEATING_INDEX, 
    DS18x20_RESOLUTION_12_BIT, true, 
    &siHeatingTemp, 
    1000, SENSOR_HEATING_ERRORS_LIMIT, nullptr, sensorsPublish);
  sensorHeating.registerParameters(pgSensors, SENSOR_HEATING_KEY, SENSOR_HEATING_TOPIC, SENSOR_HEATING_NAME);
  sensorHeating.nvsRestoreExtremums(SENSOR_HEATING_KEY);

  _sensorsNeedStore = false;
  espRegisterShutdownHandler(sensorsStoreData); // #2
}

// Текущие значения с сенсора почвы
static float sensorsGetSoilTemp()
{
  if (sensorSoil.getStatus() == SENSOR_STATUS_OK) {
    return sensorSoil.getValue1(false).filteredValue;
  };
  return NAN;
}

static float sensorsGetSoilMoisture()
{
  if (sensorSoil.getStatus() == SENSOR_STATUS_OK) {
    return sensorSoil.getValue2(false).filteredValue;
  };
  return NAN;
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------- Перелив или протечка ------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
static void ledMode();

static void waterleakInit()
{
  gpio_reset_pin((gpio_num_t)CONFIG_WATER_LEAK_GPIO1);
  gpio_reset_pin((gpio_num_t)CONFIG_WATER_LEAK_GPIO2);
  gpio_reset_pin((gpio_num_t)CONFIG_WATER_LEAK_GPIO3);
  gpio_reset_pin((gpio_num_t)CONFIG_WATER_LEAK_ACTVATOR);
  gpio_set_direction((gpio_num_t)CONFIG_WATER_LEAK_GPIO1, GPIO_MODE_INPUT);
  gpio_set_direction((gpio_num_t)CONFIG_WATER_LEAK_GPIO2, GPIO_MODE_INPUT);
  gpio_set_direction((gpio_num_t)CONFIG_WATER_LEAK_GPIO3, GPIO_MODE_INPUT);
  gpio_set_direction((gpio_num_t)CONFIG_WATER_LEAK_ACTVATOR, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)CONFIG_WATER_LEAK_ACTVATOR, CONFIG_WATER_LEAK_ACT_OFF);
}

void waterleakSendNotify(bool leak, uint8_t input)
{
  #if CONFIG_TELEGRAM_ENABLE
    if (waterleakNotify > NOTIFY_OFF) {
      if (leak) {
        tgSend(CONFIG_WATERING_NOTIFY_KIND, CONFIG_WATERING_NOTIFY_PRIORITY, waterleakNotify == NOTIFY_SOUND, CONFIG_TELEGRAM_DEVICE, 
          "⚠ <b>Внимание!</b> Обнаружен перелив по входу #<b>%d</b>", input);
      } else {
        tgSend(CONFIG_WATERING_NOTIFY_KIND, CONFIG_WATERING_NOTIFY_PRIORITY, waterleakNotify == NOTIFY_SOUND, CONFIG_TELEGRAM_DEVICE, 
          "✅ Перелив по входу #<b>%d</b> устранён", input);
      };
    };
  #endif // CONFIG_TELEGRAM_ENABLE
}

void sensorsWaterLeakMqttPublish()
{
  mqttPublish(mqttGetTopicDevice1(mqttIsPrimary(), false, CONFIG_WATER_LEAK_TOPIC), 
    malloc_stringf("{\"channel1\":%d,\"channel2\":%d,\"channel3\":%d}", 
      ((xEventGroupGetBits(_wateringFlags) & WATER_LEAK_IN1) > 0),
      ((xEventGroupGetBits(_wateringFlags) & WATER_LEAK_IN2) > 0),
      ((xEventGroupGetBits(_wateringFlags) & WATER_LEAK_IN3) > 0)), 
    CONFIG_MQTT_LOAD_QOS, CONFIG_MQTT_LOAD_RETAINED, true, true);
}

static uint32_t waterleakCount1 = 0;
static uint32_t waterleakCount2 = 0;
static uint32_t waterleakCount3 = 0;

static void sensorsCheckWaterLeak(gpio_num_t pin, EventBits_t bit, uint8_t num, uint32_t* counter)
{
  // Читаем состояние входа
  gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
  vTaskDelay(CONFIG_WATER_LEAK_DELAY_ON);
  bool wleak = gpio_get_level(pin) == CONFIG_WATER_LEAK_LEVEL;
  gpio_set_pull_mode(pin, GPIO_FLOATING);
  rlog_d(logTAG, "Water leak control #%d: %d", num, wleak);

  // Если перелив имеется, сбрасываем счетчик в любом случае
  if ((*counter > 0) && wleak) { 
    *counter = 0; 
  };
  // Сверяемся с текущим значением
  if (wleak != ((xEventGroupGetBits(_wateringFlags) & bit) > 0)) {
    if (wleak) {
      // Устанавливаем признак перелива немедленно
      xEventGroupSetBits(_wateringFlags, bit);
      waterleakSendNotify(wleak, num);
      sensorsWaterLeakMqttPublish();
      ledMode();
    } else {
      // Требуется подтверждение waterleakDebounceCount раз, дабы не было "метаний туда и обратно"
      *counter = *counter + 1;
      if (*counter >= waterleakDebounceCount) {
        xEventGroupClearBits(_wateringFlags, bit);
        waterleakSendNotify(wleak, num);
        sensorsWaterLeakMqttPublish();
        ledMode();
      };
    };
  };
}

static bool sensorsGetWaterLeaks()
{
  EventBits_t wlBits = 0x0;
  if (waterleakSensorEnabled1) wlBits |= WATER_LEAK_IN1;
  if (waterleakSensorEnabled2) wlBits |= WATER_LEAK_IN2;
  if (waterleakSensorEnabled3) wlBits |= WATER_LEAK_IN3;
  return (xEventGroupGetBits(_wateringFlags) & wlBits) > 0;
}

static bool sensorsCheckWaterLeaks()
{
  // Читаем состояние входов
  if (waterleakSensorEnabled1) {
    sensorsCheckWaterLeak((gpio_num_t)CONFIG_WATER_LEAK_GPIO1, WATER_LEAK_IN1, 1, &waterleakCount1);
    vTaskDelay(CONFIG_WATER_LEAK_DELAY_OFF);
  };
  if (waterleakSensorEnabled2) {
    sensorsCheckWaterLeak((gpio_num_t)CONFIG_WATER_LEAK_GPIO2, WATER_LEAK_IN2, 2, &waterleakCount2);
    vTaskDelay(CONFIG_WATER_LEAK_DELAY_OFF);
  };
  if (waterleakSensorEnabled3) {
    sensorsCheckWaterLeak((gpio_num_t)CONFIG_WATER_LEAK_GPIO3, WATER_LEAK_IN2, 3, &waterleakCount3);
  };

  // Возвращаем true, если есть перелив
  bool wleaks = sensorsGetWaterLeaks();
  rlog_d(logTAG, "Water leak control summary: %d", wleaks);
  return wleaks;
}

// -----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------- Уровень воды ----------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static reGPIO gpioWaterLevel(CONFIG_WATER_LEVEL_GPIO, CONFIG_WATER_LEVEL_LEVEL, CONFIG_WATER_LEVEL_PULL, CONFIG_WATER_LEVEL_INTR, CONFIG_WATER_LEVEL_DEBOUNCE, nullptr);

static time_t lastLowLevelNotify = 0;

void waterLowLevelNotify(bool level)
{
  #if CONFIG_TELEGRAM_ENABLE
    if (waterlevelNotify > NOTIFY_OFF) {
      if (level) {
        tgSend(CONFIG_WATERING_NOTIFY_KIND, CONFIG_WATERING_NOTIFY_PRIORITY, waterlevelNotify == NOTIFY_SOUND, CONFIG_TELEGRAM_DEVICE, 
          "✅ Уровень воды в норме");
      } else {
        tgSend(CONFIG_WATERING_NOTIFY_KIND, CONFIG_WATERING_NOTIFY_PRIORITY, waterlevelNotify == NOTIFY_SOUND, CONFIG_TELEGRAM_DEVICE, 
          "⚠ <b>Внимание!</b> Низкий уровень воды в ёмкости!");
      };
    };
  #endif // CONFIG_TELEGRAM_ENABLE
}

void sensorsWaterLevelMqttPublish() 
{
  mqttPublish(mqttGetTopicDevice1(mqttIsPrimary(), false, CONFIG_WATER_LEVEL_TOPIC), 
    malloc_stringf("{\"status\":%d}", 
      !(xEventGroupGetBits(_wateringFlags) & WATER_LEVEL_LOW)), 
    CONFIG_MQTT_LOAD_QOS, CONFIG_MQTT_LOAD_RETAINED, true, true);
}

static bool sensorsCheckWaterLevel()
{
  time_t now = time(nullptr);
  bool level = true;
  if (waterlevelSensorEnabled) {
    level = (xEventGroupGetBits(_wateringFlags) & WATER_LEVEL_LOW) == 0;
  };
  if (xEventGroupGetBits(_wateringFlags) & WATER_LEVEL_CHANGED) {
    xEventGroupClearBits(_wateringFlags, WATER_LEVEL_CHANGED);
    lastLowLevelNotify = now;
    ledMode();
    sensorsWaterLevelMqttPublish();
    waterLowLevelNotify(level);
  } else {
    if (!level && ((now - lastLowLevelNotify) >= CONFIG_WATERING_NOTIFY_PERIOD)) {
      lastLowLevelNotify = now;
      waterLowLevelNotify(level);
      ledMode();
    };
  };
  return level;
}

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------------- Входы --------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static void gpioInit()
{
  waterleakInit();
  gpioWaterLevel.initGPIO();
  ledWatering = ledTaskCreate(CONFIG_GPIO_WATERING_LED, true, true, "led_wtr", CONFIG_LED_TASK_STACK_SIZE, nullptr);
  ledTaskSend(ledWatering, lmFlash, 5, 500, 500);
}

static void sensorsGpioEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_id == RE_GPIO_CHANGE) {
    if (event_data) {
      gpio_data_t* data = (gpio_data_t*)event_data;
      if (data->pin == CONFIG_GPIO_WATER_LEVEL) {
        xEventGroupSetBits(_wateringFlags, WATER_LEVEL_CHANGED);
        if (data->value == 1) {
          xEventGroupSetBits(_wateringFlags, WATER_LEVEL_LOW);
        } else {
          xEventGroupClearBits(_wateringFlags, WATER_LEVEL_LOW);
        };
      };
    };
  };
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------- Управление нагрузкой ------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static bool relaysPublish(rLoadController *ctrl, char* topic, char* payload, bool free_topic, bool free_payload)
{
  return mqttPublish(topic, payload, CONFIG_MQTT_LOAD_QOS, CONFIG_MQTT_LOAD_RETAINED, free_topic, free_payload);
}

char* sensorsWateringNotifyData()
{
  return malloc_stringf("Влажность:   %.1f %%\nТемпература: %.1f°C", sensorsGetSoilMoisture(), sensorsGetSoilTemp());
}

void wateringPumpStateChange(rLoadController *ctrl, bool state, time_t duration)
{
  ledMode();
  #if CONFIG_TELEGRAM_ENABLE
    if (wateringNotify != NOTIFY_OFF) {
      char* data = sensorsWateringNotifyData();
      if (data) {
        if (state) {
          tgSend(CONFIG_WATERING_NOTIFY_KIND, CONFIG_WATERING_NOTIFY_PRIORITY, wateringNotify == NOTIFY_SOUND, CONFIG_TELEGRAM_DEVICE, 
          "💦 Полив <b>запущен</b>\n\n<code>%s</code>", data);
        } else {
          uint16_t last_h = duration / 3600;
          uint16_t last_m = duration % 3600 / 60;
          uint16_t last_s = duration % 3600 % 60;
          tgSend(CONFIG_WATERING_NOTIFY_KIND, CONFIG_WATERING_NOTIFY_PRIORITY, wateringNotify == NOTIFY_SOUND, CONFIG_TELEGRAM_DEVICE, 
          "✅ Полив <b>завершен</b>\n\n<code>%s\nВремя работы: %.2dч %.2dм %.2dc</code>", data, last_h, last_m, last_s);
        };
        free(data);
      };
    };
  #endif // CONFIG_TELEGRAM_ENABLE
}

void wateringPumpBefore(rLoadController *ctrl, bool state, time_t duration)
{
  gpioWaterLevel.deactivate(false);
}

void wateringPumpAfter(rLoadController *ctrl, bool state, time_t duration)
{
  gpioWaterLevel.activate(false);
}

static rLoadGpioController lcPump(CONFIG_GPIO_PUMP, 0x01, false, CONFIG_WATERING_KEY, 
      &wateringMoistureCycleTime, &wateringMoistureCycleInterval, TI_SECONDS,
      wateringPumpBefore, wateringPumpAfter, wateringPumpStateChange, relaysPublish);

static void ledMode()
{
  if (sensorsGetWaterLeaks()) {
    ledTaskSend(ledWatering, lmBlinkOn, CONFIG_LED_WATER_LEAK);
  } else if (xEventGroupGetBits(_wateringFlags) & WATER_LEVEL_LOW) {
    ledTaskSend(ledWatering, lmBlinkOn, CONFIG_LED_WATER_LEVEL);
  } else if (lcPump.getState()) {
    ledTaskSend(ledWatering, lmBlinkOn, CONFIG_LED_WATER_ON);
  } else {
    ledTaskSend(ledWatering, lmBlinkOff, CONFIG_LED_WATER_OFF);
  };
}

static void relaysInit()
{
  #if defined(CONFIG_ELTARIFFS_ENABLED) && CONFIG_ELTARIFFS_ENABLED
  lcPump.setPeriodStartDay(elTariffsGetReportDayAddress());
  #endif // CONFIG_ELTARIFFS_ENABLED
  lcPump.loadInit(false);
  lcPump.countersNvsRestore();
}

static void relaysMqttTopicsCreate(bool primary)
{
  lcPump.mqttTopicCreate(primary, false, CONFIG_WATERING_TOPIC, nullptr, nullptr);
}

static void relaysMqttTopicsFree()
{
  lcPump.mqttTopicFree();
}

static void relaysMqttPublishState()
{
  lcPump.mqttPublish();
}

static void relaysStoreData()
{
  rlog_i(logTAG, "Store relays data");

  lcPump.countersNvsStore();
}

static void relaysTimeEventHandler(int32_t event_id, void* event_data)
{
  lcPump.countersTimeEventHandler(event_id, event_data);

  /**************************************************************************
  // 2023-07-19: Отладка счетчиков
  if (event_id == RE_TIME_START_OF_DAY) {
    tgSend(MK_MAIN, MP_ORDINARY, true, "DEBUG", "⏰ Получено событие: RE_TIME_START_OF_DAY");

    if ((event_data)) {
      int* mday = (int*)event_data;
      if (*mday == 24) {
        tgSend(MK_MAIN, MP_ORDINARY, true, "DEBUG", "⏰ Получено событие: RE_TIME_START_OF_PERIOD");
      };
    };
  }
  else if (event_id == RE_TIME_START_OF_WEEK) {
    tgSend(MK_MAIN, MP_ORDINARY, true, "DEBUG", "⏰ Получено событие: RE_TIME_START_OF_WEEK");
  }
  else if (event_id == RE_TIME_START_OF_MONTH) {
    tgSend(MK_MAIN, MP_ORDINARY, true, "DEBUG", "⏰ Получено событие: RE_TIME_START_OF_MONTH");
  }
  else if (event_id == RE_TIME_START_OF_YEAR) {
    tgSend(MK_MAIN, MP_ORDINARY, true, "DEBUG", "⏰ Получено событие: RE_TIME_START_OF_YEAR");
  };
  **************************************************************************/
}

static void relaysOtaHandler()
{
  lcPump.loadSetState(false, true, true);  
}


void wateringControl() 
{
  bool oldPump = lcPump.getState();
  
  // Пролучаем данные с датчиков
  bool waterLeak = sensorsCheckWaterLeaks();
  bool waterLevel = sensorsCheckWaterLevel();
  float soilMoisture = sensorsGetSoilMoisture();

  // Проверяем перелив, уровень воды и расписание
  bool newPump = !waterLeak && waterLevel
    && (wateringMode != WATERING_OFF) 
    && checkTimespanNowEx(wateringTimespan, true);

  // Проверяем уровень влажности почвы
  if (newPump && (wateringMode == WATERING_SENSORS)) {
    if (!isnan(soilMoisture)) {
      if (oldPump) {
        newPump = soilMoisture < wateringMoistureMax;
      } else {
        newPump = soilMoisture <= wateringMoistureMin;
      };
    } else {
      newPump = false;
    };

    // Если полив еще не начат, дополнительно учитываем температуру почвы
    float soilTemp = sensorsGetSoilTemp();
    if (newPump && !oldPump && !isnan(soilTemp)) {
      newPump = (soilTemp >= wateringSoilTempMin) && (soilTemp <= wateringSoilTempMax);
    };
  };

  // Контроль общего времени и интервалов полива
  if (newPump && (wateringMoistureMaxDuration > 0)) {
    if (oldPump) {
      if (checkTimeInterval(lcPump.getLastOn(), wateringMoistureMaxDuration, TI_MINUTES, true)) {
        newPump = false;
      };
    } else {
      if (checkTimeInterval(lcPump.getLastOff(), wateringMoistureMaxDuration, TI_MINUTES, false)) {
        newPump = false;
      };
    };
  };

  // Управление насосом
  rlog_i(logTAG, "Watering state: %d", newPump);
  lcPump.loadSetState(newPump, false, true);
}

// -----------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------- Event  handlers ---------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static void sensorsMqttEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_id == RE_MQTT_CONNECTED) {
    re_mqtt_event_data_t* data = (re_mqtt_event_data_t*)event_data;
    sensorsMqttTopicsCreate(data->primary);
    relaysMqttTopicsCreate(data->primary);
  } 
  else if ((event_id == RE_MQTT_CONN_LOST) || (event_id == RE_MQTT_CONN_FAILED)) {
    sensorsMqttTopicsFree();
    relaysMqttTopicsFree();
  }
}

static void sensorsTimeEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_id == RE_TIME_EVERY_MINUTE) {
    xEventGroupSetBits(_wateringFlags, TIME_MINUTE_EVENT);
  } else if (event_id == RE_TIME_START_OF_DAY) {
    _sensorsNeedStore = true;
  } else if (event_id == RE_TIME_SILENT_MODE_ON) {
    ledTaskSend(ledWatering, lmEnable, 0, 0, 0);
  } else if (event_id == RE_TIME_SILENT_MODE_OFF) {
    ledTaskSend(ledWatering, lmEnable, 1, 0, 0);
  };
  relaysTimeEventHandler(event_id, event_data);
}

static void sensorsResetExtremumsSensor(rSensor* sensor, const char* sensor_name, uint8_t mode) 
{ 
  if (mode == 0) {
    sensor->resetExtremumsTotal();
    #if CONFIG_TELEGRAM_ENABLE
      tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
        CONFIG_MESSAGE_TG_SENSOR_CLREXTR_TOTAL_DEV, sensor_name);
    #endif // CONFIG_TELEGRAM_ENABLE
  } else if (mode == 1) {
    sensor->resetExtremumsDaily();
    #if CONFIG_TELEGRAM_ENABLE
      tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
        CONFIG_MESSAGE_TG_SENSOR_CLREXTR_DAILY_DEV, sensor_name);
    #endif // CONFIG_TELEGRAM_ENABLE
  } else if (mode == 2) {
    sensor->resetExtremumsWeekly();
    #if CONFIG_TELEGRAM_ENABLE
      tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
        CONFIG_MESSAGE_TG_SENSOR_CLREXTR_WEEKLY_DEV, sensor_name);
    #endif // CONFIG_TELEGRAM_ENABLE
  } else if (mode == 3) {
    sensor->resetExtremumsEntirely();
    #if CONFIG_TELEGRAM_ENABLE
      tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
        CONFIG_MESSAGE_TG_SENSOR_CLREXTR_ENTIRELY_DEV, sensor_name);
    #endif // CONFIG_TELEGRAM_ENABLE
  };
}

static void sensorsResetExtremumsSensors(uint8_t mode)
{
  if (mode == 0) {
    sensorSoil.resetExtremumsTotal();
    sensorIndoor.resetExtremumsTotal();
    sensorHeating.resetExtremumsTotal();
    #if CONFIG_TELEGRAM_ENABLE
      tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
        CONFIG_MESSAGE_TG_SENSOR_CLREXTR_TOTAL_ALL);
    #endif // CONFIG_TELEGRAM_ENABLE
  } else if (mode == 1) {
    sensorSoil.resetExtremumsDaily();
    sensorIndoor.resetExtremumsDaily();
    sensorHeating.resetExtremumsDaily();
    #if CONFIG_TELEGRAM_ENABLE
      tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
        CONFIG_MESSAGE_TG_SENSOR_CLREXTR_DAILY_ALL);
    #endif // CONFIG_TELEGRAM_ENABLE
  } else if (mode == 2) {
    sensorSoil.resetExtremumsWeekly();
    sensorIndoor.resetExtremumsWeekly();
    sensorHeating.resetExtremumsWeekly();
    #if CONFIG_TELEGRAM_ENABLE
      tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
        CONFIG_MESSAGE_TG_SENSOR_CLREXTR_WEEKLY_ALL);
    #endif // CONFIG_TELEGRAM_ENABLE
  } else if (mode == 3) {
    sensorSoil.resetExtremumsEntirely();
    sensorIndoor.resetExtremumsEntirely();
    sensorHeating.resetExtremumsEntirely();
    #if CONFIG_TELEGRAM_ENABLE
      tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
        CONFIG_MESSAGE_TG_SENSOR_CLREXTR_ENTIRELY_ALL);
    #endif // CONFIG_TELEGRAM_ENABLE
  };
};

static void sensorsCommandsEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if ((event_id == RE_SYS_COMMAND) && (event_data)) {
    char* buf = malloc_string((char*)event_data);
    if (buf != nullptr) {
      const char* seps = " ";
      char* cmd = nullptr;
      char* mode = nullptr;
      char* sensor = nullptr;
      uint8_t imode = 0;
      cmd = strtok(buf, seps);
      
      // Команды сброса датчиков
      if ((cmd != nullptr) && (strcasecmp(cmd, CONFIG_SENSOR_COMMAND_EXTR_RESET) == 0)) {
        rlog_i(logTAG, "Reset extremums: %s", buf);
        sensor = strtok(nullptr, seps);
        if (sensor != nullptr) {
          mode = strtok(nullptr, seps);
        };
      
        // Опрделение режима сброса
        if (mode == nullptr) {
          // Возможно, вторым токеном идет режим, в этом случае сбрасываем для всех сенсоров
          if (sensor) {
            if (strcasecmp(sensor, CONFIG_SENSOR_EXTREMUMS_DAILY) == 0) {
              sensor = nullptr;
              imode = 1;
            } else if (strcasecmp(sensor, CONFIG_SENSOR_EXTREMUMS_WEEKLY) == 0) {
              sensor = nullptr;
              imode = 2;
            } else if (strcasecmp(sensor, CONFIG_SENSOR_EXTREMUMS_ENTIRELY) == 0) {
              sensor = nullptr;
              imode = 3;
            };
          };
        } else if (strcasecmp(mode, CONFIG_SENSOR_EXTREMUMS_DAILY) == 0) {
          imode = 1;
        } else if (strcasecmp(mode, CONFIG_SENSOR_EXTREMUMS_WEEKLY) == 0) {
          imode = 2;
        } else if (strcasecmp(mode, CONFIG_SENSOR_EXTREMUMS_ENTIRELY) == 0) {
          imode = 3;
        };

        // Определение сенсора
        if ((sensor == nullptr) || (strcasecmp(sensor, CONFIG_SENSOR_COMMAND_SENSORS_PREFIX) == 0)) {
          sensorsResetExtremumsSensors(imode);
        } else {
          if (strcasecmp(sensor, SENSOR_SOIL_TOPIC) == 0) {
            sensorsResetExtremumsSensor(&sensorSoil, SENSOR_SOIL_TOPIC, imode);
          } else if (strcasecmp(sensor, SENSOR_INDOOR_TOPIC) == 0) {
            sensorsResetExtremumsSensor(&sensorIndoor, SENSOR_INDOOR_TOPIC, imode);
          } else if (strcasecmp(sensor, SENSOR_HEATING_TOPIC) == 0) {
            sensorsResetExtremumsSensor(&sensorHeating, SENSOR_HEATING_TOPIC, imode);
          } else {
            rlog_w(logTAG, "Sensor [ %s ] not found", sensor);
            #if CONFIG_TELEGRAM_ENABLE
              tgSend(CONFIG_SENSOR_COMMAND_KIND, CONFIG_SENSOR_COMMAND_PRIORITY, CONFIG_SENSOR_COMMAND_NOTIFY, CONFIG_TELEGRAM_DEVICE,
                CONFIG_MESSAGE_TG_SENSOR_CLREXTR_UNKNOWN, sensor);
            #endif // CONFIG_TELEGRAM_ENABLE
          };
        };
      };
    };
    if (buf != nullptr) free(buf);
  };
}

static void sensorsOtaEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if ((event_id == RE_SYS_OTA) && (event_data)) {
    re_system_event_data_t* data = (re_system_event_data_t*)event_data;
    if (data->type == RE_SYS_SET) {
      relaysOtaHandler();
      sensorsStoreData();
      wateringTaskSuspend();
    } else {
      wateringTaskResume();
    };
  };
}

bool sensorsEventHandlersRegister()
{
  return eventHandlerRegister(RE_MQTT_EVENTS, ESP_EVENT_ANY_ID, &sensorsMqttEventHandler, nullptr) 
      && eventHandlerRegister(RE_TIME_EVENTS, ESP_EVENT_ANY_ID, &sensorsTimeEventHandler, nullptr)
      && eventHandlerRegister(RE_GPIO_EVENTS, ESP_EVENT_ANY_ID, &sensorsGpioEventHandler, nullptr)
      && eventHandlerRegister(RE_SYSTEM_EVENTS, RE_SYS_COMMAND, &sensorsCommandsEventHandler, nullptr)
      && eventHandlerRegister(RE_SYSTEM_EVENTS, RE_SYS_OTA, &sensorsOtaEventHandler, nullptr);
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------- Задача --------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

void wateringTaskExec(void *pvParameters)
{
  // -------------------------------------------------------------------------------------------------------
  // Инициализация устройств и сенсоров
  // -------------------------------------------------------------------------------------------------------
  gpioInit();
  relaysInit();
  sensorsInitModbus();
  sensorsInitParameters();
  sensorsInitSensors();

  // -------------------------------------------------------------------------------------------------------
  // Инициализация контроллеров
  // -------------------------------------------------------------------------------------------------------
  // Инициализация контроллеров OpenMon
  #if CONFIG_OPENMON_ENABLE
    dsChannelInit(EDS_OPENMON, 
      CONFIG_OPENMON_CTR01_ID, CONFIG_OPENMON_CTR01_TOKEN, 
      CONFIG_OPENMON_MIN_INTERVAL, CONFIG_OPENMON_ERROR_INTERVAL);
  #endif // CONFIG_OPENMON_ENABLE
  
  // Инициализация контроллеров NarodMon
  #if CONFIG_NARODMON_ENABLE
    dsChannelInit(EDS_NARODMON, 
      CONFIG_NARODMON_DEVICE01_ID, CONFIG_NARODMON_DEVICE01_KEY, 
      CONFIG_NARODMON_MIN_INTERVAL, CONFIG_NARODMON_ERROR_INTERVAL);
  #endif // CONFIG_NARODMON_ENABLE

  // Инициализация каналов ThingSpeak
  #if CONFIG_THINGSPEAK_ENABLE
    dsChannelInit(EDS_THINGSPEAK, 
      CONFIG_THINGSPEAK_CHANNEL01_ID, CONFIG_THINGSPEAK_CHANNEL01_KEY, 
      CONFIG_THINGSPEAK_MIN_INTERVAL, CONFIG_THINGSPEAK_ERROR_INTERVAL);
  #endif // CONFIG_THINGSPEAK_ENABLE

  // -------------------------------------------------------------------------------------------------------
  // Таймеры публикции данных с сенсоров
  // -------------------------------------------------------------------------------------------------------
  int8_t mqttIndex = 0;
  esp_timer_t mqttPubTimer;
  timerSet(&mqttPubTimer, iMqttPubInterval*1000);
  #if CONFIG_OPENMON_ENABLE
    esp_timer_t omSendTimer;
    timerSet(&omSendTimer, iOpenMonInterval*1000);
  #endif // CONFIG_OPENMON_ENABLE
  #if CONFIG_NARODMON_ENABLE
    esp_timer_t nmSendTimer;
    timerSet(&nmSendTimer, iNarodMonInterval*1000);
  #endif // CONFIG_NARODMON_ENABLE
  #if CONFIG_THINGSPEAK_ENABLE
    esp_timer_t tsSendTimer;
    timerSet(&tsSendTimer, iThingSpeakInterval*1000);
  #endif // CONFIG_THINGSPEAK_ENABLE

  TickType_t startTicks = 0;
  TickType_t currTicks = 0;
  TickType_t waitTicks = 0;
  while (1) {
    // Ждем тайамута или любого события
    xEventGroupWaitBits(_wateringFlags, FORCED_CONTROL, pdFALSE, pdFALSE, waitTicks);
    xEventGroupClearBits(_wateringFlags, TIME_MINUTE_EVENT);
    // Фиксируем время начала данного рабочего цикла
    startTicks = xTaskGetTickCount(); 

    // -----------------------------------------------------------------------------------------------------
    // Чтение данных с сенсоров
    // -----------------------------------------------------------------------------------------------------
    sensorSoil.readData();
    if (sensorSoil.getStatus() == SENSOR_STATUS_OK) {
      rlog_i("SOIL", "Values raw: %.1f %% / %.1f °С | out: %.1f %% / %.1f °С | min: %.1f %% / %.1f °С | max: %.1f %% / %.1f °С", 
        sensorSoil.getValue2(false).rawValue, sensorSoil.getValue1(false).rawValue, 
        sensorSoil.getValue2(false).filteredValue, sensorSoil.getValue1(false).filteredValue,
        sensorSoil.getExtremumsDaily2(false).minValue.filteredValue, sensorSoil.getExtremumsDaily1(false).minValue.filteredValue,
        sensorSoil.getExtremumsDaily2(false).maxValue.filteredValue, sensorSoil.getExtremumsDaily1(false).maxValue.filteredValue);
    };

    sensorIndoor.readData();
    if (sensorIndoor.getStatus() == SENSOR_STATUS_OK) {
      rlog_i("INDOOR", "Values raw: %.2f °С / %.2f %% | out: %.2f °С / %.2f %% | min: %.2f °С / %.2f %% | max: %.2f °С / %.2f %%", 
        sensorIndoor.getValue2(false).rawValue, sensorIndoor.getValue1(false).rawValue, 
        sensorIndoor.getValue2(false).filteredValue, sensorIndoor.getValue1(false).filteredValue, 
        sensorIndoor.getExtremumsDaily2(false).minValue.filteredValue, sensorIndoor.getExtremumsDaily1(false).minValue.filteredValue, 
        sensorIndoor.getExtremumsDaily2(false).maxValue.filteredValue, sensorIndoor.getExtremumsDaily1(false).maxValue.filteredValue);
    };

    sensorHeating.readData();
    if (sensorHeating.getStatus() == SENSOR_STATUS_OK) {
      rlog_i("HEATING", "Values raw: %.1f °С | out: %.1f °С | min: %.1f °С | max: %.1f °С", 
        sensorHeating.getValue(false).rawValue,
        sensorHeating.getValue(false).filteredValue,
        sensorHeating.getExtremumsDaily(false).minValue.filteredValue,
        sensorHeating.getExtremumsDaily(false).maxValue.filteredValue);
    };

    // -----------------------------------------------------------------------------------------------------
    // Управление нагрузкой
    // -----------------------------------------------------------------------------------------------------
    
    wateringControl();

    // -----------------------------------------------------------------------------------------------------
    // Сохранение экстремумов с сенсоров
    // -----------------------------------------------------------------------------------------------------

    if (_sensorsNeedStore) {
      _sensorsNeedStore = false;
      sensorsStoreData();
    };

    // -----------------------------------------------------------------------------------------------------
    // Публикация данных с сенсоров
    // -----------------------------------------------------------------------------------------------------

    // MQTT брокер
    if (mqttIsConnected()) {
      // Если таймер вышел, сбрасываем индекс и публикуем локальные данные
      if (timerTimeout(&mqttPubTimer)) {
        timerSet(&mqttPubTimer, iMqttPubInterval*1000);
        sensorSoil.publishData(false);
        sensorIndoor.publishData(false);
        sensorHeating.publishData(false);
        sensorsWaterLeakMqttPublish();
        sensorsWaterLevelMqttPublish();
        relaysMqttPublishState();
      };
    };

    // open-monitoring.online
    #if CONFIG_OPENMON_ENABLE
      if (timerTimeout(&omSendTimer)) {
        char * omValues = nullptr;
        // 01. Почва влажность:FLOAT:~:ON:OFF
        // 02. Почва температура:FLOAT:~:OFF:OFF
        if (sensorSoil.getStatus() == SENSOR_STATUS_OK) {
          omValues = concat_strings_div(omValues, 
            malloc_stringf("p1=%.2f&p2=%.2f", 
              sensorSoil.getValue2(false).filteredValue,
              sensorSoil.getValue1(false).filteredValue),
            "&");
        };
        // 03. Полив:INT:~:ON:OFF
        omValues = concat_strings_div(omValues, 
          malloc_stringf("p3=%d", 
            lcPump.getState()),
          "&");
        // 04. Перелив 1:INT:~:ON:OFF
        // 05. Перелив 2:INT:~:ON:OFF
        // 06. Перелив 3:INT:~:ON:OFF
        // 07. Вода:INT:~:ON:OFF
        omValues = concat_strings_div(omValues, 
          malloc_stringf("p4=%d&p5=%d&p6=%d&p7=%d", 
            ((xEventGroupGetBits(_wateringFlags) & WATER_LEAK_IN1) > 0),
            ((xEventGroupGetBits(_wateringFlags) & WATER_LEAK_IN2) > 0),
            ((xEventGroupGetBits(_wateringFlags) & WATER_LEAK_IN3) > 0),
            ((xEventGroupGetBits(_wateringFlags) & WATER_LEVEL_LOW) == 0)),
          "&");
        // 08. Комната температура:FLOAT:~:OFF:OFF
        // 09. Комната влажность:FLOAT:~:OFF:OFF
        if (sensorIndoor.getStatus() == SENSOR_STATUS_OK) {
          omValues = concat_strings_div(omValues, 
            malloc_stringf("p8=%.2f&p9=%.2f", 
              sensorIndoor.getValue2(false).filteredValue,
              sensorIndoor.getValue1(false).filteredValue),
            "&");
        };
        // 10. Батареи отопления:FLOAT:~:OFF:OFF
        if (sensorHeating.getStatus() == SENSOR_STATUS_OK) {
          omValues = concat_strings_div(omValues, 
            malloc_stringf("p10=%.3f", 
              sensorHeating.getValue(false).filteredValue),
            "&");
        };
        // 11. Резерв:FLOAT:~:OFF:OFF
        // 12. Резерв:FLOAT:~:OFF:OFF
        // 13. Резерв:FLOAT:~:OFF:OFF
        // 14. Резерв:INT:~:OFF:OFF
        // 15. Резерв:INT:~:OFF:OFF
        // 16. Резерв:INT:~:OFF:OFF

        // Отправляем сформированный пакет на сервер
        if (omValues) {
          timerSet(&omSendTimer, iOpenMonInterval*1000);
          dsSend(EDS_OPENMON, CONFIG_OPENMON_CTR01_ID, omValues, false);
          free(omValues);
        };
      };
    #endif // CONFIG_OPENMON_ENABLE

    // narodmon.ru
    #if CONFIG_NARODMON_ENABLE
      if (statesInetIsAvailabled() && timerTimeout(&nmSendTimer)) {
        char * nmValues = nullptr;
        // Отправляем сформированный пакет на сервер
        if (nmValues) {
          timerSet(&nmSendTimer, iNarodMonInterval*1000);
          dsSend(EDS_NARODMON, CONFIG_NARODMON_DEVICE01_ID, nmValues, false);
          free(nmValues);
        };
      };
    #endif // CONFIG_NARODMON_ENABLE

    // thingspeak.com
    #if CONFIG_THINGSPEAK_ENABLE
      if (timerTimeout(&tsSendTimer)) {
        char * tsValues = nullptr;
        // Field 1 Почва температура
        // Field 2 Почва влажность
        if (sensorSoil.getStatus() == SENSOR_STATUS_OK) {
          tsValues = concat_strings_div(tsValues, 
            malloc_stringf("field1=%.1f&field2=%.1f", 
              sensorSoil.getValue1(false).filteredValue,
              sensorSoil.getValue2(false).filteredValue),
            "&");
        };
        // Field 3 Полив
        tsValues = concat_strings_div(tsValues, 
          malloc_stringf("field3=%d", 
            lcPump.getState()),
          "&");
        // Field 4 Перелив
        tsValues = concat_strings_div(tsValues, 
          malloc_stringf("field4=%d", 
            sensorsGetWaterLeaks()),
          "&");
        // Field 5 Уровень воды
        tsValues = concat_strings_div(tsValues, 
          malloc_stringf("field5=%d", 
            ((xEventGroupGetBits(_wateringFlags) & WATER_LEVEL_LOW) == 0)),
          "&");
        // Field 6 Комната температура
        // Field 7 Комната влажность
        if (sensorIndoor.getStatus() == SENSOR_STATUS_OK) {
          tsValues = concat_strings_div(tsValues, 
            malloc_stringf("field6=%.1f&field7=%.1f", 
              sensorIndoor.getValue2(false).filteredValue,
              sensorIndoor.getValue1(false).filteredValue),
            "&");
        };
        // Field 8 Батареи температура
        if (sensorHeating.getStatus() == SENSOR_STATUS_OK) {
          tsValues = concat_strings_div(tsValues, 
            malloc_stringf("field8=%.1f", 
              sensorHeating.getValue(false).filteredValue),
            "&");
        };

        // Отправляем сформированный пакет на сервер
        if (tsValues) {
          timerSet(&tsSendTimer, iThingSpeakInterval*1000);
          dsSend(EDS_THINGSPEAK, CONFIG_THINGSPEAK_CHANNEL01_ID, tsValues, false);
          free(tsValues);
        };
      };
    #endif // CONFIG_THINGSPEAK_ENABLE

    // -----------------------------------------------------------------------------------------------------
    // Вычисление времени ожидания
    // -----------------------------------------------------------------------------------------------------
    currTicks = xTaskGetTickCount();
    if ((currTicks - startTicks) >= pdMS_TO_TICKS(_sensorsReadInterval*1000)) {
      waitTicks = 0;
    } else {
      waitTicks = pdMS_TO_TICKS(_sensorsReadInterval*1000) - (currTicks - startTicks);
    };
  };

  vTaskDelete(nullptr);
  espRestart(RR_UNKNOWN);
}

bool wateringTaskStart()
{
  #if CONFIG_WATERING_STATIC_ALLOCATION
    static StaticEventGroup_t wateringFlagsBuffer;
    static StaticTask_t wateringTaskBuffer;
    static StackType_t wateringTaskStack[CONFIG_WATERING_TASK_STACK_SIZE];
    _wateringFlags = xEventGroupCreateStatic(&wateringFlagsBuffer);
    _wateringTask = xTaskCreateStaticPinnedToCore(wateringTaskExec, wateringTaskName, 
      CONFIG_WATERING_TASK_STACK_SIZE, NULL, CONFIG_TASK_PRIORITY_SENSORS, wateringTaskStack, &wateringTaskBuffer, CONFIG_TASK_CORE_SENSORS);
  #else
    __wateringFlags = xEventGroupCreate();
    xTaskCreatePinnedToCore(wateringTaskExec, wateringTaskName, 
      CONFIG_WATERING_TASK_STACK_SIZE, NULL, CONFIG_TASK_PRIORITY_SENSORS, &_wateringTask, CONFIG_TASK_CORE_SENSORS);
  #endif // CONFIG_WATERING_STATIC_ALLOCATION
  if (_wateringFlags && _wateringTask) {
    xEventGroupClearBits(_wateringFlags, 0x00FFFFFF);
    rloga_i("Task [ %s ] has been successfully created and started", wateringTaskName);
    return sensorsEventHandlersRegister();
  }
  else {
    rloga_e("Failed to create a task for processing sensor readings!");
    return false;
  };
}

bool wateringTaskSuspend()
{
  if ((_wateringTask) && (eTaskGetState(_wateringTask) != eSuspended)) {
    vTaskSuspend(_wateringTask);
    if (eTaskGetState(_wateringTask) == eSuspended) {
      rloga_d("Task [ %s ] has been suspended", wateringTaskName);
      return true;
    } else {
      rloga_e("Failed to suspend task [ %s ]!", wateringTaskName);
    };
  };
  return false;
}

bool wateringTaskResume()
{
  if ((_wateringTask) && (eTaskGetState(_wateringTask) == eSuspended)) {
    vTaskResume(_wateringTask);
    if (eTaskGetState(_wateringTask) != eSuspended) {
      rloga_i("Task [ %s ] has been successfully resumed", wateringTaskName);
      return true;
    } else {
      rloga_e("Failed to resume task [ %s ]!", wateringTaskName);
    };
  };
  return false;
}


