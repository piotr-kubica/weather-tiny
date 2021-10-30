// ----------------------------------
// DISPLAY --------------------------
// ----------------------------------
#include <SPI.h>
#include <GxEPD.h>

#include <GxGDE0213B72B/GxGDE0213B72B.h> // 2.13" b/w
// #include <GxDEPG0213BN/GxDEPG0213BN.h>  // 2.13" b/w newer panel

#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#define SPI_MOSI 23
#define SPI_MISO -1
#define SPI_CLK 18
#define ELINK_SS 5
#define ELINK_BUSY 4
#define ELINK_RESET 16
#define ELINK_DC 17

GxIO_Class io(SPI, ELINK_SS, ELINK_DC, ELINK_RESET);
GxEPD_Class display(io, ELINK_RESET, ELINK_BUSY);

#define SCREEN_WIDTH   250
#define SCREEN_HEIGHT  122

// ----------------------------------
// LIBS -----------------------------
// ----------------------------------
#define ARDUINOJSON_USE_DOUBLE 1

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <rom/rtc.h> 
#include <Preferences.h>

#define ADC_PIN 35
#define WAKE_BTN_PIN 39

// ----------------------------------
// FONTS ----------------------------
// ----------------------------------

#include <Fonts/Monofonto10pt.h>
#include <Fonts/Monofonto12pt.h>
#include <Fonts/Monofonto18pt.h>
#include <Fonts/MeteoCons8pt.h>
#include <Fonts/MeteoCons10pt.h>
#include <Fonts/Cousine6pt.h>

// ----------------------------------
// LOCAL FILES AND DECLARATIONS -----
// ----------------------------------

#include "fmt.h"
#include "i18n.h"
#include "config.h"
#include "units.h"
#include "api_request.h"
#include "display.h"
#include "view.h"

#define MEMORY_ID "mem"
#define LOC_MEMORY_ID "loc"

#define NOT_SET_MODE 0
#define CONFIG_MODE 1
#define VALIDATING_MODE 2
#define OPERATING_MODE 3

// http server for obtaining configuration
DNSServer dnsServer;
AsyncWebServer server(80);
Preferences preferences;

int cached_MODE = 0;
int curr_loc = 0;

struct WeatherRequest weather_request;
struct AirQualityRequest airquality_request;
struct GeocodingNominatimRequest location_request;
struct TimeZoneDbRequest datetime_request;

int location_cnt = 0;
struct Location location[2];
struct WifiCredentials wifi;
struct View view;

int get_mode(bool cached_mode=false);
DynamicJsonDocument deserialize(WiFiClient& resp_stream, const int size, bool is_embeded=false);


// ----------------------------------
// FUNCTION DEFINITIONS -------------
// ----------------------------------

template<typename T>
T value_or_default(JsonObject jobj, String key, T default_value) {
    if (!jobj.containsKey(key)) {
        return default_value;
    } else {
        return jobj[key];
    }
}


template<typename T>
T nested_value_or_default(JsonObject parent_jobj, String key, String nested_key, T default_value) {
    if (!parent_jobj.containsKey(key)) {
        return default_value;
    } else {
        return parent_jobj[key][nested_key];
    }
}


void update_header_view(View& view, bool data_updated) {
    view.location = location[curr_loc].name.substring(0,7);
    view.battery_percent = get_battery_percent(analogRead(ADC_PIN));
    int percent_display = view.battery_percent;
    
    if (percent_display > 100) {
        percent_display = 100;
    }
    view.battery_percent_display = String(percent_display) + "%";
    view.datetime = header_datetime(&datetime_request.response.dt, data_updated);
}


void update_air_quality_view(View& view, bool data_updated) {
    if (!data_updated) {
        return;
    }
    view.aq_pm25 = left_pad(String(airquality_request.response.pm25), 3);
    view.aq_pm25_unit = "PM2.5";
}


