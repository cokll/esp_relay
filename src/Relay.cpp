#include "Relay.h"
#include "RadioReceive.h"
#include "Dimming.h"
#include "Rtc.h"
#ifdef USE_HOMEKIT
#include "HomeKit.h"
#endif
//---------------- INA226------------------
#include <Arduino.h>

#include <Wire.h>
#include <INA226_dev.h>

INA226 ina;
void checkConfig()
{
  Serial.print("Mode:                  ");
  switch (ina.getMode())
  {
    case INA226_MODE_POWER_DOWN:      Serial.println("Power-Down"); break;
    case INA226_MODE_SHUNT_TRIG:      Serial.println("Shunt Voltage, Triggered"); break;
    case INA226_MODE_BUS_TRIG:        Serial.println("Bus Voltage, Triggered"); break;
    case INA226_MODE_SHUNT_BUS_TRIG:  Serial.println("Shunt and Bus, Triggered"); break;
    case INA226_MODE_ADC_OFF:         Serial.println("ADC Off"); break;
    case INA226_MODE_SHUNT_CONT:      Serial.println("Shunt Voltage, Continuous"); break;
    case INA226_MODE_BUS_CONT:        Serial.println("Bus Voltage, Continuous"); break;
    case INA226_MODE_SHUNT_BUS_CONT:  Serial.println("Shunt and Bus, Continuous"); break;
    default: Serial.println("unknown");
  }
  
  Serial.print("Samples average:       ");
  switch (ina.getAverages())
  {
    case INA226_AVERAGES_1:           Serial.println("1 sample"); break;
    case INA226_AVERAGES_4:           Serial.println("4 samples"); break;
    case INA226_AVERAGES_16:          Serial.println("16 samples"); break;
    case INA226_AVERAGES_64:          Serial.println("64 samples"); break;
    case INA226_AVERAGES_128:         Serial.println("128 samples"); break;
    case INA226_AVERAGES_256:         Serial.println("256 samples"); break;
    case INA226_AVERAGES_512:         Serial.println("512 samples"); break;
    case INA226_AVERAGES_1024:        Serial.println("1024 samples"); break;
    default: Serial.println("unknown");
  }

  Serial.print("Bus conversion time:   ");
  switch (ina.getBusConversionTime())
  {
    case INA226_BUS_CONV_TIME_140US:  Serial.println("140uS"); break;
    case INA226_BUS_CONV_TIME_204US:  Serial.println("204uS"); break;
    case INA226_BUS_CONV_TIME_332US:  Serial.println("332uS"); break;
    case INA226_BUS_CONV_TIME_588US:  Serial.println("558uS"); break;
    case INA226_BUS_CONV_TIME_1100US: Serial.println("1.100ms"); break;
    case INA226_BUS_CONV_TIME_2116US: Serial.println("2.116ms"); break;
    case INA226_BUS_CONV_TIME_4156US: Serial.println("4.156ms"); break;
    case INA226_BUS_CONV_TIME_8244US: Serial.println("8.244ms"); break;
    default: Serial.println("unknown");
  }

  Serial.print("Shunt conversion time: ");
  switch (ina.getShuntConversionTime())
  {
    case INA226_SHUNT_CONV_TIME_140US:  Serial.println("140uS"); break;
    case INA226_SHUNT_CONV_TIME_204US:  Serial.println("204uS"); break;
    case INA226_SHUNT_CONV_TIME_332US:  Serial.println("332uS"); break;
    case INA226_SHUNT_CONV_TIME_588US:  Serial.println("558uS"); break;
    case INA226_SHUNT_CONV_TIME_1100US: Serial.println("1.100ms"); break;
    case INA226_SHUNT_CONV_TIME_2116US: Serial.println("2.116ms"); break;
    case INA226_SHUNT_CONV_TIME_4156US: Serial.println("4.156ms"); break;
    case INA226_SHUNT_CONV_TIME_8244US: Serial.println("8.244ms"); break;
    default: Serial.println("unknown");
  }
  
  Serial.print("Max possible current:  ");
  Serial.print(ina.getMaxPossibleCurrent());
  Serial.println(" A");

  Serial.print("Max current:           ");
  Serial.print(ina.getMaxCurrent());
  Serial.println(" A");

  Serial.print("Max shunt voltage:     ");
  Serial.print(ina.getMaxShuntVoltage());
  Serial.println(" V");

  Serial.print("Max power:             ");
  Serial.print(ina.getMaxPower());
  Serial.println(" W");
  Log::Info(PSTR("Max possible current: %.2f A"), ina.getMaxPossibleCurrent());
  Log::Info(PSTR("Max current: %.2f A"), ina.getMaxCurrent());
  Log::Info(PSTR("Max shunt voltage: %.2f V"), ina.getMaxShuntVoltage());
  Log::Info(PSTR("Max power : %.2f W"), ina.getMaxPower());
}

/**************************************************************************************************
** Declare program Constants                                                                     **
**************************************************************************************************/
const uint8_t  INA_ALERT_PIN = 2;       ///< Pin-Change pin used for the INA "ALERT" functionality

const uint32_t SERIAL_SPEED  = 115200;  ///< Use fast serial speed
/**************************************************************************************************
** Declare global variables and instantiate classes                                              **
**************************************************************************************************/


static long lastMillis = millis();  // Store the last time we printed something
static long lastDelayMillis = millis();  // Store the last time we started the delay
const unsigned long requiredDuration = 60000; // 10秒（单位：毫秒）
unsigned long startTime = 0;
unsigned long lowVoltageStartTime = 0; // 记录低电压开始时间
// 分压电阻的阻值，单位为欧姆
float Rshunt = 0.001; // R100，即 100 mΩ
uint8_t LED_PIN = 99;
uint8_t RFRECV_PIN = 99;
uint8_t RELAY_PIN[MAX_RELAY_NUM];
uint8_t BOTTON_PIN[MAX_RELAY_NUM + MAX_PWM_NUM];
uint8_t RELAY_LED_PIN[MAX_RELAY_NUM + MAX_PWM_NUM];
uint8_t RELAY_LED_BACKLIGHT_PIN = 99;


#ifdef USE_DIMMING
uint8_t dimmingState[MAX_PWM_NUM]; // 0: 是否下降亮度， 1：进入调光模式  2：已经调光
unsigned long colorOffTime[MAX_PWM_NUM];

uint8_t PWM_BRIGHTNESS_PIN[MAX_PWM_NUM];
uint8_t PWM_TEMPERATURE_PIN[MAX_PWM_NUM];
uint8_t ROT_PIN[2];
#endif

#pragma region 继承

String Relay::getModuleCNName()
{
    mytmplt m;
    memcpy_P(&m, &Modules[config.module_type], sizeof(m));
    return String(m.name);
}

