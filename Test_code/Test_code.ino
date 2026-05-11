#include <Arduino.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

SensirionI2cScd4x sensor;
static char    errorMessage[64];
static int16_t error;
static bool    setupOk = false;   // blocks loop() if init failed

// ── I2C scanner — run this first to confirm sensor is visible ──
void scanI2C() {
  Serial.println("Scanning I2C bus...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0)
    Serial.println("  *** NO devices found — check SDA/SCL wiring! ***");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);
  delay(1000);   // let USB serial stabilize (fixes garbled startup chars)

  Serial.println("\n====== SCD40 Init ======\n");
  Wire.begin();

  scanI2C();     // tells you exactly what's on the bus

  sensor.begin(Wire, SCD41_I2C_ADDR_62);
  delay(30);

  // wakeUp() is SCD41 low-power mode only — skip it for SCD40
  // sensor.wakeUp();   <-- removed

  // stopPeriodicMeasurement may "fail" on first boot — that's fine
  error = sensor.stopPeriodicMeasurement();
  if (error != NO_ERROR) {
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.print("stopPeriodicMeasurement (warning, ok on cold start): ");
    Serial.println(errorMessage);
  }
  delay(500);

  error = sensor.reinit();
  if (error != NO_ERROR) {
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.print("reinit error: ");
    Serial.println(errorMessage);
  }
  delay(20);

  // getSerialNumber is the real test — if this fails, wiring is wrong
  uint64_t serialNumber = 0;
  error = sensor.getSerialNumber(serialNumber);
  if (error != NO_ERROR) {
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.print("getSerialNumber FAILED: ");
    Serial.println(errorMessage);
    Serial.println(">>> Sensor not found. Check SDA/SCL connected to");
    Serial.println("    dedicated SDA/SCL header pins and VCC to 3.3V. <<<");
    return;   // stops here; loop() will print a safe message
  }
  Serial.print("SCD40 serial: 0x");
  Serial.print((uint32_t)(serialNumber >> 32), HEX);
  Serial.println((uint32_t)(serialNumber & 0xFFFFFFFF), HEX);

  error = sensor.startPeriodicMeasurement();
  if (error != NO_ERROR) {
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.print("startPeriodicMeasurement FAILED: ");
    Serial.println(errorMessage);
    return;
  }

  Serial.println("Init OK — readings every 5s\n");
  setupOk = true;
}

void loop() {
  if (!setupOk) {
    Serial.println("Setup failed — fix wiring then press reset.");
    delay(5000);
    return;
  }

  delay(5000);

  bool dataReady = false;
  error = sensor.getDataReadyStatus(dataReady);
  if (error != NO_ERROR) {
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.print("getDataReadyStatus error: ");
    Serial.println(errorMessage);
    return;
  }
  if (!dataReady) {
    Serial.println("Data not ready yet...");
    return;
  }

  uint16_t co2 = 0;
  float temperature = 0.0, relativeHumidity = 0.0;
  error = sensor.readMeasurement(co2, temperature, relativeHumidity);
  if (error != NO_ERROR) {
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.print("readMeasurement error: ");
    Serial.println(errorMessage);
    return;
  }

  Serial.print("CO2      : "); Serial.print(co2);            Serial.println(" ppm");
  Serial.print("Temp     : "); Serial.print(temperature, 1); Serial.println(" C");
  Serial.print("Humidity : "); Serial.print(relativeHumidity, 1); Serial.println(" %");
  Serial.println();
}