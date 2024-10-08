/* 
  Hidden dependency project
  Copyright (c) 2024 Sangbong Lee <sangbong@me.com>
  
  * Bluetooth scanner for ESP32 board
  * This code allows the esp32 board to scan Bluetooth devices nearby and transmits a command to the Teensy board through i2C.

  This work is licensed under the Creative Commons Attribution 4.0 International License.
  To view a copy of this license, visit http://creativecommons.org/licenses/by/4.0/.
*/

#include <Arduino.h>

// I2c Header
#include <Wire.h>

// BLE Headers
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Store known BLE devices (e.g. My phone, BLE earbuds, etc)
// Remove all the addreses when the device is initially installed in the exhibition place
String knownBLEAddresses[] = { "aa:bc:cc:dd:ee:ee", "54:2c:7b:87:71:a2", "72:09:b9:28:37:6c", 
                               "6c:9a:00:3a:65:47", "66:f4:d1:6c:fc:b2", "5a:2b:f4:61:71:aa", 
                               "f2:dc:7e:bd:f1:ab", "49:36:ef:f5:9f:0c", "4f:08:07:83:c3:62", 
                               "5b:51:f2:1d:66:4d", "53:11:d2:bf:fd:04", "74:be:f6:a4:81:2f", 
                               "d7:42:99:28:27:63" };

int RSSI_THRESHOLD = -80;           // Normal Bluetooth detection radius
int RSSI_THRESHOLD_FOOTSTEP = -50;  // Footstep Bluetooth detection radius

int SCAN_INTERVAL = 25;             // Scanning interval time -> 25
int SCAN_INTERVAL_WINDOW = 24;      // Scanning interval time(window) -> 24 // Less or equal to scan interval time

int RSSI_TH_COUNT = 0;              // Number of devices inside of the threshold radius
int RSSI_TH_COUNT_FOOTSTEP = 0;     // Number of devices inside of the threshold radius (Close to the center)

int scanTime = 5;   // Scanning duration time -> 5ms
BLEScan* pBLEScan;  // BLE scan objects (Array)

//********** Device detection flags **********//
bool RSSI_TH_FLAG = false;
bool RSSI_TH_FOOTSTEP_FLAG = false;

bool DEVICE_PRESENCE = false;   // True -> More than 1 device in the area
bool DEVICE_SMALL_NUM = false;  // True -> More than 5 addresses and less than 15 addresses in the area
bool DEVICE_LARGE_NUM = false;  // True -> More than 16 addresses in the area

bool known_device_found = false; // True -> Known devices are found

//********** I2C Command variable **********//
char message = 's';

//********** LED Pins **********//
int LED_GREEN = 18; // Green LED Control Pin (Devices are within RSSI_THRESHOLD range)
int LED_RED = 5;    // Red LED Control Pin (Devices are within RSSI_THRESHOLD_FOOTSTEP range)

//Call back function => it will be called once every few second. 
//It checks if any new devices are available or not. => Set a flag if there is a new one
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  
  // Print all addresses of the scanned BLE devices 
  void printAllBLEDevices(BLEAdvertisedDevice _device, int _index) {
      //Uncomment to Enable Debug Information
      Serial.println("************* Start **************");
      Serial.println(sizeof(knownBLEAddresses));
      Serial.println(sizeof(knownBLEAddresses[0]));
      Serial.println(sizeof(knownBLEAddresses)/sizeof(knownBLEAddresses[0]));
      Serial.println(_device.getAddress().toString().c_str());
      Serial.println(knownBLEAddresses[_index].c_str());
      Serial.println("************* End **************");
  }
  
  // Flag check if there are known BLE devices in the TH range
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    for (int i = 0; i < (sizeof(knownBLEAddresses) / sizeof(knownBLEAddresses[0])); i++) {
      printAllBLEDevices(advertisedDevice, i);
      if (strcmp(advertisedDevice.getAddress().toString().c_str(), knownBLEAddresses[i].c_str()) == 0) {
        known_device_found = true;
        break;
      } else {
        known_device_found = false;
      }
    }
    Serial.printf("Advertised Device: %s", advertisedDevice.toString().c_str());
    Serial.printf(" Known Device Found Flag: %s \n", (String)known_device_found);
  }
};

// LED Notification
void ledNotification() {
  // LED Blinks
  for (int i = 0; i < RSSI_TH_COUNT; i++) {
    digitalWrite(LED_GREEN, HIGH);
    delay(15);
    digitalWrite(LED_GREEN, LOW);
    delay(15);
  }
  for (int i = 0; i < RSSI_TH_COUNT_FOOTSTEP; i++) {
    digitalWrite(LED_RED, HIGH);
    delay(15);
    digitalWrite(LED_RED, LOW);
    delay(15);
  }
};

