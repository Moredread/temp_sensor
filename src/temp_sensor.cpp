#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <TaskScheduler.h>

#include <Adafruit_LEDBackpack.h>
#include <SparkFunBME280.h>

#include "credentials.h"

struct sensor_data_t {
    float temperature;
    float pressure;
    float height;
    float humidity;
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

Scheduler runner;

const int led = 13;

void handleRoot() {
    digitalWrite(led, 1);
    String message = "sensor.temperature ";
    message += sensor_data.temperature;
    message += "\nsensor.pressure ";
    message += sensor_data.pressure;
    message += "\nsensor.humidity ";
    message += sensor_data.humidity;
    message += "\nsensor.height ";
    message += sensor_data.height;
    message += "\nuptime ";
    message += millis();
    server.send(200, "text/plain", message);
    digitalWrite(led, 0);
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

bool page = true;

const uint8_t brightness = 1;

static const int SENSOR_ADDRESS = 0x77;
static const int MATRIX_UP_ADDRESS = 0x70;
static const int MATRIX_DOWN_ADDRESS = 0x71;
static const int BAUD = 115200;

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

void setup() {
    Serial.begin(BAUD);
    Serial.println("Temperature and Pressure sensor");

    setup_matrix(matrix_up, MATRIX_UP_ADDRESS);
    setup_matrix(matrix_down, MATRIX_DOWN_ADDRESS);

    setup_sensor(sensor);

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

    if (MDNS.begin("sensor")) {
        Serial.println("MDNS responder started");
    }

    server.on("/", handleRoot);

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");

    runner.init();

    runner.addTask(sensor_task);
    runner.addTask(server_task);
    runner.addTask(display_task);

    runner.enableAll();
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

    Serial.println();
}

struct sensor_data_t get_sensor_data(BME280 &sensor) {
    sensor_data_t sensor_data;

    sensor_data.temperature = sensor.readTempC();
    sensor_data.pressure = sensor.readFloatPressure();
    sensor_data.height = sensor.readFloatAltitudeMeters();
    sensor_data.humidity = sensor.readFloatHumidity();

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

