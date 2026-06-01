# ElbrusFirmware

The firmware is designed for copying manipulator based on the STM32F407VETX microcontroller.
It performs the following tasks:
1. Reading data from encoders;
2. Sending UDP packets containing the received encoder values to external software using the Ethernet interface.

IP address of Elbrus: 192.169.1.10; port: 10003;
The checksum calculation mode for outgoing TCP/IP packets has been changed from hardware to software.
When connecting the Elbrus and a PC with external software into a common network, the end nodes will have the following roles:
* PC is CLIENT
* Elbrus is SERVER