void update_weather_view(View& view, bool data_updated) {
    if (!data_updated) {
        return;
    }
    view.weather_icon = String(icon2meteo_font(weather_request.hourly[0].icon));
    
    String descr = weather_request.hourly[0].descr;
    view.weather_desc = capitalize(descr);
    
    view.temp_curr = left_pad(String(weather_request.hourly[0].temp), 3);
    view.temp_unit = '*';  // celsius
    view.temp_high = "Hi" + left_pad(String(weather_request.hourly[0].max_t), 3);
    view.temp_low = "Lo" + left_pad(String(weather_request.hourly[0].min_t), 3);
    view.temp_feel = "Fl" + left_pad(String(weather_request.hourly[0].feel_t), 3);

    view.pressure = left_pad(String(weather_request.hourly[0].pressure), 4);
    view.pressure_unit = "hPa";

    view.wind = left_pad(String(weather_request.hourly[0].wind_bft), 2);
    view.wind_deg = weather_request.hourly[0].wind_deg;
    view.wind_unit = "Bft";

    view.percip_time_unit = "hour";
    view.percip_unit = "mm";
    view.percic_pop_unit = "%";

    for (int i = 0; i < PERCIP_SIZE; i++) {
        view.percip[i] = fmt_2f1(weather_request.rain[i].snow + weather_request.rain[i].rain);
        view.percip_time[i] = ts2H(weather_request.rain[i].date_ts + datetime_request.response.gmt_offset);
        view.percip_icon[i] = String(icon2meteo_font(weather_request.rain[i].icon));
        view.percic_pop[i] = left_pad(String(weather_request.rain[i].pop), 3);
    }
}


void update_location(GeocodingNominatimResponse& location_resp, JsonObject& jobj) {
    location_resp.lat = jobj["data"][0]["latitude"].as<float>();
    location_resp.lon = jobj["data"][0]["longitude"].as<float>();
    location_resp.label = String(jobj["data"][0]["label"].as<char*>()).substring(0, 25);
}


void update_datetime(TimeZoneDbResponse& datetime_resp, JsonObject& jobj) {
    datetime_resp.dt = jobj["timestamp"].as<int>();
    datetime_resp.gmt_offset = jobj["gmtOffset"].as<int>();
    datetime_resp.dst = jobj["dst"].as<int>();
}


void update_current_weather(WeatherResponseHourly& hourly, JsonObject& root) {
    hourly.date_ts = root["current"]["dt"].as<int>();
    hourly.sunr_ts = root["current"]["sunrise"].as<int>();
    hourly.suns_ts = root["current"]["sunset"].as<int>();
    hourly.temp = kelv2cels(root["current"]["temp"].as<float>());
    hourly.feel_t = kelv2cels(root["current"]["feels_like"].as<float>());
    hourly.max_t = kelv2cels(root["daily"][0]["temp"]["max"].as<float>());
    hourly.min_t = kelv2cels(root["daily"][0]["temp"]["min"].as<float>());
    hourly.pressure = root["current"]["pressure"].as<int>();
    hourly.clouds = root["current"]["clouds"].as<int>();
    hourly.wind_bft = wind_ms2bft(root["current"]["wind_speed"].as<float>());
    hourly.wind_deg = root["current"]["wind_deg"].as<int>();
    hourly.icon = String(root["current"]["weather"][0]["icon"].as<char*>()).substring(0, 2);
    hourly.descr = root["current"]["weather"][0]["description"].as<char*>();
    hourly.pop = round(root["hourly"][1]["pop"].as<float>() * 100);
    hourly.snow = value_or_default(root["current"], "snow", 0.0f);
    hourly.rain = value_or_default(root["current"], "rain", 0.0f);
}


void update_forecast_weather(WeatherResponseDaily& daily, JsonObject& root, const int day_offset) {
    daily.date_ts = root["daily"][day_offset]["dt"].as<int>();
    daily.max_t = kelv2cels(root["daily"][day_offset]["temp"]["max"].as<float>());
    daily.min_t = kelv2cels(root["daily"][day_offset]["temp"]["min"].as<float>());
    daily.wind_bft = wind_ms2bft(root["daily"][day_offset]["wind_speed"].as<float>());
    daily.wind_deg = root["daily"][day_offset]["wind_deg"].as<int>();
    daily.pop = round(root["daily"][day_offset]["pop"].as<float>() * 100);
    daily.snow = value_or_default(root["daily"][day_offset], "snow", 0.0f);
    daily.rain = value_or_default(root["daily"][day_offset], "rain", 0.0f);
}


