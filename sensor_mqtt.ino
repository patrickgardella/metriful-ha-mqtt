/*
  Query Metriful MS430 in cycle mode.

  Transmit environmental data in a repeating cycle to an MQTT Queue. User can choose 
  from a cycle time period of 3, 100, or 300 seconds. Also view the output 
  in the Serial Monitor.

  For code examples, datasheet and user guide, visit
  https://github.com/metriful/sensor

  Find the full UNO R4 WiFi Network documentation here:
  https://docs.arduino.cc/tutorials/uno-r4-wifi/wifi-examples#connect-with-wpa
 */

#include <SPI.h>
#include <WiFiS3.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <Metriful_sensor.h>

#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;  // the WiFi radio's status

//////////////////////////////////////////////////////////
// USER-EDITABLE SETTINGS

#define USE_METRIC_UNITS false

// Availability configuration (for MQTT Discovery)
#define AVAILABILITY_TOPIC MQTT_BASE_TOPIC "/status"
#define PAYLOAD_AVAILABLE "online"
#define PAYLOAD_NOT_AVAILABLE "offline"

// How often to read data (every 3, 100, or 300 seconds)
uint8_t cycle_period = CYCLE_PERIOD_100_S;

// How to print the data over the serial port.
// If printDataAsColumns = true, data are columns of numbers, useful
// to copy/paste to a spreadsheet application. Otherwise, data are
// printed with explanatory labels and units.
bool printDataAsColumns = false;

// END OF USER-EDITABLE SETTINGS
//////////////////////////////////////////////////////////

// Structs for MQTT Client
WiFiClient net;
MQTTClient mqtt_client;
unsigned long lastMillis = 0;

// Structs for data
AirData_t airData = { 0 };
AirQualityData_t airQualityData = { 0 };
LightData_t lightData = { 0 };
SoundData_t soundData = { 0 };
ParticleData_t particleData = { 0 };

char strbuff[100] = { 0 };

// Counter for consecutive sensor failures
uint8_t sensorFailureCount = 0;
const uint8_t MAX_SENSOR_FAILURES = 3;

//////////////////////////////////////////////////////////
// Sensor Health Check and Reset Functions
//////////////////////////////////////////////////////////

// Check if sensor data is invalid (all zeros indicates sensor failure)
bool isDataInvalid(const AirData_t* airData) {
  return (airData->P_Pa == 0 && 
          airData->H_pc_int == 0 && 
          airData->G_ohm == 0);
}

// Reset and reinitialize the Metriful MS430 sensor
void resetMetrifulSensor() {
  Serial.println("Resetting Metriful MS430...");
  
  // Send reset command
  TransmitI2C(I2C_ADDRESS, RESET_CMD, 0, 0);
  delay(5);
  
  // Wait for reset completion
  while (digitalRead(READY_PIN) == HIGH) {
    yield();
  }
  
  // Reconfigure the sensor
  uint8_t particleSensor = PARTICLE_SENSOR;
  TransmitI2C(I2C_ADDRESS, PARTICLE_SENSOR_SELECT_REG, &particleSensor, 1);
  TransmitI2C(I2C_ADDRESS, CYCLE_TIME_PERIOD_REG, &cycle_period, 1);
  
  // Restart cycle mode
  ready_assertion_event = false;
  TransmitI2C(I2C_ADDRESS, CYCLE_MODE_CMD, 0, 0);
  
  Serial.println("Metriful MS430 reset complete.");
}

