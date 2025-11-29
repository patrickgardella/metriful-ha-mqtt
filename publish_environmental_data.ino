/*
  Publish environmental data from Metriful MS430 sensor to MQTT.
  
  Each measurement is published as JSON: {"value": 70.5, "unit": "F"}
  
  Topic structure: home/office/<category>/<measurement>
  Categories: climate, airquality, light, sound, particles
*/

// Reusable JSON document - sized for simple payloads
StaticJsonDocument<96> jsonDoc;
char jsonBuffer[96];
char topicBuffer[64];

//////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////

// Publish a float value to MQTT
void publishValue(MQTTClient* client, const char* topic, float value) {
  jsonDoc.clear();
  jsonDoc["value"] = value;
  
  serializeJson(jsonDoc, jsonBuffer);
  client->publish(topic, jsonBuffer);
  
  // Debug output
  Serial.print("MQTT: ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(jsonBuffer);
}

// Publish an integer value to MQTT
void publishIntValue(MQTTClient* client, const char* topic, int32_t value) {
  jsonDoc.clear();
  jsonDoc["value"] = value;
  
  serializeJson(jsonDoc, jsonBuffer);
  client->publish(topic, jsonBuffer);
  
  // Debug output
  Serial.print("MQTT: ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(jsonBuffer);
}

// Publish a string value with unit to MQTT (for accuracy labels, etc.)
void publishStringValue(MQTTClient* client, const char* topic, const char* value) {
  jsonDoc.clear();
  jsonDoc["value"] = value;
  
  serializeJson(jsonDoc, jsonBuffer);
  client->publish(topic, jsonBuffer);
  
  // Debug output
  Serial.print("MQTT: ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(jsonBuffer);
}

// Build topic path: home/office/<category>/<measurement>
void buildTopic(const char* category, const char* measurement) {
  snprintf(topicBuffer, sizeof(topicBuffer), "%s/%s/%s", MQTT_BASE_TOPIC, category, measurement);
}

//////////////////////////////////////////////////////////
// Climate Functions (AirData_t)
//////////////////////////////////////////////////////////

void publishTemperature(MQTTClient* client, const AirData_t* data) {
  uint8_t intPart = 0;
  uint8_t fractionalPart = 0;
  bool isPositive = true;
  const char* unit = getTemperature(data, &intPart, &fractionalPart, &isPositive);
  
  float value = (float)intPart + (float)fractionalPart / 10.0f;
  if (!isPositive) value = -value;
  
  // Convert units if needed based on USE_METRIC_UNITS setting
  const char* displayUnit;
  #if USE_METRIC_UNITS
    // Metriful library returns based on TEMPERATURE_UNIT setting
    // If library is set to Fahrenheit but we want Celsius, convert
    if (unit[0] == 'F' || unit[0] == 'f') {
      value = (value - 32.0f) * 5.0f / 9.0f;
    }
    displayUnit = "C";
  #else
    // If library is set to Celsius but we want Fahrenheit, convert
    if (unit[0] == 'C' || unit[0] == 'c') {
      value = value * 9.0f / 5.0f + 32.0f;
    }
    displayUnit = "F";
  #endif
  
  buildTopic("climate", "temperature");
  publishValue(client, topicBuffer, value);
}

void publishPressure(MQTTClient* client, const AirData_t* data) {
  // Always publish in Pascals (metric) for Home Assistant discovery compatibility
  float value = (float)data->P_Pa;
  
  buildTopic("climate", "pressure");
  publishValue(client, topicBuffer, value);
}

void publishHumidity(MQTTClient* client, const AirData_t* data) {
  float value = (float)data->H_pc_int + (float)data->H_pc_fr_1dp / 10.0f;
  
  buildTopic("climate", "humidity");
  publishValue(client, topicBuffer, value);
}

void publishGasResistance(MQTTClient* client, const AirData_t* data) {
  buildTopic("climate", "gas_resistance");
  publishIntValue(client, topicBuffer, data->G_ohm);
}

//////////////////////////////////////////////////////////
// Air Quality Functions (AirQualityData_t)
//////////////////////////////////////////////////////////

void publishAQI(MQTTClient* client, const AirQualityData_t* data) {
  float value = (float)data->AQI_int + (float)data->AQI_fr_1dp / 10.0f;
  
  buildTopic("airquality", "aqi");
  publishValue(client, topicBuffer, value);
}

void publishAirQualityAccuracy(MQTTClient* client, const AirQualityData_t* data) {
  const char* accuracyLabels[] = {
    "not_yet_valid",
    "low",
    "medium", 
    "high"
  };
  
  uint8_t accuracy = data->AQI_accuracy;
  if (accuracy > 3) accuracy = 0;
  
  buildTopic("airquality", "accuracy");
  publishStringValue(client, topicBuffer, accuracyLabels[accuracy]);
}

//////////////////////////////////////////////////////////
// Light Functions (LightData_t)
//////////////////////////////////////////////////////////

void publishIlluminance(MQTTClient* client, const LightData_t* data) {
  float value = (float)data->illum_lux_int + (float)data->illum_lux_fr_2dp / 100.0f;
  
  buildTopic("light", "illuminance");
  publishValue(client, topicBuffer, value);
}

void publishWhiteLevel(MQTTClient* client, const LightData_t* data) {
  buildTopic("light", "white_level");
  publishIntValue(client, topicBuffer, data->white);
}

//////////////////////////////////////////////////////////
// Sound Functions (SoundData_t)
//////////////////////////////////////////////////////////

void publishSoundLevel(MQTTClient* client, const SoundData_t* data) {
  float value = (float)data->SPL_dBA_int + (float)data->SPL_dBA_fr_1dp / 10.0f;
  
  buildTopic("sound", "spl");
  publishValue(client, topicBuffer, value);
}

void publishPeakAmplitude(MQTTClient* client, const SoundData_t* data) {
  float value = (float)data->peak_amp_mPa_int + (float)data->peak_amp_mPa_fr_2dp / 100.0f;
  
  buildTopic("sound", "peak_amplitude");
  publishValue(client, topicBuffer, value);
}

void publishSoundBands(MQTTClient* client, const SoundData_t* data) {
  const char* bandTopics[] = {
    "band_125hz",
    "band_250hz",
    "band_500hz",
    "band_1000hz",
    "band_2000hz",
    "band_4000hz"
  };
  
  for (uint8_t i = 0; i < SOUND_FREQ_BANDS; i++) {
    float value = (float)data->SPL_bands_dB_int[i] + (float)data->SPL_bands_dB_fr_1dp[i] / 10.0f;
    
    buildTopic("sound", bandTopics[i]);
    publishValue(client, topicBuffer, value);
  }
}

//////////////////////////////////////////////////////////
// Particle Functions (ParticleData_t)
//////////////////////////////////////////////////////////

void publishParticleDutyCycle(MQTTClient* client, const ParticleData_t* data) {
  float value = (float)data->duty_cycle_pc_int + (float)data->duty_cycle_pc_fr_2dp / 100.0f;
  
  buildTopic("particles", "duty_cycle");
  publishValue(client, topicBuffer, value);
}

void publishParticleConcentration(MQTTClient* client, const ParticleData_t* data) {
  float value = (float)data->concentration_int + (float)data->concentration_fr_2dp / 100.0f;
  
  buildTopic("particles", "concentration");
  publishValue(client, topicBuffer, value);
}

//////////////////////////////////////////////////////////
// Master Publish Function
//////////////////////////////////////////////////////////

void publishAllSensorData(MQTTClient* client, 
                          const AirData_t* airData,
                          const AirQualityData_t* airQualityData,
                          const LightData_t* lightData,
                          const SoundData_t* soundData,
                          const ParticleData_t* particleData) {
  
  Serial.println("--- Publishing all sensor data to MQTT ---");
  
  // Climate data
  publishTemperature(client, airData);
  publishPressure(client, airData);
  publishHumidity(client, airData);
  publishGasResistance(client, airData);
  
  // Air quality data (only if calibration is complete)
  publishAQI(client, airQualityData);
  publishAirQualityAccuracy(client, airQualityData);
  
  // Light data
  publishIlluminance(client, lightData);
  publishWhiteLevel(client, lightData);
  
  // Sound data
  publishSoundLevel(client, soundData);
  publishPeakAmplitude(client, soundData);
  publishSoundBands(client, soundData);
  
  // Particle data (only if sensor is connected)
  if (PARTICLE_SENSOR != PARTICLE_SENSOR_OFF) {
    publishParticleDutyCycle(client, particleData);
    publishParticleConcentration(client, particleData);
  }
  
  Serial.println("--- Done publishing sensor data ---");
}