void update_percip_forecast(WeatherResponseRainHourly& percip, JsonObject& root, const int hour_offset) {
    percip.date_ts = root["hourly"][hour_offset]["dt"].as<int>();
    percip.pop = round(root["hourly"][hour_offset]["pop"].as<float>() * 100);
    percip.snow = nested_value_or_default(root["hourly"][hour_offset], "snow", "1h", 0.0f);
    percip.rain = nested_value_or_default(root["hourly"][hour_offset], "rain", "1h", 0.0f);
    percip.icon = String(root["hourly"][hour_offset]["weather"][0]["icon"].as<char*>()).substring(0, 2);
}


bool location_handler(WiFiClient& resp_stream, Request request) {
    const int json_size = 20 * 1024;
    DynamicJsonDocument doc = deserialize(resp_stream, json_size, true);
    JsonObject api_resp = doc.as<JsonObject>();

    if (api_resp.isNull()) {
        return false;
    }
    location_request = GeocodingNominatimRequest(request);
    GeocodingNominatimResponse& location_resp = location_request.response;
    Serial.print("Geocoding...");
    update_location(location_resp, api_resp);
    location_resp.print();
    return true;
}


bool datetime_handler(WiFiClient& resp_stream, Request request) {
    const int json_size = 10 * 1024;
    DynamicJsonDocument doc = deserialize(resp_stream, json_size, true);
    JsonObject api_resp = doc.as<JsonObject>();

    if (api_resp.isNull()) {
        return false;
    }
    datetime_request = TimeZoneDbRequest(request);
    TimeZoneDbResponse& datetime_response = datetime_request.response;
    update_datetime(datetime_response, api_resp);
    datetime_response.print();
    return true;
}
    

bool weather_handler(WiFiClient& resp_stream, Request request) {
    const int json_size = 35 * 1024;
    DynamicJsonDocument doc = deserialize(resp_stream, json_size);
    JsonObject api_resp = doc.as<JsonObject>();

    if (api_resp.isNull()) {
        return false;
    }
    weather_request = WeatherRequest(request);
    WeatherResponseHourly& hourly = weather_request.hourly[0];
    WeatherResponseDaily& next_day = weather_request.daily[0];
    WeatherResponseDaily& second_next_day = weather_request.daily[1];

    update_current_weather(hourly, api_resp);
    hourly.print();
    update_forecast_weather(next_day, api_resp, 1);
    next_day.print();
    update_forecast_weather(second_next_day, api_resp, 2);
    second_next_day.print();

    for (int hour = 0; hour < 5; hour++) {
        int offset = hour + 1;
        update_percip_forecast(weather_request.rain[hour], api_resp, offset);
        weather_request.rain[hour].print();
    }
    return true;
}


bool air_quality_handler(WiFiClient& resp_stream, Request request) {
    const int json_size = 6 * 1024;
    DynamicJsonDocument doc = deserialize(resp_stream, json_size);
    JsonObject api_resp = doc.as<JsonObject>();
    
    if (api_resp.isNull()) {
        return false;
    }
    if (!String(api_resp["status"].as<char*>()).equals("ok")) {
        return false;
    }
    airquality_request = AirQualityRequest(request);
    
    if (api_resp["data"]["iaqi"].containsKey("pm25")) {
        airquality_request.response.pm25 = api_resp["data"]["iaqi"]["pm25"]["v"].as<int>();
    } else if (api_resp["data"]["forecast"]["daily"].containsKey("pm25")) {
        airquality_request.response.pm25 = api_resp["data"]["forecast"]["daily"]["pm25"][0]["max"].as<int>();
    }
    airquality_request.response.print();
    
    return true;
}


DynamicJsonDocument deserialize(WiFiClient& resp_stream, const int size, bool is_embeded) {
    // https://arduinojson.org/v6/assistant/
    Serial.print("\nDeserializing json, size:" + String(size) + " bytes...");
    DynamicJsonDocument doc(size);
    DeserializationError error;
    
    if (is_embeded) {
        String stream_as_string = resp_stream.readString();
        int begin = stream_as_string.indexOf('{');
        int end = stream_as_string.lastIndexOf('}');
        Serial.print("\nEmbeded json algorithm obtained document...\n");
        String trimmed_json = stream_as_string.substring(begin, end+1);
        Serial.println(trimmed_json);
        error = deserializeJson(doc, trimmed_json);
    } else {
        error = deserializeJson(doc, resp_stream);
    }
    if (error) {
        Serial.print(F("\ndeserialization error:"));
        Serial.println(error.c_str());
    } else {
        Serial.println("deserialized.");
    }
    Serial.println("");
    return doc;
}


