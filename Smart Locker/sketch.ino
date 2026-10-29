
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ===== WIFI WOKWI =====
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ===== THINGSPEAK =====
const char* API_KEY = "7GUE67JV11XWHRI5";
const char* TS_URL = "http://api.thingspeak.com/update";

// ===== PIN ESP32 =====
const int SERVO_PIN = 18;
const int BUZZER_PIN = 19;
const int LED_PIN = 23;
const int DOOR_PIN = 4;

// ===== LCD DAN SERVO =====
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo lockServo;

// ===== KEYPAD 4x4 =====
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};

Keypad keypad = Keypad(
  makeKeymap(keys),
  rowPins,
  colPins,
  ROWS,
  COLS
);

// ===== PENGATURAN =====
const String PIN_BENAR = "1234";
const unsigned long DURASI_BUKA = 8000;
const unsigned long INTERVAL_KIRIM = 20000;

String inputPIN = "";

bool lokerTerbuka = false;
bool alarmAktif = false;

int percobaanSalah = 0;

unsigned long waktuBuka = 0;
unsigned long kirimTerakhir = 0;

// Status pintu: 0 tertutup, 1 terbuka
int statusPintu() {
  return digitalRead(DOOR_PIN) == HIGH ? 1 : 0;
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lockServo.attach(SERVO_PIN);
  lockServo.write(0);  // Posisi terkunci

  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  lcd.setCursor(0, 0);
  lcd.print("SMART LOCKER");
  lcd.setCursor(0, 1);
  lcd.print("Menghubungkan");

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int percobaanWiFi = 0;
  while (WiFi.status() != WL_CONNECTED &&
         percobaanWiFi < 30) {
    delay(500);
    percobaanWiFi++;
  }

  Serial.println();
  Serial.println(
    WiFi.status() == WL_CONNECTED
      ? "WiFi terhubung"
      : "WiFi belum terhubung"
  );

  tampilkanUtama();
}

void loop() {
  char tombol = keypad.getKey();

  if (tombol) {
    prosesTombol(tombol);
  }

  // Kunci otomatis setelah 8 detik
  if (lokerTerbuka &&
      millis() - waktuBuka >= DURASI_BUKA) {
    kunciLoker();
  }

  // Alarm jika pintu terbuka ketika loker terkunci
  if (!lokerTerbuka && statusPintu() == 1) {
    alarmAktif = true;
  }

  digitalWrite(LED_PIN, alarmAktif ? HIGH : LOW);

  // Kirim snapshot status ke dashboard
  if (millis() - kirimTerakhir >= INTERVAL_KIRIM) {
    kirimTerakhir = millis();
    kirimThingSpeak();
  }

  delay(10);
}

void prosesTombol(char tombol) {
  if (tombol == '*') {
    inputPIN = "";
    tampilkanUtama();
    return;
  }

  if (tombol == '#') {
    periksaPIN();
    return;
  }

  // Hanya menerima angka
  if (tombol >= '0' && tombol <= '9') {
    if (inputPIN.length() < 8) {
      inputPIN += tombol;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Masukkan PIN:");

    lcd.setCursor(0, 1);
    for (int i = 0; i < inputPIN.length(); i++) {
      lcd.print('*');
    }
  }
}

void periksaPIN() {
  if (inputPIN == PIN_BENAR) {
    inputPIN = "";
    percobaanSalah = 0;
    alarmAktif = false;
    bukaLoker();
  } else {
    inputPIN = "";
    percobaanSalah++;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PIN SALAH!");

    lcd.setCursor(0, 1);
    lcd.print("Gagal: ");
    lcd.print(percobaanSalah);

    tone(BUZZER_PIN, 1200, 250);

    if (percobaanSalah >= 3) {
      alarmAktif = true;
      digitalWrite(LED_PIN, HIGH);

      tone(BUZZER_PIN, 1800, 1000);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ALARM AKTIF!");
      lcd.setCursor(0, 1);
      lcd.print("3x PIN salah");
    }

    Serial.printf(
      "PIN salah. Jumlah gagal: %d\n",
      percobaanSalah
    );

    delay(800);
    tampilkanUtama();
  }
}

void bukaLoker() {
  lokerTerbuka = true;
  waktuBuka = millis();

  lockServo.write(90);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PIN BENAR");
  lcd.setCursor(0, 1);
  lcd.print("Loker terbuka");

  tone(BUZZER_PIN, 1500, 150);

  Serial.println("EVENT: LOKER DIBUKA");
}

void kunciLoker() {
  lockServo.write(0);
  lokerTerbuka = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Loker terkunci");

  Serial.println("EVENT: LOKER DIKUNCI");

  delay(500);
  tampilkanUtama();
}

void tampilkanUtama() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMART LOCKER");

  lcd.setCursor(0, 1);

  if (alarmAktif) {
    lcd.print("PERINGATAN!");
  } else if (lokerTerbuka) {
    lcd.print("LOKER TERBUKA");
  } else {
    lcd.print("PIN lalu #");
  }
}

void kirimThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus");
    return;
  }

  if (String(API_KEY) == "MASUKKAN_WRITE_API_KEY") {
    Serial.println("Isi API_KEY ThingSpeak");
    return;
  }

  HTTPClient http;

  // Field 1: status kunci (0 terkunci, 1 terbuka)
  // Field 2: jumlah percobaan PIN salah
  // Field 3: status pintu (0 tertutup, 1 terbuka)
  // Field 4: alarm (0 mati, 1 aktif)

  String url = String(TS_URL)
    + "?api_key=" + API_KEY
    + "&field1=" + String(lokerTerbuka ? 1 : 0)
    + "&field2=" + String(percobaanSalah)
    + "&field3=" + String(statusPintu())
    + "&field4=" + String(alarmAktif ? 1 : 0);

  http.begin(url);
  http.setTimeout(5000);

  int kodeHTTP = http.GET();

  if (kodeHTTP > 0) {
    Serial.print("ThingSpeak response: ");
    Serial.println(http.getString());
  } else {
    Serial.print("Pengiriman gagal: ");
    Serial.println(http.errorToString(kodeHTTP));
  }

  http.end();
}

