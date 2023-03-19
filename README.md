# womolin.gaslevel

DIY project to build a smart gas scale for RVs or in other projects.
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
 * Usable in all type of bottles

## Show it to me!
<img src="images/sensor_with_holder.jpg?raw=true" alt="Sensor with the 3D printed holder" width="30%">
<img src="images/mounted_plates.jpg?raw=true" alt="Mounted plates to put bottles on" width="30%">

#### The UI
<img src="images/status.png?raw=true" alt="Status" width="20%"><img src="images/calibration1.png?raw=true" alt="Calibration" width="20%"><img src="images/calibration2.png?raw=true" alt="Calibration" width="20%"><img src="images/wifi1.png?raw=true" alt="Known Wifi AP list" width="20%"><img src="images/wifi2.png?raw=true" alt="Available APs in range" width="20%"><img src="images/wifi3.png?raw=true" alt="Add new Wifi AP" width="20%"><img src="images/settings.png?raw=true" alt="Settings" width="20%"><img src="images/ota.png?raw=true" alt="OTA Over the Air updates" width="20%">

#### Schematics
<img src="images/schematic.png?raw=true" alt="Schematic" width="40%">

## Focus of this Project

The following aspects are the focus of this project:

 * Easy to use and rebuild
 * Precise in the evaluation of the data
 * Stand alone usable
 * Usable in all types of bottles

## BOM - Bill of Materials

To build this sensor yourself, you need:

 * 1x [ESP32 Wemos D1 mini](https://womolin.de/produkt/wemos-d1-mini-esp32/)
 * 2x [4x Half Bridge Load Cell](https://womolin.de/produkt/4x-50-kg-waegezelle/)
 * 2x [HX711 Breakout board](https://womolin.de/produkt/hx711-breakout-board/)
 * 1x [BMP280](https://womolin.de/produkt/bmp280-33v/)
 * 1x Pushbutton (~0.15€)
 * 1x Small 12V to 3.3V power supply (~1€)
 * 2x Wood/Plasic/Metal plate in the size of your needs
 * 8x 3D printed sensor holder [CAD](https://cad.onshape.com/documents/3d3042e5cf8f3498753a8372/w/5c6080c6f47943c52b1afecc/e/5ba80aad4182341acda3708c?renderMode=0&uiState=62c9947f0bab70377a474cf9)
 
 In addition you need some few small cables and soldering equipment to build the circuit.

## Webinstaller

To make it super simple, our pre build firmware can be uploaded to your ESP32 using our Webinstaller.
You can find it at [https://webinstaller.womolin.de](https://webinstaller.womolin.de).

## OTA (Over-the-Air) Firmware files

If you want to update an already installed Sensor, you can upload the binarys directly to the sensor using the Web UI.
The latest version (current git main branch) is available at [https://s3.womolin.de/webinstaller/gaslevel-latest/firmware.bin](https://s3.womolin.de/webinstaller/gaslevel-latest/firmware.bin) and [https://s3.womolin.de/webinstaller/gaslevel-latest/littlefs.bin](https://s3.womolin.de/webinstaller/gaslevel-latest/littlefs.bin).

## How to build this PlatformIO based project

1. [Install PlatformIO Core](http://docs.platformio.org/page/core.html)
2. Run these commands:

```
    # Change directory into the code folder
    > cd womolin.gaslevel

    # Build project
    > platformio run -e wemos_d1_mini32

    # Upload firmware
    > platformio run -e wemos_d1_mini32 --target upload
```

### Upload prebuild file

Here is a example command to upload existing firmware files.
```
/path/to/esptool.py --chip esp32 --port "/dev/ttyUSB0" --baud 921600 --before default_reset --after hard_reset \
    write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB \
    0x1000 bootloader_dio_80m.bin \
    0x8000 partitions.bin \
    0xe000 boot_app0.bin \
    0x10000 firmware.bin \
    0x1A0000 firmware.bin \
    0x330000 littlefs.bin
```
Please make sure that your ESP32 runs with these settings before uploading it.

## How to build the UI

As the UI requires a valid FontAweSome License, you can find a generated `littlefs.bin` with my subscription.
It's automatically uploaded to our S3 at [https://s3.womolin.de/webinstaller/gaslevel-latest/littlefs.bin](https://s3.womolin.de/webinstaller/gaslevel-latest/littlefs.bin).
Please feel free to take this one for your sensor.

### Build your own littlefs.bin

As I haven't found good icons with a free license, I choosed the pro version of fontawesome.
Therefore it's required to have a valid subscription in order to build the UI yourself.

Set your FontAweSome key:
```
npm config set "@fortawesome:registry" https://npm.fontawesome.com/
npm config set "//npm.fontawesome.com/:_authToken" XXXXXXXXXXXXXXXXXXXX
```

Build the UI:
```
cd ui
npm install
npm run build
cd ..
pio run -e esp32dev -t buildfs
```

## Operation

When the sensor is started for the first time, a WiFi configuration portal opens via which a connection to the central access point can be established.
As soon as the connection has been established, the sensor is available on the IP assigned by the DHCP and the hostname [gaslevel.local](http://gaslevel.local) provided by MDNS.
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

# License

womolin.gaslevel (c) by Martin Verges.

This Project is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You should have received a copy of the license along with this work.
If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