int get_battery_percent(int adc_value) {
    float voltage = adc_value / 4095.0 * 7.5;
    Serial.println("Battery voltage ~" + String(voltage) + "V");
    if (voltage > 4.35) {
        return 101;  // charging / DC powered
    }
    if (voltage > 4.2) {
        return 100;
    } 
    if (voltage < 3.3) {
        return 1;
    }
    int percent = (int)((voltage - 3.3)/(4.1 - 3.3)*99) + 1;
    Serial.println("Battery percent: " + String(percent) + "%");
    return percent;
}


bool http_request_data(WiFiClient& client, Request request, unsigned int retry=3) {
    
    bool ret_val = false;

    while(!ret_val && retry--) {
        ret_val = true;
        client.stop();
        HTTPClient http;
        Serial.printf("\nHTTP connecting to %s%s [retry left: %s]", request.server.c_str(), request.path.c_str(), String(retry).c_str());
        http.begin(client, request.server, 80, request.path);
        int http_code = http.GET();
        
        if(http_code == HTTP_CODE_OK) {
            Serial.println("\nHTTP connection established");
            if (!request.handler(http.getStream(), request)) {
                ret_val = false;
            }
        } else {
            Serial.printf("\nHTTP connection failed %s, error: %s \n\n", String(http_code).c_str(), http.errorToString(http_code).c_str());
            ret_val = false;
        }
        client.stop();
        http.end();
    }
    return ret_val;
}


bool connect_to_wifi(unsigned int retry=5) {

    int wifi_conn_status = WL_IDLE_STATUS;
    WiFi.mode(WIFI_STA); // Access Point mode off
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    while(wifi_conn_status != WL_CONNECTED && retry--) {
        Serial.println("\nConnecting to: " + wifi.ssid + " [retry left: " + retry +"]");
        unsigned long start = millis();
        wifi_conn_status = WiFi.begin(wifi.ssid.c_str(), wifi.pass.c_str());
        
        while (true) {
            if (millis() > start + 10000) { // 10s
                break;
            }
            delay(100);
    
            wifi_conn_status = WiFi.status();
            
            if (wifi_conn_status == WL_CONNECTED) {
                Serial.println("Wifi connected. IP: " + WiFi.localIP().toString());    
                break;
            } else if(wifi_conn_status == WL_CONNECT_FAILED) {
                Serial.println("Wifi failed to connect.");
                break;
            }
        }
        if (wifi_conn_status == WL_CONNECTED) {
            return true;
        }
        delay(2000); // 2sec
    }
    return false;
}

void print_reset_reason(RESET_REASON reason) {
    switch ( reason) {
        case 1 : Serial.print("POWERON_RESET"); break;
        case 3 : Serial.print("SW_RESET"); break;
        case 4 : Serial.print("OWDT_RESET"); break;
        case 5 : Serial.print("DEEPSLEEP_RESET"); break;
        case 6 : Serial.print("SDIO_RESET"); break; 
        case 7 : Serial.print("TG0WDT_SYS_RESET"); break;
        case 8 : Serial.print("TG1WDT_SYS_RESET"); break;
        case 9 : Serial.print("RTCWDT_SYS_RESET"); break;
        case 10 : Serial.print("INTRUSION_RESET"); break;
        case 11 : Serial.print("TGWDT_CPU_RESET"); break;
        case 12 : Serial.print("SW_CPU_RESET"); break;
        case 13 : Serial.print("RTCWDT_CPU_RESET"); break;
        case 14 : Serial.print("EXT_CPU_RESET"); break;
        case 15 : Serial.print("RTCWDT_BROWN_OUT_RESET"); break;
        case 16 : Serial.print("RTCWDT_RTC_RESET"); break;
        default : Serial.print("UNKNOWN");
    }
}


void wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch(wakeup_reason){
        Serial.println("Location variable: " + String(curr_loc));
        
        case ESP_SLEEP_WAKEUP_EXT0 : 
            Serial.println("\nWakeup by ext signal RTC_IO -> GPIO39"); 
            if (get_mode() == OPERATING_MODE) {
                // Toggle between 2 screens caused by button press WAKE_BTN_PIN
                if (location_cnt > 1) {
                    curr_loc = (curr_loc+1) % 2;
                    // save location
                    save_location_to_memory(curr_loc);
                }
            }        
            break;
            
        case ESP_SLEEP_WAKEUP_EXT1 : 
            Serial.println("Wakeup by ext signal RTC_CNTL -> GPIO34"); 
            set_mode_and_reboot(CONFIG_MODE);
            break;
            
        case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup by ULP program"); break;
        default : Serial.printf("Wakeup not caused by deep sleep: %d\n", wakeup_reason); break;
    }
    Serial.print("CPU0 reset reason: ");
    print_reset_reason(rtc_get_reset_reason(0));
    Serial.print(",  CPU1 reset reason: ");
    print_reset_reason(rtc_get_reset_reason(1));
    Serial.println();
}


void enable_timed_sleep(int interval_minutes) {
    // sleep and wake up round minutes, ex every 15 mins
    // will wake up at 7:15, 7:30, 7:45 etc.

    struct tm* timeinfo;
    timeinfo = localtime(&datetime_request.response.dt);
    int current_time_min = timeinfo->tm_min;
    int current_time_sec = timeinfo->tm_sec;
    int sleep_minutes_left = interval_minutes - current_time_min % interval_minutes - 1;  // - 1 minute running in seconds
    int sleep_seconds_left = 60 - current_time_sec;
    int sleep_time_seconds = sleep_minutes_left * 60 + sleep_seconds_left;
    
    long sleep_time_micro_sec = sleep_time_seconds * 1000 * 1000;
    esp_sleep_enable_timer_wakeup(sleep_time_micro_sec);
    Serial.printf("\nWake up in %d minutes and %d seconds", sleep_minutes_left, sleep_seconds_left);
}


void begin_deep_sleep() {
#ifdef BUILTIN_LED
    pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
    digitalWrite(BUILTIN_LED, HIGH);
#endif

    display.powerDown();
    disconnect_from_wifi();
    
#ifdef WAKE_BTN_PIN
    //  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/sleep_modes.html
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_BTN_PIN, LOW);
#endif

    // config triggering button
#define BUTTON_PIN_BITMASK 0x0400000000 // pin 34
    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);

    // this should lower power consumtion
    // https://www.reddit.com/r/esp32/comments/idinjr/36ma_deep_sleep_in_an_eink_ttgo_t5_v23/
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);
    gpio_deep_sleep_hold_en();

    // start sleep
    esp_deep_sleep_start();
}


void disconnect_from_wifi() {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}


void http_resource_not_found(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}


String get_param(AsyncWebServerRequest *request, String name) {
    String value;
    if (request->hasParam(name, true)) {
        value = request->getParam(name, true)->value();
        return value;
    }
    return "";
}


String get_param(AsyncWebServerRequest *request, String name, bool& exists) {
    String value;
    if (request->hasParam(name, true)) {
        value = request->getParam(name, true)->value();

        if (value.equals("")) {
            exists = false;
            return "";        
        }
        exists = exists && true;
        return value;
    }
    exists = false;
    return "";
}


String create_validation_message(bool valid_wifi, bool valid_location_1, bool valid_location_2) {
    String wifi_error_msg = "";
    String location_1_error_msg = "";
    String location_2_error_msg = "";
        
    if (!valid_wifi) {
        wifi_error_msg = "Missing WIFI credentials. Please provide SSID and password.\n";
    }        
    if (!valid_location_1) {
        location_1_error_msg = "Invalid location 1. Please provide name and postition.";
    }
    if (!valid_location_2) {
        location_2_error_msg = "Incomplete location 2 data. Location will be omitted.";
    }
    String message_result = "";
    String message_color;

    if (!valid_wifi || !valid_location_1) {
        message_result = "in";  // add prefix for valid
        message_color = "red";
    } else {
        message_color = "green";
    }
    String response_msg = String("<h1>Weather Station Configuration</h1><br><br>") +
    String("<h2 style=\"color:") + message_color + String("\">Configuration is ") + message_result + String("valid.</h2><br>") +
    String("<h3>") + wifi_error_msg + String("</h3><br>") +
    String("<h3>") + location_1_error_msg + String("</h3><br>") +
    String("<h3>") + location_2_error_msg + String("</h3><br>");
        
    return response_msg;
}


