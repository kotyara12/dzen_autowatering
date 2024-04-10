/*
   -----------------------------------------------------------------------------------------------------------------------
   EN: Project configuration file, accessible from all libraries connected to the project
   RU: Файл конфигурации проекта, он должен быть доступен из всех файлов проекта, в том числе и библиотек
   -----------------------------------------------------------------------------------------------------------------------
   (с) 2023 Разживин Александр | Razzhivin Alexander
   kotyara12@yandex.ru | https://kotyara12.ru | tg: @kotyara1971
*/

#pragma once

#include <stdint.h>
#include "esp_task.h"

// -----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------- EN - Version ----------------------------------------------------------
// ----------------------------------------------- RU - Версии -----------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
#define APP_VERSION "20231205.026"
// 20231205.026: Первая публичная версия

// -----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------- EN - GPIO -------------------------------------------------------------
// ----------------------------------------------- RU - GPIO -------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Pin number for system (status) LED
// RU: Номер вывода для системного (статусного) светодиода
#define CONFIG_GPIO_SYSTEM_LED      23
#define CONFIG_GPIO_WATERING_LED    4
#define CONFIG_GPIO_PUMP            13
#define CONFIG_GPIO_WATER_LEAK_ACT  19
#define CONFIG_GPIO_WATER_LEAK1     33
#define CONFIG_GPIO_WATER_LEAK2     25
#define CONFIG_GPIO_WATER_LEAK3     26
#define CONFIG_GPIO_WATER_LEVEL     27
#define CONFIG_GPIO_DS18B20         32
// EN: Analog inputs
// RU: Аналоговые входы
#define CONFIG_GPIO_COIL_MS1        36
#define CONFIG_GPIO_COIL_MS2        39
#define CONFIG_GPIO_COIL_MS3        34
#define CONFIG_GPIO_COIL_MS4        35
// EN: RS485 bus 
// RU: Шина RS485
#define CONFIG_GPIO_RS485_RX        17
#define CONFIG_GPIO_RS485_TX        16
// EN: I2C bus #0: pins, pullup, frequency
// RU: Шина I2C #0: выводы, подтяжка, частота
#define CONFIG_I2C_PORT0_SDA        21
#define CONFIG_I2C_PORT0_SCL        22
#define CONFIG_I2C_PORT0_PULLUP     false 
#define CONFIG_I2C_PORT0_FREQ_HZ    100000
#define CONFIG_I2C_PORT0_STATIC     3
// EN: I2C bus #1: pins, pullup, frequency
// RU: Шина I2C #1: выводы, подтяжка, частота
// #define CONFIG_I2C_PORT1_SDA     16
// #define CONFIG_I2C_PORT1_SCL     17
// #define CONFIG_I2C_PORT1_PULLUP  false
// #define CONFIG_I2C_PORT1_FREQ_HZ 100000
// #define CONFIG_I2C_PORT1_STATIC  2
// EN: Blocking access to I2C buses. Makes sense if I2C devices are accessed from different threads. 
// The I2C APIs are not thread-safe, if you want to use one I2C port in different tasks, you need to take care of the multi-thread issue.
// RU: Блокировка доступа к шинам I2C. Имеет смысл, если доступ к устройствам I2C осущствляется из разных потоков
// API-интерфейсы I2C не являются потокобезопасными, если вы хотите использовать один порт I2C в разных задачах, вам нужно позаботиться о проблеме многопоточности.
#define CONFIG_I2C_LOCK 1

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------- EN - Common parameters ----------------------------------------------------
// -------------------------------------------- RU - Общие параметры -----------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Restart device on memory allocation errors
// RU: Перезапустить устройство при ошибках выделения памяти
#define CONFIG_HEAP_ALLOC_FAILED_RESTART 1
#define CONFIG_HEAP_ALLOC_FAILED_RETRY 0
#define CONFIG_HEAP_ALLOC_FAILED_RESTART_DELAY 300000

// EN: The system has a real time clock DS1307 or DS3231
// RU: В системе установлены часы реального времени DS1307 или DS3231
#define CONFIG_RTC_INSTALLED 0
// #define CONFIG_RTC_TYPE      DS3231
// #define CONFIG_RTC_I2C_BUS   0

// EN: The system has an LCD display without russification
// RU: В системе установлен дисплей LCD без русификации
#define CONFIG_LCD_RUS_CODEPAGE 0

// EN: Default task priority
// RU: Приоритет задачи "по умолчанию"
#define CONFIG_DEFAULT_TASK_PRIORITY 5

