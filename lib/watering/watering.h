#ifndef __WATERING_H__
#define __WATERING_H__

#include "project_config.h"
#include "def_consts.h"
#include "esp_timer.h"
#include "rTypes.h" 
#include "reSensor.h" 
#include "reCWTSoilS.h"
#include "reDS18x20.h"
#include "reHTU2x.h"
#include "reLed.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------- Сенсоры -------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

// RS485 Modbus RTU
#define SENSOR_MODBUS_PORT              UART_NUM_1
#define SENSOR_MODBUS_SPEED             9600
#define SENSOR_MODBUS_PIN_RXD           CONFIG_GPIO_RS485_RX
#define SENSOR_MODBUS_PIN_TXD           CONFIG_GPIO_RS485_TX
#define SENSOR_MODBUS_PIN_RTS           -1
#define SENSOR_MODBUS_PIN_CTS           -1

static void* _modbus = nullptr;

// Температура почвы + влажность
#define SENSOR_SOIL_NAME                "Почва (CWT-TH)"
#define SENSOR_SOIL_KEY                 "soil"
#define SENSOR_SOIL_ADDRESS             0x01
#define SENSOR_SOIL_TYPE                CWTS_TH
#define SENSOR_SOIL_TOPIC               "soil"
#define SENSOR_SOIL_FILTER_MODE         SENSOR_FILTER_RAW
#define SENSOR_SOIL_FILTER_SIZE         0
#define SENSOR_SOIL_ERRORS_LIMIT        16

static reCWTSoilS sensorSoil(1);

// Комната
#define SENSOR_INDOOR_NAME              "Комната (SHT20)"
#define SENSOR_INDOOR_KEY               "in"
#define SENSOR_INDOOR_BUS               I2C_NUM_0
#define SENSOR_INDOOR_ADDRESS           HTU2X_ADDRESS
#define SENSOR_INDOOR_TOPIC             "indoor"
#define SENSOR_INDOOR_FILTER_MODE       SENSOR_FILTER_RAW
#define SENSOR_INDOOR_FILTER_SIZE       0
#define SENSOR_INDOOR_ERRORS_LIMIT      16

static HTU2x sensorIndoor(2);

// Батареи отопления
#define SENSOR_HEATING_NAME             "Батареи отопления (DS18B20)"
#define SENSOR_HEATING_KEY              "heat"
#define SENSOR_HEATING_INDEX            1 
#define SENSOR_HEATING_PIN              CONFIG_GPIO_DS18B20
#define SENSOR_HEATING_TOPIC            "heating"
#define SENSOR_HEATING_FILTER_MODE      SENSOR_FILTER_RAW
#define SENSOR_HEATING_FILTER_SIZE      0
#define SENSOR_HEATING_ERRORS_LIMIT     16

static DS18x20 sensorHeating(3);

// Период публикации данных с сенсоров на MQTT
static uint32_t iMqttPubInterval = CONFIG_MQTT_SENSORS_SEND_INTERVAL;
// Период публикации данных с сенсоров на OpenMon
#if CONFIG_OPENMON_ENABLE
static uint32_t iOpenMonInterval = CONFIG_OPENMON_SEND_INTERVAL;
#endif // CONFIG_OPENMON_ENABLE
// Период публикации данных с сенсоров на NarodMon
#if CONFIG_NARODMON_ENABLE
static uint32_t iNarodMonInterval = CONFIG_NARODMON_SEND_INTERVAL;
#endif // CONFIG_NARODMON_ENABLE
// Период публикации данных с сенсоров на ThingSpeak
#if CONFIG_THINGSPEAK_ENABLE
static uint32_t iThingSpeakInterval = CONFIG_THINGSPEAK_SEND_INTERVAL;
#endif // CONFIG_THINGSPEAK_ENABLE

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------ Светодиод ------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static ledQueue_t ledWatering = nullptr;

#define CONFIG_LED_WATER_LEAK    1, 100, 100
#define CONFIG_LED_WATER_LEVEL   2, 100, 1000
#define CONFIG_LED_WATER_ON      1, 500, 500
#define CONFIG_LED_WATER_OFF     0, 0, 0

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------------- Общие --------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

// Уведомления в telegram
typedef enum {
  NOTIFY_OFF    = 0,
  NOTIFY_SILENT = 1,
  NOTIFY_SOUND  = 2
} notify_type_t;

#define CONFIG_MQTT_LOAD_QOS              0 
#define CONFIG_MQTT_LOAD_RETAINED         true

#define CONFIG_MODE_KEY                   "mode"
#define CONFIG_MODE_FRIENDLY              "Режим работы" 
#define CONFIG_TIMESPAN_KEY               "timespan"
#define CONFIG_TIMESPAN_FRIENDLY          "Расписание работы" 

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------------- Полив --------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

// Интервал суток полива
static uint32_t wateringTimespan = 18002100U;

// Режим работы
typedef enum {
  WATERING_OFF      = 0,     // Отключено
  WATERING_FORCED   = 1,     // Включено принудительно
  WATERING_SENSORS  = 2      // Управление по датчикам
} watering_mode_t;
static watering_mode_t wateringMode = WATERING_SENSORS;

// Уведомления в telegram
static notify_type_t wateringNotify = NOTIFY_OFF;
static notify_type_t waterleakNotify = NOTIFY_SILENT;
static notify_type_t waterlevelNotify = NOTIFY_SILENT;

