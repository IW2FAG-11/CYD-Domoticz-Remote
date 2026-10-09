#include "userExtensions.h"

/*
 *  User extension: BME280 (I2C) -> Domoticz Temp+Hum+Baro sensor
 *  ------------------------------------------------------------------
 *  Enable it from platformio.ini (in [env] -> build_flags) by
 *  uncommenting the BME280 flags, for example:
 *
 *      -DBME280                 # enable the feature
 *      -DBME280_IDX=123         # Domoticz idx of the Temp+Hum+Baro device
 *      -DBME280_SDA=27          # I2C SDA (CN1 connector on the CYD)
 *      -DBME280_SCL=22          # I2C SCL (CN1 connector on the CYD)
 *      -DBME280_ADDR=0x76       # I2C address (0x76 or 0x77)
 *      -DBME280_INTERVAL=60     # seconds between two updates
 *
 *  The values are pushed to Domoticz with:
 *      /json.htm?type=command&param=udevice&idx=IDX&nvalue=0
 *          &svalue=TEMP;HUM;HUM_STAT;BAR;BAR_FOR
 *  (HUM_STAT: 0=Normal 1=Comfortable 2=Dry 3=Wet
 *   BAR_FOR : 0=NoInfo 1=Sunny 2=PartlyCloudy 3=Cloudy 4=Rain)
 */

#ifdef BME280

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "../core/ip_engine.h"

// ------- Defaults (overridable from platformio.ini build_flags) -------
#ifndef BME280_IDX
    #define BME280_IDX 1
#endif
#ifndef BME280_SDA
    #define BME280_SDA 27
#endif
#ifndef BME280_SCL
    #define BME280_SCL 22
#endif
#ifndef BME280_ADDR
    #define BME280_ADDR 0x76
#endif
#ifndef BME280_INTERVAL
    #define BME280_INTERVAL 60
#endif
#ifndef BME280_TEMP_OFFSET
    #define BME280_TEMP_OFFSET 0.0f
#endif

static Adafruit_BME280 bme;
static bool bme_ok = false;
static bool bme_first = true;
static unsigned long bme_timer = 0;

// Compute the Domoticz humidity status (0..3)
static int bme_humidity_status(float h)
{
    if (h < 30.0f) return 2;            // Dry
    if (h > 70.0f) return 3;            // Wet
    if (h >= 40.0f && h <= 60.0f) return 1; // Comfortable
    return 0;                           // Normal
}

// Compute a simple barometer forecast (0..4)
static int bme_barometer_forecast(float p)
{
    if (p > 1020.0f) return 1;          // Sunny
    if (p > 1000.0f) return 2;          // Partly cloudy
    if (p > 990.0f)  return 3;          // Cloudy
    return 4;                           // Rain
}

static void bme_send_to_domoticz(float t, float h, float p)
{
    int hum_stat = bme_humidity_status(h);
    int bar_for  = bme_barometer_forecast(p);

    char url[192];
    snprintf(url, sizeof(url),
             "/json.htm?type=command&param=udevice&idx=%d&nvalue=0"
             "&svalue=%.2f;%.1f;%d;%.1f;%d",
             BME280_IDX, t, h, hum_stat, p, bar_for);

    Serial.printf("[BME280] %.2f C  %.1f %%  %.1f hPa -> Domoticz idx %d\n",
                  t, h, p, BME280_IDX);

    if (HTTPGETRequest(url))
    {
        Serial.println(F("[BME280] Domoticz update OK"));
    }
    else
    {
        Serial.println(F("[BME280] Domoticz update FAILED"));
    }
}

// Called once at the end of setup()
void userSetup(void)
{
    Serial.printf("[BME280] Init I2C SDA=%d SCL=%d addr=0x%02X\n",
                  BME280_SDA, BME280_SCL, BME280_ADDR);

    Wire.begin(BME280_SDA, BME280_SCL);

    bme_ok = bme.begin(BME280_ADDR, &Wire);
    if (!bme_ok)
    {
        Serial.println(F("[BME280] Sensor not found! Check wiring / I2C address."));
        return;
    }

    bme_timer = millis();
    Serial.println(F("[BME280] Sensor ready"));
}

// Called at every loop()
void userLoop(void)
{
    if (!bme_ok) return;

    // First push quickly (10 s), then every BME280_INTERVAL seconds
    unsigned long wait = bme_first ? 10000UL : (unsigned long)BME280_INTERVAL * 1000UL;
    if (millis() - bme_timer < wait) return;

    bme_timer = millis();
    bme_first = false;

    float t = bme.readTemperature() + BME280_TEMP_OFFSET;
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0f; // Pa -> hPa

    if (isnan(t) || isnan(h) || isnan(p))
    {
        Serial.println(F("[BME280] Read failed"));
        return;
    }

    bme_send_to_domoticz(t, h, p);
}

#else
// BME280 disabled: provide empty stubs so main.cpp still links
void userSetup(void) {}
void userLoop(void) {}
#endif
