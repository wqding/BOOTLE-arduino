// Include the libraries we need
#include <Arduino.h>
#include <ArduinoBLE.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Adafruit_ThinkInk.h"

#include "Battery.h"
#include <Arduino_PMIC.h>

// EINK pinouts (Correspond to pin #s on the board)
#define EPD_CS      7
#define EPD_DC      6
#define SRAM_CS     5  // changed from pin 11 because 11 is the i2c and interferes with PMIC register reads
#define EPD_RESET   14 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    13 // can set to -1 to not use a pin (will wait a fixed delay)

// Initialize the EINK display
ThinkInk_290_Mono_M06 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

// STORES THE CURRENT TEMP VALUE
float tempC;

// STORES THE CURRENT WATER LEVEL IN INCHES
float waterheight;

// STORES PERCENT FULL OF BOTTLE (ASSUMING MAX HEIGHT IS 5 INCHES)
float percentfull;

// STORES BATTERY PERCENTAGE
int bat_percent;

// STORES BATTERY CHARGING STATE
bool bat_charge;

// STORES CURRENT WEATHER TEMPERATURE
float weather;

// STORES whether connected to a phone
bool is_connected;

// the value of the 'other' resistor
#define SERIESRESISTOR 560    
 
// Analog pin to connect the water level sensor to
#define SENSORPIN A3 

// BLUETOOTH STUFF
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BATTERY_READ_UUID "6d68efe5-04b6-4a85-abc4-c2670b7bf7fd"
#define TEMPERATURE_READ_UUID "6ade8e90-47fd-41ce-b0fc-8069d8b36656"
#define HEIGHT_READ_UUID "3b53d5f2-c5f7-4652-aadc-220bcbe2a3a1"
#define WEATHER_WRITE_UUID "c64387ae-75f1-4194-b83d-bbb261c81f39"
#define DIGITAL_WRITE_UUID "f27b53ad-c63d-49a0-8c0f-9f297e6cc520"

BLEService newService(SERVICE_UUID); // creating the service

// Characteristics for sending data back to the app
BLEByteCharacteristic heightSend(HEIGHT_READ_UUID, BLERead | BLENotify | BLEWrite); // creating the Analog Value characteristic
BLEByteCharacteristic tempSend(TEMPERATURE_READ_UUID, BLERead | BLENotify | BLEWrite); // creating the Analog Value characteristic
BLEByteCharacteristic batterySend(BATTERY_READ_UUID, BLERead | BLENotify | BLEWrite); // creating the Analog Value characteristic

// Characteristics for receiving data on the device
BLEByteCharacteristic weatherDisplay(WEATHER_WRITE_UUID, BLERead | BLENotify | BLEWrite); // creating the LED characteristic
BLEByteCharacteristic displayMode(DIGITAL_WRITE_UUID, BLERead | BLENotify | BLEWrite); // creating the LED characteristic

long previousMillis = 0;
long previousDisplayMillis = 0;

// Define the battery voltage object
Battery battery = Battery(3000, 4200, ADC_BATTERY);

/*
 * Setup function
 */
void setup(void)
{
  // Start serial port to baud rate of 9600
  Serial.begin(9600);
  //while (!Serial) { delay(10); }

  // Initialize PMIC I2C API
  if (!PMIC.begin()) {
    Serial.println("Failed to initialize PMIC!");
    while (1);
  }

  // Start battery, temperature sensor (sensors) and display instances
  battery.begin(3300, 1.275, &sigmoidal); // https://github.com/rlogiacco/BatterySense/issues/29 somehow works?
  sensors.begin();
  display.begin(THINKINK_MONO);

  // Show booting message on bottle
  DisplayWelcome();

  // Initialize the built-in LED to indicate when a central is connected
  pinMode(LED_BUILTIN, OUTPUT); 
  digitalWrite(LED_BUILTIN, LOW); 

  // Initialize the bluetooth API object
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  // Setting a name that will appear when scanning for bluetooth devices
  BLE.setLocalName("BOOTLE"); 
  BLE.setAdvertisedService(newService);

  // Add characteristics to a new service
  newService.addCharacteristic(displayMode); 
  newService.addCharacteristic(heightSend);
  newService.addCharacteristic(tempSend);
  newService.addCharacteristic(batterySend);
  newService.addCharacteristic(weatherDisplay);

  // Adding the service
  BLE.addService(newService);

  //et initial value for characteristics
  displayMode.writeValue(3); 
  heightSend.writeValue(1);
  tempSend.writeValue(1);
  batterySend.writeValue(1);
  weatherDisplay.writeValue(1);

  BLE.advertise(); // Start advertising the service
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

  get_data();
  update_display(0);
}

