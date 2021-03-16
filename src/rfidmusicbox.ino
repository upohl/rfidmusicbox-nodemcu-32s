/**
* \file rfidmusicbox ino
* \brief Music Box for kids
*
* \remarks  Music Box with connection to Rednode MQTT Server to play songs on Amazon Alexa Echo and build in MP3 player for standalone mode. 
* Tested on a NodeMCU-32s board with connections to a DFPlayerMini and MFRC522 RFID Reader.
* Tested with https://flows.nodered.org/node/node-red-contrib-alexa-remote2 and https://flows.nodered.org/node/node-red-contrib-aedes on Node Red for sending signals to node red.
* Inspired by  https://www.voss.earth/tonuino/ 
*
* @author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date 16/03/2021
* platform = espressif32
* board = nodemcu-32s
**/

/** Importation of librairies*/
#include <Arduino.h>

#include <WiFi.h>         //WIFI
#include <PubSubClient.h> //MQTT

//RFID
#include <SPI.h>
#include <MFRC522.h>

//MP3 DFPlayer Mini
#include <SoftwareSerial.h> //DFPLAYER Communication

#include <DFRobotDFPlayerMini.h> //DFPlayer MP3

//EXTRA INCLUDES FOR SLEEPING FUNCTIONS on NodeMCU-32s
#include <esp_wifi.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>
#include <esp_err.h>
#include <esp_bt.h>
#include <driver/adc.h>

//load crendentials.h file. Make sure you have a credentials.h in the same folder.
// cresentials.h defines WIFI_SSID and WIFI_PASSWD
#include "credentials.h"

//RFID MFRC522 PINS
//SCLK = 18, MISO = 19, MOSI = 23, SS = 5
/**
* \def SS_PIN
* Description
*/
#define SS_PIN 5
/**
* \def RST_PIN
* Description
*/
#define RST_PIN 33

//DEFINES FOR SLEEP MODE and Timeouts
/**
* \def uS_TO_S_FACTOR
* Description
*/
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
/**
* \def TIME_TO_SLEEP
* Description
*/
#define TIME_TO_SLEEP 10       /* Time ESP32 will go to sleep (in seconds) */
/**
* \def WIFI_TIMEOUT
* Description
*/
#define WIFI_TIMEOUT 10000     // 10seconds in milliseconds

//DFPlayer Pins
/**
* \def softwareRX
* Description
*/
#define softwareRX 16
/**
* \def softwareTX
* Description
*/
#define softwareTX 17

//DEFINE BUTTON PINS
/**
* \def leftButton
*  the number of the left button pin
*/
#define leftButton 32  
/**
* \def middleButton
* the number of the middle button pin
*/
#define middleButton 35 
/**
* \def rightButton
* the number of the right button pin
*/
#define rightButton 34  

//DFPlayerMP3
SoftwareSerial mySoftwareSerial(16, 17); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
void printDFPlayerError(uint8_t type, int value);

//DFPlayer Volume Parameter
int volume = 14;
const int MAX_VOLUME = 20;
const int MIN_VOLUME = 0;

//WIFI DATA. Uses data from credentials.h
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWD;

//MQTT DATA
const char *mqtt_server = MQTT_IP;

//BUTTON DELAY
const int delayTime = 200;

//GLOBAL HELPER VARIABLES
RTC_DATA_ATTR String lastRFIDTag = "0000";

// variable for storing the pushbutton status
int rightButtonState = 1;
int middleButtonState = 1;
int leftButtonState = 1;
int playstate = 1;

//RFID
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

//WIFI / MQTT
WiFiClient espClient;
PubSubClient client(espClient);


/**
* \fn void setupMP3Player()
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Init DFPlayer Mini
* \remarks None
*/
void setupMP3Player()
{
  mySoftwareSerial.begin(9600);
  //Serial.begin(115200);

  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  pinMode(rightButton, INPUT);
  pinMode(middleButton, INPUT);
  pinMode(leftButton, INPUT);

  if (!myDFPlayer.begin(mySoftwareSerial))
  { //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
  }
  else
  {
    Serial.println(F("DFPlayer Mini online."));
    myDFPlayer.volume(volume); //Set volume value. From 0 to 30
    myDFPlayer.play(3);        //Play the third mp3
  }
}

/**
* \fn void MQTTCallback(char *topic, byte *payload, unsigned int length)
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief 
* \remarks None
* \param topic 
* \param payload 
* \param int 
*/
void MQTTCallback(char *topic, byte *payload, unsigned int length)
{
}

/**
* \fn bool reconnect()
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Reconnects WIFI and MQTT if connection is lost
* \remarks None
* \return bool
*/
bool reconnect(void)
{
  bool success = false;
  if (WiFi.status() != WL_CONNECTED)
  {
    connectToWiFi(WIFI_TIMEOUT);
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    int connectionAttempt = 0;
    while (!client.connected() && connectionAttempt < 2)
    {
      connectionAttempt++;
      Serial.print("Attempting MQTT connection...");

      // Create a client ID
      String clientId = "MusicBox";

      // Attempt to connect
      if (client.connect(clientId.c_str()))
      {
        Serial.println("connected");
        success = true;
      }
      else
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 10 milliseconds");

        delay(10);
      }
      if (connectionAttempt >= 2)
      {
        Serial.print("failed 3 times, rc=");
        Serial.print(client.state());
        return success;
      }
    }
  }
  return success;
}

