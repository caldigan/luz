#include <Wire.h>
#include <BH1750.h>

BH1750 lightMeter;

void setup(){
  //Opens serial port sets data rate to 9600 bps
  Serial.begin(9600);
  Wire.begin();
  lightMeter.begin();    
}


void loop() {
  //read lux values
  String lux = String(lightMeter.readLightLevel());

  //pad with zeroes
  int numZeros = 6 - (lux.length() - 3);
  for (int i{ }; i < numZeros; i++){
    lux += "O";
  }

  //add separator
  lux += ",";

  //write to serial port
  for (char character: lux)
  {
    Serial.write(character);   
  }
}
