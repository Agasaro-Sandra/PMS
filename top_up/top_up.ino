#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9
#define PLATE_BLOCK 2
#define BALANCE_BLOCK 4

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

enum SystemState {
  WAIT_FOR_CARD,
  NEW_CARD_DETECTED,
  REGISTER_PLATE,
  REGISTER_BALANCE,
  TOP_UP_PROMPT,
  CONFIRM_WRITE
};

SystemState currentState = WAIT_FOR_CARD;
String currentPlate = "";
String currentBalance = "";
String pendingPlate = "";
String pendingBalance = "";

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  showWaitForCard();
}

void loop() {
  switch(currentState) {
    case WAIT_FOR_CARD:
      if (detectCard()) {
        readCardData();
        if (currentPlate.length() == 0 && currentBalance.length() == 0) {
          currentState = NEW_CARD_DETECTED;
          showNewCardPrompt();
        } else if (currentPlate.length() > 0) {
          currentState = TOP_UP_PROMPT;
          showTopUpPrompt();
        } else {
          currentState = TOP_UP_PROMPT;
          showAnonymousCardPrompt();
        }
      }
      break;

    case NEW_CARD_DETECTED:
      if (getUserInput("PLATE")) {
        pendingPlate = getInputValue();
        currentState = REGISTER_BALANCE;
        showRegisterBalancePrompt();
      }
      break;

    case REGISTER_BALANCE:
      if (getUserInput("BALANCE")) {
        pendingBalance = getInputValue();
        currentState = CONFIRM_WRITE;
        showConfirmRegistration();
      }
      break;

    case TOP_UP_PROMPT:
      if (getUserInput("BALANCE")) {
        pendingBalance = getInputValue();
        currentState = CONFIRM_WRITE;
        showConfirmTopUp();
      }
      break;

    case CONFIRM_WRITE:
      if (detectCard()) {
        if (writeCardData()) {
          showTransactionComplete();
        } else {
          showWriteError();
        }
        resetSystem();
      }
      break;
  }
  delay(100);
}

bool detectCard() {
  return mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial();
}

void readCardData() {
  currentPlate = readBlockToString(PLATE_BLOCK);
  currentBalance = readBlockToString(BALANCE_BLOCK);
  mfrc522.PICC_HaltA();
}

bool writeCardData() {
  bool success = true;
  
  if (pendingPlate.length() > 0) {
    byte plateData[16] = {0};
    pendingPlate.toCharArray((char*)plateData, 16);
    if (!writeBlock(PLATE_BLOCK, plateData)) success = false;
    currentPlate = pendingPlate;
  }

  byte balanceData[16] = {0};
  int newBalance = currentBalance.toInt() + pendingBalance.toInt();
  itoa(newBalance, (char*)balanceData, 10);
  if (!writeBlock(BALANCE_BLOCK, balanceData)) success = false;
  currentBalance = String(newBalance);

  mfrc522.PICC_HaltA();
  return success;
}

String readBlockToString(byte block) {
  byte buffer[18];
  byte size = sizeof(buffer);
  String result = "";
  
  if (readBlock(block, buffer, &size)) {
    for (byte i = 0; i < 16; i++) {
      if (buffer[i] >= 32 && buffer[i] <= 126) {
        result += (char)buffer[i];
      }
    }
    result.trim();
  }
  return result;
}

bool readBlock(byte block, byte *buffer, byte *size) {
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    return false;
  }
  
  status = mfrc522.MIFARE_Read(block, buffer, size);
  return (status == MFRC522::STATUS_OK);
}

bool writeBlock(byte block, byte *data) {
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    return false;
  }
  
  status = mfrc522.MIFARE_Write(block, data, 16);
  return (status == MFRC522::STATUS_OK);
}

/* ========== USER INTERFACE ========== */
void showWaitForCard() {
  Serial.println("\n================================");
  Serial.println("    PARKING MANAGEMENT SYSTEM");
  Serial.println("================================");
  Serial.println("Please scan your RFID card...");
  Serial.println("--------------------------------");
}

