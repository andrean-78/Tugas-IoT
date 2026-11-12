#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// =====================================
// 1. KONFIGURASI PIN ESP32
// =====================================

const int ledPin = 16;
const int buttonPin = 4;
const int ldrPin = 34;
const int relayPin = 19;
const int servoPin = 18;
const int trigPin = 5;
const int echoPin = 17;

// =====================================
// 2. KONFIGURASI LED PWM
// =====================================

int brightnessLevel = 0;

int brightnessValues[] = {
  0, 77, 179, 255
};

int lastButtonReading = HIGH;
int stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// =====================================
// 3. KONFIGURASI LDR
// =====================================

int adcValue = 0;
float darkness = 0;

unsigned long lastLDRRead = 0;
const unsigned long ldrInterval = 1000;

// =====================================
// 4. KONFIGURASI RELAY
// =====================================

// Sesuaikan dengan jenis relay.
// Kebanyakan modul relay Active LOW.
const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH;

bool relayState = false;
bool manualRelay = false;

unsigned long lastRelayChange = 0;

// =====================================
// 5. KONFIGURASI SERVO DAN HC-SR04
// =====================================

Servo palang;

long duration;
float distance;

unsigned long lastUltrasonicRead = 0;
const unsigned long ultrasonicInterval = 200;

// =====================================
// 6. KONFIGURASI MQTT DAN WIFI
// =====================================

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqttServer = "broker.emqx.io";
const int mqttPort = 1883;

// GANTI NIM_KAMU DENGAN NIM ASLI
const char* topicCahaya =
  "unisda/informatika/24063008/cahaya";

const char* topicLampu =
  "unisda/informatika/24063008/lampu";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublish = 0;
const unsigned long publishInterval = 5000;

// =====================================
// 7. FUNGSI MENGATUR RELAY
// =====================================

void setRelay(bool status) {

  relayState = status;

  if (status) {
    digitalWrite(relayPin, RELAY_ON);
    Serial.println("RELAY ON - Lampu Menyala");
  } else {
    digitalWrite(relayPin, RELAY_OFF);
    Serial.println("RELAY OFF - Lampu Padam");
  }
}

// =====================================
// 8. FUNGSI LED DAN PUSH BUTTON
// =====================================

void controlLED() {

  int reading = digitalRead(buttonPin);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > debounceDelay) {

    if (reading != stableButtonState) {

      stableButtonState = reading;

      if (stableButtonState == LOW) {

        brightnessLevel =
          (brightnessLevel + 1) % 4;

        ledcWrite(
          ledPin,
          brightnessValues[brightnessLevel]
        );

        Serial.print("Kecerahan LED: ");
        Serial.print(
          brightnessValues[brightnessLevel] * 100 / 255
        );
        Serial.println("%");
      }
    }
  }

  lastButtonReading = reading;
}

// =====================================
// 9. FUNGSI MEMBACA LDR
// =====================================

void readLDR() {

  if (millis() - lastLDRRead >= ldrInterval) {

    lastLDRRead = millis();

    adcValue = analogRead(ldrPin);

    darkness =
      100.0 - (adcValue / 4095.0 * 100.0);

    Serial.print("ADC LDR: ");
    Serial.print(adcValue);

    Serial.print(" | Kegelapan: ");
    Serial.print(darkness, 1);
    Serial.println("%");
  }
}

// =====================================
// 10. FUNGSI RELAY OTOMATIS
// =====================================

void controlRelay() {

  // Jika sudah menerima perintah MQTT,
  // mode manual memiliki prioritas.
  if (manualRelay) {
    return;
  }

  unsigned long currentTime = millis();

  if (relayState) {

    // ON selama 5 detik
    if (currentTime - lastRelayChange >= 5000) {

      setRelay(false);
      lastRelayChange = currentTime;
    }

  } else {

    // OFF selama 3 detik
    if (currentTime - lastRelayChange >= 3000) {

      setRelay(true);
      lastRelayChange = currentTime;
    }
  }
}

