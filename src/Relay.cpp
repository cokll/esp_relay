#include "Relay.h"
#include "RadioReceive.h"
#include "Rtc.h"
#ifdef USE_HOMEKIT
#include "HomeKit.h"
#endif
#include <ESP8266Ping.h>

#pragma region 继承

String Relay::getModuleCNName()
{
    return String(channels) + F("路开关模块");
}

void Relay::init()
{
    loadModule(config.module_type);
    if (GPIO_PIN[GPIO_LED_POWER] != 99)
    {
        Led::init(GPIO_PIN[GPIO_LED_POWER], HIGH);
    }
    else if (GPIO_PIN[GPIO_LED_POWER_INV] != 99)
    {
        Led::init(GPIO_PIN[GPIO_LED_POWER_INV], LOW);
    }

#ifdef USE_RCSWITCH
    if (GPIO_PIN[GPIO_RFRECV] != 99)
    {
        radioReceive = new RadioReceive();
        radioReceive->init(this, GPIO_PIN[GPIO_RFRECV]);
    }
#endif

    channels = 0;
    for (uint8_t ch = 0; ch < 4; ch++)
    {
        if (GPIO_PIN[GPIO_REL1 + ch] == 99)
        {
            continue;
        }
        channels++;

        pinMode(GPIO_PIN[GPIO_REL1 + ch], OUTPUT); // 继电器
        if (GPIO_PIN[GPIO_LED1 + ch] != 99)
        {
            pinMode(GPIO_PIN[GPIO_LED1 + ch], OUTPUT); // LED
        }
    }

    strcpy(powerStatTopic, Mqtt::getStatTopic(F("power1")).c_str());

    for (uint8_t ch = 0; ch < channels; ch++)
    {
        if (GPIO_PIN[GPIO_KEY1 + ch] != 99)
        {
            pinMode(GPIO_PIN[GPIO_KEY1 + ch], INPUT_PULLUP);
            if (digitalRead(GPIO_PIN[GPIO_KEY1 + ch]))
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

    if (WiFi.status() == WL_CONNECTED && Mqtt::mqttClient.connected())
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
    for (size_t ch = 0; ch < channels; ch++)
    {
        cheackButton(ch);
    }

#ifdef USE_RCSWITCH
    if (radioReceive)
    {
        radioReceive->loop();
    }
#endif

    if (bitRead(operationFlag, 0))
    {
        bitClear(operationFlag, 0);
        if (perSecond % 60 == 0)
        {
            checkCanLed();
        }
        if (config.report_interval > 0 && (perSecond % config.report_interval) == 0)
        {
            reportPower();
        }
    }

    

}

void Relay::perSecondDo()
{
    bitSet(operationFlag, 0);
}
#pragma endregion

#pragma region 配置

void Relay::readConfig()
{
    Config::moduleReadConfig(MODULE_CFG_VERSION, sizeof(RelayConfigMessage), RelayConfigMessage_fields, &config);
    if (config.led_light == 0)
    {
        config.led_light = 100;
    }
    if (config.led_time == 0)
    {
        config.led_time = 2;
    }
    ledLight = config.led_light * 10 + 23;

    if (config.module_type >= MAXMODULE)
    {
        config.module_type = SONOFF_BASIC;
    }
}

void Relay::resetConfig()
{
    Debug::AddInfo(PSTR("moduleResetConfig . . . OK"));
    memset(&config, 0, sizeof(RelayConfigMessage));
    config.module_type = SupportedModules::SONOFF_BASIC;
    config.led_light = 50;
    config.led_time = 3;
}

void Relay::saveConfig(bool isEverySecond)
{
    Config::moduleSaveConfig(MODULE_CFG_VERSION, RelayConfigMessage_size, RelayConfigMessage_fields, &config);
}
#pragma endregion

#pragma region MQTT

void Relay::mqttCallback(char *topic, char *payload, char *cmnd)
{
    if (strlen(cmnd) == 6 && strncmp(cmnd, "power", 5) == 0) // strlen("power1") = 6
    {
        uint8_t ch = cmnd[5] - 49;
        if (ch < channels)
        {
            switchRelay(ch, (strcmp(payload, "on") == 0 ? true : (strcmp(payload, "off") == 0 ? false : !bitRead(lastState, ch))), true);
            return;
        }
    }
    else if (strcmp(cmnd, "report") == 0)
    {
        reportPower();
    }
}

void Relay::mqttConnected()
{
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
                                  "\"value_template\":\"{{ value_json.state }}\","
                                  "\"payload_off\":\"OFF\","
                                  "\"payload_on\":\"ON\","
                                  "\"command_topic\":\"%s\","
                                  "\"state_topic\":\"%s\","
                                  "\"availability_topic\":\"%s\","
                                  "\"payload_available\":\"online\","
                                  "\"payload_not_available\":\"offline\","
                                  "\"unique_id\":\"esp_relay_%s_%d\","
                                  "\"device\":{"
                                  "\"identifiers\":\"%s\","
                                  "\"name\":\"%s\","
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

void Relay::httpAdd(ESP8266WebServer *server)
{
    server->on(F("/relay_do"), std::bind(&Relay::httpDo, this, server));
    server->on(F("/relay_setting"), std::bind(&Relay::httpSetting, this, server));
    server->on(F("/ha"), std::bind(&Relay::httpHa, this, server));
#ifdef USE_RCSWITCH
    server->on(F("/rf_do"), std::bind(&Relay::httpRadioReceive, this, server));
#endif
#ifdef USE_HOMEKIT
    server->on(F("/homekit"), std::bind(&homekit_http, server));
#endif
}

String Relay::httpGetStatus(ESP8266WebServer *server)
{
    String data;
    for (size_t ch = 0; ch < channels; ch++)
    {
        data += ",\"relay_" + String(ch + 1) + "\":";
        data += bitRead(lastState, ch) ? 1 : 0;
    }
    return data.substring(1);
}

void Relay::httpHtml(ESP8266WebServer *server)
{
    server->sendContent_P(
        PSTR("<table class='gridtable'><thead><tr><th colspan='2'>开关状态</th></tr></thead><tbody>"
             "<tr style='text-align:center'><td colspan='2'>"));

    for (size_t ch = 0; ch < channels; ch++)
    {
        snprintf_P(tmpData, sizeof(tmpData),
                   PSTR(" <button type='button' style='width:50px' onclick=\"ajaxPost('/relay_do', 'do=T&c=%d');\" id='relay_%d' class='btn-%s'>%s</button>"),
                   ch + 1, ch + 1,
                   bitRead(lastState, ch) ? PSTR("success") : PSTR("info"),
                   bitRead(lastState, ch) ? PSTR("开") : PSTR("关"));
        server->sendContent_P(tmpData);
    }

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
            snprintf_P(tmpData, sizeof(tmpData), PSTR("<option value='%d'>%s</option>"), count, Modules[count].name);
            server->sendContent_P(tmpData);
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

    server->sendContent_P(
        PSTR("<tr><td>开关模式</td><td>"
             "<label class='bui-radios-label'><input type='radio' name='power_mode' value='0'/><i class='bui-radios'></i> 自锁</label>&nbsp;&nbsp;&nbsp;&nbsp;"
             "<label class='bui-radios-label'><input type='radio' name='power_mode' value='1'/><i class='bui-radios'></i> 互锁</label>"
             "</td></tr>"));

    server->sendContent_P(
        PSTR("<tr><td>开关类型</td><td>"
             "<label class='bui-radios-label'><input type='radio' name='switch_mode' value='0'/><i class='bui-radios'></i> 自动</label>&nbsp;&nbsp;&nbsp;&nbsp;"
             "<label class='bui-radios-label'><input type='radio' name='switch_mode' value='1'/><i class='bui-radios'></i> 自复位开关</label>&nbsp;&nbsp;&nbsp;&nbsp;"
             "<label class='bui-radios-label'><input type='radio' name='switch_mode' value='2'/><i class='bui-radios'></i> 传统开关</label>"
             "</td></tr>"));

    if (GPIO_PIN[GPIO_LED1] != 99)
    {
        server->sendContent_P(
            PSTR("<tr><td>面板指示灯</td><td>"
                 "<label class='bui-radios-label'><input type='radio' name='led_type' value='0'/><i class='bui-radios'></i> 无</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='led_type' value='1'/><i class='bui-radios'></i> 普通</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='led_type' value='2'/><i class='bui-radios'></i> 呼吸灯</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 //"<label class='bui-radios-label'><input type='radio' name='led_type' value='3'/><i class='bui-radios'></i> WS2812</label>"
                 "</td></tr>"));

        snprintf_P(tmpData, sizeof(tmpData),
                   PSTR("<tr><td>指示灯亮度</td><td><input type='range' min='1' max='100' name='led_light' value='%d' onchange='ledLightRangOnChange(this)'/>&nbsp;<span>%d%</span></td></tr>"),
                   config.led_light, config.led_light);
        server->sendContent_P(tmpData);

        snprintf_P(tmpData, sizeof(tmpData),
                   PSTR("<tr><td>渐变时间</td><td><input type='number' name='relay_led_time' value='%d'>毫秒</td></tr>"),
                   config.led_time);
        server->sendContent_P(tmpData);

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

    if (GPIO_PIN[GPIO_LED_POWER] != 99 || GPIO_PIN[GPIO_LED_POWER_INV] != 99)
    {
        server->sendContent_P(
            PSTR("<tr><td>LED</td><td>"
                 "<label class='bui-radios-label'><input type='radio' name='led' value='0'/><i class='bui-radios'></i> 常亮</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='led' value='1'/><i class='bui-radios'></i> 常灭</label>&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<label class='bui-radios-label'><input type='radio' name='led' value='2'/><i class='bui-radios'></i> 闪烁</label><br>未连接WIFI或者MQTT时为快闪"
                 "</td></tr>"));
    }

    snprintf_P(tmpData, sizeof(tmpData),
               PSTR("<tr><td>主动上报间隔</td><td><input type='number' min='0' max='3600' name='report_interval' required value='%d'>&nbsp;秒，0关闭</td></tr>"),
               config.report_interval);
    server->sendContent_P(tmpData);

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
            snprintf_P(tmpData, sizeof(tmpData),
                       PSTR(" <button type='button' style='width:60px' onclick=\"ajaxPost('/rf_do', 'do=s&c=%d')\" class='btn-success'>%d路</button>"),
                       ch + 1, ch + 1);
            server->sendContent_P(tmpData);
        }

        server->sendContent_P(PSTR("</td></tr>"
                                   "<tr><td>删除模式</td><td>"));
        for (size_t ch = 0; ch < channels; ch++)
        {
            snprintf_P(tmpData, sizeof(tmpData),
                       PSTR(" <button type='button' style='width:60px' onclick=\"ajaxPost('/rf_do', 'do=d&c=%d')\" class='btn-info'>%d路</button>"),
                       ch + 1, ch + 1);
            server->sendContent_P(tmpData);
        }

        server->sendContent_P(
            PSTR("</td></tr>"
                 "<tr><td>全部删除</td><td>"));
        for (size_t ch = 0; ch < channels; ch++)
        {
            snprintf_P(tmpData, sizeof(tmpData),
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
             "function setDataSub(data,key){if(key.substr(0,5)=='relay'){var t=id(key);var v=data[key];t.setAttribute('class',v==1?'btn-success':'btn-info');t.innerHTML=v==1?'开':'关';return true}return false}"));

    snprintf_P(tmpData, sizeof(tmpData), PSTR("id('module_type').value=%d;setRadioValue('power_on_state', '%d');setRadioValue('power_mode', '%d');setRadioValue('switch_mode', '%d');"),
               config.module_type, config.power_on_state, config.power_mode, config.switch_mode);
    server->sendContent_P(tmpData);

    if (GPIO_PIN[GPIO_LED1] != 99)
    {
        snprintf_P(tmpData, sizeof(tmpData), PSTR("setRadioValue('led_type', '%d');id('led_start').value=%d;id('led_end').value=%d;"),
                   config.led_type, config.led_start, config.led_end);
        server->sendContent_P(tmpData);
        server->sendContent_P(PSTR("function ledLightRangOnChange(the){the.nextSibling.nextSibling.innerHTML=the.value+'%'};"));
    }

    if (GPIO_PIN[GPIO_LED_POWER] != 99 || GPIO_PIN[GPIO_LED_POWER_INV] != 99)
    {
        snprintf_P(tmpData, sizeof(tmpData), PSTR("setRadioValue('led', '%d');"), config.led);
        server->sendContent_P(tmpData);
    }
    server->sendContent_P(PSTR("</script>"));
}

void Relay::httpDo(ESP8266WebServer *server)
{
    String c = server->arg(F("c"));
    if (c != F("1") && c != F("2") && c != F("3") && c != F("4"))
    {
        server->send_P(200, PSTR("application/json"), PSTR("{\"code\":0,\"msg\":\"参数错误。\"}"));
        return;
    }
    uint8_t ch = c.toInt() - 1;
    if (ch > channels)
    {
        server->send_P(200, PSTR("application/json"), PSTR("{\"code\":0,\"msg\":\"继电器数量错误。\"}"));
        return;
    }
    String str = server->arg(F("do"));
    switchRelay(ch, (str == F("on") ? true : (str == F("off") ? false : !bitRead(lastState, ch))));

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send_P(200, PSTR("application/json"), PSTR("{\"code\":1,\"msg\":\"操作成功\",\"data\":{"));
    server->sendContent(httpGetStatus(server));
    server->sendContent_P(PSTR("}}"));
}

#ifdef USE_RCSWITCH
void Relay::httpRadioReceive(ESP8266WebServer *server)
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

void Relay::httpSetting(ESP8266WebServer *server)
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
        ledLight = config.led_light * 10 + 23;
    }
    if (server->hasArg(F("relay_led_time")))
    {
        config.led_time = server->arg(F("relay_led_time")).toInt();
        if (config.led_type == 2 && ledTicker.active())
        {
            ledTicker.detach();
        }
    }
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

void Relay::httpHa(ESP8266WebServer *server)
{
    char attachment[100];
    snprintf_P(attachment, sizeof(attachment), PSTR("attachment; filename=%s.yaml"), UID);

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->sendHeader(F("Content-Disposition"), attachment);
    server->send_P(200, PSTR("Content-Type: application/octet-stream"), "");

    String availability = Mqtt::getTeleTopic(F("availability"));
    char cmndTopic[100];
    strcpy(cmndTopic, Mqtt::getCmndTopic(F("power1")).c_str());
    server->sendContent_P(PSTR("light:\r\n"));
    for (size_t ch = 0; ch < channels; ch++)
    {
        cmndTopic[strlen(cmndTopic) - 1] = ch + 49;           // 48 + 1 + ch
        powerStatTopic[strlen(powerStatTopic) - 1] = ch + 49; // 48 + 1 + ch

        snprintf_P(tmpData, sizeof(tmpData),
                   PSTR("  - platform: mqtt\r\n"
                        "    name: \"%s_l%d\"\r\n"
                        "    state_topic: \"%s\"\r\n"
                        "    command_topic: \"%s\"\r\n"
                        "    payload_on: \"on\"\r\n"
                        "    payload_off: \"off\"\r\n"
                        "    availability_topic: \"%s\"\r\n"
                        "    payload_available: \"online\"\r\n"
                        "    payload_not_available: \"offline\"\r\n\r\n"),
                   UID, ch + 1, powerStatTopic, cmndTopic, availability.c_str());
        server->sendContent_P(tmpData);
    }
}
#pragma endregion

#pragma region Led

void Relay::ledTickerHandle()
{
    for (uint8_t ch = 0; ch < channels; ch++)
    {
        if (!bitRead(lastState, ch) && GPIO_PIN[GPIO_LED1 + ch] != 99)
        {
            analogWrite(GPIO_PIN[GPIO_LED1 + ch], ledLevel);
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
        analogWrite(GPIO_PIN[GPIO_LED1 + ch], 0);
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
            Debug::AddInfo(PSTR("ledTicker detach"));
        }
    }
    else
    {
        if (!ledTicker.active())
        {
            ledTicker.attach_ms(config.led_time, []() { ((Relay *)module)->ledTickerHandle(); });
            Debug::AddInfo(PSTR("ledTicker active"));
        }
    }
}

void Relay::led(uint8_t ch, bool isOn)
{
    if (config.led_type == 0 || GPIO_PIN[GPIO_LED1 + ch] == 99)
    {
        return;
    }

    if (config.led_type == 1)
    {
        //digitalWrite(GPIO_PIN[GPIO_LED1 + ch], isOn ? LOW : HIGH);
        analogWrite(GPIO_PIN[GPIO_LED1 + ch], isOn ? 0 : ledLight);
    }
    else if (config.led_type == 2)
    {
        ledPWM(ch, isOn);
    }
}

bool Relay::checkCanLed(bool re)
{
    bool result;
    if (config.led_start != config.led_end && Rtc::rtcTime.valid)
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
            Debug::AddInfo(PSTR("ledTicker detach2"));
        }
        Relay::canLed = result;
        Debug::AddInfo(result ? PSTR("led can light") : PSTR("led can not light"));
        for (uint8_t ch = 0; ch < channels; ch++)
        {
            if (GPIO_PIN[GPIO_LED1 + ch] != 99)
            {
                result &&config.led_type != 0 ? led(ch, bitRead(lastState, ch)) : analogWrite(GPIO_PIN[GPIO_LED1 + ch], 0);
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
        Debug::AddInfo(PSTR("invalid channel: %d"), ch);
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
    Debug::AddInfo(PSTR("Relay %d . . . %s"), ch + 1, isOn ? "ON" : "OFF");
    digitalWrite(GPIO_PIN[GPIO_REL1 + ch], isOn ? HIGH : LOW);

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
}


void Relay::cheackButton(uint8_t ch)
{
    if (GPIO_PIN[GPIO_KEY1 + ch] == 99)
    {
        return;
    }
    bool currentState = digitalRead(GPIO_PIN[GPIO_KEY1 + ch]);
    if (currentState != ((buttonStateFlag[ch] & UNSTABLE_STATE) != 0))
    {
        buttonTimingStart[ch] = millis();
        buttonStateFlag[ch] ^= UNSTABLE_STATE;
    }
    else if (millis() - buttonTimingStart[ch] >= BUTTON_DEBOUNCE_TIME)
    {
        if (currentState != ((buttonStateFlag[ch] & DEBOUNCED_STATE) != 0))
        {
            buttonTimingStart[ch] = millis();
            buttonStateFlag[ch] ^= DEBOUNCED_STATE;

            switchCount[ch] += 1;
            buttonIntervalStart[ch] = millis();

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
        Debug::AddInfo(PSTR("switchCount %d : %d"), ch + 1, switchCount[ch]);

#ifdef USE_RCSWITCH
        if (switchCount[ch] == 10 && radioReceive)
        {
            radioReceive->study(ch);
        }
        else if (switchCount[ch] == 12 && radioReceive)
        {
            radioReceive->del(ch);
        }
        else if (switchCount[ch] == 16 && radioReceive)
        {
            radioReceive->delAll();
        }
#endif
        if (switchCount[ch] == 20)
        {
            Wifi::setupWifiManager(false);
        }
        switchCount[ch] = 0;
    }
    /*
    String c = "2";
    ch = c.toInt() - 1;

    if (Ping.ping(target)) {
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
    */


}

void Relay::loadModule(uint8_t module)
{
    for (uint16_t i = 0; i < GPIO_MAX; i++)
    {
        GPIO_PIN[i] = 99;
    }

    mytmplt m;
    memcpy_P(&m, &Modules[module], sizeof(m));

    uint8_t j = 0;
    for (uint8_t i = 0; i < sizeof(m.io); i++)
    {
        if (6 == i)
        {
            j = 9;
        }
        if (8 == i)
        {
            j = 12;
        }
        GPIO_PIN[m.io[i]] = j;
        j++;
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
    powerStatTopic[strlen(powerStatTopic) - 1] = ch + 49; // 48 + 1 + ch
    Mqtt::publish(powerStatTopic, bitRead(lastState, ch) ? "on" : "off", globalConfig.mqtt.retain);

}
