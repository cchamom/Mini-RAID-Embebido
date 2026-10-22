#include <SPI.h>
#include <SdFat.h>
#include <DHT.h>
#include <avr/wdt.h>

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

#define N_DRIVES            2
#define FILE_NAME           "datalog.csv"
#define CSV_HEADER          "ms,seq,temp_c,hum_pct\n"
#define SAMPLE_MS           2000UL
#define RETRY_MS            1000UL

#define LED_OK              4
#define LED_ALARM           3
#define BUZZER              2
#define DHT_PIN             5
#define DHT_TYPE            DHT11
#define MISO_PIN            12

const uint8_t CS_PIN[2]     = { 9, 8 };
const char* NAMES[2]        = { "SD-A", "SD-B" };

SdFat32   sd;
DHT       dht(DHT_PIN, DHT_TYPE);
bool      ok[2]             = { false, false };
uint32_t  lastTry[2]        = { 0, 0 };
uint8_t   probeFails[2]     = { 0, 0 };
bool      skipProbe[2]      = { false, false };

uint32_t seqNo = 0;
uint32_t lastSample = 0;

void releaseAllCS() {
  for (int j = 0; j < N_DRIVES; j++) {
    pinMode(CS_PIN[j], OUTPUT);
    digitalWrite(CS_PIN[j], HIGH);
  }
}

// Reset del hardware SPI + pull-up en MISO para que una SD ausente lea 0xFF
void resetSpiBus() {
  SPI.end();
  delayMicroseconds(100);
  SPI.begin();
  pinMode(MISO_PIN, INPUT_PULLUP);
}

// Devuelve la respuesta R1 al CMD0 (0x01 = tarjeta presente y en idle), 0xFF = no responde / ausente
uint8_t probeCard(int i) {
  releaseAllCS();
  resetSpiBus();

  uint8_t r = 0xFF;
  for (uint8_t intento = 0; intento < 3 && r == 0xFF; intento++) {
    SPI.beginTransaction(SPISettings(250000, MSBFIRST, SPI_MODE0));
    for (uint8_t k = 0; k < 10; k++) SPI.transfer(0xFF);   // >74 pulsos con CS alto

    digitalWrite(CS_PIN[i], LOW);
    SPI.transfer(0xFF);
    SPI.transfer(0x40);                                      // CMD0
    SPI.transfer(0x00); SPI.transfer(0x00); SPI.transfer(0x00); SPI.transfer(0x00);
    SPI.transfer(0x95);                                      // CRC

    for (uint8_t k = 0; k < 16 && r == 0xFF; k++) r = SPI.transfer(0xFF);

    digitalWrite(CS_PIN[i], HIGH);
    SPI.transfer(0xFF);
    SPI.endTransaction();
    releaseAllCS();

    if (r == 0xFF) delay(5);   // da tiempo a que una tarjeta recien insertada se estabilice
  }
  wdt_reset();
  return r;
}

bool beginDrive(int i) {
  releaseAllCS();
  return sd.begin(SdSpiConfig(CS_PIN[i], SHARED_SPI, SD_SCK_MHZ(1)));
}

bool mountDrive(int i) {
  // Siempre se comprueba presencia antes de llamar a sd.begin()
  uint8_t r = probeCard(i);
  bool presente = (r == 0x01) || (skipProbe[i] && r != 0xFF);
  if (!presente) {
    releaseAllCS();
    return false;
  }

  bool m = beginDrive(i);
  if (!m) skipProbe[i] = false;

  releaseAllCS();
  return m;
}

bool ensureHeader(int i) {
  if (!mountDrive(i)) return false;
  if (!sd.exists(FILE_NAME)) {
    File32 f = sd.open(FILE_NAME, O_WRONLY | O_CREAT);
    if (!f) {
      releaseAllCS();
      return false;
    }
    f.write(CSV_HEADER);
    f.close();
  }
  releaseAllCS();
  return true;
}

