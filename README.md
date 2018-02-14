# Garage door opener dongle

This is a simple garage door opener application for FreeRTOS on the Espressif ESP32 chip. It should work with basically any ESP32 board, although you might have to change the crystal frequency from 26MHz to 40MHz for some devices.

The gist of it is, on powerup it connects to your home wifi network and sends a single, cryptographically secure packet to a specified host. When decoded and authenticated this will trigger a device (Raspberry Pi, for example) to fire off a relay to open your garage door. See garagedoor-server.py for an example receiver.

The key is initialized to 0x00... in garagedoor.c. You want to replace this with your own key. You also want to do a "make menuconfig" in order to set the SSID and password for the dongle to connect to.
