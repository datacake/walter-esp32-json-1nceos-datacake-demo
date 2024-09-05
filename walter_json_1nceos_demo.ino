#include <WalterModem.h>  // Include Walter Library
#include <ArduinoJson.h>  // Include ArduinoJson library

#define SERV_ADDR "10.60.2.239"
#define SERV_PORT 4445
#define INTERVAL_DURATION 30000 // 5 minutes in milliseconds

// Fake sensor variables
float temperature = 25.0;
float humidity = 50.0;
float pressure = 1013.25;
float co2 = 400.0;
float battery = 85.0;
String location = "(34.522,6.453)";
int signalStrength = -70;

unsigned long last_interval = 0;

// Modem object
WalterModem modem;

/**
 * @brief Connect to a network and UDP socket.
 * 
 * This function will set-up the modem and connect an UDP socket.
 * 
 * @param apn The APN to use for the PDP context.
 * @param user The APN username.
 * @param pass The APN password.
 * @param ip The IP address of the server to connect to.
 * @param port The port to connect to.
 * 
 * @return True on success, false on error.
 */
bool connect(const char *apn, const char *user, const char *pass, const char *ip, uint16_t port) {
  
  if(!modem.reset()) {
    Serial.println("Could not reset the modem");
    return false;
  }

  if(!modem.checkComm()) {
    Serial.println("Modem communication error");
    return false;
  }

  if(!modem.configCMEErrorReports()) {
    Serial.println("Could not configure CME error reports");
    return false;
  }

  if(!modem.configCEREGReports()) {
    Serial.println("Could not configure CEREG reports");
    return false;
  }

  WalterModemRsp rsp = {};
  if(!modem.getOpState(&rsp)) {
    Serial.println("Could not retrieve modem operational state");
    return false;
  }

  // if(modem.getRadioBands(&rsp)) {
  //   Serial.println("Modem is configured for the following bands:");
    
  //   for(int i = 0; i < rsp.data.bandSelCfgSet.count; ++i) {
  //     WalterModemBandSelection *bSel = rsp.data.bandSelCfgSet.config + i;
  //     Serial.printf("  - Operator '%s' on %s: 0x%05X\n",
  //       bSel->netOperator.name,
  //       bSel->rat == WALTER_MODEM_RAT_NBIOT ? "NB-IoT" : "LTE-M",
  //       bSel->bands);
  //   }
  // } else {
  //   Serial.println("Could not retrieve configured radio bands");
  //   return false;
  // }

  if(!modem.setOpState(WALTER_MODEM_OPSTATE_NO_RF)) {
    Serial.println("Could not set operational state to NO RF");
    return false;
  }

  /* Give the modem time to detect the SIM */
  delay(2000);

  if(modem.unlockSIM()) {
    Serial.println("Successfully unlocked SIM card");
  } else {
    Serial.println("Could not unlock SIM card");
    return false;
  }

  /* Create PDP context */
  if(user != NULL) {
    if(!modem.createPDPContext(
        apn,
        WALTER_MODEM_PDP_AUTH_PROTO_PAP,
        user,
        pass))
    {
      Serial.println("Could not create PDP context");
      return false;
    }
  } else {
    if(!modem.createPDPContext(apn)) {
      Serial.println("Could not create PDP context");
      return false;
    }
  }

  /* Authenticate the PDP context */
  // Not needed for 1NCE SIM as they don't require auth
  // if(!modem.authenticatePDPContext()) {
  //   Serial.println("Could not authenticate the PDP context");
  //   //return false;
  // }

  /* Set the operational state to full */
  if(!modem.setOpState(WALTER_MODEM_OPSTATE_FULL)) {
    Serial.println("Could not set operational state to FULL");
    return false;
  }

  /* Set the network operator selection to automatic */
  if(!modem.setNetworkSelectionMode(WALTER_MODEM_NETWORK_SEL_MODE_AUTOMATIC)) {
    Serial.println("Could not set the network selection mode to automatic");
    return false;
  }

  /* Wait for the network to become available */
  WalterModemNetworkRegState regState = modem.getNetworkRegState();
  while(!(regState == WALTER_MODEM_NETWORK_REG_REGISTERED_HOME ||
          regState == WALTER_MODEM_NETWORK_REG_REGISTERED_ROAMING))
  {
    delay(100);
    regState = modem.getNetworkRegState();
  }
  Serial.println("Connected to the network");

  /* Activate the PDP context */
  if(!modem.setPDPContextActive(true)) {
    Serial.println("Could not activate the PDP context");
    return false;
  }

  /* Attach the PDP context */
  if(!modem.attachPDPContext(true)) {
    Serial.println("Could not attach to the PDP context");
    return false;
  }

  if(modem.getPDPAddress(&rsp)) {
    Serial.println("PDP context address list: ");
    Serial.printf("  - %s\n", rsp.data.pdpAddressList.pdpAddress);
    if(rsp.data.pdpAddressList.pdpAddress2[0] != '\0') {
      Serial.printf("  - %s\n", rsp.data.pdpAddressList.pdpAddress2);
    }
  } else {
    Serial.println("Could not retrieve PDP context addresses");
    return false;
  }

  /* Construct a socket */
  if(!modem.createSocket(&rsp)) {
    Serial.println("Could not create a new socket");
    return false;
  }

  /* Configure the socket */
  if(!modem.configSocket()) {
    Serial.println("Could not configure the socket");
    return false;
  }

  /* Connect to the UDP test server */
  if(modem.connectSocket(ip, port, port)) {
    Serial.printf("Connected to UDP server %s:%d\n", ip, port);
  } else {
    Serial.println("Could not connect UDP socket");
    return false;
  }

  return true;
}