void Relay::init()
{

    loadModule(config.module_type);
    if (LED_PIN != 99)
    {
        Led::init(LED_PIN > 50 ? LED_PIN - 50 : LED_PIN, LED_PIN > 50 ? LOW : HIGH);
    }


#ifdef USE_RCSWITCH
    if (RFRECV_PIN != 99)
    {
        radioReceive = new RadioReceive();
        radioReceive->init(this, RFRECV_PIN);
    }
#endif

    channels = 0;
    for (uint8_t ch = 0; ch < (sizeof(RELAY_PIN) / sizeof(RELAY_PIN[0])); ch++)
    {
        if (RELAY_PIN[ch] == 99)
        {
            continue;
        }
        channels++;

        pinMode(RELAY_PIN[ch], OUTPUT); // 继电器
    }

#ifdef USE_DIMMING
    if (PWM_BRIGHTNESS_PIN[0] != 99)
    {
        dimming = new Dimming();
        dimming->init(this);
        strcpy(brightnessStatTopic, Mqtt::getStatTopic(F("brightness1")).c_str());
        strcpy(color_tempStatTopic, Mqtt::getStatTopic(F("color_temp1")).c_str());
    }
#endif

    strcpy(powerStatTopic, Mqtt::getStatTopic(F("power1")).c_str());
    for (uint8_t ch = 0; ch < channels; ch++)
    {
        if (RELAY_LED_PIN[ch] != 99)
        {
            pinMode(RELAY_LED_PIN[ch], OUTPUT); // LED
        }
        if (BOTTON_PIN[ch] != 99)
        {
            pinMode(BOTTON_PIN[ch], INPUT_PULLUP);
            if (digitalRead(BOTTON_PIN[ch]))
            {
                buttonStateFlag[ch] |= DEBOUNCED_STATE | UNSTABLE_STATE;
            }
        }

        // 0:开关通电时断开  1:开关通电时闭合  2:开关通电时状态与断电前相反  3:开关通电时保持断电前状态
        if (config.power_on_state == 2)
        {
            switchRelay(ch, !bitRead(config.last_state, ch), false); // 开关通电时状态与断电前相反
        }
        else if (config.power_on_state == 3)
        {
            switchRelay(ch, bitRead(config.last_state, ch), false); // 开关通电时保持断电前状态
        }
        else
        {
            switchRelay(ch, config.power_on_state == 1, false); // 开关通电时闭合
        }
    }

    if (RELAY_LED_BACKLIGHT_PIN != 99)
    {
        pinMode(RELAY_LED_BACKLIGHT_PIN, OUTPUT); // 开关面板背光灯
    }
    // 初始化软件串口
    pinMode(INA_ALERT_PIN, INPUT_PULLUP);  // Declare pin with internal pull-up resistor
    
    Serial.println("Initialize INA226");
    Serial.println("-----------------------------------------------");

    Wire.begin();

    // Default INA226 address is 0x40
    bool success = ina.begin();

    // Check if the connection was successful, stop if not
    if(!success)
    {
        Serial.println("Connection error");
        while(1);
    }

    // Configure INA226
    ina.configure(INA226_AVERAGES_1, INA226_BUS_CONV_TIME_1100US, INA226_SHUNT_CONV_TIME_1100US, INA226_MODE_SHUNT_BUS_CONT);
    
    // Calibrate INA226. Rshunt = 0.01 ohm, Max excepted current = 4A
    ina.calibrate(0.01, 8);

    // Display configuration
    checkConfig();

    checkCanLed(true);
}

bool Relay::moduleLed()
{
#ifdef USE_RCSWITCH
    if (radioReceive && radioReceive->studyCH > 0)
    {
        Led::on();
        return true;
    }
#endif

    if (bitRead(Config::statusFlag, 0) && bitRead(Config::statusFlag, 1))
    {
        if (config.led == 0)
        {
            Led::on();
            return true;
        }
        else if (config.led == 1)
        {
            Led::off();
            return true;
        }
    }
    return false;
}
IPAddress target(192, 168, 1, 12);
const int maxAttempts = 10;


int failedAttempts = 0;
int successfulAttempts = 0;
bool relaySwitched = true; // 添加一个标志以跟踪是否已经切换了继电器
void Relay::loop()
{
    busvoltage=ina.readBusVoltage()+0.4;
    busPower=ina.readBusPower();
    shuntvoltage = ina.readShuntVoltage();
    current_mA = ina.readShuntCurrent() * 1000;
    loadvoltage = busvoltage + (shuntvoltage / 1000);



#ifdef USE_DIMMING
    if (dimming)
    {
        dimming->loop();
    }
#endif
    for (size_t ch = 0; ch < channels; ch++)
    {
        checkButton(ch);
    }
    uint8_t ch = 0;

    if (millis() - lastDelayMillis >= 600000) {  // Check if 1 second has passed
        lastDelayMillis = millis();  // Reset delay timer

        if (busvoltage<9){
            switchRelay(ch, false, false); // 切换继电器
            Log::Info(PSTR("电压过低: %.2f"), busvoltage);
            
        }
        /*
        Serial.print("Bus voltage:   ");
        Serial.print(busvoltage, 5);
        Serial.println(" V");
        busPower=ina.readBusPower();
        Serial.print("Bus power:     ");
        Serial.print(busPower, 5);
        Serial.println(" W");

        shuntvoltage=ina.readShuntVoltage();
        Serial.print("Shunt voltage: ");
        Serial.print(shuntvoltage, 5);
        Serial.println(" V");
        current_mA=ina.readShuntCurrent();
        Serial.print("Shunt current: ");
        Serial.print(current_mA, 5);
        Serial.println(" A");

        Serial.println("");*/
    }



    if (busvoltage > 15) {
        if (startTime == 0) {
            // 电压首次超过15时记录开始时间
            startTime = millis();
        } else if (millis() - startTime >= requiredDuration) {
            // 如果电压已经持续超过15达到或超过10秒
            successfulAttempts++; // 增加成功尝试次数
            failedAttempts = 0; // 重置失败尝试次数
            if (relaySwitched) { // 如果继电器已经切换，则将其切换回来
                switchRelay(ch, true, false);
                switchRelay(1, true, false);
                relaySwitched = false; // 重置标志
                Log::Info(PSTR("BusVoltage: %.2f"), busvoltage);
                Log::Info(PSTR("Relay switched on successfully"));
            }
        }
        // 如果电压回升到15V以上，则取消切换继电器的操作
        if (lowVoltageStartTime != 0 && busvoltage > 15) {
            lowVoltageStartTime = 0; // 重置低电压开始时间
        }
    } else if (busvoltage < 12) {
        // 电压低于13V时关闭继电器
        switchRelay(ch, false, false);
        switchRelay(1, false, false);
        relaySwitched = true; // 设置标志以防止重复切换
        Log::Info(PSTR("BusVoltage: %.2f"), busvoltage);
        Log::Info(PSTR("Relay switched off successfully"));
    } else {
        // 电压在13V到15V之间时，保持继电器状态不变
        // 检查失败尝试次数是否超过阈值，以决定是否切换继电器
        failedAttempts++; // 增加失败尝试次数
        successfulAttempts = 0; // 重置成功尝试次数
        if (failedAttempts >= maxAttempts && !relaySwitched) {
            Serial.println("Exceeded maximum failed attempts, switching relay");
            switchRelay(ch, false, false); // 切换继电器
            switchRelay(1, false, false);
            relaySwitched = true; // 设置标志以防止重复切换
            Log::Info(PSTR("BusVoltage: %.2f"), busvoltage);
            Log::Info(PSTR("Relay switched off successfully"));
        }
        
        // 如果电压低于15V，记录低电压开始时间
        if (busvoltage <= 15 && lowVoltageStartTime == 0) {
            lowVoltageStartTime = millis();
        }
        
        // 检查低电压持续时间是否超过10秒且电压已经回升到15V以上
        if (lowVoltageStartTime != 0 && millis() - lowVoltageStartTime >= 10000 && busvoltage > 15) {
            // 取消切换继电器的操作
            if (relaySwitched) {
                switchRelay(ch, true, false);
                switchRelay(1, true, false);
                relaySwitched = false; // 重置标志
                Log::Info(PSTR("BusVoltage: %.2f"), busvoltage);
                Log::Info(PSTR("Cancelled relay switch due to voltage recovery"));
            }
            lowVoltageStartTime = 0; // 重置低电压开始时间
        }
    }


    /*
    String c = "2";
    uint8_t ch = c.toInt() - 1;
    if(WiFi.status() == WL_CONNECTED){
        if (Ping.ping(target,1)) {
            Serial.println("Ping successful");
            successfulAttempts++; // 增加成功尝试次数
            failedAttempts = 0; // 重置失败尝试次数
            if (relaySwitched) { // 如果继电器已经切换，则将其切换回来
                switchRelay(ch, true, false);
                relaySwitched = false; // 重置标志
                Serial.println("relaySwitched On successful");
            }
        } else {
            Serial.println("Ping failed");
            failedAttempts++; // 增加失败尝试次数
            successfulAttempts = 0; // 重置成功尝试次数
            if (failedAttempts >= maxAttempts && !relaySwitched) {
                Serial.println("Exceeded maximum failed attempts, switching relay");
                switchRelay(ch, false, false); // 切换继电器
                relaySwitched = true; // 设置标志以防止重复切换
                Serial.println("relaySwitched Off successful");
            }
        }
    }*/

    // 从软串口读取数据并打印到硬件串口（控制台）

    //Log::Info(PSTR("HLK-105: %s"), c);


#ifdef USE_RCSWITCH
    if (radioReceive)
    {
        radioReceive->loop();
    }
#endif
}

