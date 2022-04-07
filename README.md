# rv-smart-gas-scale

DIY project to build a smart fuel sensor for RVs or in other projects.
It is based on pressure and is able to measure all types of bottles and their weights.

This project is still in development, it's a working sensor and I'm happy about pull requests to add more functionality, improve existing ones or just feedback.

## Why?

After I have long found on the market for camping nothing that is on the one hand affordable and on the other hand also technically good, I have started the development of my own.

Thanks to the ESP32 this can be queried in the future directly via WLAN or Bluetooth. 

## Focus of this Project

The following aspects are the focus of this project:

 * Easy to use and rebuild
 * Precise in the evaluation of the data
 * Stand alone usable
 * Usable in all liquids

## BOM - Bill of Materials

To build this sensor yourself, you need:

 * 1x ESP32 Wemos D1 mini (around 2€)
 * 1x 2x HX711 with 8x Half Bridge Load Cell (13€ https://www.amazon.de/gp/product/B09H6SLXZ6/)
 * 1x Pushbutton (~0.15€)
 * 1x Small 12V to 3.3V power supply (~1€)
 * 2x Wood/Plasic/Metal plate in the size of your needs
 
 In addition you need some few small cables and soldering equipment to build the circuit.

## How to build this PlatformIO based project

1. [Install PlatformIO Core](http://docs.platformio.org/page/core.html)
2. Run these commands:

```
    # Change directory into the code folder
    > cd rv-smart-gas-scale

    # Build project
    > platformio run

    # Upload firmware
    > platformio run --target upload
```

## Operation

When the sensor is started for the first time, a WiFi configuration portal opens via which a connection to the central access point can be established.
As soon as the connection has been established, the sensor is available on the IP assigned by the DHCP and the hostname [gasscale.local](http://gasscale.local) provided by MDNS.
You can now log in to the webobverface and proceed with the sensor setup. 

## Android Bluetooth Low Energy (BLE) App

This sensor can be displayed using my [Android App](https://github.com/MartinVerges/smartsensors/).

## Power saving mode

This sensor is equipped with various techniques to save power.
Unfortunately, the operation of WiFi causes relatively high power consumption (__45~55mA idle__, up to 250mA during transmission).
This is not a problem in RVs with larger batteries or solar panels, but may be too high in small installations.
Therefore, the WiFi module can be easily and conveniently switched off via the web interface, putting the sensor into a deep sleep mode.
Here, the consumption of the esp32 officially drops to about 10μA.
Including the used step down power supply it will still consume __12.2mA in this sleep mode__ with RTC ADC enabled, so around 1/4 of full featured wifi enabled consumtion.
At regular intervals, the sensor switches on briefly, checks the tank level and updates the analog output of the sensor.

Since the WiFi portal is not available in this mode, you can reactivate the WiFi by pressing the button on the device once.

## Wifi connection failed or unable to interact

The button on the device switches from Powersave to Wifi Mode.
When the button is pressed again, the sensor restarts and opens its own access point.
Here you can correct the WLAN data.

## API to the Sensor

All provided RESTful APIs have been documented using the free [Insomnia software](https://insomnia.rest/).
You can import the collection file called [Insomnia_api_spec.json](/Insomnia_api_spec.json?raw=true) and find all available API endpoints and run and test them directly.

<img src="images/api.png?raw=true" alt="Insomnia API description" width="40%">

## Alternatives

### Pressure based (like this project)

The only know alternative SuperSense Gas Scale is available at around 259€ for purchase, but the price is extremely high and the data cannot be accessed digitally as it's proprietary.

### Resistance based

There are a lot of cheapest probes which all show only rough and mostly wrong data.
These are mostly based on measuring the resistance or continuity between two or more conductive rods that come into contact with water.

These probes usually cost more than the components in this project, but are extremely inaccurate, prone to calcification and malfunction.

# License

rv-smart-gas-scale (c) by Martin Verges.

rv-smart-gas-scale is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You should have received a copy of the license along with this work.
If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.