/*
  MQTT Discovery for Home Assistant
  
  Publishes discovery configurations for all Metriful MS430 sensors
  so they auto-configure in Home Assistant.
  
  Discovery topic format: homeassistant/sensor/metriful/<sensor_id>/config
  Availability topic: home/office/status
*/

// Device configuration
#define DEVICE_ID "metriful"
#define DEVICE_NAME "metriful"
#define DEVICE_MANUFACTURER "Metriful"
#define DEVICE_MODEL "MS430"

// Discovery topic prefix
#define DISCOVERY_PREFIX "homeassistant"

// Larger buffer for discovery payloads
StaticJsonDocument<512> discoveryDoc;
char discoveryBuffer[512];
char discoveryTopicBuffer[96];
char uniqueIdBuffer[48];

//////////////////////////////////////////////////////////
// Discovery Helper Functions
//////////////////////////////////////////////////////////

// Build discovery topic: homeassistant/sensor/metriful/<sensor_id>/config
void buildDiscoveryTopic(const char* sensorId) {
  snprintf(discoveryTopicBuffer, sizeof(discoveryTopicBuffer), 
           "%s/sensor/%s/%s/config", DISCOVERY_PREFIX, DEVICE_ID, sensorId);
}

// Build unique ID: metriful_<sensor_id>
void buildUniqueId(const char* sensorId) {
  snprintf(uniqueIdBuffer, sizeof(uniqueIdBuffer), "%s_%s", DEVICE_ID, sensorId);
}

// Build state topic: home/office/<category>/<measurement>
// Reuses topicBuffer from publish_environmental_data.ino
void buildStateTopicForDiscovery(const char* category, const char* measurement) {
  snprintf(topicBuffer, sizeof(topicBuffer), "%s/%s/%s", MQTT_BASE_TOPIC, category, measurement);
}

// Add device info to discovery document (call after setting up sensor-specific fields)
void addDeviceInfo() {
  JsonObject device = discoveryDoc.createNestedObject("device");
  JsonArray identifiers = device.createNestedArray("identifiers");
  identifiers.add(DEVICE_ID);
  device["name"] = DEVICE_NAME;
  device["manufacturer"] = DEVICE_MANUFACTURER;
  device["model"] = DEVICE_MODEL;
}

// Add availability info to discovery document
void addAvailabilityInfo() {
  discoveryDoc["availability_topic"] = AVAILABILITY_TOPIC;
  discoveryDoc["payload_available"] = PAYLOAD_AVAILABLE;
  discoveryDoc["payload_not_available"] = PAYLOAD_NOT_AVAILABLE;
}

// Publish discovery config for a measurement sensor
void publishMeasurementDiscovery(MQTTClient* client, 
                                  const char* name,
                                  const char* sensorId,
                                  const char* category,
                                  const char* measurement,
                                  const char* deviceClass,
                                  const char* unit,
                                  const char* icon) {
  discoveryDoc.clear();
  
  discoveryDoc["name"] = name;
  
  buildUniqueId(sensorId);
  discoveryDoc["unique_id"] = uniqueIdBuffer;
  
  buildStateTopicForDiscovery(category, measurement);
  discoveryDoc["state_topic"] = topicBuffer;
  
  discoveryDoc["value_template"] = "{{ value_json.value }}";
  
  if (unit != NULL && strlen(unit) > 0) {
    discoveryDoc["unit_of_measurement"] = unit;
  }
  
  if (deviceClass != NULL) {
    discoveryDoc["device_class"] = deviceClass;
  }
  
  discoveryDoc["state_class"] = "measurement";
  discoveryDoc["icon"] = icon;
  
  addAvailabilityInfo();
  addDeviceInfo();
  
  serializeJson(discoveryDoc, discoveryBuffer);
  buildDiscoveryTopic(sensorId);
  client->publish(discoveryTopicBuffer, discoveryBuffer, true, 0);
  
  Serial.print("Discovery: ");
  Serial.println(discoveryTopicBuffer);
}

// Publish discovery config for an enum sensor
void publishEnumDiscovery(MQTTClient* client,
                          const char* name,
                          const char* sensorId,
                          const char* category,
                          const char* measurement,
                          const char* icon,
                          const char** options,
                          uint8_t optionCount) {
  discoveryDoc.clear();
  
  discoveryDoc["name"] = name;
  
  buildUniqueId(sensorId);
  discoveryDoc["unique_id"] = uniqueIdBuffer;
  
  buildStateTopicForDiscovery(category, measurement);
  discoveryDoc["state_topic"] = topicBuffer;
  
  discoveryDoc["value_template"] = "{{ value_json.value }}";
  discoveryDoc["device_class"] = "enum";
  discoveryDoc["icon"] = icon;
  
  JsonArray optionsArray = discoveryDoc.createNestedArray("options");
  for (uint8_t i = 0; i < optionCount; i++) {
    optionsArray.add(options[i]);
  }
  
  addAvailabilityInfo();
  addDeviceInfo();
  
  serializeJson(discoveryDoc, discoveryBuffer);
  buildDiscoveryTopic(sensorId);
  client->publish(discoveryTopicBuffer, discoveryBuffer, true, 0);
  
  Serial.print("Discovery: ");
  Serial.println(discoveryTopicBuffer);
}