void Relay::perSecondDo()
{
    if (perSecond % 60 == 0)
    {
        checkCanLed();
    }
    if (config.report_interval > 0 && (perSecond % config.report_interval) == 0)
    {
        reportPower();
    }
}
#pragma endregion

#pragma region 配置

void Relay::readConfig()
{
    bool isOk = false;
#ifdef USE_UFILESYS
    isOk = Config::FSReadConfig(RELAY_CONFIG, RELAY_CFG_VERSION, sizeof(RelayConfigMessage), RelayConfigMessage_fields, &config, configCrc);
#endif
    if (!isOk)
    {
        Config::moduleReadConfig(RELAY_CFG_VERSION, sizeof(RelayConfigMessage), RelayConfigMessage_fields, &config);
    }
    if (config.led_light == 0)
    {
        config.led_light = 100;
    }
    if (config.led_backlight == 0)
    {
        config.led_backlight = 100;
    }
    if (config.led_time == 0)
    {
        config.led_time = 2;
    }
    ledLight = RELAY_LED_LIGHT;
    ledBackLight = RELAY_LED_BackLIGHT;

    if (config.module_type >= MAXMODULE)
    {
        config.module_type = 0;
    }
}

void Relay::resetConfig()
{
    Log::Info(PSTR("moduleResetConfig . . . OK"));
    memset(&config, 0, sizeof(RelayConfigMessage));
    config.module_type = 0;
    config.led_light = 50;
    ledLight = RELAY_LED_LIGHT;
    config.led_backlight = 50;
    ledBackLight = RELAY_LED_BackLIGHT;
    config.led_time = 3;
    config.led_type = 1;
    config.max_pwm = 100;

// 定义开关的默认模块
#ifdef WIFI_SSID
#ifdef ESP8266
// #ifdef USE_DIMMING
//     config.module_type = Yeelight;
// #else
//     config.module_type = CH3;
// #endif
    config.module_type = CH3;
#else
    config.module_type = ESP32CH3;
#endif
    globalConfig.mqtt.interval = 60 * 60;
    globalConfig.debug.type = globalConfig.debug.type | 4;
    config.power_on_state = 0;
    config.power_mode = 0;
    config.switch_mode = 0;
    config.led_type = 1;
    config.led_light = 50;
    config.led_start = 0;
    config.led_end = 0;
    config.led = 1;
    config.report_interval = 60 * 5;
#endif
}

void Relay::saveConfig(bool isEverySecond)
{
#ifdef USE_UFILESYS
    if (Config::FSSaveConfig(RELAY_CONFIG, RELAY_CFG_VERSION, RelayConfigMessage_size, RelayConfigMessage_fields, &config, configCrc))
    {
        //globalConfig.cfg_version = 0;
        //globalConfig.module_crc = 0;
        //globalConfig.module_cfg.size = 0;
        //memset(globalConfig.module_cfg.bytes, 0, 500);
        //return;
    }
#endif
    Config::moduleSaveConfig(RELAY_CFG_VERSION, RelayConfigMessage_size, RelayConfigMessage_fields, &config);
}
#pragma endregion

#pragma region MQTT

bool Relay::mqttCallback(char *topic, char *payload, char *cmnd)
{
    if (strlen(cmnd) == 6 && strncmp(cmnd, "power", 5) == 0) // strlen("power1") = 6
    {
        uint8_t ch = cmnd[5] - 49;
        if (ch < channels)
        {
            switchRelay(ch, (strcmp(payload, "on") == 0 ? true : (strcmp(payload, "off") == 0 ? false : !bitRead(lastState, ch))), true);
            return true;
        }
    }
    else if (strcmp(cmnd, "report") == 0)
    {
        reportPower();
        return true;
    }
#ifdef USE_DIMMING
    else if (dimming)
    {
        bool result = dimming->mqttCallback(topic, payload, cmnd);
        if (result)
        {
            return true;
        }
    }
#endif
    return false;
}

void Relay::mqttConnected()
{
#ifdef USE_DIMMING
    if (PWM_BRIGHTNESS_PIN[0] != 99)
    {
        strcpy(brightnessStatTopic, Mqtt::getStatTopic(F("brightness1")).c_str());
        strcpy(color_tempStatTopic, Mqtt::getStatTopic(F("color_temp1")).c_str());
    }
#endif
    strcpy(powerStatTopic, Mqtt::getStatTopic(F("power1")).c_str());
    if (globalConfig.mqtt.discovery)
    {
        mqttDiscovery(true);
    }
    reportPower();
}

