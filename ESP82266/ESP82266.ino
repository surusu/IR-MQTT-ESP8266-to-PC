#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFiUDP.h>

#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";

const char* serverIP = "192.168.0.106";  // Replace with the IP address of your C# program

// MQTT broker details
const char* mqttServer = "";
const int mqttPort = 8883;
const char* mqttUsername = "";
const char* mqttPassword = "";
const char* mqttTopicIN = "ESP8266Command";
const char* mqttTopicOUT = "ESP8266Thermometer";
const char* mqttClientId = "ESP8266Client";

// MAC address of the computer to wake up
byte targetMAC[] = { 0x2C, 0xF0, 0x5D, 0x80, 0xCC, 0x73 }; // Replace with your computer's MAC address

const int IR_PIN = D3;  // GPIO pin for IR receiver
IRrecv irrecv(IR_PIN);
decode_results results;

// Temperature
#define ADC_BITS 10
#define THERMISTORPIN A0
//const int THERMISTOR_PIN = A0;  // Analog input pin for the thermistor
#define VOLTAGE 3.3
#define SERIESRESISTOR 9515.14 // My 10K resistor seems to be not 10K.

// Steinhart–Hart coefficients
#define A 5.909400599e-3
#define B -2.907053169e-4
#define C 1.397790260e-7

// Science
#define KELVIN_FREEZING 273.15

#define ADC_MAX float(pow(2,ADC_BITS)-1)

WiFiClientSecure espClient;
PubSubClient client(espClient);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(10);

  irrecv.enableIRIn();  // Start the IR receiver

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Wi-Fi connected. IP address: ");
  Serial.println(WiFi.localIP());

  espClient.setInsecure();

  // Connect to MQTT broker
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  while (!client.connected()) {
    if (client.connect(mqttClientId, mqttUsername, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(mqttTopicIN);
    }
    else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (irrecv.decode(&results)) {
    unsigned int value = results.value;
    // Output the received IR command
    switch (value) {
      case 0xFFE21D:  // Power button code (replace with your remote's power button code)
        Serial.println("POWER button pressed");
        sendWakeOnLAN();
        Serial.println("Wake on LAN packet sent");
        break;
      case 0xFFFFFFFF:  // Hold button code
        break;
      default:
        Serial.print("Received IR command: 0x");
        Serial.println(value, HEX);

        /*char buffer[20]; // Create a character array to hold the converted string
  
        // Convert the long to a string
        ltoa(number, buffer, 10);
  
        // Print the converted string
        Serial.println(buffer);*/

        StaticJsonDocument<200> jsonDoc;
        jsonDoc["IR"] = String(value, HEX);
        String jsonStr;
        serializeJson(jsonDoc, jsonStr);

        sendToPC(jsonStr);
        break;
    }

    irrecv.resume();  // Receive the next IR signal
  }

  // put your main code here, to run repeatedly:
  /*if (!client.connected()) {
    reconnect();
  }
  client.loop();*/

  //float temperature = readTemperature();
  //sendTemperatureData(temperature);

  static unsigned long previousTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - previousTime >= 10000) {  // Send temperature data every 10 seconds
    float temperature = readTemperature();
    //sendTemperatureData(temperature);

    StaticJsonDocument<200> jsonDoc;
    jsonDoc["temperature"] = temperature;
    String jsonStr;
    serializeJson(jsonDoc, jsonStr);

    sendToPC(jsonStr);
    sendToMQTT(jsonStr);

    previousTime = currentTime;
  }


}

void sendToPC(String command) {
  // Construct the command packet
  //String command = String(value, HEX);
  command.toLowerCase();

  // Send the command to the C# program via UDP
  WiFiUDP udp;
  udp.beginPacket(serverIP, 1234);  // Send to the C# program's IP address and port
  
  /*StaticJsonDocument<200> jsonDoc;
  jsonDoc["command"] = command;
  String jsonStr;
  serializeJson(jsonDoc, jsonStr);*/
  
  udp.print(command);
  udp.endPacket();

  Serial.println("Data sent to PC: " + command);
}

