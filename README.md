# Wlan Profiler:

ESP32 Wi-Fi promiscuous sniffer built with the Espressif Systems ESP-IDF that keeps track of the number of unique client MAC addresses in 'data' (h_type 2) packets, using channel hoping to cover the 3 most comon channels (1,6,11).

## Features coming soon:

- Switch from singly linked list to static hash map for faster lookups
- Handling managment frames to estimate the number of AP's visible, and provide greater accuracy to the current system
- Implemnt a similar system for bluetooth
- Create a UI to display information without a terminal connection.