void Relay::mqttDiscovery(bool isEnable)
{
    char topic[50];
    char message[500];

    String availability = Mqtt::getTeleTopic(F("availability"));
    char cmndTopic[80];
    strcpy(cmndTopic, Mqtt::getCmndTopic(F("power1")).c_str());
    for (size_t ch = 0; ch < channels; ch++)
    {
        sprintf(topic, PSTR("%s/light/%s_l%d/config"), globalConfig.mqtt.discovery_prefix, UID, (ch + 1));
        if (isEnable)
        {
            cmndTopic[strlen(cmndTopic) - 1] = ch + 49;           // 48 + 1 + ch
            powerStatTopic[strlen(powerStatTopic) - 1] = ch + 49; // 48 + 1 + ch
            sprintf(message, PSTR("{\"name\":\"%s_%d\","
                                  "\"command_topic\":\"%s\","
                                  "\"state_topic\":\"%s\","
                                  "\"payload_off\":\"off\","
                                  "\"payload_on\":\"on\","
                                  "\"availability_topic\":\"%s\","
                                  "\"payload_available\":\"online\","
                                  "\"payload_not_available\":\"offline\","
                                  "\"unique_id\":\"%s_%d\","
                                  "\"device\":{"
                                  "\"identifiers\":\"%s\","
                                  "\"name\":\"%s\","
                                  "\"sw_version\":\"esp_relay-%s\","
                                  "\"sw_version\":\"esp_relay-%s\","
                                  "\"sw_version\":\"esp_relay-%s\","
                                  "\"sw_version\":\"esp_relay-%s\","
                                  "\"model\":\"esp_relay\","
                                  "\"manufacturer\":\"espressif\"}}"
                                  ),
                    UID, (ch + 1),
                    cmndTopic,
                    powerStatTopic,
                    availability.c_str(),
                    UID, (ch + 1),
                    UID,
                    UID,
                    getModuleVersion().c_str());
            //Debug::AddInfo(PSTR("discovery: %s - %s"), topic, message);
            Mqtt::publish(topic, message, true);
        }
        else
        {
            Mqtt::publish(topic, "", true);
        }
    }
    if (isEnable)
    {
        Mqtt::availability();
    }
}
#pragma endregion

#pragma region Http

void Relay::httpAdd(WebServer *server)
{
    server->on(F("/relay_do"), std::bind(&Relay::httpDo, this, server));
#ifdef USE_DIMMING
    if (dimming)
    {
        server->on(F("/set_brightness"), std::bind(&Dimming::httpSetBrightness, dimming, server));
    }
#endif
    server->on(F("/relay_setting"), std::bind(&Relay::httpSetting, this, server));
    server->on(F("/ha"), std::bind(&Relay::httpHa, this, server));
#ifdef USE_RCSWITCH
    server->on(F("/rf_do"), std::bind(&Relay::httpRadioReceive, this, server));
#endif
#ifdef USE_HOMEKIT
    server->on(F("/homekit"), std::bind(&homekit_http, server));
#endif
}

String Relay::httpGetStatus(WebServer *server)
{
    String data;
    for (size_t ch = 0; ch < channels; ch++)
    {
        data += ",\"relay_" + String(ch + 1) + "\":";
        data += bitRead(lastState, ch) ? 1 : 0;
    }
#ifdef USE_DIMMING
    if (dimming)
    {
        data += ",";
        data += dimming->httpGetStatus(server);
    }
#endif
    return data.substring(1);
}