// EN: Event loop
// RU: Цикл событий
#define CONFIG_EVENT_LOOP_QUEUE_SIZE 64
#define CONFIG_EVENT_LOOP_CORE 1

/* Silent mode (no sounds, no blinks) */
// EN: Allow "quiet" mode. Quiet mode is the period of time of day when LED flashes and sounds are blocked.
// RU: Разрешить "тихий" режим. Тихий режим - это период времени суток, когда блокируются вспышки светодиодов и звуки.
#define CONFIG_SILENT_MODE_ENABLE 1
// EN: Interval in H1M1H2M2 format. That is, the interval 21:00 -> 06:00 is 21000600
// RU: Интервал в формате H1M1H2M2. То есть интервал 21:00 -> 06:00 это 21000600
#define CONFIG_SILENT_MODE_INTERVAL 0 

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- EN - Watering ----------------------------------------------------
// ----------------------------------------------------- RU - Полив ------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Here you can specify any parameters related to the main task of the device
// RU: Здесь можно указать вообще любые параметры, связанные с прикладной задачей устройства

// EN: Interval of reading data from sensors in milliseconds
// RU: Интервал чтения данных с сенсоров в миллисекундах
#define CONFIG_WATERING_TASK_CYCLE 30000
// EN: Use static memory allocation for the task and queue. CONFIG_FREERTOS_SUPPORT_STATIC_ALLOCATION must be enabled!
// RU: Использовать статическое выделение памяти под задачу и очередь. Должен быть включен параметр CONFIG_FREERTOS_SUPPORT_STATIC_ALLOCATION!
#define CONFIG_WATERING_STATIC_ALLOCATION 1
// EN: Stack size for main task
// RU: Размер стека для главной задачи
#define CONFIG_WATERING_TASK_STACK_SIZE 5*1024
// EN: Priority of the main task
// RU: Приоритет главной задачи
#define CONFIG_WATERING_TASK_PRIORITY CONFIG_DEFAULT_TASK_PRIORITY+2
// EN: Processor core of the main task
// RU: Процессорное ядро главной задачи
#define CONFIG_WATERING_TASK_CORE 1

// EN: Allow publishing of raw RAW data (no correction or filtering): 0 - only processed value, 1 - always both values, 2 - only when there is processing
// RU: Разрешить публикацию необработанных RAW-данных (без коррекции и фильтрации): 0 - только обработанное значение, 1 - всегда оба значения, 2 - только когда есть обработка
#define CONFIG_SENSOR_RAW_ENABLE 1
// EN: Allow publication of sensor status
// RU: Разрешить публикацию форматированных данных в виде строки
#define CONFIG_SENSOR_STRING_ENABLE 0
// EN: Allow the publication of the time stamp of reading data from the sensor
// RU: Разрешить публикацию отметки времени чтения данных с сенсора
#define CONFIG_SENSOR_TIMESTAMP_ENABLE 1
// EN: Allow the publication of formatted data as "value + time"
// RU: Разрешить публикацию форматированных данных в виде "значение + время"
#define CONFIG_SENSOR_TIMESTRING_ENABLE 1
// EN: Allow dew point calculation and publication
// RU: Разрешить вычисление и публикацию точки росы
#define CONFIG_SENSOR_DEWPOINT_ENABLE 0
// EN: Allow publishing of mixed value, for example "temperature + humidity"
// RU: Разрешить публикацию смешанного значения, например "температура + влажность"
#define CONFIG_SENSOR_DISPLAY_ENABLED 1
// EN: Allow publication of absolute minimum and maximum
// RU: Разрешить публикацию абсолютного минимума и максимума
#define CONFIG_SENSOR_EXTREMUMS_ENTIRELY_ENABLE 1
// EN: Allow publication of daily minimum and maximum
// RU: Разрешить публикацию ежедневного минимума и максимума
#define CONFIG_SENSOR_EXTREMUMS_DAILY_ENABLE 1
// EN: Allow publication of weekly minimum and maximum
// RU: Разрешить публикацию еженедельного минимума и максимума
#define CONFIG_SENSOR_EXTREMUMS_WEEKLY_ENABLE 1
// EN: Publish extremums only when they are changed
// RU: Публиковать экстеремумы только при их изменении
#define CONFIG_SENSOR_EXTREMUMS_OPTIMIZED 1

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------- EN - Wifi networks -----------------------------------------------------
// ------------------------------------------------ RU - WiFi сети -------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
#define CONFIG_WIFI_1_SSID "wifi1ssid"
#define CONFIG_WIFI_1_PASS "wifi1password"
#define CONFIG_WIFI_2_SSID "wifi2ssid"
#define CONFIG_WIFI_2_PASS "wifi2password"
#define CONFIG_WIFI_3_SSID "wifi3ssid"
#define CONFIG_WIFI_3_PASS "wifi3password"

