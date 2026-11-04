
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ArduinoJson.h>

// ==========================================
// 1. DEFINISI PIN SENSOR
// ==========================================
const int PIN_DHT = 15;
const int PIN_TRIG = 5;
const int PIN_ECHO = 18;
const int PIN_GAS_ANALOG = 34;
const int PIN_LED_STATUS = 2;

// ==========================================
// 2. INISIALISASI SENSOR
// ==========================================
DHTesp dhtSensor;

// ==========================================
// 3. KONFIGURASI WIFI DAN MQTT
// ==========================================
const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// Tetap menggunakan topik MQTT milikmu
const char* mqtt_topic = "unisda/iot/pertemuan7/andrean";

WiFiClient espClient;
PubSubClient client(espClient);

// ==========================================
// 4. INTERVAL GATEWAY
// ==========================================
const unsigned long SAMPLING_INTERVAL = 1000;
const unsigned long GATEWAY_PUSH_INTERVAL = 5000;
const unsigned long MQTT_RETRY_INTERVAL = 5000;
const unsigned long LED_INTERVAL = 500;

unsigned long lastSampleTime = 0;
unsigned long lastPushTime = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastLedTime = 0;

// ==========================================
// 5. AMBANG BATAS
// ==========================================
const float BATAS_JARAK_PENUH = 10.0;
const float BATAS_SUHU_KRITIS = 45.0;
const float BATAS_GAS_KRITIS = 75.0;

// ==========================================
// 6. BUFFER AGREGASI DATA
// ==========================================
float sumTemperature = 0;
float sumHumidity = 0;
float sumDistance = 0;
float sumGasLevel = 0;

int sampleCount = 0;

// ==========================================
// 7. STATUS SISTEM
// ==========================================
String activeStatus = "NORMAL";
String lastPublishedStatus = "";

bool pendingStatusPublish = false;

float latestTemperature = NAN;
float latestHumidity = NAN;
float latestDistance = -1;
float latestGas = 0;

// ==========================================
// 8. PEMBACAAN SENSOR ULTRASONIK
// ==========================================
float readUltrasonicDistanceCM() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000);

  if (duration == 0) {
    return -1.0;
  }

  return duration * 0.034 / 2.0;
}

// ==========================================
// 9. PEMBACAAN SENSOR GAS SIMULASI
// ==========================================
float readSimulatedGasLevel() {
  int rawADC = analogRead(PIN_GAS_ANALOG);

  return (rawADC / 4095.0) * 100.0;
}

// ==========================================
// 10. MENENTUKAN STATUS SISTEM
// ==========================================
String determineStatus(float distance, float temperature, float gas) {

  // Prioritas pertama: tangki penuh
  if (distance >= 0 && distance < BATAS_JARAK_PENUH) {
    return "TANK_OVERFLOW";
  }

  // Prioritas kedua: kondisi kritis
  if ((!isnan(temperature) &&
       temperature > BATAS_SUHU_KRITIS) ||
      gas > BATAS_GAS_KRITIS) {

    return "ALERT_CRITICAL";
  }

  return "NORMAL";
}

// ==========================================
// 11. FUNGSI PUBLISH JSON MQTT
// ==========================================
bool publishTelemetry(
  String status,
  float temperature,
  float humidity,
  float distance,
  float gas,
  int samples,
  bool isAverage,
  String eventType
) {

  if (!client.connected()) {
    return false;
  }

  StaticJsonDocument<384> jsonDoc;

  jsonDoc["gateway_id"] = "GW-INFORMATIKA-07";
  jsonDoc["uptime_sec"] = millis() / 1000;
  jsonDoc["status"] = status;
  jsonDoc["samples_aggregated"] = samples;
  jsonDoc["event_type"] = eventType;

  // Data agregasi dan data perubahan status
  // dibedakan agar tidak salah menafsirkan nilainya.
  JsonObject telemetry;

  if (isAverage) {
    telemetry = jsonDoc.createNestedObject("telemetry_avg");
  } else {
    telemetry = jsonDoc.createNestedObject("telemetry_current");
  }

  if (!isnan(temperature)) {
    telemetry["temperature_c"] = temperature;
  }

  if (!isnan(humidity)) {
    telemetry["humidity_pct"] = humidity;
  }

  if (distance >= 0) {
    telemetry["tank_distance_cm"] = distance;
  }

  telemetry["gas_level_pct"] = gas;

  String jsonOutput;
  serializeJson(jsonDoc, jsonOutput);

  Serial.println();
  Serial.println("======================================");
  Serial.println("        GATEWAY MQTT UPLINK");
  Serial.println("======================================");
  Serial.println(jsonOutput);

  bool success = client.publish(
    mqtt_topic,
    jsonOutput.c_str()
  );

  if (success) {
    Serial.println("[MQTT] PUBLISH BERHASIL");
    Serial.print("Topik: ");
    Serial.println(mqtt_topic);
  } else {
    Serial.println("[MQTT] PUBLISH GAGAL");
  }

  return success;
}

// ==========================================
// 12. KONEKSI ULANG MQTT NON-BLOCKING
// ==========================================
void reconnectMQTT() {

  if (client.connected()) {
    return;
  }

  unsigned long now = millis();

  if (now - lastMqttAttempt < MQTT_RETRY_INTERVAL) {
    return;
  }

  lastMqttAttempt = now;

  Serial.print("Menghubungkan ke MQTT EMQX... ");

  String clientId =
    "ESP32Gateway-" + String(random(0xffff), HEX);

  if (client.connect(clientId.c_str())) {
    Serial.println("TERHUBUNG!");
  } else {
    Serial.print("Gagal, rc=");
    Serial.println(client.state());
  }
}