void Relay::httpHtml(WebServer *server)
{
    char html[512] = {0};
    server->sendContent_P(
        PSTR("<table class='gridtable'><thead><tr><th colspan='2'>开关状态</th></tr></thead><tbody>"
             "<tr style='text-align:center'><td colspan='2'>"));

    for (size_t ch = 0; ch < channels; ch++)
    {
        snprintf_P(html, sizeof(html),
                   PSTR(" <button type='button' style='width:50px' onclick=\"ajaxPost('/relay_do', 'do=T&c=%d');\" id='relay_%d' class='btn-%s'>%s</button>"),
                   ch + 1, ch + 1,
                   bitRead(lastState, ch) ? PSTR("success") : PSTR("info"),
                   bitRead(lastState, ch) ? PSTR("开") : PSTR("关"));
        server->sendContent_P(html);
        if (channels >= 12 && (ch + 1) == (channels / 2))
        {
            server->sendContent_P("<br><br>");
        }
    }

#ifdef USE_DIMMING
    if (dimming)
    {
        dimming->httpHtml(server);
    }
#endif

    server->sendContent_P(
        PSTR("</td></tr></tbody></table>"
             "<form method='post' action='/relay_setting' onsubmit='postform(this);return false'>"
             "<table class='gridtable'><thead><tr><th colspan='2'>开关设置</th></tr></thead><tbody>"));

    if (SupportedModules::MAXMODULE > 1)
    {
        server->sendContent_P(
            PSTR("<tr><td>模块类型</td><td>"
                 "<select id='module_type' name='module_type' style='width:150px'>"));
        for (int count = 0; count < SupportedModules::MAXMODULE; count++)
        {
            snprintf_P(html, sizeof(html), PSTR("<option value='%d'>%s</option>"), count, Modules[count].name);
            server->sendContent_P(html);
        }

        server->sendContent_P(PSTR("</select></td></tr>"));
    }
    server->sendContent_P(
        PSTR("<tr><td>上电状态</td><td>"
             "<label class='bui-radios-label'><input type='radio' name='power_on_state' value='0'/><i class='bui-radios'></i> 开关通电时断开</label><br/>"
             "<label class='bui-radios-label'><input type='radio' name='power_on_state' value='1'/><i class='bui-radios'></i> 开关通电时闭合</label><br/>"
             "<label class='bui-radios-label'><input type='radio' name='power_on_state' value='2'/><i class='bui-radios'></i> 开关通电时状态与断电前相反</label><br/>"
             "<label class='bui-radios-label'><input type='radio' name='power_on_state' value='3'/><i class='bui-radios'></i> 开关通电时保持断电前状态</label>"
             "</td></tr>"));

    if (channels > 1)
    {
        server->sendContent_P(
            PSTR("<tr><td>开关模式</td><td>"
                 "<label class='bui-radios-label'><input type='radio' name='power_mode' value='0'/><i class='bui-radios'></i> 自锁</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='power_mode' value='1'/><i class='bui-radios'></i> 互锁</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='power_mode' value='2'/><i class='bui-radios'></i> 点动</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "</td></tr>"));
    }

    server->sendContent_P(
        PSTR("<tr><td>开关类型</td><td>"
             "<label class='bui-radios-label'><input type='radio' name='switch_mode' value='0'/><i class='bui-radios'></i> 自动</label>&nbsp;&nbsp;&nbsp;&nbsp;"
             "<label class='bui-radios-label'><input type='radio' name='switch_mode' value='1'/><i class='bui-radios'></i> 自复位开关</label>&nbsp;&nbsp;&nbsp;&nbsp;"
             "<label class='bui-radios-label'><input type='radio' name='switch_mode' value='2'/><i class='bui-radios'></i> 传统开关</label>"
             "</td></tr>"));

    if (RELAY_LED_PIN[0] != 99)
    {
        server->sendContent_P(
            PSTR("<tr><td>面板指示灯</td><td>"
                 "<label class='bui-radios-label'><input type='radio' name='led_type' value='0'/><i class='bui-radios'></i> 无</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='led_type' value='1'/><i class='bui-radios'></i> 普通</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='led_type' value='2'/><i class='bui-radios'></i> 呼吸灯</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 //"<label class='bui-radios-label'><input type='radio' name='led_type' value='3'/><i class='bui-radios'></i> WS2812</label>"
                 "</td></tr>"));

        snprintf_P(html, sizeof(html),
                   PSTR("<tr><td>指示灯亮度</td><td><input type='range' min='1' max='100' name='led_light' value='%d' onchange='ledLightRangOnChange(this)'/>&nbsp;<span>%d</span></td></tr>"),
                   config.led_light, config.led_light);
        server->sendContent_P(html);

        snprintf_P(html, sizeof(html),
                   PSTR("<tr><td>背光灯亮度</td><td><input type='range' min='1' max='100' name='led_backlight' value='%d' onchange='ledLightRangOnChange(this)'/>&nbsp;<span>%d</span></td></tr>"),
                   config.led_backlight, config.led_backlight);
        server->sendContent_P(html);

        snprintf_P(html, sizeof(html),
                   PSTR("<tr><td>渐变时间</td><td><input type='number' name='relay_led_time' value='%d'>毫秒</td></tr>"),
                   config.led_time);
        server->sendContent_P(html);

        String tmp = "";
        for (uint8_t i = 0; i <= 23; i++)
        {
            tmp += F("<option value='{v1}'>{v}:00</option>");
            tmp += F("<option value='{v2}'>{v}:30</option>");
            tmp.replace(F("{v1}"), String(i * 100));
            tmp.replace(F("{v2}"), String(i * 100 + 30));
            tmp.replace(F("{v}"), i < 10 ? "0" + String(i) : String(i));
        }

        server->sendContent_P(
            PSTR("<tr><td>指示灯时间段</td><td>"
                 "<select id='led_start' name='led_start'>"));

        server->sendContent(tmp);

        server->sendContent_P(
            PSTR("</select>"
                 "&nbsp;&nbsp;到&nbsp;&nbsp;"
                 "<select id='led_end' name='led_end'>"));
        server->sendContent(tmp);
        server->sendContent_P(PSTR("</select>"));
        server->sendContent_P(PSTR("</td></tr>"));
    }

    if (LED_PIN != 99)
    {
        server->sendContent_P(
            PSTR("<tr><td>LED</td><td>"
                 "<label class='bui-radios-label'><input type='radio' name='led' value='0'/><i class='bui-radios'></i> 常亮</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='led' value='1'/><i class='bui-radios'></i> 常灭</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='led' value='2'/><i class='bui-radios'></i> 闪烁</label><br>未连接WIFI或者MQTT时为快闪"
                 "</td></tr>"));
    }

    snprintf_P(html, sizeof(html),
               PSTR("<tr><td>主动上报间隔</td><td><input type='number' min='0' max='3600' name='report_interval' required value='%d'>&nbsp;秒，0关闭</td></tr>"),
               config.report_interval);
    server->sendContent_P(html);

#ifdef USE_DIMMING
    if (dimming)
    {
        snprintf_P(html, sizeof(html),
                   PSTR("<tr><td>PWM最大亮度</td><td><input type='range' min='20' max='100' name='max_pwm' value='%d' onchange='this.nextSibling.nextSibling.innerHTML=this.value'/>&nbsp;<span>%d</span></td></tr>"),
                   config.max_pwm, config.max_pwm);
        server->sendContent_P(html);
    }
#endif

    server->sendContent_P(
        PSTR("<tr><td colspan='2'><button type='submit' class='btn-info'>设置</button><br>"
             "<button type='button' class='btn-success' style='margin-top:10px' onclick='window.location.href=\"/ha\"'>下载HA配置文件</button></td></tr>"
             "</tbody></table></form>"));

#ifdef USE_RCSWITCH
    if (radioReceive)
    {
        server->sendContent_P(
            PSTR("<table class='gridtable'><thead><tr><th colspan='2'>射频管理</th></tr></thead><tbody>"
                 "<tr><td>学习模式</td><td>"));
        for (size_t ch = 0; ch < channels; ch++)
        {
            snprintf_P(html, sizeof(html),
                       PSTR(" <button type='button' style='width:60px' onclick=\"ajaxPost('/rf_do', 'do=s&c=%d')\" class='btn-success'>%d路</button>"),
                       ch + 1, ch + 1);
            server->sendContent_P(html);
        }

        server->sendContent_P(PSTR("</td></tr>"
                                   "<tr><td>删除模式</td><td>"));
        for (size_t ch = 0; ch < channels; ch++)
        {
            snprintf_P(html, sizeof(html),
                       PSTR(" <button type='button' style='width:60px' onclick=\"ajaxPost('/rf_do', 'do=d&c=%d')\" class='btn-info'>%d路</button>"),
                       ch + 1, ch + 1);
            server->sendContent_P(html);
        }

        server->sendContent_P(
            PSTR("</td></tr>"
                 "<tr><td>全部删除</td><td>"));
        for (size_t ch = 0; ch < channels; ch++)
        {
            snprintf_P(html, sizeof(html),
                       PSTR(" <button type='button' style='width:60px' onclick=\"javascript:if(confirm('确定要清空射频遥控？')){ajaxPost('/rf_do', 'do=c&c=%d');}\" class='btn-danger'>%d路</button>"),
                       ch + 1, ch + 1);
        }

        server->sendContent_P(
            PSTR(" <button type='button' style='width:50px' onclick=\"javascript:if(confirm('确定要清空全部射频遥控？')){ajaxPost('/rf_do', 'do=c&c=0');}\" class='btn-danger'>全部</button>"
                 "</td></tr>"
                 "</tbody></table>"));
    }
#endif

#ifdef USE_HOMEKIT
    homekit_html(server);
#endif

    server->sendContent_P(
        PSTR("<script type='text/javascript'>"
             "function setDataSub(data,key){if(key.substr(0,10)=='brightness'||key.substr(0,5)=='color'){var t=id(key);var v=data[key];t.value=v;t.nextSibling.nextSibling.innerHTML=v+(key.substr(0,10)=='brightness'?\"%\":\"\");return true}else if(key.substr(0,5)=='relay'){var t=id(key);var v=data[key];t.setAttribute('class',v==1?'btn-success':'btn-info');t.innerHTML=v==1?'开':'关';return true}return false}"));

    snprintf_P(html, sizeof(html), PSTR("id('module_type').value=%d;setRadioValue('power_on_state', '%d');setRadioValue('power_mode', '%d');setRadioValue('switch_mode', '%d');"),
               config.module_type, config.power_on_state, config.power_mode, config.switch_mode);
    server->sendContent_P(html);

    if (RELAY_LED_PIN[0] != 99)
    {
        snprintf_P(html, sizeof(html), PSTR("setRadioValue('led_type', '%d');id('led_start').value=%d;id('led_end').value=%d;"),
                   config.led_type, config.led_start, config.led_end);
        server->sendContent_P(html);
        server->sendContent_P(PSTR("function ledLightRangOnChange(the){the.nextSibling.nextSibling.innerHTML=the.value};"));
    }

    if (LED_PIN != 99)
    {
        snprintf_P(html, sizeof(html), PSTR("setRadioValue('led', '%d');"), config.led);
        server->sendContent_P(html);
    }
    server->sendContent_P(PSTR("</script>"));
}

void Relay::httpDo(WebServer *server)
{
    uint8_t ch = server->arg(F("c")).toInt() - 1;
    if (ch > channels)
    {
        server->send_P(200, PSTR("application/json"), PSTR("{\"code\":0,\"msg\":\"继电器数量错误。\"}"));
        return;
    }
    String str = server->arg(F("do"));
    switchRelay(ch, (str == F("on") ? true : (str == F("off") ? false : !bitRead(lastState, ch))));

    char html[512] = {0};
    snprintf_P(html, sizeof(html), PSTR("{\"code\":1,\"msg\":\"操作成功\",\"data\":{%s}}"), httpGetStatus(server).c_str());
    server->send_P(200, PSTR("application/json"), html);
}

#ifdef USE_RCSWITCH
void Relay::httpRadioReceive(WebServer *server)
{
    if (!radioReceive)
    {
        server->send_P(200, PSTR("text/html"), PSTR("{\"code\":0,\"msg\":\"没有射频模块。\"}"));
        return;
    }
    String d = server->arg(F("do"));
    String c = server->arg(F("c"));
    if ((d != F("s") && d != F("d") && d != F("c")) || (c != F("0") && (c.toInt() < 1 || c.toInt() > channels)))
    {
        server->send_P(200, PSTR("text/html"), PSTR("{\"code\":0,\"msg\":\"参数错误。\"}"));
        return;
    }
    if (radioReceive->studyCH != 0)
    {
        server->send_P(200, PSTR("text/html"), PSTR("{\"code\":0,\"msg\":\"上一个操作未完成\"}"));
        return;
    }
    if (d == F("s"))
    {
        radioReceive->study(c.toInt() - 1);
    }
    else if (d == F("d"))
    {
        radioReceive->del(c.toInt() - 1);
    }
    else if (d == F("c"))
    {
        if (c == F("0"))
        {
            radioReceive->delAll();
        }
        else
        {
            config.study_index[c.toInt() - 1] = 0;
        }
    }
    Config::saveConfig();
    server->send_P(200, PSTR("text/html"), PSTR("{\"code\":1,\"msg\":\"操作成功\"}"));
}
#endif

void Relay::httpSetting(WebServer *server)
{
    config.power_on_state = server->arg(F("power_on_state")).toInt();
    config.report_interval = server->arg(F("report_interval")).toInt();

    if (server->hasArg(F("led")))
    {
        config.led = server->arg(F("led")).toInt();
    }
    if (server->hasArg(F("switch_mode")))
    {
        config.switch_mode = server->arg(F("switch_mode")).toInt();
    }
    if (server->hasArg(F("power_mode")))
    {
        config.power_mode = server->arg(F("power_mode")).toInt();
    }
    if (server->hasArg(F("led_type")))
    {
        config.led_type = server->arg(F("led_type")).toInt();
    }

    if (server->hasArg(F("led_start")) && server->hasArg(F("led_end")))
    {
        config.led_start = server->arg(F("led_start")).toInt();
        config.led_end = server->arg(F("led_end")).toInt();
    }

    if (server->hasArg(F("led_light")))
    {
        config.led_light = server->arg(F("led_light")).toInt();
        ledLight = RELAY_LED_LIGHT;
    }
    if (server->hasArg(F("led_backlight")))
    {
        config.led_backlight = server->arg(F("led_backlight")).toInt();
        ledBackLight = RELAY_LED_BackLIGHT;
    }
    if (server->hasArg(F("relay_led_time")))
    {
        config.led_time = server->arg(F("relay_led_time")).toInt();
        if (config.led_type == 2 && ledTicker.active())
        {
            ledTicker.detach();
        }
    }
#ifdef USE_DIMMING
    if (server->hasArg(F("max_pwm")))
    {
        config.max_pwm = server->arg(F("max_pwm")).toInt();
        dimming->pwm_range = PWM_RANGE * config.max_pwm / 100;
    }
#endif
    checkCanLed(true);

    if (server->hasArg(F("module_type")) && !server->arg(F("module_type")).equals(String(config.module_type)))
    {
        server->send_P(200, PSTR("text/html"), PSTR("{\"code\":1,\"msg\":\"已经更换模块类型 . . . 正在重启中。\"}"));
        config.module_type = server->arg(F("module_type")).toInt();
        Config::saveConfig();
        Led::blinkLED(400, 4);
        ESP.restart();
    }
    else
    {
        Config::saveConfig();
        server->send_P(200, PSTR("text/html"), PSTR("{\"code\":1,\"msg\":\"已经设置成功。\"}"));
    }
}

void Relay::httpHa(WebServer *server)
{
    char html[512];
    snprintf_P(html, sizeof(html), PSTR("attachment; filename=%s.yaml"), UID);

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->sendHeader(F("Content-Disposition"), html);
    server->send_P(200, PSTR("application/octet-stream"), "light:\r\n");

    String availability = Mqtt::getTeleTopic(F("availability"));
    char cmndTopic[100];
    strcpy(cmndTopic, Mqtt::getCmndTopic(F("power1")).c_str());

    for (size_t ch = 0; ch < channels; ch++)
    {
        cmndTopic[strlen(cmndTopic) - 1] = ch + 49;           // 48 + 1 + ch
        powerStatTopic[strlen(powerStatTopic) - 1] = ch + 49; // 48 + 1 + ch

        snprintf_P(html, sizeof(html),
                   PSTR("  - platform: mqtt\r\n"
                        "    name: \"%s_%d\"\r\n"
                        "    state_topic: \"%s\"\r\n"
                        "    command_topic: \"%s\"\r\n"
                        "    payload_on: \"on\"\r\n"
                        "    payload_off: \"off\"\r\n"
                        "    availability_topic: \"%s\"\r\n"
                        "    payload_available: \"online\"\r\n"
                        "    payload_not_available: \"offline\"\r\n"
                        "    busvoltage: \"%.2f\"\r\n"
                        "    current_mA: \"%.2f\"\r\n"
                        ),
                   UID, ch + 1, powerStatTopic, cmndTopic, availability.c_str(),busvoltage,current_mA);
        server->sendContent_P(html);
#ifdef USE_DIMMING
        if (dimming && ch >= dimming->pwmstartch)
        {
            dimming->httpHa(server, ch);
        }
#endif
        server->sendContent_P(PSTR("\r\n"));
    }
    server->sendContent("");
}
#pragma endregion

#pragma region Led

void Relay::ledTickerHandle()
{
    for (uint8_t ch = 0; ch < channels; ch++)
    {
        if (!bitRead(lastState, ch) && RELAY_LED_PIN[ch] != 99)
        {
            analogWrite(RELAY_LED_PIN[ch], ledLevel);
        }
    }
    if (ledUp)
    {
        ledLevel++;
        if (ledLevel >= ledLight)
        {
            ledUp = false;
        }
    }
    else
    {
        ledLevel--;
        if (ledLevel <= 50)
        {
            ledUp = true;
        }
    }
}

void Relay::ledPWM(uint8_t ch, bool isOn)
{
    if (isOn)
    {
        analogWrite(RELAY_LED_PIN[ch], 0);
        if (ledTicker.active())
        {
            for (uint8_t ch2 = 0; ch2 < channels; ch2++)
            {
                if (!bitRead(lastState, ch2))
                {
                    return;
                }
            }
            ledTicker.detach();
            // Log::Info(PSTR("ledTicker detach"));
        }
    }
    else
    {
        if (!ledTicker.active())
        {
            ledTicker.attach_ms(config.led_time, []()
                                { ((Relay *)module)->ledTickerHandle(); });
            // Log::Info(PSTR("ledTicker active"));
        }
    }
}

void Relay::led(uint8_t ch, bool isOn)
{
    if (config.led_type == 0 || RELAY_LED_PIN[ch] == 99)
    {
        return;
    }



    if (config.led_type == 1)
    {
        
        digitalWrite(RELAY_LED_PIN[ch], isOn ? LOW : HIGH);
     
        // Log::Info(PSTR("Lin: 934,ch: %d,isOn: %d."), ch, isOn);
        analogWrite(RELAY_LED_BACKLIGHT_PIN, ledBackLight);
    }
    else if (config.led_type == 2)
    {
        ledPWM(ch, isOn);
    }
}

bool Relay::checkCanLed(bool re)
{
    bool result;
    if (config.led_start != config.led_end && Rtc::rtcTime.valid)  //判断指示灯时间段，如果开始时间等于结束时间为指示灯常亮
    {
        uint16_t nowTime = Rtc::rtcTime.hour * 100 + Rtc::rtcTime.minute;
        if (config.led_start > config.led_end) // 开始时间大于结束时间 跨日
        {
            result = (nowTime >= config.led_start || nowTime < config.led_end);
        }
        else
        {
            result = (nowTime >= config.led_start && nowTime < config.led_end);
        }
    }
    else
    {
        result = true; // 没有正确时间为一直亮
    }
    if (result != Relay::canLed || re)
    {
        if ((!result || config.led_type != 2) && ledTicker.active())
        {
            ledTicker.detach();
            // Log::Info(PSTR("ledTicker detach2"));
        }
        Relay::canLed = result;
        Log::Info(result ? PSTR("led can light") : PSTR("led can not light"));

        for (uint8_t ch = 0; ch < channels; ch++)
        {

            if (RELAY_LED_PIN[ch] != 99)
            {
                result && config.led_type != 0 ? led(ch, bitRead(lastState, ch)) : analogWrite(RELAY_LED_PIN[ch], 0);
                Log::Info(PSTR("line: 1005, result: %d,config.led_type: %d."), result , config.led_type);
            }
        }
    }

    return result;
}
#pragma endregion

void Relay::switchRelay(uint8_t ch, bool isOn, bool isSave)
{
    if (ch > channels)
    {
        Log::Info(PSTR("invalid channel: %d"), ch);
        return;
    }

    if (isOn && config.power_mode == 1)
    {
        for (size_t ch2 = 0; ch2 < channels; ch2++)
        {
            if (ch2 != ch && bitRead(lastState, ch2))
            {
                switchRelay(ch2, false, isSave);
            }
        }
    }

    bitWrite(lastState, ch, isOn);
#ifdef USE_DIMMING
    if (dimming && ch >= dimming->pwmstartch)
    {
        dimming->switchRelayPWM(ch, isOn, isSave);
    }
    else
    {
#endif
        Log::Info(PSTR("Relay %d . . . %s"), ch + 1, isOn ? "ON" : "OFF");
        digitalWrite(RELAY_PIN[ch], isOn ? HIGH : LOW);
#ifdef USE_DIMMING
    }
#endif
    reportChannel(ch);

    if (isSave && config.power_on_state > 0)
    {
        bitWrite(config.last_state, ch, isOn);
        Config::delaySaveConfig(10);
    }
    if (Relay::canLed)
    {
        led(ch, isOn);
    }

    if (isOn && config.power_mode == 2)
    {
        delay(100);
        switchRelay(ch, !isOn, isSave);
    }
}

void Relay::checkButton(uint8_t ch)
{
    if (BOTTON_PIN[ch] == 99)
    {
        return;
    }


    bool currentState = digitalRead(BOTTON_PIN[ch]);

    if (currentState != ((buttonStateFlag[ch] & UNSTABLE_STATE) != 0))
    {
        buttonTimingStart[ch] = millis();
        buttonStateFlag[ch] ^= UNSTABLE_STATE;
    }
    else if (millis() - buttonTimingStart[ch] >= BUTTON_DEBOUNCE_TIME)
    {
#ifdef USE_DIMMING
        if (dimming && ch >= dimming->pwmstartch && !currentState && bitRead(dimmingState[ch - dimming->pwmstartch], 1)) // 如果低电平 并且进入调光模式
        {
            uint8_t pwmch = ch - dimming->pwmstartch;
            if (millis() - 100 > lastTime[ch])
            {
                if (bitRead(dimmingState[pwmch], 0))
                {
                    if (config.brightness[pwmch] > 2)
                    {
                        config.brightness[pwmch]--;
                    }
                }
                else
                {
                    if (config.brightness[pwmch] < 100)
                    {
                        config.brightness[pwmch]++;
                    }
                }
                //Log::Info(PSTR("brightness %d : %d"), ch + 1, config.brightness[pwmch]);
                switchRelay(ch, true, true);
                lastTime[ch] = millis();
                bitSet(dimmingState[pwmch], 2);
            }
        }
#endif

        if (currentState != ((buttonStateFlag[ch] & DEBOUNCED_STATE) != 0))
        {
            buttonTimingStart[ch] = millis();
            buttonStateFlag[ch] ^= DEBOUNCED_STATE;

            switchCount[ch] += 1;
            buttonIntervalStart[ch] = millis();

#ifdef USE_DIMMING
            if (dimming && ch >= dimming->pwmstartch)
            {
                uint8_t pwmch = ch - dimming->pwmstartch;
                if (!currentState) // 如果未开灯，低电平亮， 如果已开灯，高电平关
                {
                    if (!bitRead(lastState, ch))
                    {
                        if (PWM_TEMPERATURE_PIN[pwmch] != 99 && millis() - colorOffTime[pwmch] < (5 * 1000)) // 关灯少于5秒
                        {
                            config.color_index[pwmch] = (config.color_index[pwmch] + 1) % 3;
                            if (config.color_index[pwmch] == 1)
                            {
                                config.color_temp[pwmch] = 500;
                            }
                            else if (config.color_index[pwmch] == 2)
                            {
                                config.color_temp[pwmch] = 153;
                            }
                            else
                            {
                                config.color_temp[pwmch] = 326;
                            }
                        }
                        switchRelay(ch, true, true);
                        lastTime[ch] = 2;
                    }
                    else
                    {
                        if (config.brightness[pwmch] >= 100) // 切换调光模式
                        {
                            bitSet(dimmingState[pwmch], 0);
                        }
                        if (config.brightness[pwmch] <= 10)
                        {
                            bitClear(dimmingState[pwmch], 0);
                        }

                        lastTime[ch] = millis() + 900; // 首次1000毫秒才进入调光模式
                        //bitClear(dimmingState[pwmch], 0); // 每次进入亮度增加模式
                        bitSet(dimmingState[pwmch], 1);   // 调光模式
                        bitClear(dimmingState[pwmch], 2); // 重置为未调光状态
                    }
                }
                else
                {
                    if (bitRead(dimmingState[pwmch], 2)) // 进入调光模式就进行切换
                    {
                        bitWrite(dimmingState[pwmch], 0, !bitRead(dimmingState[pwmch], 0));
                    }
                    if (bitRead(dimmingState[pwmch], 1) && !bitRead(dimmingState[pwmch], 2))
                    {
                        switchRelay(ch, false, true);
                        colorOffTime[pwmch] = millis();
                        lastTime[ch] = millis();
                    }
                    bitClear(dimmingState[pwmch], 1);
                    bitClear(dimmingState[pwmch], 2);
                }
            }
            else if (dimming && ch >= dimming->pwmstartch && ROT_PIN[0] != 99)
            {
                if (bitRead(lastState, ch) && lastTime[ch] == 0)
                {
                    if (currentState && !dimming->RotaryButtonPressed())
                    {
                        switchRelay(ch, !bitRead(lastState, ch), true);
                    }
                }
                else if (!currentState)
                {
                    switchRelay(ch, !bitRead(lastState, ch), true);
                    lastTime[ch] = 1;
                }
                else
                {
                    lastTime[ch] = 0;
                }
            }
            else
#endif
                if (config.switch_mode == 1)
            {
                if (!currentState)
                {
                    switchRelay(ch, !bitRead(lastState, ch), true);
                }
            }
            else if (config.switch_mode == 2)
            {
                switchRelay(ch, !bitRead(lastState, ch), true);
            }
            else
            {
                if (millis() - lastTime[ch] > 300)
                {
                    switchRelay(ch, !bitRead(lastState, ch), true);
                    lastTime[ch] = millis();
                }
            }
        }
    }

    // 如果经过的时间大于超时并且计数大于0，则填充并重置计数
    if (switchCount[ch] > 0 && (millis() - buttonIntervalStart[ch]) > specialFunctionTimeout)
    {
        Led::led(200);
        Log::Info(PSTR("switchCount %d : %d"), ch + 1, switchCount[ch]);

        if (switchCount[ch] >= 20)
        {
            WifiMgr::setupWifiManager(false);
        }
        switchCount[ch] = 0;
    }
}

void Relay::loadModule(uint8_t module)
{
    for (size_t i = 0; i < MAX_RELAY_NUM; i++)
    {
        RELAY_PIN[i] = 99;
        BOTTON_PIN[i] = 99;
        RELAY_LED_PIN[i] = 99;
    }

#ifdef USE_DIMMING
    for (size_t i = 0; i < MAX_PWM_NUM; i++)
    {
        PWM_BRIGHTNESS_PIN[i] = 99;
        PWM_TEMPERATURE_PIN[i] = 99;
        BOTTON_PIN[i + MAX_RELAY_NUM] = 99;
        RELAY_LED_PIN[i + MAX_RELAY_NUM] = 99;
    }
    ROT_PIN[0] = 99;
    ROT_PIN[1] = 99;
#endif

    mytmplt m;
    memcpy_P(&m, &Modules[module], sizeof(m));

    uint8_t pos = 0;
    uint8_t t, l, v;
    while (true)
    {
        t = m.io[pos++];
        if (t < 1 || t > 9)
        {
            break;
        }
        l = m.io[pos++];
        if (l > 16)
        {
            break;
        }
        for (size_t i = 0; i < l; i++)
        {
            switch (t)
            {
            case 1:
                LED_PIN = m.io[pos++];
                // Log::Info(PSTR("LED_PIN %d"), LED_PIN);
                break;
            case 2:
                RELAY_PIN[i] = m.io[pos++];
                // Log::Info(PSTR("RELAY_PIN %d"), RELAY_PIN[i]);
                break;
            case 3:
                BOTTON_PIN[i] = m.io[pos++];
                // Log::Info(PSTR("BOTTON_PIN %d"), BOTTON_PIN[i]);
                break;
            case 4:
                RELAY_LED_PIN[i] = m.io[pos++];
                // Log::Info(PSTR("RELAY_LED_PIN %d"), RELAY_LED_PIN[i]);
                break;
            case 5:
                RFRECV_PIN = m.io[pos++];
                // Log::Info(PSTR("RFRECV_PIN %d"), RFRECV_PIN);
                break;
            case 9:
                RELAY_LED_BACKLIGHT_PIN = m.io[pos++];
                // Log::Info(PSTR("RELAY_LED_BACKLIGHT_PIN %d"), RELAY_LED_BACKLIGHT_PIN);
                break;
#ifdef USE_DIMMING
            case 6:
                PWM_BRIGHTNESS_PIN[i] = m.io[pos++];
                //Log::Info(PSTR("PWM_BRIGHTNESS_PIN %d"), PWM_BRIGHTNESS_PIN[i]);
                break;
            case 7:
                PWM_TEMPERATURE_PIN[i] = m.io[pos++];
                //Log::Info(PSTR("PWM_TEMPERATURE_PIN %d"), PWM_TEMPERATURE_PIN[i]);
                break;
            case 8:
                ROT_PIN[i] = m.io[pos++];
                //Log::Info(PSTR("ROT_PIN %d"), ROT_PIN[i]);
                break;
#endif
            default:
                m.io[pos++];
                break;
            }
        }
    }
}

void Relay::reportPower()
{
    for (size_t ch = 0; ch < channels; ch++)
    {
        reportChannel(ch);
    }
}

void Relay::reportChannel(uint8_t ch)
{
    if (!bitRead(Config::statusFlag, 1))
    {
        return;
    }
    powerStatTopic[strlen(powerStatTopic) - 1] = ch + 49; // 48 + 1 + ch
    Mqtt::publish(powerStatTopic, bitRead(lastState, ch) ? "on" : "off", globalConfig.mqtt.retain);

#ifdef USE_DIMMING
    if (dimming && ch >= dimming->pwmstartch)
    {
        brightnessStatTopic[strlen(brightnessStatTopic) - 1] = ch + 49; // 48 + 1 + ch
        uint8_t pwmch = ch - dimming->pwmstartch;
        Mqtt::publish(brightnessStatTopic, String(config.brightness[pwmch]).c_str(), globalConfig.mqtt.retain);
        if (PWM_TEMPERATURE_PIN[pwmch] != 99)
        {
            color_tempStatTopic[strlen(color_tempStatTopic) - 1] = ch + 49; // 48 + 1 + ch
            Mqtt::publish(color_tempStatTopic, String(config.color_temp[pwmch]).c_str(), globalConfig.mqtt.retain);
        }
    }
#endif
}