// Main setup function
void setup() {
    Serial.begin(115200);
    delay(5000);

    // Begin modem setup
    if (WalterModem::begin(&Serial2)) {
        Serial.println("Modem initialization OK");
    } else {
        Serial.println("Modem initialization ERROR");
        return;
    }

    last_interval = millis();
}

// Main loop function

void loop() {
    
    // Check if 5 minutes have passed
    
    if (millis() - last_interval >= INTERVAL_DURATION) {
        
        // Simulate some sensor data
        temperature = 20.0 + random(-100, 100) / 100.0; // Randomize temperature between 19 and 21
        humidity = 50.0 + random(-500, 500) / 100.0;    // Randomize humidity between 45 and 55
        pressure = 1013.25 + random(-100, 100) / 100.0; // Randomize pressure between 1012 and 1014
        co2 = 400 + random(0, 100);                     // Randomize CO2 between 400 and 500
        battery = 85.0 - random(0, 10);                 // Randomize battery between 75 and 85
        signalStrength = -70 + random(0, 10);           // Randomize signal strength between -70 and -60

        // Create JSON object
        StaticJsonDocument<256> jsonDoc;
        jsonDoc["temperature"] = temperature;
        jsonDoc["humidity"] = humidity;
        jsonDoc["pressure"] = pressure;
        jsonDoc["co2"] = co2;
        jsonDoc["battery"] = battery;
        jsonDoc["location"] = location;
        jsonDoc["signal"] = signalStrength;

        // Serialize JSON to string
        char buffer[256];
        size_t len = serializeJson(jsonDoc, buffer, sizeof(buffer));

        // Connect to server and send data
        if(!connect("iot.1nce.net", "", "", SERV_ADDR, SERV_PORT)) {
            ESP.restart();
        }

        if(!modem.socketSend((uint8_t *)buffer, len)) {
            Serial.println("Could not transmit data");
            ESP.restart();
        } else {
            Serial.println("Sensor data sent successfully as JSON.");
        }

        // Reset the interval timer
        last_interval = millis();
    }

    delay(1000); // Wait for 1 second before checking again
}
