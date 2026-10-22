/*
 * Diagnostico paso a paso - Mini-RAID (Arduino Uno, solo Arduino/C++)
 * Libreria necesaria: SdFat (Bill Greiman, 2.x)
 *
 * Monitor Serial a 115200 baudios, "Nueva linea" activado. Comandos:
 *   h -> ayuda
 *   l -> prueba de LEDs y buzzer
 *   a -> lectura del sensor DHT11
 *   1 -> prueba de la tarjeta SD-A (CS=10)
 *   2 -> prueba de la tarjeta SD-B (CS=9)
 *
 * Cableado:
 *   Bus SPI compartido por ambas SD: MISO=12, MOSI=11, SCK=13
 *   CS SD-A = 10, CS SD-B = 9
 *   LED verde = 4, LED rojo = 3, Buzzer = 2, DHT11 DATA = 5
 */

#include <SPI.h>
#include <SdFat.h>
#include <DHT.h>

// RAM libre aproximada (bytes) en AVR. El Uno solo tiene 2048 bytes en total.
int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

#define LED_OK     4
#define LED_ALARM  3
#define BUZZER     2
#define DHT_PIN    5
#define DHT_TYPE   DHT11

const uint8_t CS_PIN[2] = { 10, 9 };
SdFat32 sd[2];
DHT dht(DHT_PIN, DHT_TYPE);

void ayuda() {
  Serial.println(F("Comandos: l = LEDs/buzzer | a = sensor | 1 = SD-A | 2 = SD-B | h = ayuda"));
}

void probarSD(int i) {
  const char* nombre = (i == 0) ? "SD-A" : "SD-B";
  Serial.print(F("Probando ")); Serial.print(nombre);
  Serial.print(F(" (CS=")); Serial.print(CS_PIN[i]); Serial.println(F(")..."));

  // Ambos CS en alto antes de iniciar para que no se estorben en el bus
  digitalWrite(CS_PIN[0], HIGH);
  digitalWrite(CS_PIN[1], HIGH);

  if (!sd[i].begin(SdSpiConfig(CS_PIN[i], SHARED_SPI, SD_SCK_MHZ(4)))) {
    Serial.println(F("  FALLO al iniciar la tarjeta:"));
    sd[i].initErrorPrint(&Serial);
    Serial.println(F("  Revisa cableado, alimentacion, que este insertada y que sea FAT32."));
    return;
  }

  uint32_t mb = sd[i].card()->sectorCount() / 2048UL;
  Serial.print(F("  Tarjeta detectada. Tamano aprox: ")); Serial.print(mb); Serial.println(F(" MB"));
  Serial.print(F("  Sistema de archivos: FAT")); Serial.println(sd[i].fatType());
  if (sd[i].fatType() != 32) Serial.println(F("  AVISO: no es FAT32, formateala como FAT32."));

  File32 f = sd[i].open("test.txt", O_WRONLY | O_CREAT | O_TRUNC);
  if (!f) { Serial.println(F("  No se pudo crear test.txt")); return; }
  f.println(F("hola mini-raid"));
  f.close();
  Serial.println(F("  OK: se escribio test.txt (revisalo en la PC)."));
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_ALARM, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(CS_PIN[0], OUTPUT); digitalWrite(CS_PIN[0], HIGH);
  pinMode(CS_PIN[1], OUTPUT); digitalWrite(CS_PIN[1], HIGH);
  Serial.println(F("Diagnostico Mini-RAID (Arduino Uno)"));
  Serial.print(F("RAM libre: ")); Serial.print(freeRam()); Serial.println(F(" bytes"));
  ayuda();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();

  if (c == 'l') {
    Serial.println(F("LED verde..."));
    digitalWrite(LED_OK, HIGH); delay(800); digitalWrite(LED_OK, LOW);
    Serial.println(F("LED rojo..."));
    digitalWrite(LED_ALARM, HIGH); delay(800); digitalWrite(LED_ALARM, LOW);
    Serial.println(F("Buzzer..."));
    digitalWrite(BUZZER, HIGH); delay(300); digitalWrite(BUZZER, LOW);
    Serial.println(F("Fin. Debiste ver verde, rojo y oir el buzzer."));
  }
  else if (c == 'a') {
    Serial.println(F("Lectura del DHT11 (5 lecturas, una cada 2.2 s):"));
    for (int i = 0; i < 5; i++) {
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      if (isnan(t) || isnan(h)) {
        Serial.println(F("  ERROR de lectura: revisa VCC, GND y DATA (pin 5)."));
      } else {
        Serial.print(F("  Temp=")); Serial.print(t, 1);
        Serial.print(F(" C   Humedad=")); Serial.print(h, 0);
        Serial.println(F(" %"));
      }
      delay(2200);
    }
  }
  else if (c == '1') probarSD(0);
  else if (c == '2') probarSD(1);
  else if (c == 'h') ayuda();
}