void read_config_from_memory() {
    Serial.println("Read config from memory...");
    preferences.begin(MEMORY_ID, true);  // first param true means 'read only'

    wifi.ssid = preferences.getString("ssid");
    wifi.pass = preferences.getString("pass");
    wifi.print();

    location_cnt = preferences.getInt("locations");  // global variable
    Serial.println("Locations: " + String(location_cnt));

    location[0].name = preferences.getString("loc1", "");
    location[0].lat = preferences.getFloat("lat1", 0.0f);
    location[0].lon = preferences.getFloat("lon1", 0.0f);
    location[0].print();

    if (location_cnt > 1) {
        location[1].name = preferences.getString("loc2", "");
        location[1].lat = preferences.getFloat("lat2", 0.0f);
        location[1].lon = preferences.getFloat("lon2", 0.0f);
        location[1].print();
    }
    preferences.end();
}


int read_location_from_memory() {
    Serial.println("Read current location from memory...");
    preferences.begin(LOC_MEMORY_ID, true);  // first param true means 'read only'
    int location_id = preferences.getInt("curr_loc");
    preferences.end();
    return location_id;
}


void save_config_to_memory() {
    Serial.println("Save config to memory.");
    
    preferences.begin(MEMORY_ID, false);  // first param false means 'read/write'

    wifi.print();
    preferences.putString("ssid", wifi.ssid);
    preferences.putString("pass", wifi.pass);

    Serial.println("Locations: " + String(location_cnt));
    preferences.putInt("locations", location_cnt);  // global variable

    location[0].print();
    preferences.putString("loc1", location[0].name);
    preferences.putFloat("lat1", location[0].lat);
    preferences.putFloat("lon1", location[0].lon);

    if (location_cnt > 1) {
        location[1].print();
        preferences.putString("loc2", location[1].name);
        preferences.putFloat("lat2", location[1].lat);
        preferences.putFloat("lon2", location[1].lon);
    }
    preferences.end();
}


void save_location_to_memory(int location_id) {
    Serial.println("Save current location to memory...");
    preferences.begin(LOC_MEMORY_ID, false);
    preferences.putInt("curr_loc", location_id);
    preferences.end();
}


class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request){
        return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("text/html");
        if (request->method() == 1) {  // enum: HTTP_GET
            const char *request_form =
            "<h1>Weather Station Configuration</h1><br><br>"
            "<h2>"
            "<form action=\"/config\" method=\"post\">"
            "<label for=\"ssid\">Wifi SSID</label><br>"
            "<input type=\"text\" id=\"ssid\" name=\"ssid\"><br>"
            "<label for=\"pass\">Wifi Password:</label><br>"
            "<input type=\"password\" id=\"pass\" name=\"pass\"><br>"
            "<br>"
            "<br>"
            "<label for=\"loc1\">Location 1 Name</label><br>"
            "<input type=\"text\" id=\"loc1\" name=\"loc1\"><br>"
            "<br>"
            "<label for=\"loc1\">Location 2 Name</label><br>"
            "<input type=\"text\" id=\"loc2\" name=\"loc2\"><br>"
            "<br>"
            "<input type=\"submit\" value=\"OK\" style=\"width: 100px; height: 45px; font-size: 16pt\">"
            "</form>"
            "</h2>";
            request->send(200, "text/html", request_form);
        } else {
            request->send(404, "text/html", "Invalid request method. Page not found");
        }
    }
};


void run_config_server() {
    String network = "weather-wifi";
    String pass = String(abs((int)esp_random())).substring(0, 4) + "0000";
    WiFi.softAP(network.c_str(), pass.c_str());
    //WiFi.softAP(network.c_str());  // <- without password
    
    String ip = WiFi.softAPIP().toString();
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    Serial.printf("\nStart config server on ssid: %s, pass: %s, ip: %s \n\n", network.c_str(), pass.c_str(), ip.c_str());
    display_config_mode(network, pass, ip);
    
    display.update();

    server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
        bool valid_wifi = true;
        bool valid_location_1 = true;
        bool valid_location_2 = true;
        
        String ssid = get_param(request, "ssid", valid_wifi);
        String pass = get_param(request, "pass", valid_wifi);
        String location_1 = get_param(request, "loc1", valid_location_1);
        String location_2 = get_param(request, "loc2", valid_location_2);

        // setting config
        if (valid_wifi && valid_location_1) {
            wifi.pass = pass;
            wifi.ssid = ssid;

            location_cnt = 1;
            location[0].name = location_1;
            
            if (valid_location_2) {
                location_cnt = 2;
                location[1].name = location_2;
            }

            save_config_to_memory();
            set_mode(VALIDATING_MODE);
            server.end();
            ESP.restart();
            return;
        }

        String response_msg = create_validation_message(valid_wifi, valid_location_1, valid_location_2);
        request->send(200, "text/html", response_msg);
    });
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    server.onNotFound(http_resource_not_found);
    server.begin();
}


