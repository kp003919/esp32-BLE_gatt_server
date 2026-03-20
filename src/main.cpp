#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "DHT.h"

// ====== UUIDs ======
#define SERVICE_UUID          "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_NOTIFY      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_UUID_RW          "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"

// ====== DHT ======
#define DHTPIN 3
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ====== BLE State ======
BLECharacteristic* notifyChar = nullptr;
BLECharacteristic* rwChar = nullptr;

bool deviceConnected = false;
unsigned long lastSend = 0;
unsigned long lastDHT = 0;

float lastTemp = NAN;
float lastHum = NAN;

// ====== Server Callbacks ======
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    deviceConnected = true;
    Serial.println("Client connected");
  }
  void onDisconnect(BLEServer* s) override {
    deviceConnected = false;
    Serial.println("Client disconnected");
    BLEDevice::startAdvertising();
  }
};

// ====== RW Characteristic Callback ======
class RWCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    std::string v = c->getValue();
    if (!v.empty()) {
      Serial.printf("Client wrote: %s\n", v.c_str());
    }
  }
};

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  dht.begin();

  BLEDevice::init("ESP32-GATT-Server");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(SERVICE_UUID);

  // Notify characteristic
  notifyChar = service->createCharacteristic(
    CHAR_UUID_NOTIFY,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  notifyChar->addDescriptor(new BLE2902());

  // Read/Write characteristic
  rwChar = service->createCharacteristic(
    CHAR_UUID_RW,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  rwChar->setCallbacks(new RWCallbacks());
  rwChar->addDescriptor(new BLE2902());
  rwChar->setValue("RW characteristic ready");

  service->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("BLE server ready");
}

// ====== Loop ======
void loop() {
  unsigned long now = millis();

  // Read DHT every 2 seconds
  if (now - lastDHT > 2000) {
    lastDHT = now;
    lastTemp = dht.readTemperature();
    lastHum  = dht.readHumidity();
  }

  // Send notify every 4 seconds
  if (deviceConnected && now - lastSend > 4000) {
    lastSend = now;

    if (!isnan(lastTemp) && !isnan(lastHum)) {
      String json = "{\"temp\":" + String(lastTemp) +
                    ",\"hum\":" + String(lastHum) + "}";

      notifyChar->setValue(json.c_str());
      notifyChar->notify();

      Serial.printf("Notify: %s\n", json.c_str());
    }
  }
}
