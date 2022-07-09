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

        HX711 hx711;
        Preferences preferences;

        // Write current leveldata to non volatile storage
        bool writeToNVS();

	public:
		SCALEMANAGER(uint8_t dout, uint8_t pd_sck);
		virtual ~SCALEMANAGER();

        // Initialize the Webserver
		void begin(String nvs);

        // Read Median(10) raw value from sensor
        int getSensorMedianValue(bool cached = false);

        // Calculate current level in percent. Requires valid level setup.
        int getCalculatedPercentage(bool cached = false);

        bool isConfigured();

        // Calibration of the HX711 weight scale
        void emptyScale();
        bool setupWeight(int weight);

        // helper to get ESP32 runtime
        uint64_t runtime();
};

#endif /* SCALEMANAGER_h */