void showNewCardPrompt() {
  Serial.println("\nNEW CARD DETECTED");
  Serial.println("--------------------------------");
  Serial.println("To register vehicle, enter:");
  Serial.println("PLATE=YOUR_PLATE_NUMBER");
  Serial.println("Example: PLATE=RAB123A");
  Serial.println("--------------------------------");
}

void showRegisterBalancePrompt() {
  Serial.println("\nREGISTER NEW VEHICLE");
  Serial.println("--------------------------------");
  Serial.print("Plate: "); Serial.println(pendingPlate);
  Serial.println("Enter initial balance:");
  Serial.println("BALANCE=AMOUNT_IN_RWF");
  Serial.println("Example: BALANCE=5000");
  Serial.println("--------------------------------");
}

void showTopUpPrompt() {
  Serial.println("\nVEHICLE DETECTED");
  Serial.println("--------------------------------");
  Serial.print("Plate: "); Serial.println(currentPlate);
  Serial.print("Current balance: "); Serial.print(currentBalance); Serial.println(" RWF");
  Serial.println("Enter amount to add:");
  Serial.println("BALANCE=AMOUNT_IN_RWF");
  Serial.println("Example: BALANCE=1000");
  Serial.println("--------------------------------");
}

void showAnonymousCardPrompt() {
  Serial.println("\nPREPAID CARD DETECTED");
  Serial.println("--------------------------------");
  Serial.print("Current balance: "); Serial.print(currentBalance); Serial.println(" RWF");
  Serial.println("Enter amount to add:");
  Serial.println("BALANCE=AMOUNT_IN_RWF");
  Serial.println("Example: BALANCE=1000");
  Serial.println("--------------------------------");
}

void showConfirmRegistration() {
  Serial.println("\nCONFIRM REGISTRATION");
  Serial.println("--------------------------------");
  Serial.print("Plate: "); Serial.println(pendingPlate);
  Serial.print("Initial balance: "); Serial.print(pendingBalance); Serial.println(" RWF");
  Serial.println("Scan card again to confirm");
  Serial.println("--------------------------------");
}

void showConfirmTopUp() {
  Serial.println("\nCONFIRM TOP-UP");
  Serial.println("--------------------------------");
  if (currentPlate.length() > 0) {
    Serial.print("Vehicle: "); Serial.println(currentPlate);
  }
  Serial.print("Adding: +"); Serial.print(pendingBalance); Serial.println(" RWF");
  Serial.print("New balance will be: ");
  Serial.print(currentBalance.toInt() + pendingBalance.toInt()); Serial.println(" RWF");
  Serial.println("Scan card again to confirm");
  Serial.println("--------------------------------");
}

void showTransactionComplete() {
  Serial.println("\nTRANSACTION COMPLETE");
  Serial.println("--------------------------------");
  if (pendingPlate.length() > 0) {
    Serial.print("Registered: "); Serial.println(currentPlate);
  }
  Serial.print("New balance: "); Serial.print(currentBalance); Serial.println(" RWF");
  Serial.println("Thank you!");
  Serial.println("--------------------------------");
  showWaitForCard();
}

void showWriteError() {
  Serial.println("\nERROR: Failed to write card");
  Serial.println("Please try again");
  Serial.println("--------------------------------");
}

/* ========== INPUT HANDLING ========== */
bool getUserInput(const char* expectedType) {
  while (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.startsWith(expectedType)) {
      return true;
    }
    Serial.println("Invalid format. Please try again.");
  }
  return false;
}

String getInputValue() {
  String input = Serial.readStringUntil('\n');
  input.trim();
  int equalsIndex = input.indexOf('=');
  return input.substring(equalsIndex + 1);
}

void resetSystem() {
  pendingPlate = "";
  pendingBalance = "";
  currentState = WAIT_FOR_CARD;
}