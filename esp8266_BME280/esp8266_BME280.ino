/*
 Name:		esp8266_BME280.ino
 Created:	2019-10-10 20:38:43
 Author:	danielv
*/

/*

Wiring

ESP D2 - BME280 SDA
ESP D1 - BME280 SDL
ESP GND - BME280 GND
ESP 3v3 - BME280 VCC

*/
#include <Ticker.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// MQTT
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
char clientId[90]; // Generated on mqtt connection

// LED controls
#include <jled.h>

// Sensor libraries
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <EEPROM.h>

// Program flow
#include <Ticker.h>

bool ota_started;

// BME280 weather sensor
/*
const int BME_SCK = 6;
const int BME_MISO = 4;
const int BME_MOSI = 7;
const int BME_CS = 5;
*/
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
Adafruit_BME280 bme280;
#define SEALEVELPRESSURE_HPA (1013.25)

char* ssid = "Superbox";
char* password = "aq12wsxc";
//const char* mqtt_server = "mqtt.eclipse.org";
char* mqtt_server = "192.168.10.31";

Ticker ticker;
bool sendUpdate;
float fTemp;
float bPress;
float rH;
float dPoint;
float hIndex;

void setup()
{
	pinMode(D0, OUTPUT);
	pinMode(D4, OUTPUT);
	pinMode(D6, INPUT_PULLUP);
	
	// Try reading parameters from EEPROM
	EEPROM.begin(512);
	delay(500);
	char stored_mqtt_server[50];
	EEPROM.get(50, stored_mqtt_server);
	char stored_ssid[50];
	EEPROM.get(100, stored_ssid);
	char stored_password[50];
	EEPROM.get(150, stored_password);
	char stored_hostname[50];
	EEPROM.get(200, stored_hostname);
	if (digitalRead(D6) == LOW) {
		ssid = stored_ssid;
		password = stored_password;
		mqtt_server = stored_mqtt_server;
	}
	Serial.begin(115200);
	bool status;
	status = bme280.begin();
	if (!status) {
		Serial.println("Could not find a valid BME280 sensor, check wiring!");
		while (1);
	}
	delay(100);
	ConnectToWiFi();
	StartOTAIfRequired();
	PrintWifiStatus();
	Serial.println("Connected to wifi");
	ticker.attach(1, readSensors);
	Serial.print("Connecting to MQTT server: ");
	Serial.println(mqtt_server);
	Serial.print("EEPROM MQTT server: ");
	Serial.println(stored_mqtt_server);
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
}
// Status LEDs
auto led_wifi_connected = JLed(D4).LowActive().Breathe(3000).DelayAfter(1500).Forever();
auto led_error = JLed(D0).LowActive().Blink(100, 100).Forever();

