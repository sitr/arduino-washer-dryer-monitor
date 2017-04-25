#include <ESP8266WiFi.h>
#include <SimpleTimer.h>
#include <ArduinoJson.h>

const char* ssid     = "satin2";
const char* password = "Henrik123";
const char* homeSeerServer = "192.168.0.109";
const unsigned long HTTP_TIMEOUT = 10000;
const int STATUS_RUNNING = 100;
const int STATUS_FINISHED = 0;

int photocellReaderPin = 0;
int dryerPhotocellPin = D6;
int dryerStatusLedPin = D1;
int dryerStatus = STATUS_FINISHED;
int dryerDeviceId = 180;
int washerPhotocellPin = D7;
int washerStatusLedPin = D2;
int washerStatus = STATUS_FINISHED;
int washerDeviceId = 181;

SimpleTimer timer;
WiFiClient client;

struct DeviceData {
	char ref[4];
	char value[3];
};

void setup() {
    pinMode(washerPhotocellPin, OUTPUT);
    pinMode(dryerPhotocellPin, OUTPUT);
	pinMode(dryerStatusLedPin, OUTPUT);
    pinMode(washerStatusLedPin, OUTPUT);
    digitalWrite(dryerPhotocellPin, LOW);
    digitalWrite(washerPhotocellPin, LOW);
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	timer.setInterval(100, checkStatusChange);
}

int getStatusFromLightSensor() {
	int photocellReading = analogRead(photocellReaderPin);
	// Map analog readings to HS3 on off status value
	int status = STATUS_FINISHED;
	if(photocellReading > 500)
        status = STATUS_RUNNING;
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
    digitalWrite(dryerPhotocellPin, HIGH);
    delay(100);
    int sensorStatus = getStatusFromLightSensor();
    digitalWrite(dryerPhotocellPin, LOW);
    if(dryerStatus != sensorStatus) {
		dryerStatus = sensorStatus;
		updateHomeSeer(dryerDeviceId, dryerStatus);
	}
    if(dryerStatus == STATUS_RUNNING)
        fastToggleLed(dryerStatusLedPin);
    else
        slowToggleLed(dryerStatusLedPin);
}

void checkWasherStatus() {
    digitalWrite(washerPhotocellPin, HIGH);
    delay(100);
    int sensorStatus = getStatusFromLightSensor();
    digitalWrite(washerPhotocellPin, LOW);
    if(washerStatus != sensorStatus) {
		washerStatus = sensorStatus;
		updateHomeSeer(washerDeviceId, washerStatus);
	}
    if(washerStatus == STATUS_RUNNING)
        fastToggleLed(washerStatusLedPin);
    else
        slowToggleLed(washerStatusLedPin);
}

void checkStatusChange() {
	checkDryerStatus();
    checkWasherStatus();
}

void loop() {
	timer.run();
}