// Displays a simple booting message when the bottle powers-up
void DisplayWelcome() {
  display.clearBuffer();
  display.setTextSize(2);
  display.setCursor((display.width() - 180)/2, (display.height() - 24)/2);
  display.setTextColor(EPD_BLACK);
  display.print("Booting BOOTLE");
  display.display();
}

// Function to print the temperature for a device
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

// Function to get battery state and charging state 
void getBatteryState() {
  bat_percent = battery.level();

  switch(PMIC.chargeStatus()) {
  case 0x00: 
    bat_charge = false;
  break;
  default:
    bat_charge = true;
  }

  Serial.print("Battery Percentage: ");
  Serial.print(bat_percent);
  Serial.println("%");
  Serial.print("Battery Charging? ");
  Serial.println(bat_charge ? "true" : "false");
}

/*
 * Main function routine
 */
void loop(void)
{ 

  BLEDevice central = BLE.central(); // wait for a BLE central

  if (central) {  // if a central is connected to the peripheral
    Serial.print("Connected to central: ");
    Serial.println(central.address()); // print the central's BT address
    
    digitalWrite(LED_BUILTIN, HIGH); // turn on the LED to indicate the connection
    is_connected = true; 
    update_display(0);

    // Main routine while connected to the app
    // App sends data values via button click

    // read value 3000ms
    // while the central is connected:
    while (central.connected()) {
      long currentMillis = millis();
      
      if (currentMillis - previousMillis >= 1000) { // if 5000ms have passed, we check the A1 pin
        // It responds almost immediately. Let's print out the data

        get_data();

        // Get temperature value from sensor
        // sensors.requestTemperatures(); // Send the command to get temperatures
        // printTemperature(insideThermometer); // Use a simple function to print out the data

        // // Get water level from the sensor
        // //loop_level();
        // loop_fake();

        // // Get battery percentage and charging state
        // getBatteryState();

        previousMillis = currentMillis;

        // Send data for battery, temperature, and height back to the app
        batterySend.writeValue(bat_percent);
        tempSend.writeValue(tempC);
        heightSend.writeValue(percentfull);

        // weather information API here: 
        // HERE: some call to get it 

        // App doesn't have external temperature data, just hardcode -5 C or something
        //weather = -5;

        // Based on button press on app, change the information on the display
        if (displayMode.written()) {
          int newMode = displayMode.value();
          //displayMode.readValue(data_read, sizeof(data_read));

          if(newMode == 4) {
            Serial.println(weatherDisplay.value());
            int temp_weather = weatherDisplay.value() - 100;
            weather = 0;
            weather += (float)temp_weather + (float)(random(-30, 30))/100.00;
          }
          //Serial.println(displayMode.value());
          Serial.println(newMode);
          Serial.println(weather);
          // update_display(displayMode.value());
          update_display(newMode);
        }

      }
    }
    
    digitalWrite(LED_BUILTIN, LOW); // when the central disconnects, turn off the LED
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  } else { // if we don't have bluetooth connection, default display to show some information
    long currentMillis = millis();
    if(currentMillis - previousMillis >= 1000) {

      is_connected = false; 
      get_data();
      // // Get the thermometer temperature
      // sensors.requestTemperatures();
      // printTemperature(insideThermometer);

      // // Get the water level
      // //loop_level();
      // loop_fake();

      // // Get battery state and charging state
      // getBatteryState();

      // // Actual outside weather
      // Serial.print("Weather: ");
      // Serial.print(weather);
      // Serial.print((char)248);
      // Serial.println("C");
      
      previousMillis = currentMillis;
    }

    if(currentMillis - previousDisplayMillis >= 10000) {
        previousDisplayMillis = currentMillis;
        update_display(0);
    }
  }

}

void get_data() {
  // Get the thermometer temperature
  sensors.requestTemperatures();
  printTemperature(insideThermometer);

  // Get the water level
  //loop_level();
  //loop_fake();
  water_level();

  // Get battery state and charging state
  getBatteryState();

  // Actual outside weather
  Serial.print("Weather: ");
  Serial.print(weather);
  Serial.print((char)248);
  Serial.println("C");
}

void loop_fake() {
  waterheight = 2.00; 

  waterheight += (float)(random(-40, 40) / 100.00);

  percentfull = (waterheight/5.0) * 100;
}

// float resistances[] = {610.00, 570.00, 525.00, 490.00, 470.00, 455.00}; 
// float heights[] = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0};

float resistances[] = {900.00, 860.00, 820.00, 680.00, 590.00, 550.00}; 
float heights[] = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0};

