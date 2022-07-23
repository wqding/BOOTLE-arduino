// Include the libraries we need
#include <Arduino.h>
#include <ArduinoBLE.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Adafruit_ThinkInk.h"

#define EPD_CS      7
#define EPD_DC      6
#define SRAM_CS     11
#define EPD_RESET   14 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    13 // can set to -1 to not use a pin (will wait a fixed delay)

ThinkInk_290_Mono_M06 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

//STORES THE CURRENT TEMP VALUE
float tempC;

//STORES THE CURRENT WATER LEVEL IN INCHES
float waterheight;

//STORES PERCENT FULL OF BOTTLE (ASSUMING MAX HEIGHT IS 5 INCHES)
float percentfull;

// the value of the 'other' resistor
#define SERIESRESISTOR 560    
 
// What pin to connect the sensor to
#define SENSORPIN A3 


//BLUETOOTH STUFF
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define ANALOG_READ_UUID "6d68efe5-04b6-4a85-abc4-c2670b7bf7fd"
#define DIGITAL_WRITE_UUID "f27b53ad-c63d-49a0-8c0f-9f297e6cc520"

BLEService newService(SERVICE_UUID); // creating the service

BLEByteCharacteristic heightSend(ANALOG_READ_UUID, BLERead | BLENotify | BLEWrite); // creating the Analog Value characteristic
BLEByteCharacteristic tempSend(ANALOG_READ_UUID, BLERead | BLENotify | BLEWrite); // creating the Analog Value characteristic
BLEByteCharacteristic batterySend(ANALOG_READ_UUID, BLERead | BLENotify | BLEWrite); // creating the Analog Value characteristic
BLEByteCharacteristic displayMode(DIGITAL_WRITE_UUID, BLERead | BLENotify | BLEWrite); // creating the LED characteristic

long previousMillis = 0;
/*
 * Setup function. Here we do the basics
 */
void setup(void)
{
  // start serial port
  Serial.begin(9600);
  while (!Serial) { delay(10); }

  sensors.begin();
  display.begin(THINKINK_MONO);

  pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED to indicate when a central is connected
  digitalWrite(LED_BUILTIN, LOW); 

  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  BLE.setLocalName("BOOTLE"); //Setting a name that will appear when scanning for bluetooth devices
  BLE.setAdvertisedService(newService);

  newService.addCharacteristic(displayMode); //add characteristics to a service
  newService.addCharacteristic(heightSend);
  newService.addCharacteristic(tempSend);
  newService.addCharacteristic(batterySend);

  BLE.addService(newService);  // adding the service

  displayMode.writeValue(3); //set initial value for characteristics
  heightSend.writeValue(1);
  tempSend.writeValue(1);
  batterySend.writeValue(1);

  BLE.advertise(); //start advertising the service
  Serial.println("Bluetooth device active, waiting for connections...");

  // report parasite power requirements
//  Serial.print("Parasite power is: "); 
//  if (sensors.isParasitePowerMode()) Serial.println("ON");
//  else Serial.println("OFF");
  
  // Assign address manually. The addresses below will beed to be changed
  // to valid device addresses on your bus. Device address can be retrieved
  // by using either oneWire.search(deviceAddress) or individually via
  // sensors.getAddress(deviceAddress, index)
  // Note that you will need to use your specific address here
  //insideThermometer = { 0x28, 0x1D, 0x39, 0x31, 0x2, 0x0, 0x0, 0xF0 };

  // Method 1:
  // Search for devices on the bus and assign based on an index. Ideally,
  // you would do this to initially discover addresses on the bus and then 
  // use those addresses and manually assign them (see above) once you know 
  // the devices on your bus (and assuming they don't change).
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, 9);
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{

  // method 2 - faster
  tempC = sensors.getTempC(deviceAddress);
  if(tempC == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: Could not read temperature data");
    return;
  }
  Serial.print("Temperature: "); 
  Serial.print(tempC);
  Serial.println("");
}
/*
 * Main function. It will request the tempC from the sensors and display on Serial.
 */
void loop(void)
{ 

  BLEDevice central = BLE.central(); // wait for a BLE central

  if (central) {  // if a central is connected to the peripheral
    Serial.print("Connected to central: ");
    
    Serial.println(central.address()); // print the central's BT address
    
    digitalWrite(LED_BUILTIN, HIGH); // turn on the LED to indicate the connection

    // read value 3000ms
    // while the central is connected:
    while (central.connected()) {
      long currentMillis = millis();
      
      if (currentMillis - previousMillis >= 1000) { // if 5000ms have passed, we check the A1 pin
          // It responds almost immediately. Let's print out the data
        sensors.requestTemperatures(); // Send the command to get temperatures
        printTemperature(insideThermometer); // Use a simple function to print out the data
      
        loop_level();
        previousMillis = currentMillis;

        batterySend.writeValue(100);
        tempSend.writeValue(tempC);
        heightSend.writeValue(waterheight);

        if (displayMode.written()) {
          Serial.println(displayMode.value());
          update_display(displayMode.value());
        }

      }
    }
    
    digitalWrite(LED_BUILTIN, LOW); // when the central disconnects, turn off the LED
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }

}

void loop_level()
{
     // WATER LEVEL
 
  waterheight = analogRead(SENSORPIN);

//  float voltage = waterheight * (5.0/1023.0);
 
//  Serial.print("Analog reading "); 
//  Serial.println(waterheight);
 
  // convert the value to resistance
  waterheight = (1023 / waterheight)  - 1;
  waterheight = SERIESRESISTOR / waterheight;
//
//  Serial.print("Sensor resistance "); 
//  Serial.println(waterheight);
  
//  Serial.println(voltage);
  waterheight = 5-((waterheight-640)/140);

  Serial.print("Water height (inches): "); 
  Serial.println(waterheight);
  
  percentfull = (waterheight/5) *100;

  Serial.print("Percent full: "); 
  Serial.println(percentfull);
  
 
//  delay(1000);


}

void update_display(int mode){
  if(mode == 0){
    display.clearBuffer();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print("Height: ");
    display.print(waterheight);
    display.println("");
    display.display();
  } else if(mode == 1){
    display.clearBuffer();
    display.setTextSize(3);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print("Full: ");
    display.print(percentfull);
    display.println("");
    display.display();
  } else if(mode == 2){
    display.clearBuffer();
    display.setTextSize(3);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print("Temp: ");
    display.print(tempC);
    display.println("");
    display.display();
  } else if(mode == 3){
    display.clearBuffer();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print("Water Temp: ");
    display.print(tempC);
    display.println("");
    display.print("Percent Full: ");
    display.print(percentfull);
    display.println("");
    display.print("Water Height: ");
    display.print(waterheight);
    display.println("");
    display.display();
  }

//  delay(180000);
  
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
