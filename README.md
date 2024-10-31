| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# Human Presence Detector using WiFI CSI

Starts a task that sends NULL Data packets to the Access Point and receives ACKs

CSI (Channel State Information) is extracted from the ACKs and resulting data stream is fed to a presence detection algorithm

Optionally, python `csi_data_parse_read.py -p PORT` can be used to visualize CSI data realtime

ESP32-S3 and ESP32-C6 were tested

### Configure the project

CSI from ACK is supported in the following IDF versions: v5.2+, v5.1.3+, and v5.0.6+

Use VSCode with [ESP-IDF](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md) plugin to open, build and flash this project.

Alternatively, on command line, set target: `idf.py set-target esp32c6`

Open the project configuration menu (`idf.py menuconfig`).

In the `Project Configuration` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

* [ESP-IDF Getting Started Guide on ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html)
* [ESP-IDF Getting Started Guide on ESP32-C6](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/index.html)

## Example Output
```
CSI_DATA,100,a0:36:bc:36:c7:38,-44,11,-92,11,12622117,14,0,128,1,"[0,0,0,0,0,0,0,0,0,0,2,-4,-8,39,-11,39,-14,38,-16,38,-18,37,-20,36,-21,34,-21,34,-22,32,-22,31,-21,31,-21,30,-20,30,-20,30,-18,29,-17,30,-15,29,-13,28,-13,28,-11,28,-10,26,-9,24,-7,23,-7,21,-6,20,-6,17,0,0,-7,12,-9,8,-10,6,-9,8,-10,6,-62,18,0,0,0,0,14,117,-24,-30,-110,1,0,0,64,76,-54,68,-91,-124,-72,19,0,0,0,0,23,117,-24,-30,-110,1,0,0,0,0,87,37,29,-58,-33,84,72,65,83,-23,96,-67,-119,-14,97,117]"
No presence detected. (Confidence: 0.28)
CSI_DATA,101,a0:36:bc:36:c7:38,-43,11,-92,11,12720300,14,0,128,1,"[0,0,0,0,0,0,0,0,0,3,-1,0,-14,-31,-11,-33,-8,-34,-5,-35,-3,-36,-1,-36,1,-35,3,-35,5,-35,6,-34,6,-33,7,-33,8,-33,8,-33,7,-32,7,-32,6,-31,5,-30,6,-30,5,-29,5,-27,5,-26,4,-24,5,-22,5,-20,6,-18,0,0,8,-12,10,-9,11,-6,10,-9,11,-6,-7,-12,-8,-15,-7,-12,-8,-15,-15,-23,-17,-22,-18,-22,-18,-23,-18,-24,-19,-23,-18,-24,-19,-23,-11,-13,-10,-11,-8,-9,-8,-8,-7,-7,-3,-7,-2,-7,0,-8,1,-7,-6,10,18,1,0,0,0,0,0,0]"
Human presence detected! (Confidence: 0.55)
```

## About detecting Human Presence

* Human presence detection algorithm in python was generated using [Caude.ai](https://claude.ai/) and then converted to C to run on target using Claude.ai. 
* The following prompt was used: `write an algorithm for real-time human presence detection from WiFi CSI signal consisting of 26 subcarriers at 10/s sampling frequency`
* Follow-up prompts consisted of adding outlier rejection, and fixing run-time bugs from the resulting code.
* This algorithm is not very robust to high levels of interference and may not work in all environments. If you are experiencing issues, try adjusting the confidence threshold, or using something else.