float lin_interpolation(float x) {
      // find the two points that x lies between
    int i = 0;
    while (i < 5 && x < resistances[i + 1]) {
        i++;
    }

    if (i < 5 && abs(x - resistances[i + 1]) <= 40.0) {
      return heights[i + 1];
    }
    
    // perform linear interpolation using the two points
    float x0 = resistances[i];
    float y0 = heights[i];

    float x1 = resistances[i+1];
    float y1 = heights[i+1];

    float res = y0 + (x - x0) * ((y1 - y0)/(x1 - x0));

    return res;
}

int lowerLimit = 500;   // Sensor completely in the water
int upperLimit = 900; // 610;   // Sensor completely dry
float sensorlength = 12.7;  // in cm (around 5 inches)

float resistance = 0.0;

void water_level() {
  
  float inputValue = analogRead(SENSORPIN);
  resistance = inputValue;

  float result = 0.0;
  if(inputValue > lowerLimit && inputValue < upperLimit)
  {
    // calculate Range
    // float range = upperLimit - lowerLimit;
    // // offset input Value
    // inputValue -= lowerLimit;
    // // calculate height
    // result = sensorlength * inputValue/range;
    // result = sensorlength - result;
    
    result = lin_interpolation(inputValue);
  } 
  else if(inputValue >= upperLimit)
  {
    // set min
    result = 0.0;   
  } 
  else if(inputValue <= lowerLimit)
  {
    // set max
    result = sensorlength / 2.54;
  }
  
  waterheight = result;
  percentfull = waterheight/5 *100;
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
  Serial.println(waterheight);
  
//  Serial.println(voltage);
  //waterheight = 5-((waterheight-640)/140);
  //waterheight = 15 + waterheight * (5.5-1)/(1450-2200);

  // Serial.println( (4*(waterheight - 870))/522);
  Serial.println( (4*(waterheight - 870))/430 + 3);
  waterheight = 5 - (4*(waterheight - 870))/430 +3;

  Serial.print("Water height (inches): "); 
  Serial.println(waterheight);
  
  percentfull = (waterheight/5) *100;

  Serial.print("Percent full: "); 
  Serial.println(percentfull);
//  delay(1000);
}

void update_display(int mode){
  // TODO: 
  // Need to 1) make prettier
  // 2) keep some values (battery + charging state) persistent
  // "Create an interface" that looks relatively smart
  if(mode == 0){
    display.clearBuffer();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    if(is_connected) {
      display.print("Weather ");
      display.print(weather);
      display.print((char)248);
      display.print("C");
      display.println("");
    }
    display.print("Water Temp: ");
    display.print(tempC);
    display.print((char)248);
    display.print("C");
    display.println("");
    display.print("Percent Full: ");
    display.print(percentfull);
    display.println("%");
    display.print("Water Height: ");
    display.print(waterheight);
    display.println(" in");
    display.print("Battery Capacity: ");
    display.print(bat_percent);
    display.print("%");
    display.println("");
    display.print("Charging: ");
    display.print(bat_charge ? "true" : "false");
    display.println("");
    display.print("Connected to app: ");
    display.print(is_connected ? "Yes" : "No");
    display.println("");
    // display.print("Debug level resistance: ");
    // display.print(resistance);
    // display.println("");
    display.display();
    // display.clearBuffer();
    // display.setTextSize(2);
    // display.setCursor(0, 0);
    // display.setTextColor(EPD_BLACK);
    // display.print("Height: ");
    // display.print(waterheight);
    // display.println("");
    // display.display();
  } else if (mode == 1) {
    display.clearBuffer();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print("Battery Capacity: ");
    display.print(bat_percent);
    display.print("%");
    display.println("");
    display.print("Charging: ");
    display.print(bat_charge ? "true" : "false");
    display.println("");
    display.display();
  } else if(mode == 2) {
    display.clearBuffer();
    display.setTextSize(3);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print("Full: ");
    display.print(percentfull);
    display.println("%");
    display.display();
  } else if(mode == 3) {
    display.clearBuffer();
    display.setTextSize(3);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print("Temp: ");
    display.print(tempC);
    display.print((char)248);
    display.print("C");
    display.println("");
    display.display();
  } else if(mode == 4) {
    display.clearBuffer();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.setTextColor(EPD_BLACK);
    display.print("Weather: ");
    display.print(weather);
    display.print((char)248);
    display.print("C");
    display.println("");
    // display.print("Percent Full: ");
    // display.print(percentfull);
    // display.println("");
    // display.print("Water Height: ");
    // display.print(waterheight);
    // display.println("");
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