void sendToMQTT(String command) {
client.publish(mqttTopicOUT, command.c_str());
  Serial.println("Data sent via MQTT: " + command);
}

/*
void sendTemperatureData(float temperature) {
  // Send the temperature data via MQTT
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["temperature"] = temperature;
  String jsonStr;
  serializeJson(jsonDoc, jsonStr);

  client.publish(mqttTopic, jsonStr.c_str());

  Serial.println("Temperature data sent via MQTT: " + jsonStr);
}*/

/*float readTemperature() {
  int rawADC = analogRead(THERMISTOR_PIN);
  float resistance = 10000.0 * (1023.0 / rawADC - 1.0);
  float temperature = 1.0 / (log(resistance / 10000.0) / 3950.0 + 1.0 / 298.15) - 273.15;

  return temperature;
}*/

float readTemperature(){
  float reading = analogRead(THERMISTORPIN);

  //Serial.print("Analog reading: ");
  //Serial.println(reading);

  float voltage = reading * (VOLTAGE / ADC_MAX);
  //Serial.print("Voltage: ");
  //Serial.println(voltage);

  //float resistance = SERIESRESISTOR/(ADC_MAX/reading-1);  // 10K / (1023/ADC - 1)
  float resistance = SERIESRESISTOR * (1023.0 / (float)reading - 1.0); //вычислите сопротивление на термисторе

  //Serial.print("Thermistor resistance: ");
  //Serial.println(resistance);

  float steinhartTemperature = steinhart(resistance);
  Serial.print("Steinhart Temperature: ");
  Serial.print(steinhartTemperature);
  Serial.println("C");
  return steinhartTemperature;
}

float steinhart(float resistance){
  return 1 / (A + B * log(resistance) + C * pow(log(resistance), 3)) - KELVIN_FREEZING;     
}

/*
const float RESISTOR_VALUE = 8.75;
const float THERMISTOR_REFERENCE_TEMP = 28.2;
const float THERMISTOR_NOMINAL_RESISTANCE = 10000.0;
const float THERMISTOR_BETA_VALUE = 3950;


float readTemperature() {
  int rawADC = analogRead(THERMISTOR_PIN);
  float voltage = rawADC * (3.3 / 1023.0); // Convert raw ADC value to voltage
  float resistance = (3.3 - voltage) * RESISTOR_VALUE / voltage; // Calculate thermistor resistance
  float temperature = 1.0 / (log(resistance / THERMISTOR_NOMINAL_RESISTANCE) / THERMISTOR_BETA_VALUE + 1.0 / THERMISTOR_REFERENCE_TEMP) - 273.15; // Calculate temperature using Steinhart-Hart equation

  return temperature;
}*/

/*
void callback(char* topic, byte* payload, unsigned int length) {
  // Convert the received payload to a string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("MQTT eceived message: ");
  Serial.println(message);

  if (message == "wake_up") {
    // Check if the received message matches the expected task
    sendWakeOnLAN();
    Serial.println("Wake on LAN packet sent");
  } else {
    sendToPC(message);
  }
}*/

void callback(char* topic, byte* payload, unsigned int length) {
  // Convert the received payload to a string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("MQTT received message: ");
  Serial.println(message);

  if (message == "wake_up") {
    // Check if the received message matches the expected task
    sendWakeOnLAN();
    Serial.println("Wake on LAN packet sent");
  } else {
    sendToPC(message);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqttClientId, mqttUsername, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(mqttTopicIN);
    }
    else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void sendWakeOnLAN() {
  // Broadcast address
  IPAddress broadcastIP(255, 255, 255, 255);

  // Create UDP client
  WiFiUDP udp;
  udp.begin(9); // Use port 9 for Wake on LAN

  // Construct Wake on LAN packet
  byte packet[102];
  memset(packet, 0xFF, 6); // Destination MAC address
  for (int i = 6; i < 102; i += 6) {
    memcpy(&packet[i], &targetMAC, 6); // Repeat target MAC address 16 times
  }

  // Send Wake on LAN packet
  udp.beginPacket(broadcastIP, 9);
  udp.write(packet, sizeof(packet));
  udp.endPacket();
  udp.stop();
}