bool appendLine(int i, const char* line, size_t len) {
  if (!mountDrive(i)) {
    releaseAllCS();
    return false;
  }

  File32 f = sd.open(FILE_NAME, O_WRONLY | O_CREAT | O_APPEND);
  if (!f) {
    releaseAllCS();
    return false;
  }

  size_t n = f.write((const uint8_t*)line, len);
  bool closed = f.close();

  releaseAllCS();
  return (n == len) && closed;
}

bool writeWithRetry(int i, const char* line, size_t len) {
  for (uint8_t attempt = 0; attempt < 2; attempt++) {
    if (appendLine(i, line, len)) return true;
    delay(20);
  }
  return false;
}

void markFailed(int i) {
  if (ok[i]) {
    ok[i] = false;
    skipProbe[i] = false;
    probeFails[i] = 0;
    lastTry[i] = millis();

    Serial.print(F("LOG:[ALARMA] "));
    Serial.print(NAMES[i]);
    Serial.println(F(" fallo / extraida. Modo DEGRADADO activo."));
  }
}

bool resync(int bad, int good) {
  Serial.print(F("LOG:[REBUILD] Iniciando sincronizacion hacia "));
  Serial.println(NAMES[bad]);

  if (!ensureHeader(bad)) return false;

  if (!mountDrive(bad)) return false;
  File32 d = sd.open(FILE_NAME, O_RDONLY);
  if (!d) { releaseAllCS(); return false; }
  uint32_t dstSize = d.fileSize();
  d.close();

  if (!mountDrive(good)) return false;
  File32 s0 = sd.open(FILE_NAME, O_RDONLY);
  if (!s0) { releaseAllCS(); return false; }
  uint32_t srcSize = s0.fileSize();
  s0.close();

  uint32_t pos = dstSize;
  uint32_t total = 0;
  uint8_t buf[128];

  while (pos < srcSize) {
    wdt_reset();
    if (!mountDrive(good)) { releaseAllCS(); return false; }
    File32 s = sd.open(FILE_NAME, O_RDONLY);
    if (!s) { releaseAllCS(); return false; }
    s.seekSet(pos);
    int n = s.read(buf, sizeof(buf));
    s.close();
    if (n <= 0) break;

    if (!mountDrive(bad)) { releaseAllCS(); return false; }
    File32 o = sd.open(FILE_NAME, O_WRONLY | O_APPEND);
    if (!o) { releaseAllCS(); return false; }
    size_t w = o.write(buf, n);
    bool closed = o.close();
    if (w != (size_t)n || !closed) { releaseAllCS(); return false; }

    pos += n;
    total += n;
  }

  releaseAllCS();

  Serial.print(F("LOG:[REBUILD] "));
  Serial.print(NAMES[bad]);
  Serial.print(F(" sincronizada con exito. Bytes copiados desde copia sana: "));
  Serial.println(total);
  return true;
}

void tryRecover(int i) {
  lastTry[i] = millis();

  uint8_t resp = probeCard(i);
  bool mounted = false;

  if (resp == 0x01) {
    // Tarjeta presente y respondiendo normal
    probeFails[i] = 0;
    mounted = beginDrive(i);
  } else if (resp != 0xFF) {
    // Responde algo raro (tarjeta presente pero atipica): tras 3 veces se fuerza el montaje
    if (++probeFails[i] >= 3) {
      probeFails[i] = 0;
      mounted = beginDrive(i);
      if (mounted) skipProbe[i] = true;
    }
  } else {
    // 0xFF = ausente o sin contacto: NO se llama a sd.begin() (evita el bloqueo)
    probeFails[i] = 0;
  }

  if (!mounted) {
    releaseAllCS();
    return;
  }

  int other = (i == 0) ? 1 : 0;
  bool good;
  if (N_DRIVES == 2 && ok[other]) good = resync(i, other);
  else                            good = ensureHeader(i);

  if (!good) {
    releaseAllCS();
    return;
  }

  ok[i] = true;
  releaseAllCS();

  Serial.print(F("LOG:[RECUPERADO] "));
  Serial.print(NAMES[i]);
  Serial.println(F(" reinsertada y totalmente operativa."));
}