// EN: WiFi connection parameters. Commenting out these lines will use the default ESP-IDF parameters
// RU: Параметры WiFi подключения. Если закомментировать эти строки, будут использованы параметры по умолчанию ESP-IDF
// #define CONFIG_WIFI_STORAGE   WIFI_STORAGE_RAM
// #define CONFIG_WIFI_BANDWIDTH WIFI_BW_HT20

// EN: Restart the device if there is no WiFi connection for more than the specified time in minutes.
//     Comment out the line if you do not need to restart the device if there is no network connection for a long time
// RU: Перезапустить устройство, если нет подключения к WiFi более указанного времени в минутах. 
//     Закомментируйте строку, если не нужно перезапускать устройство при длительном отсутствии подключения к сети
#define CONFIG_WIFI_TIMER_RESTART_DEVICE 60*3

// EN: Allow periodic check of Internet availability using ping.
//     Sometimes network access may be lost, but WiFi connection works. In this case, the device will suspend all network processes.
// RU: Разрешить периодическую проверку доступности сети интернет с помошью пинга. 
//     Иногда доступ в сеть может пропасть, но подключение к WiFi при этом работает. В этом случае устройство приостановит все сетевые процессы.
#define CONFIG_PINGER_ENABLE 1

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------- EN - MQTT broker -------------------------------------------------------
// ---------------------------------------------- RU - MQTT брокер -------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Parameters of MQTT brokers. Two brokers can be defined: primary and backup
// RU: Параметры MQTT-брокеров. Можно определить два брокера: основной и резервный
// CONFIG_MQTTx_TYPE :: 0 - public, 1 - local, 2 - gateway (CONFIG_MQTT1_HOST not used)

/********************* local server ************************/
#define CONFIG_MQTT1_TYPE 2
#define CONFIG_MQTT1_HOST "192.168.1.1"
#define CONFIG_MQTT1_PING_CHECK 0
#define CONFIG_MQTT1_PING_CHECK_DURATION 250
#define CONFIG_MQTT1_PING_CHECK_LOSS 50.0
#define CONFIG_MQTT1_PING_CHECK_LIMIT 3
#define CONFIG_MQTT1_PORT_TCP 1883
#define CONFIG_MQTT1_PORT_TLS 8883
#define CONFIG_MQTT1_USERNAME "watering"
#define CONFIG_MQTT1_PASSWORD "q7katxak0g8s77hnx87xn"
#define CONFIG_MQTT1_TLS_ENABLED 0
#define CONFIG_MQTT1_TLS_STORAGE CONFIG_DEFAULT_TLS_STORAGE
#define CONFIG_MQTT1_TLS_PEM_START CONFIG_DEFAULT_TLS_PEM_START
#define CONFIG_MQTT1_TLS_PEM_END CONFIG_DEFAULT_TLS_PEM_END
#define CONFIG_MQTT1_CLEAN_SESSION 1
#define CONFIG_MQTT1_AUTO_RECONNECT 1
#define CONFIG_MQTT1_KEEP_ALIVE 60
#define CONFIG_MQTT1_TIMEOUT 10000
#define CONFIG_MQTT1_RECONNECT 10000
#define CONFIG_MQTT1_CLIENTID "esp32_watering1"
// #define CONFIG_MQTT1_LOC_PREFIX ""
// #define CONFIG_MQTT1_PUB_PREFIX ""
#define CONFIG_MQTT1_LOC_LOCATION "local"
#define CONFIG_MQTT1_PUB_LOCATION "v4225"
#define CONFIG_MQTT1_LOC_DEVICE "watering1"
#define CONFIG_MQTT1_PUB_DEVICE "watering1"