void loop()
{
	if (WiFi.isConnected()) {
		led_wifi_connected.Update();
		digitalWrite(D0, HIGH);
		// Ensure MQTT stays connected as well
		if (!client.connected()) {
			mqttReconnect();
		} else if (sendUpdate){
			// Send through MQTT if we are connected
			// The following values are set in readSensors, which runs on an interval using Ticker
			sendUpdate = false;
			publishStatistic("temperature", fTemp);
			publishStatistic("humidity", rH);
			publishStatistic("pressure", bPress);
		}
		client.loop();
	} else {
		digitalWrite(D4, HIGH);
		led_error.Update();
	}
	
	//Serial.println("Hello world");
	HandleOTA();
}
void readSensors() {
	// This is a Ticker callback, which is interupt based. Sending network calls from here causes the mcu firmware to crash.
	// Instead, we set a flag and do the network calls in loop() instead.
	sendUpdate = true;
	fTemp = bme280.readTemperature(); // In Celcius
	bPress = bme280.readPressure() / 100.0F; // In mBar
	//Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA)); // IN meters
	rH = bme280.readHumidity(); // In %

	//Serial.print("Dew Point = ");
	dPoint = fTemp - ((100 - rH) / 5);
	//Serial.print("Heat Index = ");
	hIndex = 0.5 * (fTemp + 61.0 + ((fTemp - 68.0) * 1.2) + (rH * 0.094));

}
void publishStatistic(char* statistic, float data) {
	char stringTemp[100];
	gcvt(data, 6, stringTemp);
	char topic[200] = "";
	strcat(topic, "iot/weather/");
	strcat(topic, statistic);
	strcat(topic, "/");
	strcat(topic, clientId);
	client.publish(topic, stringTemp);
}
void ConnectToWiFi()
{
	Serial.println("Booting");
	WiFi.mode(WIFI_STA);
	Serial.println("Mode set");
	WiFi.begin(ssid, password);
	Serial.println("Begin complete");
}
void HandleOTA()
{
	StartOTAIfRequired();
	ArduinoOTA.handle();
}
void StartOTAIfRequired()
{
	if (ota_started)
		return;
	// Port defaults to 8266
	// ArduinoOTA.setPort(8266);
	// Hostname defaults to esp8266-[ChipID]
	//if (ArduinoOTA.getHostname() && ArduinoOTA.getHostname().length())

	// No authentication by default
	ArduinoOTA.setPassword((const char*)"123");
	ArduinoOTA.onStart([]() {
		Serial.println("OTA Start");
		});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nOTA End");
		});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
		});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
		});
	ArduinoOTA.begin();
	ota_started = true;
	delay(500);

}
void PrintWifiStatus()
{
	// print the SSID of the network you're attached to:
	Serial.print("SSID: ");
	Serial.println(WiFi.SSID());
	//using dhcp? wait for ip or ip not set!
	if (WiFi.localIP()[0] == 0)
	{
		Serial.println("DHCP: Waiting for IP Address ...");
		while (WiFi.localIP()[0] == 0)
		{
			yield();
		}
	}
	// print your WiFi shield's IP address:
	IPAddress ip = WiFi.localIP();
	Serial.print("IP Address: ");
	Serial.println(ip);
	Serial.print("Hostname: ");
	Serial.println(WiFi.hostname());
	//Serial.println(WiFi.status());
}
void mqttReconnect() {
	// Loop until we're reconnected
	Serial.print("Attempting MQTT connection...");
	// Create a random client ID
	char prefix[] = "ESP8266_";
	char x[20] = "";
	strcat(x, WiFi.hostname().c_str());
	//ltoa(random(0, 99999), x, 16);
	memset(clientId, 0, sizeof clientId);
	strcat(clientId, prefix);
	strcat(clientId, x);
	strcat(clientId, "|BME280");
	// Attempt to connect
	if (client.connect(clientId)) {
		Serial.println("connected");
		// Once connected, publish an announcement...
		char idChar[] = "hello";
		client.publish("iot_discovery", clientId);
		// ... and resubscribe
		client.subscribe("BME280_CMD");
	}
	else {
		Serial.print("failed, rc=");
		Serial.print(client.state());
		Serial.println(" try again in 5 seconds");
		// Wait 5 seconds before retrying
		delay(5000);
	}
}
void callback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	for (int i = 0; i < length; i++) {
		Serial.print((char)payload[i]);
	}
	Serial.println();
	char value[50];
	for (int i = 0; i < length; i++) {
		const char temp = (char)payload[i];
		strcat(value, &temp);
	}
	messageHandler(topic, value, length);

	// Switch on the LED if an 1 was received as first character
	if ((char)payload[0] == '1') {
		Serial.println("Received message starting with 1!");
	}
}
void messageHandler(char* topic, char msg[], unsigned int length) {
	char body[48];
	for (int i = 2; i < length; i++) {
		body[i - 2] = msg[i];
	}
	if (msg[0] == 'P') {
		Serial.println("Received parameter message");
		switch (msg[1]) {
		case 'S': // MQTT Server
			EEPROM.put(50, body);
			break;
		case 'I': // SSID
			EEPROM.put(100, body);
			break;
		case 'P': // WIFI password
			EEPROM.put(150, body);
			break;
		case 'H': // Hostname
			EEPROM.put(200, body);
			break;
		}
		EEPROM.commit();
	}
}