void run_validating_mode() {
    server.end();
        
    read_config_from_memory();
    display_validating_mode();
    display.update();
    
    if (connect_to_wifi()) {
        location_request.handler = location_handler;
        WiFiClient client;
        bool is_location_fetched = false;
        
        if (location_cnt > 0) {
            location_request.name = location[0].name;
            location_request.make_path();
            is_location_fetched = http_request_data(client, location_request);    
        }
        if (is_location_fetched) {
            location[0].lat = location_request.response.lat;
            location[0].lon = location_request.response.lon;
                
            if (location_cnt > 1) {
                location_request.name = location[1].name;
                location_request.make_path();
                is_location_fetched = http_request_data(client, location_request);

                if (is_location_fetched) {
                    location[1].lat = location_request.response.lat;
                    location[1].lon = location_request.response.lon;

                    save_config_to_memory();
                    set_mode_and_reboot(OPERATING_MODE);
                    return;
                }
            } else {
                save_config_to_memory();
                set_mode_and_reboot(OPERATING_MODE);
                return;
            }
        }
    }
    set_mode_and_reboot(CONFIG_MODE);
}


void run_operating_mode() {
    read_config_from_memory();
    curr_loc = read_location_from_memory();
    wakeup_reason();

    if (connect_to_wifi()) {
        WiFiClient client;

        datetime_request.make_path(location[curr_loc]);
        datetime_request.handler = datetime_handler;

        weather_request.make_path(location[curr_loc]);
        weather_request.handler = weather_handler;

        airquality_request.make_path(location[curr_loc]);
        airquality_request.handler = air_quality_handler;

        bool is_time_fetched = http_request_data(client, datetime_request);
        bool is_weather_fetched = http_request_data(client, weather_request);
        bool is_aq_fetched = http_request_data(client, airquality_request);

        view = View();

        update_header_view(view, is_time_fetched); 
        update_weather_view(view, is_weather_fetched);
        update_air_quality_view(view, is_aq_fetched);
            
        Serial.println("\nUpdate display.");
        display_header(view);
        display_weather(view);
        display_air_quality(view);
    }

    display.update();
    delay(100); // too fast display powerDown displays blank (white)??

    // deep sleep stuff
    enable_timed_sleep(SLEEP_INTERVAL_MIN);
    begin_deep_sleep();
}


void set_mode(int mode) {
    preferences.begin(MEMORY_ID, false);
    preferences.putInt("mode", mode);
    preferences.end();
}


void set_mode_and_reboot(int mode) {
    set_mode(mode);
    ESP.restart();
}


int get_mode(bool cached_mode) {
    if (cached_mode) {
        return cached_MODE;
    }
    preferences.begin(MEMORY_ID, false);
    int mode = preferences.getInt("mode", NOT_SET_MODE);
    cached_MODE = mode;
    preferences.end();
    return mode;
}


void setup() {
    Serial.begin(115200); 
    Serial.println("\n\n=== WEATHER STATION ===");
    init_display();

    if (get_mode() == NOT_SET_MODE) {
        Serial.println("MODE: not set. Initializing mode to CONFIG_MODE.");
        set_mode_and_reboot(CONFIG_MODE);
    }
    const int mode = get_mode();
    
    if (mode == CONFIG_MODE) {
        Serial.println("MODE: Config");
        run_config_server();
    } else if (mode == VALIDATING_MODE) {
        Serial.println("MODE: Validating");
        run_validating_mode();
    } else if (mode == OPERATING_MODE) {
        Serial.println("MODE: Operating");
        run_operating_mode();
    }
}


void loop() {  
    // Serial.println("Handling DNS");

    // Used only in CONFIG mode 
    dnsServer.processNextRequest();
}