// Температура почвы
static float wateringSoilTempMin = 10.0;
static float wateringSoilTempMax = 30.0;
// Влажность почвы 
static float wateringMoistureMin = 30.0;
static float wateringMoistureMax = 50.0;
// Общая максимальная длительность полива в минутах
static uint32_t wateringMoistureMaxDuration = 2*60;
// Длительность включения насоса в секундах в пределах одного цикла
static uint32_t wateringMoistureCycleTime = 15;
static uint32_t wateringMoistureCycleInterval = 5*60;
// Отключение сенсоров
static uint8_t waterlevelSensorEnabled = 1; 
static uint8_t waterleakSensorEnabled1 = 1; 
static uint8_t waterleakSensorEnabled2 = 1; 
static uint8_t waterleakSensorEnabled3 = 1; 
// Количество измерений, при котором устранение перелива не учитывается (очень медленный debounce)
static uint32_t waterleakDebounceCount = 100;

#define CONFIG_WATERING_NOTIFY_KIND       MK_MAIN
#define CONFIG_WATERING_NOTIFY_PRIORITY   MP_HIGH
#define CONFIG_WATERING_NOTIFY_PERIOD     12*60*60

#define CONFIG_WATERING_KEY               "wtr"
#define CONFIG_WATERING_TOPIC             "watering" 
#define CONFIG_WATERING_FRIENDLY          "Полив"

#define CONFIG_NOTIFY_WATERING_KEY        "notify/watering"
#define CONFIG_NOTIFY_WATERING_FRIENDLY   "Уведомления о поливе" 
#define CONFIG_NOTIFY_WATERLEAK_KEY       "notify/leaks"
#define CONFIG_NOTIFY_WATERLEAK_FRIENDLY  "Уведомления о переливе" 
#define CONFIG_NOTIFY_WATERLVL_KEY        "notify/level"
#define CONFIG_NOTIFY_WATERLVL_FRIENDLY   "Уведомления об уровне воды" 

#define CONFIG_WATERING_ST_MIN_KEY        "soil/temp_min"
#define CONFIG_WATERING_ST_MIN_FRIENDLY   "Минимальная температура почвы"
#define CONFIG_WATERING_ST_MAX_KEY        "soil/temp_max"
#define CONFIG_WATERING_ST_MAX_FRIENDLY   "Максимальная температура почвы"
#define CONFIG_WATERING_MST_MIN_KEY       "soil/moist_min"
#define CONFIG_WATERING_MST_MIN_FRIENDLY  "Минимальная влажность почвы"
#define CONFIG_WATERING_MST_MAX_KEY       "soil/moist_max"
#define CONFIG_WATERING_MST_MAX_FRIENDLY  "Максимальная влажность почвы"
#define CONFIG_WATERING_MAX_DUR_KEY       "total_duration"
#define CONFIG_WATERING_MAX_DUR_FRIENDLY  "Общая максимальная длительность полива"
#define CONFIG_WATERING_CYC_TIME_KEY      "cycle_duration"
#define CONFIG_WATERING_CYC_TIME_FRIENDLY "Длительность открытия клапана"
#define CONFIG_WATERING_CYC_INTV_KEY      "cycle_interval"
#define CONFIG_WATERING_CYC_INTV_FRIENDLY "Интервал открытия клапана"

#define CONFIG_WATERLVL_ENABLED_KEY       "wlevel_sensor"
#define CONFIG_WATERLVL_ENABLED_FRIENDLY  "Датчик уровня"
#define CONFIG_WATERLEAK_ENABLED1_KEY     "wleaks_sensor1"
#define CONFIG_WATERLEAK_ENABLED1_FRIENDLY "Датчик протечки #1"
#define CONFIG_WATERLEAK_ENABLED2_KEY     "wleaks_sensor2"
#define CONFIG_WATERLEAK_ENABLED2_FRIENDLY "Датчик протечки #2"
#define CONFIG_WATERLEAK_ENABLED3_KEY     "wleaks_sensor3"
#define CONFIG_WATERLEAK_ENABLED3_FRIENDLY "Датчик протечки #3"
#define CONFIG_WATERLEAK_DEBOUNCE_KEY     "wleaks_debounce"
#define CONFIG_WATERLEAK_DEBOUNCE_FRIENDLY "Количество циклов подтверждения устранения утечки"

#define CONFIG_WATER_LEVEL_GPIO           CONFIG_GPIO_WATER_LEVEL
#define CONFIG_WATER_LEVEL_LEVEL          0
#define CONFIG_WATER_LEVEL_PULL           true
#define CONFIG_WATER_LEVEL_INTR           true
#define CONFIG_WATER_LEVEL_DEBOUNCE       100000
#define CONFIG_WATER_LEVEL_TOPIC          "water_level"

#define CONFIG_WATER_LEAK_ACTVATOR        CONFIG_GPIO_WATER_LEAK_ACT
#define CONFIG_WATER_LEAK_GPIO1           CONFIG_GPIO_WATER_LEAK1
#define CONFIG_WATER_LEAK_GPIO2           CONFIG_GPIO_WATER_LEAK2
#define CONFIG_WATER_LEAK_GPIO3           CONFIG_GPIO_WATER_LEAK3
#define CONFIG_WATER_LEAK_ACT_ON          0
#define CONFIG_WATER_LEAK_ACT_OFF         1
#define CONFIG_WATER_LEAK_LEVEL           0
#define CONFIG_WATER_LEAK_PULL            true
#define CONFIG_WATER_LEAK_DELAY_ON        1
#define CONFIG_WATER_LEAK_DELAY_OFF       10
#define CONFIG_WATER_LEAK_TOPIC           "water_leak"

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------- Задача --------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

bool wateringTaskStart();
bool wateringTaskSuspend();
bool wateringTaskResume();

#endif // __WATERING_H__