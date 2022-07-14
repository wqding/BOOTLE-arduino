// Include the libraries we need
#include <OneWire.h>
#include <DallasTemperature.h>

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
#define SENSORPIN A1 

/*
 * Setup function. Here we do the basics
 */
void setup(void)
{
  // start serial port
  Serial.begin(9600);

  sensors.begin();


  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  
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
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.print("Parasite power is: ");
  
  // It responds almost immediately. Let's print out the data
  printTemperature(insideThermometer); // Use a simple function to print out the data

  loop_level();

}

void loop_level()
{
     // WATER LEVEL
 
  waterheight = analogRead(SENSORPIN);
 
//  Serial.print("Analog reading "); 
//  Serial.println(reading);
 
  // convert the value to resistance
  waterheight = (1023 / waterheight)  - 1;
  waterheight = SERIESRESISTOR / waterheight;
//  Serial.print("Sensor resistance "); 
//  Serial.println(reading);
  waterheight = 6 - ((waterheight-400)/120);

//  Serial.print("Water height (inches): "); 
//  Serial.println(reading);
  
  percentfull = (waterheight/5) *100;

  Serial.print("Percent full: "); 
  Serial.println(percentfull);
  
 
  delay(1000);


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