/********************* public server ***********************/
#define CONFIG_MQTT2_TYPE 0
#define CONFIG_MQTT2_HOST "XX.WQTT.RU"
#define CONFIG_MQTT2_PING_CHECK 1
#define CONFIG_MQTT2_PING_CHECK_DURATION 400
#define CONFIG_MQTT2_PING_CHECK_LOSS 50.0
#define CONFIG_MQTT2_PING_CHECK_LIMIT 5
#define CONFIG_MQTT2_PORT_TCP 2632
#define CONFIG_MQTT2_PORT_TLS 2633
#define CONFIG_MQTT2_USERNAME "u_username"
#define CONFIG_MQTT2_PASSWORD "q7katxak0g8s77hnx87xn"
#define CONFIG_MQTT2_TLS_ENABLED 1
#define CONFIG_MQTT2_TLS_STORAGE TLS_CERT_BUFFER
#define CONFIG_MQTT2_TLS_PEM_START CONFIG_DEFAULT_TLS_PEM_START
#define CONFIG_MQTT2_TLS_PEM_END CONFIG_DEFAULT_TLS_PEM_END
#define CONFIG_MQTT2_CLEAN_SESSION 1
#define CONFIG_MQTT2_AUTO_RECONNECT 1
#define CONFIG_MQTT2_KEEP_ALIVE 60
#define CONFIG_MQTT2_TIMEOUT 5000
#define CONFIG_MQTT2_RECONNECT 10000
#define CONFIG_MQTT2_CLIENTID "esp32_watering1"
// #define CONFIG_MQTT2_LOC_PREFIX ""
// #define CONFIG_MQTT2_PUB_PREFIX ""
#define CONFIG_MQTT2_LOC_LOCATION "local/v4225"
#define CONFIG_MQTT2_PUB_LOCATION "v4225"
#define CONFIG_MQTT1_LOC_DEVICE "watering1"
#define CONFIG_MQTT2_PUB_DEVICE "watering1"

/****************** MQTT : pinger ********************/
// EN: Allow the publication of ping results on the MQTT broker
// RU: Разрешить публикацию результатов пинга на MQTT брокере
#define CONFIG_MQTT_PINGER_ENABLE 1
#if CONFIG_MQTT_PINGER_ENABLE
// EN: Ping results topic name
// RU: Название топика результатов пинга
#define CONFIG_MQTT_PINGER_TOPIC "ping"
#define CONFIG_MQTT_PINGER_LOCAL 0
#define CONFIG_MQTT_PINGER_QOS 0
#define CONFIG_MQTT_PINGER_RETAINED 1
#define CONFIG_MQTT_PINGER_AS_PLAIN 0
#define CONFIG_MQTT_PINGER_AS_JSON 1
#endif // CONFIG_MQTT_PINGER_ENABLE

/*************** MQTT : remote control ***************/
// EN: Allow the device to process incoming commands
// RU: Разрешить обработку входящих команд устройством
#define CONFIG_MQTT_COMMAND_ENABLE 1
// EN: Allow OTA updates via a third party server
// RU: Разрешить OTA обновления через сторонний сервер
#define CONFIG_MQTT_OTA_ENABLE 1

