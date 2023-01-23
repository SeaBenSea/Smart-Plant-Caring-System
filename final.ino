#include "DHT.h"             //DHT library
#include <LiquidCrystal.h>   //LCD library
#include <SoftwareSerial.h>  //LCD library
#include <Servo.h>           //Servo motor library

#define DEBUG false  //Set true for activite debug mode while sending and getting data

int DHT_pin = 2;   //DHT pin
int pump_pin = 8;  //Water pump pin
int rxPin = 10;    //ESP8266 RX pin
int txPin = 11;    //ESP8266 TX pin
int soilPin = A0;  //Soild moisture sensor pin

String SSN = "";      //WIFI SSN
String pass = "";    //Password of WIFI
String ip = "184.106.153.149";  //Ip adress of Thingspeak

SoftwareSerial esp(rxPin, txPin);     //Setting connections of ESP8266
LiquidCrystal lcd(1, 3, 4, 5, 6, 7);  //Define LCD pins
DHT dht;                              //Define DHT
Servo curtain_servo;                  //Define servo

int sensorValue;        //For storing humidity value that comes from sensor but in bit mode
float temp;             //For storing temperature value that comes from sensor
float humi;             //For storing humidty percentage value
String temp_txt;        //For storing the temperature value that comes from cloud
String humi_txt;        //For storing the humidty percentage value that comes from cloud
String longi_txt;       //For storing the longitude value that comes from cloud
String lat_txt;         //For storing the latitude value that comes from cloud
String sunrise_txt;     //For storing the sunrise value that comes from cloud
String sunset_txt;      //For storing the sunset value that comes from cloud
String weather_txt;     //For storing the weather condition value that comes from cloud
String hour_txt;        //For storing the hour value that comes from cloud
String minute_txt;      //For storing the minute value that comes from cloud
bool curtains = false;  //For storing the curtain condition true for opened false for closed
bool pump = false;      //For storing the pump condition true for running false for stopped

void setup() {
  Serial.begin(9600);                                        //Start serial port on 9600 baud
  dht.setup(DHT_pin);                                        //Setup dht
  lcd.begin(16, 2);                                          //Setup LCD
  pinMode(pump_pin, OUTPUT);                                 //Setup water pump
  curtain_servo.attach(9);                                   //Attach the servo to our servo object
                                                             //
  Serial.println("Started");                                 //LOG started
  esp.begin(9600);                                           //Started serial on 9600 baud with ESP
  esp.println("AT");                                         //Control the module with AT command
  Serial.println("AT Sent");                                 //LOG AT sent
  while (!esp.find("OK")) {                                  //Wait until get OK from ESP
    esp.println("AT");                                       //If fails keep sendin AT command
    Serial.println("ESP8266 can't find.");                   //LOG can not found ESP
  }                                                          //
  Serial.println("Got OK");                                  //LOG OK
  esp.println("AT+CWMODE=1");                                //Set ESP module as client
  while (!esp.find("OK")) {                                  //Wait until get OK from ESP
    esp.println("AT+CWMODE=1");                              //If fails keep sendin clint code
    Serial.println("Setting ESP....");                       //LOG Setting ESPs
  }                                                          //
  Serial.println("Mode Client");                             //LOG mode of ESP
  Serial.println("Connecting...");                           //Start connection with WIFI
  esp.println("AT+CWJAP=\"" + SSN + "\",\"" + pass + "\"");  //Connect to WIFI
  while (!esp.find("OK"))                                    //
    ;                                                        //Wait until get OK from ESP
  Serial.println("Connected.");                              //LOG Connected
  delay(1000);                                               //Sleep 1 second
}

void loop() {
  read_values();       //Read values from sensors
  update_lcd();        //Update the LCD screen
  sendth(temp, humi);  //Send data to cloud
  get_data();          //Get data from cloud
  check_curtain();     //Check the curtain condition
  check_humidty();     //Check the pump condition
  delay(1000);         //Sleep 1 second
}

// Functions

void run_pump(float humidity) {                                  //For running the pump
  digitalWrite(pump_pin, HIGH);                                  //Start the pump motor
  pump = true;                                                   //Set pump condition true for running
                                                                 //calculate the run time of the pump
                                                                 // we want to run the pump until the humidity is 40%
                                                                 //our pump water output is 80L/Hour
                                                                 //so we need to run the pump for 80L/Hour * (humidity - 40) / 100
                                                                 //we need to convert the time to miliseconds
                                                                 //so we need to multiply the time with 3600 * 1000
                                                                 //we need to convert the time to int
  int run_time = int(80 * (humidity - 40) / 100 * 3600 * 1000);  //Formula
  delay(run_time);                                               //Run the pump for calculated time
  digitalWrite(pump_pin, LOW);                                   //Stop the pump motor
  pump = false;                                                  //Set pump condition false for stopped
  delay(1);                                                      //
}

