# Metriful MS430 MQTT Sensor for Arduino UNO R4 WiFi

An Arduino sketch that reads environmental data from a [Metriful MS430](https://github.com/metriful/sensor) sensor and publishes it to an MQTT broker with Home Assistant auto-discovery support.

## Features

- **Multi-sensor environmental monitoring**: Temperature, humidity, pressure, gas resistance, air quality index, illuminance, sound levels, and particle concentration
- **MQTT Integration**: Publishes sensor data as JSON to configurable MQTT topics
- **Home Assistant Auto-Discovery**: Sensors automatically appear in Home Assistant with proper device classes, units, and icons
- **Availability Tracking**: Reports online/offline status with Last Will and Testament (LWT)
- **Automatic Recovery**: Detects sensor failures and performs automatic resets
- **Configurable Units**: Switch between metric (Celsius) and imperial (Fahrenheit) units

## Hardware Requirements

- Arduino UNO R4 WiFi
- Metriful MS430 Environmental Sensor
- Optional: Particle sensor (PPD42 or SDS011)

## Dependencies

Install the following libraries via the Arduino Library Manager:

- [WiFiS3](https://www.arduino.cc/reference/en/libraries/wifinina/) - Built into Arduino UNO R4 WiFi board package
- [MQTT](https://github.com/256dpi/arduino-mqtt) by Joel Gaehwiler
- [ArduinoJson](https://arduinojson.org/) by Benoit Blanchon
- [Metriful_sensor](https://github.com/metriful/sensor) - Metriful MS430 library

## Configuration

### 1. Create `arduino_secrets.h`

Create a file named `arduino_secrets.h` in the sketch directory with your credentials:

```cpp
// arduino_secrets.h
#define SECRET_SSID "YourWiFiSSID"
#define SECRET_PASS "YourWiFiPassword"

// MQTT topic base path
#define MQTT_LOCATION "office"
#define MQTT_BASE_TOPIC "home/" MQTT_LOCATION
```

### 2. Configure MQTT Broker

In `sensor_mqtt.ino`, update the MQTT broker IP address (line 150):

```cpp
IPAddress ipAddr(192, 168, 4, 53);  // Your MQTT broker IP
```

And the MQTT credentials in the `connect()` function (line 274):

```cpp
mqtt_client.connect("arduino", "your_username", "your_password")
```

### 3. User Settings

Edit these settings in `sensor_mqtt.ino`:

| Setting | Default | Description |
|---------|---------|-------------|
| `USE_METRIC_UNITS` | `false` | Set to `true` for Celsius, `false` for Fahrenheit |
| `cycle_period` | `CYCLE_PERIOD_100_S` | Reading interval: `CYCLE_PERIOD_3_S`, `CYCLE_PERIOD_100_S`, or `CYCLE_PERIOD_300_S` |
| `printDataAsColumns` | `false` | Serial output format |

### 4. Particle Sensor (Optional)

Configure your particle sensor model in the Metriful library's `Metriful_sensor.h`:
- `PARTICLE_SENSOR_OFF` - No particle sensor
- `PARTICLE_SENSOR_PPD42` - Shinyei PPD42
- `PARTICLE_SENSOR_SDS011` - Nova SDS011

## MQTT Topics

All sensor data is published under the base topic (default: `home/office/`):

### Climate
| Topic | Description | Unit |
|-------|-------------|------|
| `home/office/climate/temperature` | Air temperature | F or C |
| `home/office/climate/pressure` | Atmospheric pressure | Pa |
| `home/office/climate/humidity` | Relative humidity | % |
| `home/office/climate/gas_resistance` | Gas sensor resistance | Ohm |

### Air Quality
| Topic | Description | Unit |
|-------|-------------|------|
| `home/office/airquality/aqi` | Air Quality Index | - |
| `home/office/airquality/accuracy` | AQI calibration status | enum |

### Light
| Topic | Description | Unit |
|-------|-------------|------|
| `home/office/light/illuminance` | Light level | lx |
| `home/office/light/white_level` | White light level | - |

### Sound
| Topic | Description | Unit |
|-------|-------------|------|
| `home/office/sound/spl` | Sound pressure level | dBA |
| `home/office/sound/peak_amplitude` | Peak sound amplitude | mPa |
| `home/office/sound/band_125hz` | 125 Hz band level | dB |
| `home/office/sound/band_250hz` | 250 Hz band level | dB |
| `home/office/sound/band_500hz` | 500 Hz band level | dB |
| `home/office/sound/band_1000hz` | 1000 Hz band level | dB |
| `home/office/sound/band_2000hz` | 2000 Hz band level | dB |
| `home/office/sound/band_4000hz` | 4000 Hz band level | dB |

### Particles (if sensor connected)
| Topic | Description | Unit |
|-------|-------------|------|
| `home/office/particles/duty_cycle` | Particle duty cycle | % |
| `home/office/particles/concentration` | Particle concentration | ppL |

### Availability
| Topic | Description |
|-------|-------------|
| `home/office/status` | Device availability (`online`/`offline`) |

## Message Format

All sensor values are published as JSON:

```json
{"value": 72.5}
```

## Home Assistant Integration

The sketch automatically publishes MQTT Discovery configurations on startup. Sensors will appear automatically under a device named "metriful" in Home Assistant.

Discovery topics follow the pattern:
```
homeassistant/sensor/metriful/<sensor_id>/config
```

### Device Info
- **Name**: metriful
- **Manufacturer**: Metriful
- **Model**: MS430

## Error Handling

The sketch includes automatic recovery mechanisms:

1. **Sensor Failure Detection**: If the MS430 returns all zeros (indicating a communication failure), the sketch attempts to reset the sensor
2. **Progressive Recovery**: After 3 consecutive failures, the Arduino performs a full software reset
3. **MQTT Reconnection**: Automatically reconnects to the MQTT broker if the connection is lost
4. **Last Will and Testament**: If the Arduino disconnects unexpectedly, the broker publishes `offline` to the availability topic

## File Structure

```
sensor_mqtt/
├── sensor_mqtt.ino              # Main sketch: setup, loop, WiFi/MQTT connection
├── wifi.ino                     # WiFi utility functions
├── publish_environmental_data.ino  # MQTT publishing functions for sensor data
├── z_mqtt_discovery.ino         # Home Assistant MQTT Discovery configuration
├── arduino_secrets.h            # Credentials (not in git)
└── README.md
```

## Serial Output

The sketch outputs debug information to the serial monitor at 9600 baud:
- WiFi connection status and IP address
- MQTT connection status
- Published topics and values
- Sensor failure warnings

## Notes

- The air quality index requires several minutes of self-calibration before returning valid data. The `accuracy` sensor indicates calibration status.
- Particle sensor data requires approximately one minute to stabilize due to low-pass filtering.
- The sketch uses the ready pin interrupt from the MS430 to know when new data is available.

## License

See the [Metriful sensor repository](https://github.com/metriful/sensor) for sensor library licensing.

## Resources

- [Metriful MS430 Datasheet & User Guide](https://github.com/metriful/sensor)
- [Arduino UNO R4 WiFi Documentation](https://docs.arduino.cc/tutorials/uno-r4-wifi/wifi-examples)
- [Home Assistant MQTT Discovery](https://www.home-assistant.io/docs/mqtt/discovery/)
