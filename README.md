# Orbitrack computer on ESP32
A simple orbitrack computer on ESP32 with BLE connection to uECG for heart rate monitoring
It uses orbitrack's in-built revolution sensor (it's a simple closed/open sensor which generates 1 pulse per revolution) to calculate speed - with current implementation, it calculates speed in bycicle terms - and uECG data received via BLE to display ECG and BPM.
It outputs BPM, speed, distance, calories and plots ECG, BPM and speed charts (those are plotted over last 30 seconds), with color coding: grey - below zone 1, blue for zone 1, green for zone 2, yellow for zone 3, red for zone 4, and blinking red/purple for zone 5 (you don't want to get into zone 5 during training).

It uses only a standard SPI display with Adafruit_ST7735 driver (hardcoded connections are: CS - 12, RST - 13, DC - 14, SDAT - 26, SCK - 27), and expects orbitrack's revolution sensor connected between pin 25 and ground