//////////////////////////////////////////////////////////
// Master Discovery Function
//////////////////////////////////////////////////////////

void publishAllDiscoveryConfigs(MQTTClient* client) {
  Serial.println("--- Publishing MQTT Discovery configs ---");
  
  // Temperature unit depends on USE_METRIC_UNITS
  #if USE_METRIC_UNITS
    const char* tempUnit = "°C";
  #else
    const char* tempUnit = "°F";
  #endif
  
  // Climate sensors
  publishMeasurementDiscovery(client, "Temperature", "temperature", "climate", "temperature",
                               "temperature", tempUnit, "mdi:thermometer");
  delay(50);
  
  publishMeasurementDiscovery(client, "Pressure", "pressure", "climate", "pressure",
                               "atmospheric_pressure", "Pa", "mdi:gauge");
  delay(50);
  
  publishMeasurementDiscovery(client, "Humidity", "humidity", "climate", "humidity",
                               "humidity", "%", "mdi:water-percent");
  delay(50);
  
  publishMeasurementDiscovery(client, "Gas Resistance", "gas_resistance", "climate", "gas_resistance",
                               NULL, "Ω", "mdi:resistor");
  delay(50);
  
  // Air quality sensors
  publishMeasurementDiscovery(client, "Air Quality Index", "aqi", "airquality", "aqi",
                               "aqi", NULL, "mdi:air-filter");
  delay(50);
  
  const char* accuracyOptions[] = {"not_yet_valid", "low", "medium", "high"};
  publishEnumDiscovery(client, "Air Quality Accuracy", "accuracy", "airquality", "accuracy",
                       "mdi:crosshairs-question", accuracyOptions, 4);
  delay(50);
  
  // Light sensors
  publishMeasurementDiscovery(client, "Illuminance", "illuminance", "light", "illuminance",
                               "illuminance", "lx", "mdi:brightness-5");
  delay(50);
  
  publishMeasurementDiscovery(client, "White Level", "white_level", "light", "white_level",
                               NULL, NULL, "mdi:circle-outline");
  delay(50);
  
  // Sound sensors
  publishMeasurementDiscovery(client, "Sound Pressure Level", "spl", "sound", "spl",
                               "sound_pressure", "dBA", "mdi:volume-high");
  delay(50);
  
  publishMeasurementDiscovery(client, "Peak Amplitude", "peak_amplitude", "sound", "peak_amplitude",
                               NULL, "mPa", "mdi:waveform");
  delay(50);
  
  // Sound frequency bands
  publishMeasurementDiscovery(client, "Sound Band 125Hz", "band_125hz", "sound", "band_125hz",
                               NULL, "dB", "mdi:sine-wave");
  delay(50);
  
  publishMeasurementDiscovery(client, "Sound Band 250Hz", "band_250hz", "sound", "band_250hz",
                               NULL, "dB", "mdi:sine-wave");
  delay(50);
  
  publishMeasurementDiscovery(client, "Sound Band 500Hz", "band_500hz", "sound", "band_500hz",
                               NULL, "dB", "mdi:sine-wave");
  delay(50);
  
  publishMeasurementDiscovery(client, "Sound Band 1000Hz", "band_1000hz", "sound", "band_1000hz",
                               NULL, "dB", "mdi:sine-wave");
  delay(50);
  
  publishMeasurementDiscovery(client, "Sound Band 2000Hz", "band_2000hz", "sound", "band_2000hz",
                               NULL, "dB", "mdi:sine-wave");
  delay(50);
  
  publishMeasurementDiscovery(client, "Sound Band 4000Hz", "band_4000hz", "sound", "band_4000hz",
                               NULL, "dB", "mdi:sine-wave");
  delay(50);
  
  // Particle sensors (only if connected)
  if (PARTICLE_SENSOR != PARTICLE_SENSOR_OFF) {
    publishMeasurementDiscovery(client, "Particle Duty Cycle", "duty_cycle", "particles", "duty_cycle",
                                 NULL, "%", "mdi:blur");
    delay(50);
    
    publishMeasurementDiscovery(client, "Particle Concentration", "concentration", "particles", "concentration",
                                 NULL, "ppL", "mdi:blur-radial");
    delay(50);
  }
  
  Serial.println("--- Discovery configs published ---");
}

// Publish availability status
void publishAvailability(MQTTClient* client, bool online) {
  const char* status = online ? PAYLOAD_AVAILABLE : PAYLOAD_NOT_AVAILABLE;
  client->publish(AVAILABILITY_TOPIC, status, true, 0);
  
  Serial.print("Availability: ");
  Serial.println(status);
}
