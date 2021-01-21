#include <Servo.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESPNtpClient.h>
#include <Arduino_JSON.h>

// Please note: change defines below to your needs
#define MOTOR_PIN 16   // GPIO 16, on NodeMCU marked as "D0"
#define SSID "<YOUR_WIFI_SSID>"
#define SSIDPWD "<YOUR_WIFI_PASSWORD>"
#define NTP_SERVER "time.google.com"
#define TIME_ZONE TZ_America_New_York
const String apiKey = "<YOUR_OPEN_WEATHER_MAP_ID>";
const String city = "<YOUR_CITY>";
const String countryCode = "<YOUR_COUNTRY_CODE>";

ESP8266WebServer server(80);
#define GO_BACK server.sendHeader("Location", "/",true); server.send(302, "text/plane","");
#define CHECK_AUTH if ((user != "" || password != "") && !server.authenticate(user.c_str(), password.c_str())) return server.requestAuthentication();

Servo motor;

boolean timeSynchronized = false;
boolean tempSynchronized = false;
boolean doStartEngine = false;
boolean isConnected = false; 
short int currTemp = 0;

// EEPROM variables
short int tempCondition = 0;
#define TEMP_COND_ADDR 0
byte startHours[2] = {8, 8};
#define START_HOURS_ADDR 2
byte startMinutes[2] = {15, 30};
#define START_MINUTES_ADDR 4
byte weekRange = 0;
#define WEEK_RANGE_ADDR 6
byte numStarts = 2;
#define NUM_STARTS_ADDR 7
byte interval = 15;
#define INTERVAL_ADDR 8
short int prevTemp = 0;
#define PREV_TEMP_ADDR 10
String user = "";
#define USER_ADDR 12
String password = "";
#define PASS_ADDR USER_ADDR+20

// EEPROM helpers
void writeIntToEEPROM(int address, int number) 
{ 
    EEPROM.write(address, number >> 8); 
    EEPROM.write(address + 1, number & 0xFF); 
}

int readIntFromEEPROM(int address)
{
  byte byte1 = EEPROM.read(address);
  byte byte2 = EEPROM.read(address + 1);
  return (byte1 << 8) + byte2;
}

// This is HTML page for settings
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <title>Remote car engine starter</title>
  <meta name='viewport' content='width=device-width, initial-scale=1' />
  <link rel='icon' href='http://senssoft.com/car.ico' type='image/x-icon' />
  <link rel='shortcut icon' href='http://senssoft.com/car.ico' type='image/x-icon' />  
  <style>
    table {
      border-collapse: separate;
      border-spacing: 1px 1px;
    }
    th, td {
      width: 170px;
      height: 30px;
    }
    input {
      float: align;
      text-align: left;
      width: 95%%;
    }
    select {
      float: align;
      width: 95%%;
    }
    button {
      display: block;
      width: 100%%;
      border: none;
      background-color: #3a853d;
      color: white;
      padding: 10px 22px;
      font-size: 12px;
      cursor: pointer;
      text-align: center;
    }
    button:hover {
      background-color: #4CAF50;
      color: white;
    }
  </style>
  <script>
    var serverTime, upTime;

    function pad(s) {
      return (s < 10 ? '0' : '') + s;
    }

    function formatUptime(seconds) {
      var hours = Math.floor(seconds / (60 * 60));
      var days = Math.floor(hours / 24);
      if (days > 0) hours -= days * 24;
      var minutes = Math.floor(seconds %% (60 * 60) / 60);
      var seconds = Math.floor(seconds %% 60);
      var daystr = ' day' + (days == 1 ? '' : 's') + ' ';
      return (days > 0 ? days + daystr : '') + pad(hours) + ':' + pad(minutes) + ':' + pad(seconds);
    }

    function setTime() {
      upTime = %d;
      document.getElementById('upTime').innerHTML = formatUptime(upTime);
      serverTime = new Date('%s');
      document.addEventListener("visibilitychange", function() { window.location.reload(); });
      startTime();
    }

    function startTime() {
      const monthName = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
      serverTime = new Date(serverTime.getTime() + 1000);
      upTime++;
      var today = new Date(serverTime);
      document.getElementById('serverTime').innerHTML = monthName[today.getMonth()] + ' ' + today.getDate() + ', ' + today.getFullYear() + '   ' + pad(today.getHours()) + ':' + pad(today.getMinutes()) + ':' + pad(today.getSeconds());
      document.getElementById('upTime').innerHTML = formatUptime(upTime);
      var t = setTimeout(function() { startTime() }, 1000);
    }
  </script>