// Perform a full software reset of the Arduino
void softwareReset() {
  Serial.println("Performing full Arduino software reset...");
  Serial.flush();  // Ensure serial output is sent before reset
  delay(100);
  NVIC_SystemReset();  // ARM Cortex-M4 reset function (UNO R4 WiFi)
}

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware from version: " + fv);
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
  printCurrentNet();
  printWifiData();

  //setup MQTT Client
  // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported
  // by Arduino. You need to set the IP address directly.
  IPAddress brokerIp SECRET_MQTT_BROKER_IP;
  mqtt_client.begin(brokerIp, net);

  // Connect to MQTT Broker
  connect();

  // Publish MQTT Discovery configurations (retained)
  publishAllDiscoveryConfigs(&mqtt_client);

  // Publish online availability status (retained)
  publishAvailability(&mqtt_client, true);

  // Initialize the host pins, set up the serial port and reset:
  SensorHardwareSetup(I2C_ADDRESS);

  // Apply chosen settings to the MS430
  uint8_t particleSensor = PARTICLE_SENSOR;
  TransmitI2C(I2C_ADDRESS, PARTICLE_SENSOR_SELECT_REG, &particleSensor, 1);
  TransmitI2C(I2C_ADDRESS, CYCLE_TIME_PERIOD_REG, &cycle_period, 1);

  Serial.println("Entering cycle mode and waiting for data.");
  ready_assertion_event = false;
  TransmitI2C(I2C_ADDRESS, CYCLE_MODE_CMD, 0, 0);
}

void loop() {
  printIPData();

  // Execute the MQTT client loop
  mqtt_client.loop();

  // Check on MQTT broker connection and reconnect if needed
  if (!mqtt_client.connected()) {
    connect();
  }

  // Data from Metriful is ready
  // Wait for the next new data release, indicated by a falling edge on READY
  while (!ready_assertion_event) {
    // Execute the MQTT client loop
    mqtt_client.loop();

    // Check on MQTT broker connection and reconnect if needed
    if (!mqtt_client.connected()) {
      connect();
    }

    yield();
  }
  ready_assertion_event = false;

  // Read data from the MS430 into the data structs.

  // Air data
  // Choose output temperature unit (C or F) in Metriful_sensor.h
  airData = getAirData(I2C_ADDRESS);

  // Check for invalid sensor data (all zeros) and reset if needed
  if (isDataInvalid(&airData)) {
    sensorFailureCount++;
    Serial.print("WARNING: Invalid sensor data detected (all zeros)! Failure count: ");
    Serial.println(sensorFailureCount);
    
    if (sensorFailureCount >= MAX_SENSOR_FAILURES) {
      Serial.println("Max sensor failures reached. Performing full Arduino reset...");
      softwareReset();  // This will restart the entire Arduino
      // Code will not continue past this point
    }
    
    // Try resetting just the Metriful sensor first
    resetMetrifulSensor();
    return;  // Skip this cycle and wait for next valid data
  }
  
  // Reset failure counter on successful read
  sensorFailureCount = 0;

  /* Air quality data
  The initial self-calibration of the air quality data may take several
  minutes to complete. During this time the accuracy parameter is zero 
  and the data values are not valid.
  */
  airQualityData = getAirQualityData(I2C_ADDRESS);

  // Light data
  lightData = getLightData(I2C_ADDRESS);

  // Sound data
  soundData = getSoundData(I2C_ADDRESS);

  /* Particle data
  This requires the connection of a particulate sensor (invalid 
  values will be obtained if this sensor is not present).
  Specify your sensor model (PPD42 or SDS011) in Metriful_sensor.h
  Also note that, due to the low pass filtering used, the 
  particle data become valid after an initial initialization 
  period of approximately one minute.
  */
  if (PARTICLE_SENSOR != PARTICLE_SENSOR_OFF) {
    particleData = getParticleData(I2C_ADDRESS);
  }

  // Print all data to the serial port
  //printAirData(&airData, printDataAsColumns);
  //printAirQualityData(&airQualityData, printDataAsColumns);
  //printLightData(&lightData, printDataAsColumns);
  //printSoundData(&soundData, printDataAsColumns);
  //if (PARTICLE_SENSOR != PARTICLE_SENSOR_OFF) {
  //  printParticleData(&particleData, printDataAsColumns, PARTICLE_SENSOR);
  //}

  // Publish all sensor data to MQTT
  publishAllSensorData(&mqtt_client, &airData, &airQualityData, &lightData, &soundData, &particleData);

  Serial.println();
}

void connect() {
  Serial.print("\nMQTT connecting...");

  // Set Last Will and Testament for availability
  // If connection drops unexpectedly, broker will publish "offline"
  mqtt_client.setWill(AVAILABILITY_TOPIC, PAYLOAD_NOT_AVAILABLE, true, 0);

  while (!mqtt_client.connect("arduino", SECRET_MQTT_USER, SECRET_MQTT_PASS)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");
}
