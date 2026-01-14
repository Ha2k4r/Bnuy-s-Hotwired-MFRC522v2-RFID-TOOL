#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Hack.h>
#include <MFRC522Debug.h>

/* ---------------- Hardware ---------------- */
MFRC522DriverPinSimple ss_pin(10);
MFRC522DriverSPI driver(ss_pin);
MFRC522 rfid(driver);
MFRC522Hack rfidHack(rfid, true);

/* ---------------- Keys ---------------- */
MFRC522::MIFARE_Key defaultKey;


constexpr uint8_t knownKeys[][6] PROGMEM = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // FF FF FF FF FF FF
    {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5}, // A0 A1 A2 A3 A4 A5
    {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5}, // B0 B1 B2 B3 B4 B5
    {0x4d, 0x3a, 0x99, 0xc3, 0x51, 0xdd}, // 4D 3A 99 C3 51 DD
    {0x1a, 0x98, 0x2c, 0x7e, 0x45, 0x9a}, // 1A 98 2C 7E 45 9A
    {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7}, // D3 F7 D3 F7 D3 F7
    {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}, // AA BB CC DD EE FF
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 00 00 00 00 00 00
    {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7}, // d3 f7 d3 f7 d3 f7
    {0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0}, // a0 b0 c0 d0 e0 f0
    {0xa1, 0xb1, 0xc1, 0xd1, 0xe1, 0xf1}, // a1 b1 c1 d1 e1 f1
    {0x71, 0x4c, 0x5c, 0x88, 0x6e, 0x97}, // 71 4c 5c 88 6e 97
    {0x58, 0x7e, 0xe5, 0xf9, 0x35, 0x0f}, // 58 7e e5 f9 35 0f
    {0xa0, 0x47, 0x8c, 0xc3, 0x90, 0x91}, // a0 47 8c c3 90 91
    {0x53, 0x3c, 0xb6, 0xc7, 0x23, 0xf6}, // 53 3c b6 c7 23 f6
    {0x8f, 0xd0, 0xa4, 0xf2, 0x56, 0xe9}, // 8f d0 a4 f2 56 e9
    {0x73, 0x8f, 0x9a, 0x43, 0x50, 0x22}, // 73 8f 9a 43 50 22
};


constexpr uint8_t NUM_KEYS = sizeof(knownKeys) / 6;

/* ---------------- UID storage (nano sized tehe) ---------------- */
uint8_t storedUID[10];
uint8_t storedUIDSize = 0;

constexpr uint8_t newUID[4] = { 0xDA, 0xAC, 0xBE, 0xEF };

/* ------------------------------------------------ */
void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin();
  rfid.PCD_Init();

  for (uint8_t i = 0; i < 6; i++)
    defaultKey.keyByte[i] = 0xFF;

  Serial.println(F("\n=== Bnuy's Hotwired MFRC522v2 RFID TOOL ==="));
  Serial.println(F("[r] Read card + store UID"));
  Serial.println(F("[u] Clone stored UID (DANGEROUS)"));
  Serial.println(F("[c] Clone card data "));
  Serial.println(F("[b] Brute force Crack Keys"));
}

/* ------------------------------------------------ */
bool waitForCard() {
  while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(50);
  }
  return true;
}

/* ------------------------------------------------ */
uint8_t detectBlocks() {
  MFRC522::PICC_Type type = rfid.PICC_GetType(rfid.uid.sak);
  if (type == MFRC522Constants::PICC_TYPE_MIFARE_1K) return 64;
  if (type == MFRC522Constants::PICC_TYPE_MIFARE_4K) return 256;
  return 0;
}

/* ------------------------------------------------ */
void printCardInfo() {
  Serial.print(F("UID: "));
  MFRC522Debug::PrintSelectedUID(rfid, Serial);
  Serial.println();

  Serial.print(F("SAK: 0x"));
  Serial.println(rfid.uid.sak, HEX);

  Serial.print(F("Type: "));
  Serial.println(MFRC522Debug::PICC_GetTypeName(
    rfid.PICC_GetType(rfid.uid.sak)
  ));
}

/* ------------------------------------------------ */
void readCard() {
  Serial.println(F("\nPresent SOURCE card"));
  waitForCard();
  printCardInfo();

  memcpy(storedUID, rfid.uid.uidByte, rfid.uid.size);
  storedUIDSize = rfid.uid.size;

  Serial.print(F("Stored UID ("));
  Serial.print(storedUIDSize);
  Serial.println(F(" bytes)"));

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ------------------------------------------------

void rereadAndDump() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    Serial.println(F("Failed to reselect card"));
    return;
  }

  Serial.print(F("UID after write: "));
  MFRC522Debug::PrintSelectedUID(rfid, Serial);
  Serial.println();

  Serial.println(F("Full card dump:"));
  MFRC522Debug::PICC_DumpToSerial(rfid, Serial, &(rfid.uid));

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