</head>

<body onload='setTime()'>
  <table>
    <tr style='background-color:#EAFAF1;'>
      <td style='text-align:center'>System uptime:</td>
      <td align='center' id='upTime'>00:00:00</td>
    </tr>
    <tr height='12px'/>
    <tr>
      <td>
        <button onclick="window.location.href='/updateTime'">Server time</button>
      </td>
      <td>
        <button onclick="window.location.href='/updateTemperature'">Temperature</button>
      </td>
    </tr>
    <tr>
      <td id='serverTime' align='center'></td>
      <td align='center'>%i &#176;C</td>
    </tr>
  </table>
  <form action="/settings" method="POST">
    <table>
      <tr>
        <td colspan='2' align='center'><b>Engine start conditions:</b></td>
      </tr>
      <tr>
        <td>Temperature below:</td>
        <td>
          <input type='number' name='tempCondition' id='tempCondition' min='-10' max='10' value='%d'></input>
        </td>
      </tr>
      <tr>
        <td>Time:</td>
        <td>
          <input type='time' name='startTime' id='startTime' value='%02d:%02d:00'></input>
        </td>
      </tr>
      <tr>
        <td>Weekly range:</td>
        <td>
          <select name='weekRange' id='weekRange'>
            <option %s>Workdays</option>
            <option %s>All week</option>
          </select>
        </td>
      </tr>
      <tr>
        <td>How many times:</td>
        <td>
          <input type='number' name='numStarts' id='numStarts' min='0' max='2' value='%d'></input>
        </td>
      </tr>
      <tr>
        <td>Interval (min):</td>
        <td>
          <input type='number' name='interval' id='interval' min='11' max='30' value='%d'></input>
        </td>
      </tr>
      <tr>
        <td colspan='2' align='center'><b>Login credentials:</b></td>
      </tr>
      <tr>
        <td>User:</td>
        <td>
          <input name='user' id='user' style='text-align:left;' value='%s'></input>
        </td>
      </tr>
      <tr>
        <td>Password:</td>
        <td>
          <input type='password' name='password' id='password' style='text-align:left;' value='%s'></input>
        </td>
      </tr>
      <tr height='12px' />
      <tr>
        <td colspan='2'>
          <button type='submit'>Save settings</button>
        </td>
      </tr>
      <tr height='12px' />
    </table>
  </form>
  <button style='border:none; width:344px; background-color:#0020d4; color:white;' onclick="window.location.href='/startEngine'">Start car engine now</button>
</body>

</html>)rawliteral";

#define HTML_SIZE sizeof(index_html)+40
char temp[HTML_SIZE];
void handleRoot() 
{
    CHECK_AUTH

    String range_1 = (weekRange == 0) ? "selected=''" : "";
    String range_2 = (weekRange == 1) ? "selected=''" : "";

    snprintf(temp, HTML_SIZE, index_html, 
        millis()/1000, 
        NTP.getTimeDateStringForJS(), 
        currTemp, 
        tempCondition, 
        startHours[0], startMinutes[0],
        range_1.c_str(), range_2.c_str(),
        numStarts,
        interval,
        user.c_str(),
        password.c_str());

    server.send(200, "text/html", temp);
}

bool checkConnection() { isConnected = ((WiFi.status() == WL_CONNECTED) && Ping.ping("api.openweathermap.org")); return isConnected; }
void connectToInternet()
{
    Serial.printf("Connecting to %s ", SSID);        
    WiFi.begin(SSID, SSIDPWD);
    for (int i=0; i<10; i++) 
    {
        if (!checkConnection()) 
        {
            delay(500);
            Serial.print(".");
        }
        else
        {
            Serial.print("Connected to "); Serial.println(SSID);
            Serial.print("IP address: ");  Serial.println(WiFi.localIP());
            break;
        }
    }
    // Turn built-in LED on if we're connected
    digitalWrite(LED_BUILTIN, checkConnection() ? LOW : HIGH);
}

