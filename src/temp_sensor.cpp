#include <Arduino.h>
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include "Adafruit_GFX.h"
#include "Adafruit_LEDBackpack.h"
#include "SparkFunBME280.h"

Adafruit_7segment matrix_up = Adafruit_7segment();
Adafruit_7segment matrix_down = Adafruit_7segment();

BME280 sensor = BME280();

const uint8_t brightness = 1;

void setup_matrix(Adafruit_7segment &matrix, uint8_t address) {
  matrix.begin(address);
  matrix.setBrightness(brightness);
  //matrix.clear();
}

void setup_sensor(BME280 &sensor) {
  //***Driver settings********************************//
  //commInterface can be I2C_MODE or SPI_MODE
  //specify chipSelectPin using arduino pin names
  //specify I2C address.  Can be 0x77(default) or 0x76
  
  //For I2C, enable the following and disable the SPI section
  sensor.settings.commInterface = I2C_MODE;
  sensor.settings.I2CAddress = 0x77;
  
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
  Serial.begin(38400);
  Serial.println("Temperature and Pressure sensor");

  setup_matrix(matrix_up, 0x70);
  setup_matrix(matrix_down, 0x71);

  setup_sensor(sensor);
}

struct sensor_data_t {
  float temperature;
  float pressure;
  float height;
  float humidity;
};

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

void loop() {
  bool page = true;

  while(true) {
    sensor_data_t sensor_data = get_sensor_data(sensor);
    
    serial_sensor_out(sensor_data);

    if(page) {
      matrix_up.print(sensor_data.temperature);
      matrix_down.print(sensor_data.pressure/100.);
      page = false;
    } else {
      matrix_up.print(sensor_data.humidity);
      matrix_down.print(sensor_data.height);
      page = true;
    }

    matrix_up.writeDisplay();
    matrix_down.writeDisplay();

    delay(5000);
  }
}