/* ------------------------------------------------ */
void forceUIDWrite() {
  Serial.println(F("\nPresent UID-changeable card"));

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    Serial.println(F("No card detected"));
    return;
  }

  Serial.print(F("Current UID: "));
  MFRC522Debug::PrintSelectedUID(rfid, Serial);
  Serial.println();

  // HARD HALT
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(50);

  byte atqa[2];
  byte atqaSize = sizeof(atqa);

  if (rfid.PICC_WakeupA(atqa, &atqaSize) != MFRC522::StatusCode::STATUS_OK) {
    Serial.println(F("Wakeup failed"));
    return;
  }

  delay(10);

  MFRC522::MIFARE_Key key;
  for (uint8_t i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  Serial.println(F("Attempting UID write (magic backdoor)..."));

  bool ok = rfidHack.MIFARE_SetUid(
    newUID,
    4,
    key,
    true
  );

  Serial.println(ok
    ? F("UID write command accepted")
    : F("UID write attempted (verify anyway)")
  );

  // RESET + RESELECT
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(100);

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    Serial.println(F("Failed to reselect card"));
    return;
  }

  Serial.print(F("New UID: "));
  MFRC522Debug::PrintSelectedUID(rfid, Serial);
  Serial.println();

  Serial.println(F("Full card dump:"));
  MFRC522Debug::PICC_DumpToSerial(rfid, Serial, &(rfid.uid));

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}



/* ------------------------------------------------ */
void cloneCardData() {
  Serial.println(F("\nPresent SOURCE card"));
  waitForCard();
  printCardInfo();

  uint8_t blocks = detectBlocks();
  if (!blocks) {
    Serial.println(F("Unsupported card"));
    return;
  }

  Serial.println(F("Remove SOURCE card"));
  delay(2000);

  Serial.println(F("Present TARGET card"));
  waitForCard();

  uint8_t buffer[18];
  uint8_t size = sizeof(buffer);

  for (uint8_t sector = 0; sector < blocks / 4; sector++) {
    uint8_t trailer = sector * 4 + 3;

    if (rfid.PCD_Authenticate(
          MFRC522Constants::PICC_CMD_MF_AUTH_KEY_A,
          trailer,
          &defaultKey,
          &rfid.uid
        ) != MFRC522::StatusCode::STATUS_OK)
      continue;

    for (uint8_t block = sector * 4; block < sector * 4 + 3; block++) {
      if (rfid.MIFARE_Read(block, buffer, &size)
          == MFRC522::StatusCode::STATUS_OK) {
        rfid.MIFARE_Write(block, buffer, 16);
      }
    }
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.println(F("Clone complete"));
}

/* ---------------- Utility ---------------- */

uint16_t getBlockCount() {
  MFRC522::PICC_Type type = rfid.PICC_GetType(rfid.uid.sak);
  if (type == MFRC522Constants::PICC_TYPE_MIFARE_1K) return 64;
  if (type == MFRC522Constants::PICC_TYPE_MIFARE_4K) return 256;
  return 0;
}

void loadKeyFromProgmem(uint8_t index, MFRC522::MIFARE_Key &key) {
  for (uint8_t i = 0; i < 6; i++) {
    key.keyByte[i] = pgm_read_byte(&(knownKeys[index][i]));
  }
}

/* ------------------------------------------------ */

void bruteForceKeys() {
  Serial.println(F("\nPresent card"));
  waitForCard();

  Serial.println(F("UID:"));
  MFRC522Debug::PrintSelectedUID(rfid, Serial);
  Serial.println();

  uint16_t blocks = getBlockCount();
  if (!blocks) {
    Serial.println(F("Unsupported card type"));
    return;
  }

  uint8_t buffer[18];
  uint8_t size = sizeof(buffer);
  MFRC522::MIFARE_Key key;

  for (uint16_t sector = 0; sector < blocks / 4; sector++) {
    uint8_t trailer = sector * 4 + 3;
    bool unlocked = false;

    for (uint8_t k = 0; k < NUM_KEYS; k++) {
      loadKeyFromProgmem(k, key);

      if (rfid.PCD_Authenticate(
            MFRC522Constants::PICC_CMD_MF_AUTH_KEY_A,
            trailer,
            &key,
            &rfid.uid
          ) == MFRC522::StatusCode::STATUS_OK) {

        Serial.print(F("Sector "));
        Serial.print(sector);
        Serial.print(F(" unlocked with key #"));
        Serial.println(k);

        for (uint8_t block = sector * 4;
             block < sector * 4 + 3;
             block++) {

          if (rfid.MIFARE_Read(block, buffer, &size)
              == MFRC522::StatusCode::STATUS_OK) {
            Serial.print(F("  Block "));
            Serial.print(block);
            Serial.print(F(": "));
            for (uint8_t i = 0; i < 16; i++) {
              Serial.print(buffer[i] < 0x10 ? " 0" : " ");
              Serial.print(buffer[i], HEX);
            }
            Serial.println();
          }
        }

        unlocked = true;
        break;
      }
    }

    if (!unlocked) {
      Serial.print(F("Sector "));
      Serial.print(sector);
      Serial.println(F(": no detected key"));
    }
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  Serial.println(F("Done"));
}
/* ------------------------------------------------ */
void loop() {
  if (!Serial.available()) return;

  char cmd = Serial.read();
  switch (cmd) {
    case 'r': readCard(); break;
    case 'u': forceUIDWrite(); break;
    case 'c': cloneCardData(); break;
    case 'b': bruteForceKeys(); break;
  }
}