void processSyncEvent (NTPEvent_t ntpEvent) 
{
    switch (ntpEvent.event) 
    {
        // Real time sync should happened every hour
        case timeSyncd:
            timeSynchronized = true;
            tempSynchronized = false;
        case partlySync:
        case syncNotNeeded:
        case accuracyError:
            Serial.printf ("[NTP-event] %s\n", NTP.ntpEvent2str (ntpEvent));
            break;
        default:
            break;
    }
}

void getCurrentTemperature() 
{
    tempSynchronized = false;
    Serial.println("getCurrentTemperature()");
    
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + apiKey +"&mode=json&units=metric&cnt=1";
    http.begin(url);
  
    // Send HTTP POST request
    int httpResponseCode = http.GET();
    String payload = "{}"; 
  
    if (httpResponseCode > 0) 
    {
        JSONVar weatherObject = JSON.parse(http.getString());
        if (JSON.typeof(weatherObject) != "undefined") 
        {
            currTemp = (int)round((double)weatherObject["main"]["temp"]);
            Serial.print("Current temperature: "); Serial.println(currTemp);
            writeIntToEEPROM(PREV_TEMP_ADDR, currTemp);
            EEPROM.commit();
            tempSynchronized = true;
        }
    }
    else 
    {
        Serial.print("[HTTP] error code: ");
        Serial.println(httpResponseCode);
    }
    
    // Free resources
    http.end();
}

void readSettings()
{
    byte b = EEPROM.read(TEMP_COND_ADDR);
    // Do we have values stored?
    if (b != 255)
    {
        tempCondition = readIntFromEEPROM(TEMP_COND_ADDR);
        startHours[0] = EEPROM.read(START_HOURS_ADDR);
        startHours[1] = EEPROM.read(START_HOURS_ADDR+1);
        startMinutes[0] = EEPROM.read(START_MINUTES_ADDR);
        startMinutes[1] = EEPROM.read(START_MINUTES_ADDR+1);
        weekRange = EEPROM.read(WEEK_RANGE_ADDR);
        numStarts = EEPROM.read(NUM_STARTS_ADDR);
        interval = EEPROM.read(INTERVAL_ADDR);
        currTemp = prevTemp = readIntFromEEPROM(PREV_TEMP_ADDR);
        user = password = "";
        for(int i=0;i<20;i++) { b = EEPROM.read(USER_ADDR+i); if (b>0) user += char(b); else break; }
        for(int i=0;i<20;i++) { b = EEPROM.read(PASS_ADDR+i); if (b>0) password += char(b); else break; }
    }
}

void writeSettings()
{
    writeIntToEEPROM(TEMP_COND_ADDR, tempCondition);
    EEPROM.write(START_HOURS_ADDR, startHours[0]);
    EEPROM.write(START_HOURS_ADDR+1, startHours[1]);
    EEPROM.write(START_MINUTES_ADDR, startMinutes[0]);
    EEPROM.write(START_MINUTES_ADDR+1, startMinutes[1]);
    EEPROM.write(WEEK_RANGE_ADDR, weekRange);
    EEPROM.write(NUM_STARTS_ADDR, numStarts);
    EEPROM.write(INTERVAL_ADDR, interval);
    for(int i=0;i<20;i++) EEPROM.write(USER_ADDR+i, 0);
    for(int i=0;i<user.length();i++) EEPROM.write(USER_ADDR+i, user[i]);
    for(int i=0;i<20;i++) EEPROM.write(PASS_ADDR+i, 0);
    for(int i=0;i<password.length();i++) EEPROM.write(PASS_ADDR+i, password[i]);
    // store data to EEPROM
    EEPROM.commit();
}