void updateAlarm() {
  uint8_t failed = 0;
  for (int i = 0; i < N_DRIVES; i++) failed += !ok[i];
  uint32_t t = millis();
  if (failed == 0) {
    digitalWrite(LED_OK, HIGH);
    digitalWrite(LED_ALARM, LOW);
    digitalWrite(BUZZER, LOW);
  } else if (failed < N_DRIVES) {
    digitalWrite(LED_OK, LOW);
    digitalWrite(LED_ALARM, HIGH);
    digitalWrite(BUZZER, (t % 1000) < 200);
  } else {
    digitalWrite(LED_OK, LOW);
    digitalWrite(LED_ALARM, (t % 200) < 100);
    digitalWrite(BUZZER, HIGH);
  }
}

void sample() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  seqNo++;

  char tbuf[12], hbuf[8];
  if (isnan(t) || isnan(h)) {
    strcpy(tbuf, "0.0");
    strcpy(hbuf, "0");
  } else {
    int t10 = (int)(t * 10.0f + (t >= 0 ? 0.5f : -0.5f));
    snprintf(tbuf, sizeof(tbuf), "%s%d.%d", t10 < 0 ? "-" : "", abs(t10) / 10, abs(t10) % 10);
    snprintf(hbuf, sizeof(hbuf), "%d", (int)(h + 0.5f));
  }

  char line[48];
  int len = snprintf(line, sizeof(line), "%lu,%lu,%s,%s\n",
                     (unsigned long)millis(), (unsigned long)seqNo, tbuf, hbuf);

  for (int i = 0; i < N_DRIVES; i++) {
    if (ok[i]) {
      if (!writeWithRetry(i, line, len)) {
        markFailed(i);
      }
    }
    wdt_reset();
  }

  Serial.print(F("DATA:"));
  Serial.print(millis()); Serial.print(F(","));
  Serial.print(seqNo); Serial.print(F(","));
  Serial.print(tbuf); Serial.print(F(","));
  Serial.print(hbuf); Serial.print(F(","));
  Serial.print(ok[0] ? 1 : 0); Serial.print(F(","));
  Serial.print(ok[1] ? 1 : 0); Serial.print(F(","));
  Serial.println(freeRam());

  updateAlarm();
}

void setup() {
  uint8_t mcusr = MCUSR;
  MCUSR = 0;
  wdt_disable();

  Serial.begin(115200);
  dht.begin();

  pinMode(LED_OK, OUTPUT);
  pinMode(LED_ALARM, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(10, OUTPUT); // SS del Arduino Uno
  releaseAllCS();
  pinMode(MISO_PIN, INPUT_PULLUP);

  // Causa del ultimo reinicio: bit0=encendido, bit1=reset externo, bit2=BROWN-OUT, bit3=WATCHDOG
  Serial.print(F("LOG:[RESET] MCUSR="));
  Serial.println(mcusr, BIN);

  Serial.println(F("LOG:[SISTEMA] Inicializando Mini-RAID 1..."));
  for (int i = 0; i < N_DRIVES; i++) {
    ok[i] = ensureHeader(i);
    lastTry[i] = millis();
    Serial.print(F("LOG:[INICIO] "));
    Serial.print(NAMES[i]);
    Serial.println(ok[i] ? F(": Montada correctamente.") : F(": NO detectada."));
  }

  wdt_enable(WDTO_8S);
}

void loop() {
  wdt_reset();

  uint32_t now = millis();
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    sample();
  }
  for (int i = 0; i < N_DRIVES; i++) {
    if (!ok[i] && now - lastTry[i] >= RETRY_MS) tryRecover(i);
  }
  updateAlarm();
}