/**
* \fn void IRAM_ATTR handleButtons()
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Interrupt for handling buttons. 
* \remarks Can be split up in the future for an interrupt function for each button or use libary like JC_Button.h Logic can also be simplified as al the checks if the button was low in between should be handled by the interrupt itself.
*/
void IRAM_ATTR handleButtons()
{
  if (myDFPlayer.available())
  {
    printDFPlayerError(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
    return;
  }

  //Check if button is not pressed to enable it
  if (digitalRead(rightButton) == LOW)
  {
    rightButtonState = 1;
  }
  if (digitalRead(middleButton) == LOW)
  {
    middleButtonState = 1;
  }
  if (digitalRead(leftButton) == LOW)
  {
    leftButtonState = 1;
  }

  // Check if button was released and is currently pressed and if it is currently playing (not yet using the busy pin to check if the player is playomg)
  //Volume Up
  if (rightButtonState == 1 && digitalRead(rightButton) == HIGH && playstate == 1)
  {
    rightButtonState = 0; //toggle semaphore
    volume = volume + 2; //level up volume
    // if volume is greater max set volume to max
    if (volume > MAX_VOLUME)
    {
      volume = MAX_VOLUME;
    }
    myDFPlayer.volume(volume);
    return;
  }
  //Next track
  if (playstate == 0 && rightButtonState == 1 && digitalRead(rightButton) == HIGH)
  {
    myDFPlayer.next(); //Play next mp3
    playstate = 1;
    return;
  }
  
  //Pause,start and in switch track mode
  if (middleButtonState == 1 && digitalRead(middleButton) == HIGH)
  {
    middleButtonState = 0;
    if (playstate == 1)
    {
      myDFPlayer.pause();
      playstate = 0;
      return;
    }
    else
    {
      myDFPlayer.start();
      playstate = 1;
      return;
    }
  }
  
  //Volume down
  if (leftButtonState == 1 && playstate == 1 && digitalRead(leftButton) == HIGH)
  {
    volume = volume - 2;
    if (volume <= MIN_VOLUME)
    {
      volume = MIN_VOLUME;
    }
    myDFPlayer.volume(volume);
    return;
  }

  //Previous track
  if (playstate == 0 && leftButtonState == 1 && digitalRead(leftButton) == HIGH)
  {
    leftButtonState = 0;
    myDFPlayer.previous();
    playstate = 1;
    return;
  }

}


/**
* \fn void handleRFID()
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Handles RFID Reader and Tags
* \remarks None
*/
void handleRFID()
{
  //Check if card is present
  if (!rfid.PICC_IsNewCardPresent())
  {
    //Serial.println(exception);
    return;
  }
  if (!rfid.PICC_ReadCardSerial())
  {
    Serial.println("PICC_ReadCardSerial() Failed");
    return;
  }
  
  String RFIDString = printHex(rfid.uid.uidByte, rfid.uid.size); // Get RFID Tag

  Serial.println("Read:" + RFIDString);
  //If Tag is on the Whitelist and is different from previous tag then it should be published
  if (isWhiteList(RFIDString) || !RFIDString.equals(lastRFIDTag))
  {
    if(reconnect())//make sure wifi and mqtt connection is working
    {
      client.publish("MusicBox/rfid", String(RFIDString).c_str(), true);
      Serial.println("LastRFIDTag:" + lastRFIDTag);
      Serial.println("Published:" + RFIDString);
      lastRFIDTag = RFIDString;
    }
    else
    {
        Serial.println("Published failed due to missing wifi or mqtt connection");
    }
  }
  else
  {
    Serial.println("Don't Published:" + RFIDString);
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

/**
* \fn void print_wakeup_reason()
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Print Wakeup cause of ESP32. 
* \remarks Not used in the moment. 
*/
void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

/**
* \fn String printHex(byte *buffer, byte bufferSize)
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief 
* \remarks None
* \param buffer 
* \param bufferSize 
* \return 
*/
String printHex(byte *buffer, byte bufferSize)
{
  String id = "";
  for (byte i = 0; i < bufferSize; i++)
  {
    id += buffer[i] < 0x10 ? "0" : "";
    id += String(buffer[i], HEX);
  }
  return id;
}

/**
* \fn bool isWhiteList(String tag)
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Checks if RFID Tag is known.
* \remarks 
* \param tag 
* \return 
*/
bool isWhiteList(String tag)
{
  const String tagArr[] = {RFID_TAGS}; //#define RFID_TAGS "6a4c4029", "3afcff80", "8766bfda"
  int tagSize = sizeof(tagArr) / sizeof(tagArr[0]);
  Serial.println(String(tagSize));
  for (int i = 0; i < tagSize; i++)
  {
    if (tagArr[i].equals(tag))
    {
      return true;
    }
  }
  return false;
}

/**
* \fn void printDFPlayerError(uint8_t type, int value)
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Prints DFPlayer Errors. Copy from DFPlayer Mini Example
* \remarks None
* \param type 
* \param value 
*/
void printDFPlayerError(uint8_t type, int value)
{
  switch (type)
  {
  case TimeOut:
    Serial.println(F("Time Out!"));
    break;
  case WrongStack:
    Serial.println(F("Stack Wrong!"));
    break;
  case DFPlayerCardInserted:
    Serial.println(F("Card Inserted!"));
    break;
  case DFPlayerCardRemoved:
    Serial.println(F("Card Removed!"));
    break;
  case DFPlayerCardOnline:
    Serial.println(F("Card Online!"));
    break;
  case DFPlayerPlayFinished:
    Serial.print(F("Number:"));
    Serial.print(value);
    Serial.println(F(" Play Finished!"));
    break;
  case DFPlayerError:
    Serial.print(F("DFPlayerError:"));
    switch (value)
    {
    case Busy:
      Serial.println(F("Card not found"));
      break;
    case Sleeping:
      Serial.println(F("Sleeping"));
      break;
    case SerialWrongStack:
      Serial.println(F("Get Wrong Stack"));
      break;
    case CheckSumNotMatch:
      Serial.println(F("Check Sum Not Match"));
      break;
    case FileIndexOut:
      Serial.println(F("File Index Out of Bound"));
      break;
    case FileMismatch:
      Serial.println(F("Cannot Find File"));
      break;
    case Advertise:
      Serial.println(F("In Advertise"));
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

/**
* \fn void goToWLANSleep()
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Power off ADC, Wifi and Bluetooth. Make sure to power on Analog Digital Converter if used for any purpose. 
* \remarks None
*/
void goToWLANSleep()
{
  Serial.println("Going to sleep...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  adc_power_off();
  esp_wifi_stop();
  esp_bt_controller_disable();

  // Configure the timer to wake us up!
  //esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME * 60L * 1000000L);

  // Go to sleep! Zzzz
  //esp_deep_sleep_start();
}

/**
* \fn void turnOffBluetooth()
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief 
* \remarks Not used in the moment
*/
void turnOffBluetooth()
{
  Serial.println("Turn off Bluetoth...");
  btStop();
  esp_bt_controller_disable();
}

/**
 * \brief connectToWiFi() funcion in case of disabled Bluetooth
 * @author Uwe Pohlmann
 * @param timeout timeout for trying to connect to ssid in milliseconds
 *  
**/
/**
* \fn bool connectToWiFi(long timeout)
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Connects to wifi. If Wifi connection fail disable wifi to save energy.
* \remarks None
* \param timeout 
* \return bool
*/
bool connectToWiFi(long timeout)
{
  Serial.print("Connecting to WiFi... ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Keep track of when we started our attempt to get a WiFi connection
  unsigned long startAttemptTime = millis();

  // Keep looping while we're not connected AND haven't reached the timeout
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttemptTime < timeout)
  {
    delay(10);
  }

  // Make sure that we're actually connected, otherwise go to deep sleep
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("FAILED WIFI Connection");
    goToWLANSleep();
    return false;
  }

  Serial.println("Wifi Connection OK");
  return true;
}


/**
* \fn bool initBluetooth()
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief 
* \remarks Not used in the moment
* \return 
*/
bool initBluetooth()
{
  if (!btStart())
  {
    Serial.println("Failed to initialize controller");
    return false;
  }

  if (esp_bluedroid_init() != ESP_OK)
  {
    Serial.println("Failed to initialize bluedroid");
    return false;
  }

  if (esp_bluedroid_enable() != ESP_OK)
  {
    Serial.println("Failed to enable bluedroid");
    return false;
  }
  return true;
}


/**
* \fn void setup(void)
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Arduino Setup routine
* \remarks None
*/
void setup(void)
{
  // turnOffBluetooth();
  setCpuFrequencyMhz(80); //save energy

  // Button handling
  attachInterrupt(32, handleButtons, RISING);
  attachInterrupt(34, handleButtons, RISING);
  attachInterrupt(35, handleButtons, RISING);

  //rfid init
  SPI.begin();
  rfid.PCD_Init();
  Serial.begin(115200);
  Serial.println("Boot RFID-Reader...");
  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
  //               " Seconds");
  //print_wakeup_reason();

  setupMP3Player();

  //setup wifi and mqtt
  connectToWiFi(WIFI_TIMEOUT);
  client.setServer(mqtt_server, 1884);
  client.setCallback(MQTTCallback);
}



/**
* \fn void loop(void)
* \author Uwe Pohlmann github.com/upohl
* \version 0.1
* \date  16/03/2021
* \brief Main loop of Arduino
* \remarks None
*/
void loop(void)
{
  handleRFID();
  handleButtons();
  delay(10);
  //esp_deep_sleep_start();
}
