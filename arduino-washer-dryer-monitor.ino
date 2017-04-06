#include <ESP8266WiFi.h>

const char* ssid     = "satin2";
const char* password = "Henrik123";
const char* host = "192.168.0.109";

int photocellPin = 0;
int photocellReading;
int previousDeviceValue = 0;
int deviceValue;

void setup() {
	Serial.begin(115200);
	delay(10);

	// We start by connecting to a WiFi network

	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	/* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
		would try to act as both a client and an access-point and could cause
		network-issues with your other WiFi-devices on your WiFi-network. */
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");  
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}

void readLightSensorLevel() {
	photocellReading = analogRead(photocellPin);
	Serial.print("Analog reading = ");
	Serial.println(photocellReading);
	// Map analog readings to HS3 high and low values
	deviceValue = 0;
	if(photocellReading > 500)
		deviceValue = 100;

	Serial.print("Mapped value = ");
	Serial.println(deviceValue);
}

void updateHomeSeer() {
		Serial.print("connecting to ");
	Serial.println(host);
  
	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(host, httpPort)) {
		Serial.println("connection failed");
		return;
	}

	// We now create a URI for the request
	String url = "/JSON?request=controldevicebyvalue&ref=180&value=";
	url += deviceValue;

	Serial.print("Requesting URL: ");
	Serial.println(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
					"Host: " + host + "\r\n" + 
					"Connection: close\r\n\r\n");
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 5000) {
			Serial.println(">>> Client Timeout !");
			client.stop();
			return;
		}
	}
  
  // Read all the lines of the reply from server and print them to Serial
	while(client.available()){
		String line = client.readStringUntil('\r');
		Serial.print(line);
	}

	Serial.println();
	Serial.println("closing connection");
}

void loop() {
	delay(5000);
	readLightSensorLevel();
	
	if(previousDeviceValue != deviceValue) {
		previousDeviceValue = deviceValue;
		updateHomeSeer();
	}
}