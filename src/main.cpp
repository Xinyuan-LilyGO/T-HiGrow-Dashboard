/*
This example shows how to connect to Cayenne using an LilyGO T-HiGrow and send/receive sample data.

1. Install [Visual Studio Code](https://code.visualstudio.com/) on your pc, Install PlatformIO in the extension, or you can install [PlatformIO IDE](https://platformio.org/platformio-ide),it's built on top of [Microsoft's Visual Studio Code](https://code.visualstudio.com/)
2. Set the Cayenne authentication info to match the authentication info from the Dashboard.If you don't have an account, then [create an account](https://accounts.mydevices.com/auth/realms/cayenne/login-actions/registration?client_id=cayenne-web-app&tab_id=01AaoLwmlng)
3. After logging in, you will see the dashboard, click **Add new** in the upper left,Choose **Generic ESP8266**,Then you can see the MQTT username, password and ID you need to use,Replace it with the definition in `main.cpp`
4. Set the network name and password.
5. In `platformio.ini` you need to change the port number corresponding to your board
6. Compile and upload the sketch.(The arrow at the bottom left of the IDE editor)
7. A temporary widget will be automatically generated in the Cayenne Dashboard. To make the widget permanent click the plus sign on the widget.

2019/12/23 lewis he
*/

//#define CAYENNE_DEBUG
#define CAYENNE_PRINT Serial
#include <CayenneMQTTESP32.h>
#include <BH1750.h>
#include <Adafruit_BME280.h>
#include <OneButton.h>
#include <DHT12.h>

#define I2C_SDA             25
#define I2C_SCL             26
#define DHT12_PIN           16
#define BAT_ADC             33
#define SALT_PIN            34
#define SOIL_PIN            32
#define BOOT_PIN            0
#define POWER_CTRL          4
#define USER_BUTTON         35
#define DS18B20_PIN         21                  //18b20 data pin


// WiFi network info.
char ssid[] = "Xiaomi";
char wifiPassword[] = "12345678";

// Cayenne authentication info. This should be obtained from the Cayenne Dashboard.
char username[] = "b8bfdf70-22ec-11ea-ba7c-716e7f5ba423";
char password[] = "ebad11e29c4128c518e8a6fed5b6dc7a61539cf5";
char clientID[] = "21d87b60-2523-11ea-8221-599f77add412";

#define TEMPERATURE_VIRTUAL_CHANNEL 1
#define BAROMETER_VIRTUAL_CHANNEL 2
#define ALTITUDE_VIRTUAL_CHANNEL  3
#define DHT12_TEMPERATURE_VIRTUAL_CHANNEL 4
#define DHT12_HUMIDITY_VIRTUAL_CHANNEL    5
#define BATTERY_VIRTUAL_CHANNEL     6
#define LIGHTSENSOR_VIRTUAL_CHANNEL 7
#define SOIL_VIRTUAL_CHANNEL  8
#define SALT_VIRTUAL_CHANNEL 9
#define PROXIMITY_VIRTUAL_CHANNEL 10

BH1750 lightMeter(0x23); //0x23
Adafruit_BME280 bme;     //0x77
DHT12 dht12((int)DHT12_PIN, true);
OneButton button(USER_BUTTON, true);

bool lightSensorDetected = true;
bool bmeSensorDetected = true;
bool buttonClick = false;

void setup()
{
    Serial.begin(115200);
    Cayenne.begin(username, password, clientID, ssid, wifiPassword);

    Wire.begin(I2C_SDA, I2C_SCL);

    //! Sensor power control pin , use deteced must set high
    pinMode(POWER_CTRL, OUTPUT);
    digitalWrite(POWER_CTRL, 1);
    delay(200);

    dht12.begin();

    if (!bme.begin()) {
        Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
        bmeSensorDetected = false;
    }

    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
        Serial.println(F("Error initialising BH1750"));
        lightSensorDetected = false;
    }

    button.attachLongPressStart([] {
        Cayenne.digitalSensorWrite(PROXIMITY_VIRTUAL_CHANNEL, 1);

    });
    button.attachLongPressStop([] {
        Cayenne.digitalSensorWrite(PROXIMITY_VIRTUAL_CHANNEL, 0);
    });
}


void loop()
{
    button.tick();
    Cayenne.loop();
}


