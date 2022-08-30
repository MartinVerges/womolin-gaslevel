/**
 * @file scalemanager.h
 * @author Martin Verges <martin@veges.cc>
 * @version 0.1
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/gaslevel
 * 
 * License: CC BY-NC-SA 4.0
 */

#ifndef SCALEMANAGER_h
#define SCALEMANAGER_h

#define MAX_DATA_POINTS 255                        // how many level data points to store (increased accuracy)
#include <Arduino.h>
#include <Preferences.h>
#include <HX711.h>

class SCALEMANAGER
{
	private:
        String NVS = "gaslevel";                  // NVS Storage to write and read values

        int lastMedian = 0;                        // last reading median value
        float scale = 1.f;                         // hx711 scale calibration
        long tare = 0;                             // hx711 tare value

        u_int32_t emptyWeightGramms = 0;                 // Weight in Gramms of the Empty bottle
        u_int32_t fullWeightGramms = 0;                  // Weight in Gramms of the Filled bottle

        HX711 hx711;
        Preferences preferences;

        struct timeing_t {
            // Update Sensor data in loop()
            uint64_t lastSensorRead = 0;                 // last millis() from Sensor read
            const uint32_t sensorIntervalMs = 5000;      // Interval in ms to execute code
        } timing;

        // Write current leveldata to non volatile storage
        bool writeToNVS();

        // Read Median(10) raw value from sensor
        int getSensorMedianValue(bool cached = false);

        // Set the level variable to 0-100 according to the current state of lastMedian
        // You need to call getSensorMedianValue() before calculateLevel() to update lastMedian
        uint8_t calculateLevel();

        // The current level set by calculateLevel()
        uint8_t level = 0;

        // The current amount of GAS available in the bottle (without the weight of the bottle)
        uint32_t currentGasWeightGramms;

	public:
		SCALEMANAGER(uint8_t dout, uint8_t pd_sck);
		virtual ~SCALEMANAGER();

        // Get the current Gas weight inside the bottle calculcated and updated in loop()
        uint32_t getGasWeight() { return currentGasWeightGramms; }

        // Get the last Median reading value updated in loop()
        int getLastMedian() { return lastMedian; }

        // Get the current level calculcated and updated in loop()
        uint8_t getLevel() { return level; }

        // call loop
        void loop();

        // Initialize the Webserver
		void begin(String nvs);

        bool isConfigured();

        // Calibration of the HX711 weight scale
        void emptyScale();
        bool applyCalibrateWeight(int weight);

        // Set the bottle weight
        bool setBottleWeight(u_int32_t newEmptyWeightGramms, u_int32_t newFullWeightGramms);
        
        // Get the current bottle weights
        int getBottleEmptyWeight();
        int getBottleFullWeight();

        // helper to get ESP32 runtime
        uint64_t runtime();
};

#endif /* SCALEMANAGER_h */
