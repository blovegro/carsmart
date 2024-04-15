// MRK1000 Code for CarSmart project
// Written by Bill Lovegrove for the hackster.io MKR1000 contest
// Reads OBD-II data via a Sparkfun OBD-II_UART board,
// Transmits the data to the Microsoft Azure IoT hub
// V1.2 Apr 15, 2024
// Some code modelled after the examples written by Mohan Palanisamy
// This code copyright Bill Lovegrove 3/29/2016 license: CC BY-NC-SA 3/0 
// http://creativecommons.org/licenses/by-nc-sa/3.0/

#include <SPI.h>
#include <WiFi101.h>
#include <stdlib.h>

const int LED = 6 ;
char ssid[] = "***"; //  Personal Wifi Name
char pass[] = "***";   // Personal wifi password

// Azure IoTHub details
char hostname[] = "***";
char feeduri[] = "***";
char authSAS[] = "SharedAccessSignature sr=***";

char message[] = "{ \"Seq\":\"0000\", \"DeviceID\":\"CarSmart\", \"Month\":\"3\", \"Temp\":\"+000\" }";
int sequence_num = 0;
const int seq_offset = 9; // offset in message to place sequence number
const int temp_offset = 60; // offset to place temp, sign then three digits
int engineTemp = 65; // Starting engine temp, around room temp for testing
int engineTempF = 0;  // temp converted to degrees F
char rxData[20]; // OBD receive buffer
char rxIndex = 0;
 
// Details for IoTHub access
unsigned long lastConnectionTime = 0;            
const unsigned long uploadInterval = 20000L; // 20 sec delay between uploads
int status = WL_IDLE_STATUS;
WiFiSSLClient client;

void setup() {
  // USB Serial port setup for debugging
  delay(500);
  Serial.println();
  Serial.println("Setup.");
  
  pinMode(LED, OUTPUT); // LED pin direction

  // External serial port to OBD
  Serial1.begin(9600);
  Serial1.println("ATZ"); // Reset the OBD
  delay(1000);
  getResponse(); // Get the echo
  Serial.println(rxData);
  getResponse(); // Get the blsnk lines
  getResponse(); // Get the blsnk lines
  getResponse(); // Get the response
  Serial.print("ATZ Response  ");
  Serial.println(rxData);
  getResponse(); // Get the blsnk lines
  Serial1.println("ATSP0"); // Auto detect protocol
  delay(500);
  getResponse(); // Get the echo
  Serial.println(rxData);
  getResponse(); // Get the response
  Serial.print("ATSP0 Response  ");
  Serial.println(rxData);
  getResponse(); // Get the blsnk line
  processODB();
  
  //check for the presence of wifi
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("No Wifi found.");
    // Fast blink to indicate error
    while (true) {
            // Fast blink if no shield
             digitalWrite(LED, HIGH);
             delay(125);
             digitalWrite(LED, LOW);
             delay(125);
    }
  }
  Serial.println("Found Wifi Interface");
  Serial1.flush(); // Flush ODB input port
   
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
             // wait 10 seconds for connection, brief blink
             Serial.println("Attempting to connect to Lovegrove Wifi...");
             digitalWrite(LED, HIGH);
             delay(125);
             digitalWrite(LED, LOW);
             delay(10000);
  }
  Serial.println("Connected to Lovegrove Wifi");
}

void loop() 
{
  String response = "";
  char c;
  ///Check for incoming responses
  while (client.available()) {
    c = client.read();
    response.concat(c);
  }

  if (!response.equals(""))
  {
    if (response.startsWith("HTTP/1.1 204"))
    {
      Serial.println("No messages");
    }
    else
    {
      // Azure error response messages will be received and printed here
      Serial.print("Response: ");
      Serial.println(response);
    }
  }

  // After uploadInterval, do a new transmission
  if (millis() - lastConnectionTime > uploadInterval) {
    processODB();
    Serial.println("transmitting");
    digitalWrite(LED, LOW);
    azureHttpPost();
  }
}

// this method makes an HTTPS post to the IoTHub
void azureHttpPost() {

  client.stop();

  if (client.connect(hostname, 443)) {
    // Increment the sequence number and insert into message
    sequence_num = sequence_num + 1;
    message[seq_offset] = sequence_num/1000 + '0';
    message[seq_offset+1] = (sequence_num%1000)/100 + '0';
    message[seq_offset+2] = (sequence_num%100)/10 + '0';
    message[seq_offset+3] = (sequence_num%10) + '0';
    // Insert engineTemp into message
    engineTempF = ((engineTemp-40)*9)/5 + 32;
    Serial.print("Engine temp (F): ");
    Serial.println((engineTempF));
    message[temp_offset] = (engineTempF<0) ? '-' : '+';
    message[temp_offset+1] = abs(engineTempF)/100 + '0';
    message[temp_offset+2] = (abs(engineTempF)%100)/10 + '0';
    message[temp_offset+3] = abs(engineTempF)%10 + '0';
    //make the POST request to the Azure IOT device port
    client.print("POST ");  //Do a POST
    client.print(feeduri);
    client.println(" HTTP/1.1"); 
    client.print("Host: "); 
    client.println(hostname);
    client.print("Authorization: ");
    client.println(authSAS);
    client.println("ContentType: application/json");
    client.print("Content-Length: ");
    client.println(sizeof(message));
    Serial.println(sizeof(message)); // message to serial port for debugging
    Serial.println(message);
    client.println(); // must have blank line to end header
    client.print(message);
    client.println();

    // time for updateInterval comparison
    lastConnectionTime = millis();
  }
  else {
    Serial.println("connection failed");
  }
}


// The getResponse function collects incoming data from the UART into the rxData buffer
// and only exits when a carriage return character is seen. Once the carriage return
// string is detected, the rxData buffer is null terminated (so we can treat it as a string)
// and the rxData index is reset to 0 so that the next string can be copied.
// This function was copied and modified with permission from the Sparkfun.com tutorial, copyright CC BY-NC-SA 3/0 
// http://creativecommons.org/licenses/by-nc-sa/3.0/
void getResponse(void){
  char inChar=0;
  //Keep reading characters until we get a carriage return
  while(inChar != '\r'){
    //If a character comes in on the serial port, we need to act on it.
    if(Serial1.available() > 0){
      //Start by checking if we've received the end of message character ('\r').
      if(Serial1.peek() == '\r'){
        //Clear the Serial buffer
        inChar=Serial1.read();
        //Put the end of string character on our data string
        rxData[rxIndex]='\0';
        //Reset the buffer index so that the next character goes back at the beginning of the string.
        rxIndex=0;
      }
      //If we didn't get the end of message character, just add the new character to the string.
      else{
        //Get the new character from the Serial port.
        inChar = Serial1.read();
        //Add the new character to the string, and increment the index variable.
        rxData[rxIndex++]=inChar;
      }
    }
  }
}

// Read and process the ODB Data
void processODB() {
  Serial.println("processODB");
  Serial1.println("0105"); // Request engine coolant
  getResponse(); // Get the echo
  Serial.print("OBD Echo:  ");
  Serial.println(rxData);
  getResponse(); // Get the SEARCING
  Serial.print("OBD Response1:  ");
  Serial.println(rxData);
  getResponse(); // Get the real response
  Serial.print("OBD Response 2: ");
  Serial.println(rxData);
  engineTemp = strtol(&rxData[6],0,16);
  Serial.print("Engine temp: ");
  Serial.println(engineTemp);
}
