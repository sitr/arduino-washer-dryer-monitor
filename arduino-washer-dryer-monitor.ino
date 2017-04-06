#include <ESP8266WiFi.h>
#include <SimpleTimer.h>
#include <ArduinoJson.h>

const char* ssid     = "satin2";
const char* password = "Henrik123";
const char* host = "192.168.0.109";
const unsigned long HTTP_TIMEOUT = 10000;
const size_t MAX_CONTENT_SIZE = 512;

int hsDeviceRefId = 180;
int photocellPin = 0;
int statusLedPin = D1;
int photocellReading;
int previousDeviceValue = 0;
int deviceValue;
SimpleTimer timer;
WiFiClient client;

struct DeviceData {
	char ref[4];
	char value[3];
};

void setup() {
	pinMode(statusLedPin, OUTPUT);
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	timer.setInterval(5000, checkStatusChange);
}

void readLightSensorLevel() {
	photocellReading = analogRead(photocellPin);
	// Map analog readings to HS3 high and low values
	deviceValue = 0;
	if(photocellReading > 500)
		deviceValue = 100;
}

// Skip HTTP headers so that we are at the beginning of the response's body
bool skipResponseHeaders() {
	// HTTP headers end with an empty line
	char endOfHeaders[] = "\r\n\r\n";
	client.setTimeout(HTTP_TIMEOUT);
	bool ok = client.find(endOfHeaders);

	if (!ok) {
		Serial.println("No response or invalid response!");
	}
	return ok;
}

bool readReponseContent(struct DeviceData* deviceData) {
	// Compute optimal size of the JSON buffer according to what we need to parse.
	// This is only required if you use StaticJsonBuffer.
	const size_t BUFFER_SIZE =
		JSON_OBJECT_SIZE(3)    // the root object has 3 elements
		+ JSON_OBJECT_SIZE(16)  // the "device" object has 16 elements
		+ JSON_OBJECT_SIZE(6)  // the "device_type" object has 6 elements
		+ MAX_CONTENT_SIZE;    // additional space for strings

	DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);
	JsonObject& root = jsonBuffer.parseObject(client);
	if (!root.success()) {
		Serial.println("JSON parsing failed!");
		return false;
	}
	strcpy(deviceData->ref, root["Devices"][0]["ref"]);
	strcpy(deviceData->value, root["Devices"][0]["value"]);
	return true;
}

bool connect() {
  Serial.print("Connect to ");
  Serial.println(host);

  bool ok = client.connect(host, 80);

  Serial.println(ok ? "Connected" : "Connection Failed!");
  return ok;
}

bool sendRequest() {
	Serial.print("connecting to ");
	Serial.println(host);

	String url = "/JSON?request=controldevicebyvalue&ref=";
	url += hsDeviceRefId;
	url += "&value=";
	url += deviceValue;

	Serial.print("Requesting URL: ");
	Serial.println(url);

	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
					"Host: " + host + "\r\n" + 
					"Connection: close\r\n\r\n");
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 5000) {
			Serial.println(">>> Client Timeout !");
			client.stop();
			return false;
		}
	}
	return true;
}

void disconnect() {
	Serial.println("Disconnect");
	client.stop();
}

void printUserData(const struct DeviceData* deviceData) {
  Serial.print("Ref = ");
  Serial.println(deviceData->ref);
  Serial.print("Value = ");
  Serial.println(deviceData->value);
}

void checkStatusChange() {
	readLightSensorLevel();
	if(previousDeviceValue != deviceValue) {
		previousDeviceValue = deviceValue;
		if (connect()) {
			if (sendRequest() && skipResponseHeaders()) {
				DeviceData deviceData;
				if (readReponseContent(&deviceData)) {
					printUserData(&deviceData);
				}
			}
		}
		disconnect();
	}
}
void loop() {
	timer.run();
}