
#define BLYNK_TEMPLATE_ID "TMPL6kL5qFLtK"
#define BLYNK_TEMPLATE_NAME "Smart Home IoT"
#define BLYNK_AUTH_TOKEN "Ispvw315i3R-vgoKvtfSR7LGBeNP60-r"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ===============================
// KONFIGURASI WIFI
// ===============================
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// ===============================
// KONFIGURASI PIN
// ===============================
#define DHT_PIN       15
#define DHT_TYPE      DHT22

#define PIR_PIN       13
#define LDR_PIN       34

#define LAMP_PIN      2
#define FAN_PIN       4
#define MOTION_LED    18

#define BUZZER_PIN    5
#define SERVO_PIN     19

// ===============================
// OBJEK SENSOR DAN SERVO
// ===============================
DHT dht(DHT_PIN, DHT_TYPE);
Servo doorServo;
BlynkTimer timer;

// ===============================
// PENGATURAN OTOMATIS
// ===============================
const float TEMP_LIMIT = 30.0;
const int DARK_LIMIT = 1800;

// Frekuensi buzzer pasif
const int BUZZER_FREQUENCY = 2000;

// ===============================
// VARIABEL BLYNK
// ===============================
bool lampManual = false;
bool lampAuto = false;

bool fanManual = false;
bool fanAuto = false;

bool securityMode = false;
bool doorOpen = false;

// ===============================
// STATUS PERANGKAT
// ===============================
bool lampStatus = false;
bool fanStatus = false;
bool alarmStatus = false;

int motionStatus = LOW;
int lightLevel = 0;

float temperature = 0;
float humidity = 0;

// Menyimpan status alarm sebelumnya
bool previousAlarmStatus = false;

// ===============================
// BLYNK: KONTROL LAMPU MANUAL
// V0
// ===============================
BLYNK_WRITE(V0) {
  lampManual = param.asInt();
}

// ===============================
// BLYNK: MODE LAMPU OTOMATIS
// V1
// ===============================
BLYNK_WRITE(V1) {
  lampAuto = param.asInt();
}

// ===============================
// BLYNK: KONTROL KIPAS MANUAL
// V2
// ===============================
BLYNK_WRITE(V2) {
  fanManual = param.asInt();
}

// ===============================
// BLYNK: MODE KIPAS OTOMATIS
// V3
// ===============================
BLYNK_WRITE(V3) {
  fanAuto = param.asInt();
}

// ===============================
// BLYNK: SECURITY MODE
// V4
// ===============================
BLYNK_WRITE(V4) {
  securityMode = param.asInt();
}

// ===============================
// BLYNK: KONTROL PINTU
// V5
// ===============================
BLYNK_WRITE(V5) {
  doorOpen = param.asInt();
}

// ===============================
// SINKRONISASI DATA BLYNK
// ===============================
BLYNK_CONNECTED() {
  Serial.println("Blynk berhasil terhubung!");

  Blynk.syncVirtual(
    V0, V1, V2, V3, V4, V5
  );
}

// ===============================
// MEMBACA SENSOR
// ===============================
void readSensors() {

  // Membaca suhu dan kelembapan
  float newTemperature = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  // Memperbarui data jika pembacaan valid
  if (!isnan(newTemperature)) {
    temperature = newTemperature;
  }

  if (!isnan(newHumidity)) {
    humidity = newHumidity;
  }

  // Membaca LDR
  lightLevel = analogRead(LDR_PIN);

  // Membaca PIR
  motionStatus = digitalRead(PIR_PIN);

  Serial.println("========== DATA SENSOR ==========");

  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Kelembapan: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Cahaya: ");
  Serial.println(lightLevel);

  Serial.print("Gerakan: ");
  Serial.println(motionStatus == HIGH ? "TERDETEKSI" : "TIDAK ADA");

  Serial.println("=================================");
}

// ===============================
// MENGONTROL PERANGKAT
// ===============================
void controlDevices() {

  // Membaca PIR setiap siklus kontrol
  motionStatus = digitalRead(PIR_PIN);

  // -------------------------------
  // LAMPU OTOMATIS / MANUAL
  // -------------------------------
  if (lampAuto) {
    lampStatus = (lightLevel < DARK_LIMIT);
  } else {
    lampStatus = lampManual;
  }

  digitalWrite(
    LAMP_PIN,
    lampStatus ? HIGH : LOW
  );

  // -------------------------------
  // KIPAS OTOMATIS / MANUAL
  // -------------------------------
  if (fanAuto) {
    fanStatus = (temperature >= TEMP_LIMIT);
  } else {
    fanStatus = fanManual;
  }

  digitalWrite(
    FAN_PIN,
    fanStatus ? HIGH : LOW
  );

  // -------------------------------
  // INDIKATOR GERAKAN
  // -------------------------------
  digitalWrite(
    MOTION_LED,
    motionStatus == HIGH ? HIGH : LOW
  );

  // -------------------------------
  // SISTEM KEAMANAN
  // -------------------------------
  alarmStatus = securityMode && (motionStatus == HIGH);

  // Buzzer hanya diperbarui saat status alarm berubah.
  // tone() akan menghasilkan bunyi terus sampai noTone().
  if (alarmStatus != previousAlarmStatus) {

    if (alarmStatus) {
      Serial.println("!!! ALARM AKTIF: GERAKAN TERDETEKSI !!!");

      tone(BUZZER_PIN, BUZZER_FREQUENCY);
    } else {
      Serial.println("Alarm dimatikan.");

      noTone(BUZZER_PIN);
      digitalWrite(BUZZER_PIN, LOW);
    }

    previousAlarmStatus = alarmStatus;
  }

  // -------------------------------
  // KONTROL SERVO PINTU
  // -------------------------------
  if (doorOpen) {
    doorServo.write(90);
  } else {
    doorServo.write(0);
  }
}

// ===============================
// MENGIRIM DATA KE BLYNK
// ===============================
void sendToBlynk() {

  if (!Blynk.connected()) {
    return;
  }

  Blynk.virtualWrite(V6, temperature);
  Blynk.virtualWrite(V7, humidity);
  Blynk.virtualWrite(V8, motionStatus);
  Blynk.virtualWrite(V9, lightLevel);

  Blynk.virtualWrite(V10, lampStatus);
  Blynk.virtualWrite(V11, fanStatus);
  Blynk.virtualWrite(V12, alarmStatus);
}

// ===============================
// SETUP
// ===============================
void setup() {

  Serial.begin(115200);

  // Konfigurasi pin
  pinMode(PIR_PIN, INPUT);

  pinMode(LAMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(MOTION_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Matikan semua output saat awal
  digitalWrite(LAMP_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(MOTION_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // ADC ESP32
  analogReadResolution(12);

  // Inisialisasi DHT22
  dht.begin();

  // Inisialisasi servo
  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);
  doorServo.write(0);

  // WiFi dan Blynk
  Serial.println("Menghubungkan ke Blynk...");

  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );

  // Jadwal pembacaan sensor setiap 2 detik
  timer.setInterval(2000L, readSensors);

  // Jadwal kontrol perangkat setiap 500 ms
  timer.setInterval(500L, controlDevices);

  // Mengirim data ke Blynk setiap 2 detik
  timer.setInterval(2000L, sendToBlynk);

  Serial.println("Smart Home IoT siap!");
}

// ===============================
// LOOP
// ===============================
void loop() {

  Blynk.run();
  timer.run();
}