/***************** MQTT : sensors ********************/
#define CONFIG_MQTT_MAX_OUTBOX_SIZE 0
// EN: Delay between update attempts
// RU: Интервал публикации данных с сенсоров в секундах
#define CONFIG_MQTT_SENSORS_SEND_INTERVAL 60
// EN: QOS for sensor data
// RU: QOS для данных с сенсоров
#define CONFIG_MQTT_SENSORS_QOS 1
#define CONFIG_MQTT_SENSORS_LOCAL_QOS 2
// EN: Save the last sent data on the broker
// RU: Сохранять на брокере последние отправленные данные
#define CONFIG_MQTT_SENSORS_RETAINED 1
#define CONFIG_MQTT_SENSORS_LOCAL_RETAINED 0

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------- EN - http://open-monitoring.online/ --------------------------------------------
// -------------------------------------- RU - http://open-monitoring.online/ --------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Enable sending data to open-monitoring.online
// RU: Включить отправку данных на open-monitoring.online
#define CONFIG_OPENMON_ENABLE 1
#if CONFIG_OPENMON_ENABLE
// EN: Frequency of sending data in seconds
// RU: Периодичность отправки данных в секундах
#define CONFIG_OPENMON_SEND_INTERVAL 180
// EN: Controller ids and tokens for open-monitoring.online
// RU: Идентификаторы контроллеров и токены для open-monitoring.online
#define CONFIG_OPENMON_CTR01_ID 9999
#define CONFIG_OPENMON_CTR01_TOKEN "aaaaaaa"
// EN: Allow publication of ping results и системной информации on open-monitoring.online
// RU: Разрешить публикацию результатов пинга и системной информации на open-monitoring.online
#define CONFIG_OPENMON_PINGER_ENABLE 1
#if CONFIG_OPENMON_PINGER_ENABLE
#define CONFIG_OPENMON_PINGER_ID 0000
#define CONFIG_OPENMON_PINGER_TOKEN "bbbbbbb"
#define CONFIG_OPENMON_PINGER_INTERVAL 180000
#define CONFIG_OPENMON_PINGER_RSSI 1
#define CONFIG_OPENMON_PINGER_HEAP_FREE 1
#define CONFIG_OPENMON_PINGER_HOSTS 0
#endif // CONFIG_OPENMON_PINGER_ENABLE
#endif // CONFIG_OPENMON_ENABLE

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------ EN - https://thingspeak.com/ -----------------------------------------------
// ------------------------------------------ RU - https://thingspeak.com/ -----------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Enable sending data to thingspeak.com
// RU: Включить отправку данных на thingspeak.com
#define CONFIG_THINGSPEAK_ENABLE 0
#if CONFIG_THINGSPEAK_ENABLE
// EN: Frequency of sending data in seconds
// RU: Периодичность отправки данных в секундах
#define CONFIG_THINGSPEAK_SEND_INTERVAL 300
// EN: Tokens for writing on thingspeak.com
// RU: Токены для записи на thingspeak.com
#define CONFIG_THINGSPEAK_CHANNEL01_ID 9999999
#define CONFIG_THINGSPEAK_CHANNEL01_KEY "QGZF2RYBURBIYQAY"
#endif // CONFIG_THINGSPEAK_ENABLE

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------- EN - Telegram notify ---------------------------------------------------
// ------------------------------------------- RU - Уведомления в Telegram -----------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Allow Telegram notifications (general flag)
// RU: Разрешить уведомления в Telegram (общий флаг)
#define CONFIG_TELEGRAM_ENABLE 1
#if CONFIG_TELEGRAM_ENABLE
// EN: Telegram API bot token
// RU: Токен бота API Telegram
#define CONFIG_TELEGRAM_TOKEN "00000000000:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
// EN: Chat or group ID
// RU: Идентификатор чата или группы
#define CONFIG_TELEGRAM_CHAT_ID_MAIN     "-1009999999998"
#define CONFIG_TELEGRAM_CHAT_ID_SERVICE  "-1009999999999"
#define CONFIG_TELEGRAM_CHAT_ID_PARAMS   CONFIG_TELEGRAM_CHAT_ID_SERVICE
#define CONFIG_TELEGRAM_CHAT_ID_SECURITY CONFIG_TELEGRAM_CHAT_ID_MAIN
// EN: Send message header (device name, see CONFIG_TELEGRAM_DEVICE)
// RU: Отправлять заголовок сообщения (название устройства, см. CONFIG_TELEGRAM_DEVICE)
#define CONFIG_TELEGRAM_TITLE_ENABLED 0
// EN: Device name (published at the beginning of each message)
// RU: Название устройства (публикуется в начале каждого сообщения)
#define CONFIG_TELEGRAM_DEVICE "💎 ЛИМОН"
#endif // CONFIG_TELEGRAM_ENABLE

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------- EN - Notifies ----------------------------------------------------------
// --------------------------------------------- RU - Уведомления --------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Blink the system LED when sending a data
// RU: Мигать системным светодиодом при отправке данных
#define CONFIG_SYSLED_MQTT_ACTIVITY 0
#define CONFIG_SYSLED_SEND_ACTIVITY 0
#define CONFIG_SYSLED_TELEGRAM_ACTIVITY 0
// EN: Allow remote enabling or disabling of notifications without flashing the device
// RU: Разрешить удаленную включение или отключение уведомлений без перепрошивки устройства
#define CONFIG_NOTIFY_TELEGRAM_CUSTOMIZABLE 1

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------- EN - Sensors -------------------------------------------------------
// -------------------------------------------------- RU - Сенсоры -------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Allow the publication of sensor data in a simple form (each value in a separate subtopic)
// RU: Разрешить публикацию данных сенсора в простом виде (каждое значение в отдельном субтопике)
#define CONFIG_SENSOR_AS_PLAIN 0
// EN: Allow the publication of sensor data as JSON in one topic
// RU: Разрешить публикацию данных сенсора в виде JSON в одном топике
#define CONFIG_SENSOR_AS_JSON 1
// EN: Allow publication of sensor status
// RU: Разрешить публикацию статуса сенсора
#define CONFIG_SENSOR_STATUS_ENABLE 1

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------ EN - Electricity tariffs ---------------------------------------------------
// ----------------------------------------- RU - Тарифы электроэнергии --------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