void saveSettings()
{
    CHECK_AUTH

    if (server.method() == HTTP_POST)
    {
        Serial.println("saveSettings()");
        String value = "";
        if (server.hasArg("tempCondition")) { value = server.arg("tempCondition"); tempCondition = value.toInt(); }
        if (server.hasArg("numStarts")) { value = server.arg("numStarts"); numStarts = value.toInt(); } 
        if (server.hasArg("interval")) { value = server.arg("interval"); interval = value.toInt(); } 
        if (server.hasArg("user")) { user = server.arg("user"); } 
        if (server.hasArg("password")) { password = server.arg("password"); } 
        if (server.hasArg("weekRange")) { weekRange = server.arg("weekRange") == "Workdays" ? 0 : 1; }
        if (server.hasArg("startTime"))
        {
            struct tm startTime;
            memset(&startTime, 0, sizeof(tm));
            value = "1970-01-01 " + server.arg("startTime");
            strptime(value.c_str(), "%Y-%m-%d %H:%M:%S", &startTime);
            startHours[0] = startTime.tm_hour;
            startMinutes[0] = startTime.tm_min;
            // Add interval and set second time
            time_t t = mktime(&startTime);
            t += interval*60;
            startTime = *localtime(&t);
            startHours[1] = startTime.tm_hour;
            startMinutes[1] = startTime.tm_min;
        }
        writeSettings();
    }

    GO_BACK
}

void setup() 
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    // turn off built-in LED (inverse logic)
    digitalWrite(LED_BUILTIN, HIGH);

    // Initialize EEPROM and read settings
    EEPROM.begin(64);
    readSettings();

    // Setup & reset servo motor to the middle position
    motor.attach(MOTOR_PIN);
    motor.write(40);

    // Connect to WiFi & Internet
    connectToInternet();

    // setup NTP client
    NTP.setTimeZone (TIME_ZONE);
    // Sync time and temperature once per hour (in seconds)
    NTP.setInterval (3600);  
    // NTP timeout in msec           
    NTP.setNTPTimeout (1000);
    NTP.onNTPSyncEvent (processSyncEvent);
    NTP.begin(NTP_SERVER);
    
    // Setup web server
    server.on("/", handleRoot);
    server.on("/startEngine", []() { CHECK_AUTH doStartEngine = true; GO_BACK });
    server.on("/updateTime", []() { CHECK_AUTH NTP.getTime(); GO_BACK });
    server.on("/updateTemperature", []() { CHECK_AUTH timeSynchronized = true; tempSynchronized = false; GO_BACK });
    server.on("/settings", HTTP_POST, saveSettings);    
    server.onNotFound( [](){ GO_BACK });
    server.begin();
    Serial.println("System started");
}

void startCarEngine() 
{
    Serial.println("startCarEngine()");
    
    doStartEngine = false;    
    // First, press "close doors" button on remote
    motor.write(70);
    delay(1000);
    motor.write(40);
    delay(1000);
    // Now, press and hold for five seconds "start" button
    motor.write(5);
    delay(5000);
    // Move servo to middle position
    motor.write(40);
}

unsigned long prevMillis = 0, prevBlinkMillis = 0;
bool blinkState = false;
void loop() 
{
    // Check engine start conditions every minute
    if(timeSynchronized && millis()-prevMillis > 60 * 1000) 
    {
        prevMillis = millis();
        Serial.println (NTP.getTimeDateStringUs ());
        // Check internet connection
        checkConnection();
        // Turn built-in LED on if we're connected
        digitalWrite(LED_BUILTIN, isConnected ? LOW : HIGH);

        // Should we check start conditions?
        if (numStarts > 0)
        {
            // Check day of week first
            time_t t = time(NULL);
            struct tm now = *localtime(&t);
            if ((weekRange == 0 && (now.tm_wday > 0 && now.tm_wday < 6)) || weekRange == 1)
            {
                if ((now.tm_hour == startHours[0] && now.tm_min == startMinutes[0]) ||
                    (numStarts > 1 && now.tm_hour == startHours[1] && now.tm_min == startMinutes[1]))
                {
                    if (isConnected) getCurrentTemperature();
                    if (currTemp < tempCondition)
                    {
                        Serial.println ("Starting car engine!");
                        doStartEngine = true;
                    }
                }
            }
        }
    }

    // Blink if we're out of internet access
    if (!isConnected && millis()-prevBlinkMillis > 200)
    {
        prevBlinkMillis = millis();
        blinkState ^= true; // invert boolean
        digitalWrite(LED_BUILTIN, blinkState ? LOW : HIGH);        
    }

    // Should we sync current temperature?
    if (timeSynchronized && !tempSynchronized) getCurrentTemperature();

    // Should we start car engine?
    if (doStartEngine) startCarEngine();
    
    server.handleClient();
}