void read_values() {                    //Read values from sensors
  sensorValue = analogRead(soilPin);    //Read and set sensor value of humidty
  temp = dht.getTemperature();          //Read and set sensor value of temperature
  humi = (sensorValue / 1023.0) * 100;  //Set humidity value as percentage
}

void update_lcd() {     //Updating the texts on the LCD screen
  lcd.setCursor(0, 0);  //Set cursor on starting of the first line
  lcd.print("Temp: ");  //Write Temp:
  lcd.print(temp);      //Write the temperature value that we stored
  lcd.print(" C");      //Write the unit of the temperature
  lcd.setCursor(0, 1);  //Set cursor on starting of the second line
  lcd.print("Humi: ");  //Write Humi:
  lcd.print(humi);      //Write the humidty percentage value that we stored
  lcd.print(" %");      //Write the percentage sign
}

void open_curtain() {       //For opening the curtain
  curtain_servo.write(45);  //Rotate the motor counterclockwise
  delay(2000);              //Keep rotating for 2 seconds
  curtain_servo.write(90);  //Stop the motor
  delay(1);                 //Stay stopped
}

void close_curtain() {       //For closing the curtain
  curtain_servo.write(135);  //Rotate the motor clockwise
  delay(2000);               //Keep rotating for 2 seconds
  curtain_servo.write(90);   //Stop the motor
  delay(1);                  //Stay stopped
}

bool sun() {                              //For checking if the sun is up or down
  float sunrise = sunrise_txt.toFloat();  //Convert the sunrise time to float
  float sunset = sunset_txt.toFloat();    //Convert the sunset time to float
  int hour = hour_txt.toInt() + 3;        //Convert the hour to int and add 3 hours for the timezone
  int minute = minute_txt.toInt();        //Convert the minute to int
  float time = hour + (minute / 100.0);   //Calculate the time
  if (time > sunrise && time < sunset) {  //Check if the time is between sunrise and sunset
    return true;                          //If it is return true
  } else {                                //If it is not
    return false;                         //Return false
  }                                       //
}

bool weather() {             //For checking the weather condition
  if (weather_txt == "1") {  //Check if the weather Clear
    return true;             //If it is return true
  } else {                   //If it is not
    return false;            //Return false
  }                          //
}

//Functions for the checks
void check_curtain() {                    //For checking every needed data and deciding the curtain mode
                                          //Open the curtain if the sun is up and the weather is clear
  if (sun() && weather() && !curtains) {  //Check if the sun is up, the weather is clear and the curtain is closed
    open_curtain();                       //Open the curtain
    curtains = true;                      //Set the curtain mode to opened
  } else if (!sun() && curtains) {        //Check if the sun is down and the curtain is opened
    close_curtain();                      //Close the curtain
    curtains = false;                     //Set the curtain mode to closed
  }                                       //If the sun is down and the weather is not clear, do nothing
}

void check_humidty() {             //For checking the humidty and pump mode and run the pump if needed
  if (humi < 40 && !pump) {        //Check if the humidty is less than 40% and the pump is not running
    run_pump(humi);                //Run the pump
  } else if (humi > 40 && pump) {  //Check if the humidty is more than 40% and the pump is running
    digitalWrite(pump_pin, LOW);   //Stop the pump motor
    pump = false;                  //Set the pump mode to stopped
  }                                //If the humidty is between 40% and 100% and the pump is not running, do nothing
}


//Functions for the communucaiton between cloud and Arduino