// EN: Use switching between multiple electricity tariffs
// RU: Использовать переключение между несколькими тарифами электроэнергии
#define CONFIG_ELTARIFFS_ENABLED 0
#if defined(CONFIG_ELTARIFFS_ENABLED) && CONFIG_ELTARIFFS_ENABLED
// EN: Maximum number of tariffs
// RU: Максимальное количество тарифов
#define CONFIG_ELTARIFFS_COUNT 2
// EN: Type of values for tariff parameters (OPT_KIND_PARAMETER - only for this device, OPT_KIND_PARAMETER_LOCATION - for all location devices)
// RU: Тип занчений для параметров тарифов (OPT_KIND_PARAMETER - только для этого устройства, OPT_KIND_PARAMETER_LOCATION - для всех устройств локации)
#define CONFIG_ELTARIFFS_PARAMS_TYPE OPT_KIND_PARAMETER_LOCATION
// EN: Tariff parameter values: days of the week, time intervals and cost of 1 kW/h
// RU: Значения параметров тарифов: дни недели, интервалы времени и стоимость 1кВт/ч
#define CONFIG_ELTARIFFS_TARIF1_DAYS WEEK_ANY
#define CONFIG_ELTARIFFS_TARIF1_TIMESPAN 7002300UL
#define CONFIG_ELTARIFFS_TARIF1_PRICE 5.78
#define CONFIG_ELTARIFFS_TARIF2_DAYS WEEK_ANY
#define CONFIG_ELTARIFFS_TARIF2_TIMESPAN 23000700UL
#define CONFIG_ELTARIFFS_TARIF2_PRICE 3.02
#endif // CONFIG_ELTARIFFS_ENABLED

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------- EN - Log -----------------------------------------------------------
// ------------------------------------------------ RU - Отладка ---------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------
// EN: Debug message level. Anything above this level will be excluded from the code
// RU: Уровень отладочных сообщений. Всё, что выше этого уровня, будет исключено из кода
#define CONFIG_RLOG_PROJECT_LEVEL RLOG_LEVEL_DEBUG
// EN: Add text color codes to messages. Doesn't work on Win7
// RU: Добавлять коды цвета текста к сообщениям. Не работает на Win7
#define CONFIG_RLOG_PROJECT_COLORS 1
// EN: Add time stamp to messages
// RU: Добавлять отметку времени к сообщениям
#define CONFIG_RLOG_SHOW_TIMESTAMP 1
// EN: Add file and line information to messages
// RU: Добавлять информацию о файле и строке к сообщениям
#define CONFIG_RLOG_SHOW_FILEINFO 0

// EN: Preserve debugging information across device software restarts
// RU: Сохранять отладочную информацию при программном перезапуске устройства
#define CONFIG_RESTART_DEBUG_INFO 1
// EN: Allow heap information to be saved periodically, with subsequent output on restart
// RU: Разрешить периодическое сохранение информации о куче / памяти с последующем выводом при перезапуске
#define CONFIG_RESTART_DEBUG_HEAP_SIZE_SCHEDULE 1
// EN: Depth to save the processor stack on restart. 0 - do not save
// RU: Глубина сохранения стека процессора при перезапуске. 0 - не сохранять
#define CONFIG_RESTART_DEBUG_STACK_DEPTH 28
// EN: Allow publishing debug info from WiFi module
// RU: Разрешить публикацию отладочной информации из модуля WiFi
#define CONFIG_WIFI_DEBUG_ENABLE 1
// EN: Allow periodic publication of system information
// RU: Разрешить периодическую публикацию системной информации
#define CONFIG_MQTT_SYSINFO_ENABLE 1
#define CONFIG_MQTT_SYSINFO_INTERVAL 60000           
#define CONFIG_MQTT_SYSINFO_SYSTEM_FLAGS 1
// EN: Allow periodic publication of task information. CONFIG_FREERTOS_USE_TRACE_FACILITY / configUSE_TRACE_FACILITY must be allowed
// RU: Разрешить периодическую публикацию информации о задачах. Должен быть разрешен CONFIG_FREERTOS_USE_TRACE_FACILITY / configUSE_TRACE_FACILITY
#define CONFIG_MQTT_TASKLIST_ENABLE 1
#define CONFIG_MQTT_TASKLIST_INTERVAL 300000           