// =====================================
// 11. FUNGSI SENSOR ULTRASONIK DAN SERVO
// =====================================

void controlServo() {

  if (millis() - lastUltrasonicRead >=
      ultrasonicInterval) {

    lastUltrasonicRead = millis();

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);

    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH, 30000);

    if (duration == 0) {

      Serial.println("Objek tidak terdeteksi");

      palang.write(0);

    } else {

      distance = duration * 0.0343 / 2.0;

      Serial.print("Jarak Objek: ");
      Serial.print(distance);
      Serial.println(" cm");

      if (distance < 10) {

        palang.write(90);
        Serial.println("PALANG TERBUKA");

      } else {

        palang.write(0);
        Serial.println("PALANG TERTUTUP");
      }
    }
  }
}

// =====================================
// 12. FUNGSI WIFI
// =====================================

void connectWiFi() {

  Serial.print("Menghubungkan WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Terhubung!");
}

// =====================================
// 13. FUNGSI CALLBACK MQTT
// =====================================

void callback(
  char* topic,
  byte* payload,
  unsigned int length
) {

  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Pesan MQTT: ");
  Serial.println(message);

  if (String(topic) == topicLampu) {

    if (message == "ON") {

      manualRelay = true;
      setRelay(true);

      Serial.println("Kontrol MQTT: ON");

    } else if (message == "OFF") {

      manualRelay = true;
      setRelay(false);

      Serial.println("Kontrol MQTT: OFF");
    }
  }
}

// =====================================
// 14. FUNGSI RECONNECT MQTT
// =====================================

void reconnectMQTT() {

  while (!client.connected()) {

    Serial.print("Menghubungkan MQTT...");

    String clientId = "ESP32-UNISDA-";
    clientId += String(
      (uint32_t)ESP.getEfuseMac(), HEX
    );

    if (client.connect(clientId.c_str())) {

      Serial.println("MQTT Terhubung!");

      client.subscribe(topicLampu);

      Serial.println("Subscribe berhasil!");

    } else {

      Serial.print("Gagal, kode: ");
      Serial.println(client.state());

      delay(2000);
    }
  }
}

// =====================================
// 15. FUNGSI PUBLISH DATA LDR
// =====================================

void publishLDR() {

  if (millis() - lastPublish >= publishInterval) {

    lastPublish = millis();

    char data[20];

    snprintf(
      data,
      sizeof(data),
      "%.1f",
      darkness
    );

    if (client.connected()) {

      client.publish(topicCahaya, data);

      Serial.print("MQTT Publish: ");
      Serial.print(data);
      Serial.println("%");
    }
  }
}

// =====================================
// 16. SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  // Inisialisasi LED
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  ledcAttach(ledPin, 5000, 8);
  ledcWrite(ledPin, 0);

  // Inisialisasi LDR
  pinMode(ldrPin, INPUT);
  analogReadResolution(12);

  // Inisialisasi relay
  pinMode(relayPin, OUTPUT);

  setRelay(true);
  lastRelayChange = millis();

  // Inisialisasi ultrasonik
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Inisialisasi servo
  palang.setPeriodHertz(50);
  palang.attach(servoPin, 500, 2400);
  palang.write(0);

  // Koneksi WiFi
  connectWiFi();

  // Konfigurasi MQTT
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  Serial.println("==========================");
  Serial.println("SISTEM UTS PRAKTIKUM IoT");
  Serial.println("SELURUH MODUL AKTIF");
  Serial.println("==========================");
}

// =====================================
// 17. LOOP UTAMA
// =====================================

void loop() {

  // Soal 1: LED PWM dan tombol
  controlLED();

  // Soal 2: Pembacaan sensor LDR
  readLDR();

  // Soal 3: Relay otomatis
  controlRelay();

  // Soal 4: Servo dan ultrasonik
  controlServo();

  // Soal 5: Koneksi dan komunikasi MQTT
  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  // Mengirim data LDR setiap 5 detik
  publishLDR();
}
