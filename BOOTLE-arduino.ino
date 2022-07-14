#include <Arduino.h>
#include <ArduinoBLE.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define ANALOG_READ_UUID "6d68efe5-04b6-4a85-abc4-c2670b7bf7fd"
#define DIGITAL_WRITE_UUID "f27b53ad-c63d-49a0-8c0f-9f297e6cc520"

BLEService newService(SERVICE_UUID); // creating the service

BLEByteCharacteristic readPin(ANALOG_READ_UUID, BLERead | BLENotify); // creating the Analog Value characteristic
BLEByteCharacteristic led(DIGITAL_WRITE_UUID, BLERead | BLEWrite); // creating the LED characteristic

const int ledPin = 2;
long previousMillis = 0;

void setup() {
  Serial.begin(9600);    // initialize serial communication
  while (!Serial);       //starts the program if we open the serial monitor.

  pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED to indicate when a central is connected
  pinMode(ledPin, OUTPUT); // initialize the LED pin 2 to indicate when we turn on LED

  //initialize BLE library
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  BLE.setLocalName("BOOTLE"); //Setting a name that will appear when scanning for bluetooth devices
  BLE.setAdvertisedService(newService);

  newService.addCharacteristic(led); //add characteristics to a service
  newService.addCharacteristic(readPin);

  BLE.addService(newService);  // adding the service

  led.writeValue(1); //set initial value for characteristics
  readPin.writeValue(1);

  BLE.advertise(); //start advertising the service
  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop() {
  
  BLEDevice central = BLE.central(); // wait for a BLE central

  if (central) {  // if a central is connected to the peripheral
    Serial.print("Connected to central: ");
    
    Serial.println(central.address()); // print the central's BT address
    
    digitalWrite(LED_BUILTIN, HIGH); // turn on the LED to indicate the connection

    // read value 3000ms
    // while the central is connected:
    while (central.connected()) {
      long currentMillis = millis();
      
      if (currentMillis - previousMillis >= 5000) { // if 5000ms have passed, we check the A1 pin
        previousMillis = currentMillis;

        int randomValue = analogRead(A1);
        readPin.writeValue(randomValue);

        if (led.written()) {
          Serial.println(led.value());
          if (led.value()) {   // any value other than 0
            Serial.println("LED on");
            digitalWrite(ledPin, HIGH);         // will turn the LED on
          } else {                              // a 0 value
            Serial.println(F("LED off"));
            digitalWrite(ledPin, LOW);          // will turn the LED off
          }
        }

      }
    }
    
    digitalWrite(LED_BUILTIN, LOW); // when the central disconnects, turn off the LED
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }
}