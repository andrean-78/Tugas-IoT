#include <WiFi.h>
#include "DHTesp.h"

// Definisi Pin Periferal
const int DHT_PIN = 15;
const int LED_NORMAL = 2;
const int LED_ALARM = 4;
const int BUZZER_PIN = 5;

// Batas Kritis (Threshold) Baru
const float AMBANG_SUHU_KRITIS = 38.0;        // Derajat Celcius
const float AMBANG_KELEMBABAN_KRITIS = 20.0; // Persen (%)

// Konfigurasi Virtual WiFi Wokwi (Opsional/Non-blocking)
const char* ssid = "Wokwi-GUEST";
const char* password = "";

DHTesp dhtSensor;

// Variabel Penjadwalan Waktu (Non-blocking Timer)
unsigned long waktuSebelumnya = 0;
const unsigned long intervalBaca = 2000; // Baca sensor setiap 2 detik

void setup() {
  Serial.begin(115200);
  Serial.println("--- Inisialisasi Node IoT ESP32 (Stand-alone Mode) ---");
  
  pinMode(LED_NORMAL, OUTPUT);
  pinMode(LED_ALARM, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Inisialisasi Sensor DHT22
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  
  // Inisialisasi Wi-Fi Non-blocking dengan Timeout
  Serial.print("Memeriksa koneksi Wi-Fi: ");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(200);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Wi-Fi Connected!] IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[Mode Offline / Stand-alone] Melanjutkan simulasi tanpa gateway internet.");
  }
}

void loop() {
  unsigned long waktuSekarang = millis();
  
  // Jalankan pembacaan sensor setiap 2 detik (Non-blocking)
  if (waktuSekarang - waktuSebelumnya >= intervalBaca) {
    waktuSebelumnya = waktuSekarang;
    
    // Ambil data dari sensor DHT22 
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    
    // Validasi pembacaan sensor
    if (isnan(data.temperature) || isnan(data.humidity)) {
      Serial.println("[Error] Gagal membaca data dari sensor DHT22!");
      return;
    }
    
    // Baca nilai RSSI 
    int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -65; // Mock RSSI jika offline
   
    // Tampilkan data pembacaan sensor beserta nilai RSSI 
    Serial.printf("[Sensor Data] Suhu: %.2f °C | Kelembaban: %.2f %% | RSSI: %d dBm\n",
                  data.temperature, data.humidity, rssi);
                  
    // Evaluasi Status Pemicu (Suhu > 38°C ATAU Kelembaban < 20%)
    bool isSuhuKritis = (data.temperature > AMBANG_SUHU_KRITIS);
    bool isKelembabanKritis = (data.humidity < AMBANG_KELEMBABAN_KRITIS);

    if (isSuhuKritis || isKelembabanKritis) {
      digitalWrite(LED_ALARM, HIGH);
      digitalWrite(LED_NORMAL, LOW);
      tone(BUZZER_PIN, 1000); // Bunyikan alarm 1kHz
      
      // Indikasi teks pembeda pada Serial Monitor
      if (isSuhuKritis && isKelembabanKritis) {
        Serial.println(">>> PERINGATAN GANDA: Suhu > 38°C DAN Kelembaban < 20% ! <<<");
      } else if (isSuhuKritis) {
        Serial.println(">>> PERINGATAN: Suhu Melebihi Batas Kritis (> 38°C)! <<<");
      } else if (isKelembabanKritis) {
        Serial.println(">>> PERINGATAN: Kelembaban Udara Terlalu Rendah (< 20%)! <<<");
      }
      
    } else {
      digitalWrite(LED_ALARM, LOW);
      digitalWrite(LED_NORMAL, HIGH);
      noTone(BUZZER_PIN);     // Matikan buzzer
      Serial.println("Status: Lingkungan Normal & Terkendali.");
    }
  }
}
