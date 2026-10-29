#include <DHT.h>
#include <ESP32Servo.h>

// Definisi Pin Sensor

#define TRIG_PIN 5
#define ECHO_PIN 18
#define DHT_PIN 4
#define DHTTYPE DHT22

// Definisi Pin Aktuator

#define SERVO_PIN 13
#define LED_HIJAU 2
#define LED_MERAH 15

// Inisialisasi Objek

DHT dht(DHT_PIN, DHTTYPE);
Servo myServo;

void setup() {
  Serial.begin(115200);
  Serial.println("--- Inisialisasi Sistem Sensor & Aktuator ---");

  // Konfigurasi Pin Sensor Ultrasonik

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Konfigurasi Pin Indikator

  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);

  // Inisialisasi Sensor DHT & Servo

  dht.begin();
  myServo.attach(SERVO_PIN, 500, 2400); // Pulse width standard SG90
  myServo.write(0);                    // Posisi awal aktuator: 0 derajat
  digitalWrite(LED_HIJAU, LOW);
  digitalWrite(LED_MERAH, LOW);

  delay(1000);

}

float readUltrasonicCM() {

  // Pancarkan sinyal ultrasonik 10us

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Hitung durasi pantulan (Time of Flight)

  long duration = pulseIn(ECHO_PIN, HIGH);

  // Jarak = (Durasi * 0.0343 cm/us) / 2

  float distance = duration * 0.0343 / 2.0;
  return distance;

}

void loop() {

  // 1. Pembacaan Sensor

  float distance = readUltrasonicCM();
  float suhu = dht.readTemperature();
  float kelembaban = dht.readHumidity();

  // Validasi pembacaan DHT

  if (isnan(suhu) || isnan(kelembaban)) {
    Serial.println("Gagal membaca dari sensor DHT22!");
    delay(1000);
    return;

  }

  // Tampilkan data ke Serial Monitor

  Serial.println("-------------------------------------");
  Serial.printf("Jarak Objek : %.1f cm\n", distance);
  Serial.printf("Suhu        : %.1f C\n", suhu);
  Serial.printf("Kelembaban  : %.1f %%\n", kelembaban);

  // 2. Logika Kendali Aktuator (Closed-Loop)
  // Skenario: Portal otomatis membuka jika objek mendekat (< 20 cm)
  // ATAU membuka darurat jika suhu ruangan terlalu panas (> 35 C)

  if (distance < 20.0 || suhu > 35.0) {
    Serial.println("[AKTUATOR AKTIF]: Palang TERBUKA (90 Deg) | Status: SIAGA");
    myServo.write(90);             // Gerakkan servo membuka 90 derajat
    digitalWrite(LED_MERAH, HIGH); // Nyalakan indikator siaga
    digitalWrite(LED_HIJAU, LOW);

  } else {
    Serial.println("[AKTUATOR IDLE]: Palang TERTUTUP (0 Deg) | Status: NORMAL");
    myServo.write(0);              // Kembalikan posisi servo ke 0 derajat
    digitalWrite(LED_MERAH, LOW);
    digitalWrite(LED_HIJAU, HIGH); // Nyalakan indikator aman

  }

  delay(1500); // Interval pembacaan

}