uint32_t readSalt()
{
    uint8_t samples = 120;
    uint32_t humi = 0;
    uint16_t array[120];

    for (int i = 0; i < samples; i++) {
        array[i] = analogRead(SALT_PIN);
        delay(2);
    }
    std::sort(array, array + samples);
    for (int i = 0; i < samples; i++) {
        if (i == 0 || i == samples - 1)continue;
        humi += array[i];
    }
    humi /= samples - 2;
    return humi;
}

uint16_t readSoil()
{
    uint16_t soil = analogRead(SOIL_PIN);
    return map(soil, 0, 4095, 100, 0);
}

float readBattery()
{
    int vref = 1100;
    uint16_t volt = analogRead(BAT_ADC);
    float battery_voltage = ((float)volt / 4095.0) * 2.0 * 3.3 * (vref);
    return battery_voltage;
}


// Default function for processing actuator commands from the Cayenne Dashboard.
// You can also use functions for specific channels, e.g CAYENNE_IN(1) for channel 1 commands.
CAYENNE_IN_DEFAULT()
{
    CAYENNE_LOG("Channel %u, value %s", request.channel, getValue.asString());
    //Process message here. If there is an error set an error message using getValue.setError(), e.g getValue.setError("Error message");
}

CAYENNE_IN(1)
{
    CAYENNE_LOG("Channel %u, value %s", request.channel, getValue.asString());
}


// This function is called at intervals to send temperature sensor data to Cayenne.
CAYENNE_OUT(TEMPERATURE_VIRTUAL_CHANNEL)
{
    if (bmeSensorDetected) {
        float bmeTemperature = bme.readTemperature();
        Cayenne.celsiusWrite(TEMPERATURE_VIRTUAL_CHANNEL, bmeTemperature);
    }
}

// This function is called at intervals to send barometer sensor data to Cayenne.
CAYENNE_OUT(BAROMETER_VIRTUAL_CHANNEL)
{
    if (bmeSensorDetected) {
        float bmePressure = (bme.readPressure() / 100.0F);
        Cayenne.hectoPascalWrite(BAROMETER_VIRTUAL_CHANNEL, bmePressure);
    }
}


CAYENNE_OUT(ALTITUDE_VIRTUAL_CHANNEL)
{
    if (bmeSensorDetected) {
        float bmeAltitude = bme.readAltitude(1013.25);
        Cayenne.digitalSensorWrite(ALTITUDE_VIRTUAL_CHANNEL, bmeAltitude);
    }
}

CAYENNE_OUT(DHT12_TEMPERATURE_VIRTUAL_CHANNEL)
{
    float t12 = dht12.readTemperature();
    if (!isnan(t12)) {
        Cayenne.celsiusWrite(DHT12_TEMPERATURE_VIRTUAL_CHANNEL, t12);
    }
}

CAYENNE_OUT(DHT12_HUMIDITY_VIRTUAL_CHANNEL)
{
    float h12 = dht12.readHumidity();
    if (!isnan(h12)) {
        Cayenne.virtualWrite(DHT12_HUMIDITY_VIRTUAL_CHANNEL, h12, TYPE_RELATIVE_HUMIDITY, UNIT_PERCENT);
    }
}

CAYENNE_OUT(BATTERY_VIRTUAL_CHANNEL)
{
    float mv = readBattery();
    Cayenne.virtualWrite(BATTERY_VIRTUAL_CHANNEL, mv, TYPE_VOLTAGE, UNIT_MILLIVOLTS);
}


CAYENNE_OUT(LIGHTSENSOR_VIRTUAL_CHANNEL)
{
    if (lightSensorDetected) {
        float lux = lightMeter.readLightLevel();
        Cayenne.luxWrite(LIGHTSENSOR_VIRTUAL_CHANNEL, lux);
    }
}

CAYENNE_OUT(SOIL_VIRTUAL_CHANNEL)
{
    uint16_t soil = readSoil();
    Cayenne.virtualWrite(SOIL_VIRTUAL_CHANNEL, soil, TYPE_RELATIVE_HUMIDITY, UNIT_PERCENT);
}

CAYENNE_OUT(SALT_VIRTUAL_CHANNEL)
{
    uint16_t salt = readSalt();
    Cayenne.virtualWrite(SALT_VIRTUAL_CHANNEL, salt, TYPE_RELATIVE_HUMIDITY, UNIT_PERCENT);
}
