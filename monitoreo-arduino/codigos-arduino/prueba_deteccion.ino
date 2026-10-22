/*
 * PRUEBA DE DETECCION DE TARJETA SD (Arduino Uno + SdFat)
 * Solo verifica que la tarjeta responda; no escribe nada.
 * Reintenta cada 3 segundos, asi puedes mover cables o el adaptador
 * y ver al instante si pasa de FALLO a OK.
 *
 * Cableado (por la etiqueta del modulo):
 *   GND -> GND | 5V -> 5V | 3V3 -> sin conectar
 *   MISO -> 12 | SCK -> 13 | MOSI -> 11 | CS -> 10
 *
 * Monitor Serial a 115200 baudios.
 */

#include <SPI.h>
#include <SdFat.h>

#define CS_PIN    10
#define CS_PIN_2  9          // CS de un posible 2o modulo: se deja en alto

SdFat32  sd;
uint16_t intento = 0;
uint16_t aciertos = 0;

void probarDeteccion() {
  intento++;
  Serial.print(F("Intento "));
  Serial.print(intento);
  Serial.print(F(": "));

  if (!sd.cardBegin(SdSpiConfig(CS_PIN, SHARED_SPI, SD_SCK_MHZ(1)))) {
    Serial.println(F("NO se detecta la tarjeta."));
    sd.initErrorPrint(&Serial);
    return;
  }

  aciertos++;
  Serial.println(F("TARJETA DETECTADA"));

  Serial.print(F("   Tipo: "));
  switch (sd.card()->type()) {
    case SD_CARD_TYPE_SD1:  Serial.println(F("SD1 (capacidad estandar)")); break;
    case SD_CARD_TYPE_SD2:  Serial.println(F("SD2")); break;
    case SD_CARD_TYPE_SDHC: Serial.println(F("SDHC / microSDHC")); break;
    default:                Serial.println(F("desconocido")); break;
  }

  Serial.print(F("   Tamano aprox: "));
  Serial.print(sd.card()->sectorCount() / 2048UL);
  Serial.println(F(" MB"));

  if (sd.volumeBegin()) {
    Serial.print(F("   Sistema de archivos: FAT"));
    Serial.println(sd.fatType());
  } else {
    Serial.println(F("   La tarjeta responde, pero no se encontro un volumen FAT valido."));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(CS_PIN, OUTPUT);   digitalWrite(CS_PIN, HIGH);
  pinMode(CS_PIN_2, OUTPUT); digitalWrite(CS_PIN_2, HIGH);
  delay(500);
  Serial.println(F("=== Prueba de deteccion de tarjeta SD ==="));
  Serial.println(F("Reintenta cada 3 s. Mueve cables o el adaptador y mira el resultado."));
  Serial.println();
}

void loop() {
  probarDeteccion();
  Serial.print(F("   Aciertos hasta ahora: "));
  Serial.print(aciertos);
  Serial.print(F(" de "));
  Serial.println(intento);
  Serial.println();
  delay(3000);
}