// ==========================================
// 13. SETUP
// ==========================================
void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("======================================");
  Serial.println("   IOT GATEWAY EDGE PROCESSING");
  Serial.println("======================================");

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED_STATUS, OUTPUT);

  analogReadResolution(12);

  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);

  // Koneksi WiFi
  Serial.print("Menghubungkan WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[WIFI] TERHUBUNG!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Konfigurasi MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);
  client.setKeepAlive(60);

  Serial.println("[GATEWAY] Siap melakukan Edge Processing");
}

// ==========================================
// 14. LOOP UTAMA
// ==========================================
void loop() {

  // Menjaga koneksi MQTT
  reconnectMQTT();
  client.loop();

  unsigned long currentMillis = millis();

  // ========================================
  // HEARTBEAT LED NON-BLOCKING
  // ========================================
  if (currentMillis - lastLedTime >= LED_INTERVAL) {

    lastLedTime = currentMillis;

    digitalWrite(
      PIN_LED_STATUS,
      !digitalRead(PIN_LED_STATUS)
    );
  }

  // ========================================
  // TAHAP 1: DATA COLLECTION
  // Sampling setiap 1 detik
  // ========================================
  if (currentMillis - lastSampleTime >= SAMPLING_INTERVAL) {

    lastSampleTime = currentMillis;

    TempAndHumidity dhtData =
      dhtSensor.getTempAndHumidity();

    float currentDistance =
      readUltrasonicDistanceCM();

    float currentGas =
      readSimulatedGasLevel();

    latestTemperature = dhtData.temperature;
    latestHumidity = dhtData.humidity;
    latestDistance = currentDistance;
    latestGas = currentGas;

    bool validDHT =
      !isnan(dhtData.temperature) &&
      !isnan(dhtData.humidity);

    bool validDistance = currentDistance >= 0;

    Serial.println();
    Serial.println("[EDGE SAMPLING]");

    if (validDHT) {
      Serial.printf(
        "Suhu: %.1f C | Kelembapan: %.1f %%\n",
        dhtData.temperature,
        dhtData.humidity
      );
    } else {
      Serial.println("Data DHT22 tidak valid");
    }

    if (validDistance) {
      Serial.printf(
        "Jarak: %.1f cm\n",
        currentDistance
      );
    } else {
      Serial.println("Data HC-SR04 tidak valid");
    }

    Serial.printf("Gas: %.1f %%\n", currentGas);

    // ======================================
    // TAHAP 2: AGREGASI DATA LOKAL
    // ======================================
    if (validDHT && validDistance) {

      sumTemperature += dhtData.temperature;
      sumHumidity += dhtData.humidity;
      sumDistance += currentDistance;
      sumGasLevel += currentGas;

      sampleCount++;
    }

    // ======================================
    // TUGAS MODUL BAGIAN 8 NOMOR 2
    // DETEKSI TANK OVERFLOW LANGSUNG
    // ======================================

    String newStatus = determineStatus(
      currentDistance,
      dhtData.temperature,
      currentGas
    );

    if (newStatus != activeStatus) {

      activeStatus = newStatus;
      pendingStatusPublish = true;

      Serial.println();
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Serial.println("       STATUS BERUBAH!");
      Serial.print("STATUS: ");
      Serial.println(activeStatus);
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }

    // Kirim status pertama atau status yang berubah
    if (lastPublishedStatus == "") {
      pendingStatusPublish = true;
    }

    if (pendingStatusPublish && client.connected()) {

      bool sent = publishTelemetry(
        activeStatus,
        latestTemperature,
        latestHumidity,
        latestDistance,
        latestGas,
        1,
        false,
        "STATUS_CHANGE"
      );

      if (sent) {
        lastPublishedStatus = activeStatus;
        pendingStatusPublish = false;
      }
    }
  }

  // ========================================
  // TAHAP 3: EDGE PROCESSING
  // AGREGASI DAN PUBLISH SETIAP 5 DETIK
  // ========================================
  if (currentMillis - lastPushTime >= GATEWAY_PUSH_INTERVAL) {

    lastPushTime = currentMillis;

    if (sampleCount > 0) {

      float avgTemp =
        sumTemperature / sampleCount;

      float avgHum =
        sumHumidity / sampleCount;

      float avgDist =
        sumDistance / sampleCount;

      float avgGas =
        sumGasLevel / sampleCount;

      Serial.println();
      Serial.println("======================================");
      Serial.println("       EDGE PROCESSING");
      Serial.println("======================================");

      Serial.printf("Rata-rata suhu: %.2f C\n", avgTemp);
      Serial.printf("Rata-rata kelembapan: %.2f %%\n", avgHum);
      Serial.printf("Rata-rata jarak: %.2f cm\n", avgDist);
      Serial.printf("Rata-rata gas: %.2f %%\n", avgGas);

      // Status aktif berasal dari pembacaan sensor
      // terakhir, sehingga overflow tidak tertunda.
      bool sent = publishTelemetry(
        activeStatus,
        avgTemp,
        avgHum,
        avgDist,
        avgGas,
        sampleCount,
        true,
        "AGGREGATED_TELEMETRY"
      );

      if (sent) {

        lastPublishedStatus = activeStatus;
        pendingStatusPublish = false;

        // Reset buffer setelah publish berhasil
        sumTemperature = 0;
        sumHumidity = 0;
        sumDistance = 0;
        sumGasLevel = 0;

        sampleCount = 0;

        Serial.println("[EDGE] Buffer agregasi direset");
      } else {
        Serial.println(
          "[EDGE] Data dipertahankan karena publish gagal"
        );
      }
    }
  }
}