//I2C communication
void requestEvent() {
  Wire.write(message);  // respond with message of 6 bytes
  // as expected by master
  Serial.println("Send MODE 2 signal" + message);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Wire.begin(8);
  Wire.onRequest(requestEvent);  // register event

  // LED Indicators
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // ***** BLE Scanner initialize ***** //
  Serial.println("BLE Scanning...");  // Print Scanning
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();                                            // Create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());  // Init Callback Function
  pBLEScan->setActiveScan(true);                                              // Active scan uses more power, but get results faster
  pBLEScan->setInterval(SCAN_INTERVAL);                                       // Set Scan interval
  pBLEScan->setWindow(SCAN_INTERVAL_WINDOW);                                  // Less or equal setInterval value
}

void loop() {
  // put your main code here, to run repeatedly:
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  std::vector<std::string> stationaryBLEAddresses;

  // ***** Device counting function ***** //
  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    int rssi = device.getRSSI();
    //Serial.print("RSSI: ");
    //Serial.print(rssi);
    
    // Normal RSSI radius from the center
    if (rssi > RSSI_THRESHOLD && known_device_found == false) {

      RSSI_TH_COUNT++;
      RSSI_TH_FLAG = true;

      // Small RSSI radius from the center
      if (rssi > RSSI_THRESHOLD_FOOTSTEP) {
        RSSI_TH_COUNT_FOOTSTEP++;
        RSSI_TH_FOOTSTEP_FLAG = true;
      }
    }
    stationaryBLEAddresses.push_back(device.getAddress().toString().c_str());
    Serial.printf("  Device Found Address: %s \n", stationaryBLEAddresses[i].c_str());
  }

  // ***** Function to display the number of devices in the range of threshold ***** //
  Serial.print("Number of BLE Devices (Green LED): ");
  Serial.print(RSSI_TH_COUNT);
  Serial.print("  Number of BLE Devices in close (Red LED): ");
  Serial.println(RSSI_TH_COUNT_FOOTSTEP);

  // ***** Function to display whether the number of scanned machines is in a small or large range ***** //
  if (RSSI_TH_COUNT > 0) {
    DEVICE_PRESENCE = true;
    Serial.print("POTENTIOAL AUDIENCE : ");
    if (RSSI_TH_COUNT >= 5 && RSSI_TH_COUNT <= 15) {
      DEVICE_SMALL_NUM = true;
      DEVICE_LARGE_NUM = false;
      Serial.print(RSSI_TH_COUNT);
      Serial.println(" SMALL NUMBER !! ");
    } else if (RSSI_TH_COUNT > 15) {
      DEVICE_LARGE_NUM = true;
      DEVICE_SMALL_NUM = false;
      Serial.print(RSSI_TH_COUNT);
      Serial.println(" LARGE NUMBER !! ");
    }
  } else {
    DEVICE_PRESENCE = false;
    DEVICE_SMALL_NUM = false;
    DEVICE_LARGE_NUM = false;
    Serial.println("NO AUDIENCE!! ");
  }

  // ***** Function to display the device presence check flag in the each range  ***** //
  Serial.print("DEVICES IN RANGE : ");
  if (RSSI_TH_FLAG == 1) {
    Serial.print("TRUE  /");
  } else {
    Serial.print("FALSE  /");
  }
  Serial.print("  DEVICES IN CLOSE RANGE : ");
  if (RSSI_TH_FOOTSTEP_FLAG == 1) {
    Serial.println("TRUE");
  } else {
    Serial.println("FALSE");
  }
  Serial.println();
  Serial.println();

  // ***** Turn on LEDs depends on the number of devices in the each range ***** //
  ledNotification();

  // ***** SET THE COMMAND MESSAGE TO I2C COMM ***** //
  // Devices are detacted within the threshold radius
  if (DEVICE_PRESENCE == true) {
    if (DEVICE_SMALL_NUM == true) {
      // Send an i2c message for mode 2 == Footstep
      message = 'f';  // SMALL
    } else {
      // Send an i2c message for mode 1 == Random vibration
      message = 'r';  // LARGE
    }
  } else {  
    // No devices in the threshold radius
    // Send an i2c message for mode 0 == Stop
    message = 's';  // NO
  }

  // ***** Reset device counting variables ***** //
  RSSI_TH_COUNT = 0;
  RSSI_TH_COUNT_FOOTSTEP = 0;
  RSSI_TH_FLAG = false;
  RSSI_TH_FOOTSTEP_FLAG = false;

  // delete results fromBLEScan buffer to release memory
  pBLEScan->clearResults();

  // clear stationaryBLEAddresses
  stationaryBLEAddresses.clear();
}