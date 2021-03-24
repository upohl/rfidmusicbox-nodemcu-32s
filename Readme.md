# Schematic
![alt text](https://github.com/upohl/rfidmusicbox-nodemcu-32s/blob/main/rfidmusicbox_schem.png?raw=true)

# NodeMCU-32-Pinout
3.3 V; GND
EN; 23
36;22
39; 1(TX)
34;3(RX)
35;21
32;GND
33;19
25;18
26;5
27;17
14;16
2;4
GND;0
13;2
9;15
10;8
11;7
Vin (5V);6


# DFPlayer
- https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299

VCC (5V); BUSY
RX
TX
DAC_R
DAC_I
SPK_1 +
GND
SpeaPK_2 -

# RC522 RFID
VCC (3.3V)
RST
GND
IRQ
MISO
MOSI
SCK
SS/SDA/rx

# Wiring
## DFPlayer
ESP32; DFPlayer; Color
Vin; VCC
16;RX
17+1KOHM;TX

## Speaker
DFPlayer; Speaker; Color
SPK_1; +; ReadWire PurplePlug
SPK_2; -; Blackire GreyPlug

## RFID
ESP32; RC522; Color
3.3V; VCC; Yellow
RST; 33; Blue
GND; GND; Black
IRQ; -; -
MISO; 19; Brown
MOSI; 23; DarkGreen
SCK; 18; Red
SS/SDA/rx; 5; Orange