void sendth(float temp, float humi) {                                              //Send stored temperature and humidty percentage values to cloud
  esp.println("AT+CIPSTART=\"TCP\",\"" + ip + "\",80");                            //Connecting to Thingspeak
  if (esp.find("Error")) {                                                         //Check for the connection error
    Serial.println("AT+CIPSTART Error");                                           //LOG ERROR
  }                                                                                //
  String data = "GET https://api.thingspeak.com/update?api_key=";  //Our api call address
  data += "&field1=";                                                              //field 1 (temperature)
  data += String(temp);                                                            //field 1 (temperature) data
  data += "&field2=";                                                              //field 2 (humidty)
  data += String(humi);                                                            //field 2 (humidty) data
  data += "\r\n\r\n";                                                              //End of the call
  esp.print("AT+CIPSEND=");                                                        //Sending the length of our data that we will send to ESP
  esp.println(data.length() + 2);                                                  //Add the length here
  delay(2000);                                                                     //Sleep 2 seconds
  if (esp.find(">")) {                                                             //Runs the commands inside of the ESP when it is ready
    Serial.println(esp.read());                                                    //Send the data
    Serial.println(esp.print(data));                                               //Send the data
    Serial.println("Data sent");                                                   //LOG data sent
    delay(1000);                                                                   //Sleep 1 second
  }                                                                                //
  Serial.println("Connection Closed.");                                            //LOG Connection closed
  esp.println("AT+CIPCLOSE");                                                      //Closing the connection with ESP
  delay(5000);                                                                     //Sleep 5 seconds
}

String sendData(String command, const int timer, boolean debug) {  //For sending the get data commands
  String response = "";                                            //Set default response text to empty
  esp.print(command);                                              //Run the command on ESP
  long int time = millis();                                        //Create time object
  while ((time + timer) > millis()) {                              //Until the passed time finished run it
    while (esp.available()) {                                      //If there is a data on ESP's in port
      char c = esp.read();                                         //Read every character and
      response += c;                                               //Add every charecter to response text
    }                                                              //
  }                                                                //
  if (debug) {                                                     //If debug mode enabled
    Serial.print(response);                                        //LOG response that we got
  }                                                                //
  return response;                                                 //Return response text
}

void get_data() {                                                                       //For seperating the only important parts of our response data
  esp.println("AT+CIPSTART=\"TCP\",\"" + ip + "\",80");                                 //Connect Thingspeak with TCP
  if (esp.find("Error")) {                                                              //Check for the connection errors
    Serial.println("AT+CIPSTART Error");                                                //LOG Error
  }                                                                                     //
  String rest = "AT+CIPSEND=90";                                                        //Create a string for rest API call
  rest += "\r\n";                                                                       //Add next lines end of the string for the ESP to understand the call is ended
  sendData(rest, 2000, 0);                                                              //Send data
  String host = "GET /channels//feeds.json?api_key=&results=1";  //Create another string for getting the result
  host += "\r\n";                                                                       //Add next lines end of the string for the ESP to understand the call is ended
  host += "Host:api.thingspeak.com";                                                    //Set the host
  host += "\r\n\r\n\r\n\r\n\r\n";                                                       //Add next lines end of the string for the ESP to understand the call is ended
  String response = sendData(host, 2000, 0);                                            //GET call ( GET /apps/thinghttp/send_request?api_key=XXXXXXXXXXXXXXXX Host: Host_server_name )
                                                                                        //
  int start_index = response.lastIndexOf(':');                                          //Find and store last index of the : from response
  hour_txt = response.substring(start_index - 139, start_index - 137);                  //Get the hour from the response
  minute_txt = response.substring(start_index - 135, start_index - 134);                //Get the minute from the response
  temp_txt = response.substring(start_index - 104, start_index - 99);                   //Get the temperature from the response
  humi_txt = response.substring(start_index - 87, start_index - 82);                    //Get the humidty percentage from the response
  longi_txt = response.substring(start_index - 70, start_index - 63);                   //Get the longitude from the response
  lat_txt = response.substring(start_index - 51, start_index - 44);                     //Get the latitude from the response
  sunrise_txt = response.substring(start_index - 32, start_index - 27);                 //Get the sunrise time from the response
  sunset_txt = response.substring(start_index - 15, start_index - 10);                  //Get the sunset time from the response
  weather_txt = response.substring(start_index + 1, start_index + 2);                   //Get the weather condition from the response
                                                                                        //
  Serial.println("temp:" + temp_txt);                                                   //LOG temperature
  Serial.println("humi:" + humi_txt);                                                   //LOG humidty percentage
  Serial.println("longi:" + longi_txt);                                                 //LOG longitude
  Serial.println("lat:" + lat_txt);                                                     //LOG latitude
  Serial.println("sunrise:" + sunrise_txt);                                             //LOG sunrise time
  Serial.println("sunset:" + sunset_txt);                                               //LOG sunset time
  Serial.println("weather:" + weather_txt);                                             //LOG weather condition
  Serial.println("Connection Closed.");                                                 //LOG connection closed
  esp.println("AT+CIPCLOSE");                                                           //Close the connection
}
