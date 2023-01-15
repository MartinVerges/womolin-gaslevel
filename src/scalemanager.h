/**
 * @file scalemanager.h
 * @author Martin Verges <martin@verges.cc>
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
        String NVS = "gaslevel";                        // NVS Storage to write and read values

        uint64_t lastMedian = 0;                        // last reading median value
        double scale = 1.f;                             // hx711 scale calibration
        uint64_t tare = 0;                              // hx711 tare value

        uint32_t emptyWeightGramms = 0;                 // Weight in Gramms of the Empty bottle
        uint32_t fullWeightGramms = 0;                  // Weight in Gramms of the Filled bottle

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
        uint32_t getSensorMedianValue(bool cached = false);

        // Set the level variable to 0-100 according to the current state of lastMedian
        // You need to call getSensorMedianValue() before calculateLevel() to update lastMedian
        uint8_t calculateLevel();

        // The current level set by calculateLevel()
        uint8_t level = 0;

        // The current amount of GAS available in the bottle (without the weight of the bottle)
        uint32_t currentGasWeightGramms;

	public:
		SCALEMANAGER(uint8_t dout, uint8_t pd_sck);
        SCALEMANAGER(uint8_t dout, uint8_t pd_sck, uint8_t gain);
		virtual ~SCALEMANAGER();

        // Get the current Gas weight inside the bottle calculcated and updated in loop()
        uint32_t getGasWeight() { return currentGasWeightGramms; }

        // Get the last Median reading value updated in loop()
        uint64_t getLastMedian() { return lastMedian; }

        // Get the current level calculcated and updated in loop()
        uint8_t getLevel() { return level; }

        // call loop
        void loop();

        // Initialize the Webserver
		void begin(String nvs);

        bool isConfigured();

        // Calibration of the HX711 weight scale
        void emptyScale();
        bool applyCalibrateWeight(uint32_t weight);

        // Set the bottle weight
        bool setBottleWeight(uint32_t newEmptyWeightGramms, uint32_t newFullWeightGramms);
        
        // Get the current bottle weights
        uint32_t getBottleEmptyWeight();
        uint32_t getBottleFullWeight();

        // helper to get ESP32 runtime
        uint64_t runtime();
};

#endif /* SCALEMANAGER_h */
