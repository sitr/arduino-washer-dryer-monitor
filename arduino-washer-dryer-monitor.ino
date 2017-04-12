#include <ESP8266WiFi.h>
#include <SimpleTimer.h>
#include <ArduinoJson.h>

const char* ssid     = "satin2";
const char* password = "Henrik123";
const char* homeSeerServer = "192.168.0.109";
const unsigned long HTTP_TIMEOUT = 10000;
const int STATUS_ON = 100;
const int STATUS_OFF = 0;
const size_t MAX_CONTENT_SIZE = 512;

int dryerPhotocellPin = 0;
int dryerStatusLedPin = D1;
int dryerStatus = STATUS_OFF;
int dryerDeviceId = 180;
int washerPhotocellPin = 1;
int washerStatusLedPin = D2;
int washerStatus = STATUS_OFF;
int washerDeviceId = 181;

SimpleTimer timer;
WiFiClient client;

struct DeviceData {
	char ref[4];
	char value[3];
};

void setup() {
	pinMode(dryerStatusLedPin, OUTPUT);
    pinMode(washerStatusLedPin, OUTPUT);
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	timer.setInterval(100, checkStatusChange);
}

int getStatusFromLightSensor(int sensorPin) {
	int photocellReading = analogRead(sensorPin);
	// Map analog readings to HS3 on off status value
	int status = STATUS_OFF;
	if(photocellReading > 500)
		status = STATUS_ON;
    return status;
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
	DynamicJsonBuffer jsonBuffer(0);
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
  Serial.println(homeSeerServer);

  bool ok = client.connect(homeSeerServer, 80);

  Serial.println(ok ? "Connected" : "Connection Failed!");
  return ok;
}

bool sendRequest(int deviceId, int statusValue) {
	Serial.print("connecting to ");
	Serial.println(homeSeerServer);

	String url = "/JSON?request=controldevicebyvalue&ref=";
	url += deviceId;
	url += "&value=";
	url += statusValue;

	Serial.print("Requesting URL: ");
	Serial.println(url);

	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
					"homeSeerServer: " + homeSeerServer + "\r\n" + 
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

void fastToggleLed(int ledPin) {
  static unsigned long fastLedTimer;
  if (millis() - fastLedTimer >= 100UL) {
    digitalWrite(ledPin, !digitalRead(ledPin));
    fastLedTimer = millis();
  }
}

void slowToggleLed (int ledPin) {
  static unsigned long slowLedTimer;
  if (millis() - slowLedTimer >= 1250UL) {
    digitalWrite(ledPin, !digitalRead(ledPin));
    slowLedTimer = millis();
  }
}

void updateHomeSeer(int deviceId, int deviceStatus) {
    if (connect()) {
        if (sendRequest(deviceId, deviceStatus) && skipResponseHeaders()) {
            DeviceData deviceData;
            if (readReponseContent(&deviceData)) {
                printUserData(&deviceData);
            }
        }
    }
    disconnect();
}

void checkDryerStatus() {
    int sensorStatus = getStatusFromLightSensor(dryerPhotocellPin);
    if(dryerStatus != sensorStatus) {
		dryerStatus = sensorStatus;
		updateHomeSeer(dryerDeviceId, dryerStatus);
	}
    if(dryerStatus == STATUS_OFF)
        slowToggleLed(dryerStatusLedPin);
    else
        fastToggleLed(dryerStatusLedPin);
}

void checkWasherStatus() {
    int sensorStatus = getStatusFromLightSensor(washerPhotocellPin);
    if(washerStatus != sensorStatus) {
		washerStatus = sensorStatus;
		updateHomeSeer(washerDeviceId, washerStatus);
	}
    if(washerStatus == STATUS_OFF)
        slowToggleLed(washerStatusLedPin);
    else
        fastToggleLed(washerStatusLedPin);
}

void checkStatusChange() {
	checkDryerStatus();
    checkWasherStatus();
}

void loop() {
	timer.run();
}