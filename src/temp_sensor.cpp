#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <TaskScheduler.h>

#include <Adafruit_LEDBackpack.h>
#include <SparkFunBME280.h>

//#define OTA

#ifdef OTA
#include <ArduinoOTA.h>
#endif

#include "credentials.h"

struct sensor_data_t {
    float temperature;
    float pressure;
    float height;
    float humidity;
    int co2;
    int tvoc;
};

sensor_data_t sensor_data;

const char* ssid = SSID;
const char* password = PASSWORD;

void sensor_update();
void server_update();
void display_update();

Adafruit_7segment matrix_up = Adafruit_7segment();
Adafruit_7segment matrix_down = Adafruit_7segment();

BME280 sensor = BME280();
ESP8266WebServer server(80);

Task sensor_task(500, TASK_FOREVER, &sensor_update);
Task display_task(5000, TASK_FOREVER, &display_update);
Task server_task(100, TASK_FOREVER, &server_update);

#ifdef OTA
Task ota_task(10, TASK_FOREVER, []() { ArduinoOTA.handle(); });
#else
Task ota_task(10, TASK_ONCE, []() { });
#endif

Scheduler runner;

bool page = true;

const uint8_t brightness = 1;

static const char *const VERSION = "0.0.20190729";

static const int SENSOR_ADDRESS = 0x77;
static const int MATRIX_UP_ADDRESS = 0x70;
static const int MATRIX_DOWN_ADDRESS = 0x71;
static const int BAUD = 115200;
static const char *const MDNS_NAME = "sensor-wohnzimmer";

const int led = 13;

#include "Adafruit_CCS811.h"
Adafruit_CCS811 ccs;

void handleMetrics() {
    String message = "sensor_temperature_celsius ";
    message += sensor_data.temperature;
    message += "\nsensor_pressure_pascal ";
    message += sensor_data.pressure;
    message += "\nsensor_humidity_percent ";
    message += sensor_data.humidity;
    message += "\nsensor_co2_ppm ";
    message += sensor_data.co2;
    message += "\nsensor_tvoc_ppm ";
    message += sensor_data.tvoc;
    message += "\nuptime_seconds ";
    message += millis() / 1000.;
    message += "\n";
    server.send(200, "text/plain", message);
}

void handleNotFound(){
    digitalWrite(led, 1);
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i=0; i<server.args(); i++){
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
    digitalWrite(led, 0);
}


void setup_matrix(Adafruit_7segment &matrix, uint8_t address) {
    matrix.begin(address);
    matrix.setBrightness(brightness);
    matrix.clear();
}

void setup_sensor(BME280 &sensor) {
    //***Driver settings********************************//
    //commInterface can be I2C_MODE or SPI_MODE
    //specify chipSelectPin using arduino pin names
    //specify I2C address.  Can be 0x77(default) or 0x76

    ccs.begin();

    //For I2C, enable the following and disable the SPI section
    sensor.settings.commInterface = I2C_MODE;
    sensor.settings.I2CAddress = SENSOR_ADDRESS;

    //For SPI enable the following and dissable the I2C section
    //mySensor.settings.commInterface = SPI_MODE;
    //mySensor.settings.chipSelectPin = 10;


    //***Operation settings*****************************//

    //renMode can be:
    //  0, Sleep mode
    //  1 or 2, Forced mode
    //  3, Normal mode
    sensor.settings.runMode = 3; //Forced mode

    //tStandby can be:
    //  0, 0.5ms
    //  1, 62.5ms
    //  2, 125ms
    //  3, 250ms
    //  4, 500ms
    //  5, 1000ms
    //  6, 10ms
    //  7, 20ms
    sensor.settings.tStandby = 0;

    //filter can be off or number of FIR coefficients to use:
    //  0, filter off
    //  1, coefficients = 2
    //  2, coefficients = 4
    //  3, coefficients = 8
    //  4, coefficients = 16
    sensor.settings.filter = 4;

    //tempOverSample can be:
    //  0, skipped
    //  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
    sensor.settings.tempOverSample = 5;

    //pressOverSample can be:
    //  0, skipped
    //  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
    sensor.settings.pressOverSample = 5;

    //humidOverSample can be:
    //  0, skipped
    //  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
    sensor.settings.humidOverSample = 5;

    Serial.print("Starting BME280... result of .begin(): 0x");
    //Calling .begin() causes the settings to be loaded
    delay(10);//Needs > 2 ms to start up
    Serial.println(sensor.begin(), HEX);
}

void setup_runner() {
    runner.init();

    runner.addTask(sensor_task);
    runner.addTask(server_task);
    runner.addTask(display_task);
    runner.addTask(ota_task);

    runner.enableAll();
}

void setup_server() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin(MDNS_NAME)) {
        Serial.println("MDNS responder started");
    }

    server.on("/metrics", handleMetrics);
    server.on("/version", []() { server.send(200, "text/plain", String(VERSION) + "\n"); });
    server.on("/free", []() { server.send(200, "text/plain", String(ESP.getFreeSketchSpace()) + "\n"); });

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void setup_ota() {
#ifdef OTA
    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    // ArduinoOTA.setHostname("myesp8266");

    // No authentication by default
    // ArduinoOTA.setPassword(OTA_PASSWORD);

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
#endif
}

void setup() {
    Serial.begin(BAUD);
    Serial.println("Temperature and Pressure sensor");

    setup_matrix(matrix_up, MATRIX_UP_ADDRESS);
    setup_matrix(matrix_down, MATRIX_DOWN_ADDRESS);

    setup_sensor(sensor);
    setup_server();
    setup_ota();
    setup_runner();
}

void serial_sensor_out(struct sensor_data_t sensor_data) {
    Serial.print("Temperature: ");
    Serial.println(sensor_data.temperature);

    Serial.print("Pressure: ");
    Serial.println(sensor_data.pressure);

    Serial.print("Humidity: ");
    Serial.println(sensor_data.humidity);

    Serial.print("Height: ");
    Serial.println(sensor_data.height);

    Serial.print("CO2: ");
    Serial.println(sensor_data.co2);
    Serial.print("TVOC: ");
    Serial.println(sensor_data.tvoc);

    Serial.println();
}

struct sensor_data_t get_sensor_data(BME280 &sensor) {
    sensor_data_t sensor_data;

    sensor_data.temperature = sensor.readTempC();
    sensor_data.pressure = sensor.readFloatPressure();
    sensor_data.height = sensor.readFloatAltitudeMeters();
    sensor_data.humidity = sensor.readFloatHumidity();

    if(ccs.available()) {
      Serial.println("Sensor data available");
      if(ccs.readData()) {
        sensor_data.co2 = ccs.geteCO2();
        sensor_data.tvoc = ccs.getTVOC();
      } else {
        Serial.println("Error");
      };
    };

    return sensor_data;
}

void sensor_update() {
    sensor_data = get_sensor_data(sensor);
}

void display_update() {
    serial_sensor_out(sensor_data);

    if (page) {
        matrix_up.print(sensor_data.temperature);
        matrix_down.print(sensor_data.pressure / 100.);
        page = false;
    } else {
        matrix_up.print(sensor_data.humidity);
        matrix_down.print(sensor_data.height);
        page = true;
    }

    matrix_up.writeDisplay();
    matrix_down.writeDisplay();
}

void server_update() {
    server.handleClient();
}

void loop() {
    runner.execute();
}

