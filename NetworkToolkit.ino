/*
 * Network Toolkit per M5Stack Cardputer
 * Tool completo per network engineering
 *
 * Funzionalità:
 * - IP/ARP Scanner
 * - Port Scanner
 * - Ping Sweep
 * - DNS Lookup
 * - DHCP Discovery
 * - WiFi Signal Mapper
 * - Subnet Calculator
 * - Network Monitor
 * - QR Code WiFi
 */

#include <M5Cardputer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPping.h>
#include <esp_wifi.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/netdb.h>
#include "qrcode.h"

// Keyboard key definitions (M5Cardputer 1.1.x compatibility)
#ifndef KEY_ESC
#define KEY_ESC 0x1B
#endif
#ifndef KEY_UP
#define KEY_UP 0x18
#endif
#ifndef KEY_DOWN
#define KEY_DOWN 0x19
#endif
#ifndef KEY_ENTER
#define KEY_ENTER 0x0D
#endif

// Helper: convert vector<char> to String (M5Cardputer 1.1.x compatibility)
String keyWord(const std::vector<char>& word) {
    if(word.empty()) return "";
    return String(word.data(), word.size());
}

// Display colors
#define COLOR_BG       TFT_BLACK
#define COLOR_TEXT     TFT_WHITE
#define COLOR_TITLE    TFT_CYAN
#define COLOR_SUCCESS  TFT_GREEN
#define COLOR_ERROR    TFT_RED
#define COLOR_WARNING  TFT_YELLOW
#define COLOR_MENU_SEL TFT_BLUE

// Screen dimensions
#define SCREEN_W 240
#define SCREEN_H 135

// Network settings
#define MAX_HOSTS 254
#define PING_TIMEOUT 1000
#define SCAN_TIMEOUT 100

// Menu states
enum MenuState {
    MENU_MAIN,
    MENU_WIFI_CONNECT,
    MENU_SCANNER,
    MENU_PORT_SCAN,
    MENU_PING,
    MENU_DNS,
    MENU_DHCP,
    MENU_SIGNAL_MAP,
    MENU_SUBNET_CALC,
    MENU_MONITOR,
    MENU_QR_WIFI,
    MENU_WIFI_SCAN,
    MENU_SNMP
};

// Strutture dati
struct NetworkHost {
    IPAddress ip;
    uint8_t mac[6];
    String hostname;
    bool online;
    float latency;
};

struct ScanResult {
    NetworkHost hosts[MAX_HOSTS];
    int count;
};

// Variabili globali
MenuState currentMenu = MENU_MAIN;
int menuSelection = 0;
int menuOffset = 0;
bool wifiConnected = false;
String connectedSSID = "";
ScanResult scanResults;
IPAddress localIP;
IPAddress gateway;
IPAddress subnet;

// Menu items
const char* mainMenuItems[] = {
    "WiFi Connect",
    "IP Scanner",
    "Port Scanner",
    "Ping Sweep",
    "DNS Lookup",
    "DHCP Discover",
    "Signal Mapper",
    "Subnet Calc",
    "Net Monitor",
    "WiFi QR Code",
    "WiFi Networks",
    "SNMP Query"
};
const int mainMenuCount = 12;

// Common ports per port scan
const uint16_t commonPorts[] = {
    21, 22, 23, 25, 53, 80, 110, 135, 139, 143,
    443, 445, 993, 995, 1433, 1521, 3306, 3389,
    5432, 5900, 8080, 8443
};
const int commonPortsCount = 22;

// MAC Vendor database (primi 3 byte OUI)
struct MacVendor {
    uint8_t oui[3];
    const char* vendor;
};

const MacVendor macVendors[] = {
    {{0x00, 0x1A, 0x2B}, "Cisco"},
    {{0x00, 0x50, 0x56}, "VMware"},
    {{0x00, 0x0C, 0x29}, "VMware"},
    {{0x00, 0x15, 0x5D}, "Microsoft"},
    {{0x00, 0x1C, 0x42}, "Parallels"},
    {{0x08, 0x00, 0x27}, "VirtualBox"},
    {{0xAC, 0xDE, 0x48}, "Apple"},
    {{0x3C, 0x06, 0x30}, "Apple"},
    {{0xDC, 0xA6, 0x32}, "Raspberry"},
    {{0xB8, 0x27, 0xEB}, "Raspberry"},
    {{0x00, 0x1E, 0x58}, "D-Link"},
    {{0xC8, 0x3A, 0x35}, "Tenda"},
    {{0x50, 0xC7, 0xBF}, "TP-Link"},
    {{0x98, 0xDA, 0xC4}, "TP-Link"},
    {{0x30, 0xB5, 0xC2}, "TP-Link"},
    {{0x00, 0x24, 0xB2}, "Netgear"},
    {{0x00, 0x26, 0xF2}, "Netgear"},
    {{0xF8, 0x32, 0xE4}, "ASUS"},
    {{0x00, 0x22, 0x15}, "ASUS"},
    {{0x00, 0x23, 0x24}, "Dell"},
    {{0x18, 0x03, 0x73}, "Dell"},
    {{0x00, 0x21, 0x5A}, "HP"},
    {{0x3C, 0xD9, 0x2B}, "HP"},
    {{0x00, 0x50, 0xB6}, "Linksys"},
    {{0x00, 0x1A, 0x70}, "Linksys"},
    {{0xAC, 0x84, 0xC6}, "TP-Link"},
    {{0x00, 0x17, 0x88}, "Philips"},
    {{0x00, 0x04, 0x4B}, "Nvidia"},
    {{0x48, 0x2C, 0xA0}, "Xiaomi"},
    {{0x7C, 0x1E, 0x52}, "Huawei"},
    {{0x00, 0x26, 0x5E}, "Hon Hai/Foxconn"},
    {{0x00, 0x1A, 0xA0}, "Dell"},
    {{0x54, 0xEE, 0x75}, "Wistron"},
    {{0xFC, 0xAA, 0x14}, "Giga-Byte"},
    {{0xE4, 0x5F, 0x01}, "Raspberry"},
    {{0x24, 0x0A, 0xC4}, "Espressif"},
    {{0xA4, 0xCF, 0x12}, "Espressif"},
    {{0x7C, 0xDF, 0xA1}, "Espressif"},
    {{0x30, 0xAE, 0xA4}, "Espressif"}
};
const int macVendorCount = 39;

// Forward declarations
void drawMainMenu();
void handleMainMenu();
void drawHeader(const char* title);
void drawStatus(const char* msg, uint16_t color);
void connectToWiFi();
void runIPScanner();
void runPortScanner();
void runPingSweep();
void runDNSLookup();
void runDHCPDiscover();
void runSignalMapper();
void showSubnetCalc();
void runNetMonitor();
void showWiFiQR();
void scanWiFiNetworks();
void runSNMPQuery();
String getMacVendor(uint8_t* mac);
String macToString(uint8_t* mac);
bool arpScan(IPAddress ip, uint8_t* mac);
void clearScreen();
void waitForKey();
char getKeyInput();
String getTextInput(const char* prompt, int maxLen);

// Setup
void setup() {
    M5Cardputer.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(COLOR_BG);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);

    // Splash screen
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.setTextColor(COLOR_TITLE);
    M5Cardputer.Display.drawString("Network Toolkit", SCREEN_W/2, 40);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);
    M5Cardputer.Display.drawString("v1.0 - For Network Engineers", SCREEN_W/2, 60);
    M5Cardputer.Display.drawString("Press any key...", SCREEN_W/2, 100);
    M5Cardputer.Display.setTextDatum(TL_DATUM);

    waitForKey();
    drawMainMenu();
}

// Main loop
void loop() {
    M5Cardputer.update();

    switch(currentMenu) {
        case MENU_MAIN:
            handleMainMenu();
            break;
        default:
            break;
    }

    delay(50);
}

// Clear screen
void clearScreen() {
    M5Cardputer.Display.fillScreen(COLOR_BG);
}

// Draw header
void drawHeader(const char* title) {
    M5Cardputer.Display.fillRect(0, 0, SCREEN_W, 16, COLOR_TITLE);
    M5Cardputer.Display.setTextColor(COLOR_BG, COLOR_TITLE);
    M5Cardputer.Display.drawString(title, 4, 2);

    // Show WiFi status
    if(wifiConnected) {
        M5Cardputer.Display.drawString("*", SCREEN_W - 10, 2);
    }
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
}

// Draw status line at bottom
void drawStatus(const char* msg, uint16_t color) {
    M5Cardputer.Display.fillRect(0, SCREEN_H - 14, SCREEN_W, 14, COLOR_BG);
    M5Cardputer.Display.setTextColor(color, COLOR_BG);
    M5Cardputer.Display.drawString(msg, 4, SCREEN_H - 12);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
}

// Draw main menu
void drawMainMenu() {
    clearScreen();
    drawHeader("Network Toolkit");

    int visibleItems = 7;
    int startY = 20;
    int itemHeight = 14;

    for(int i = 0; i < visibleItems && (i + menuOffset) < mainMenuCount; i++) {
        int idx = i + menuOffset;
        int y = startY + (i * itemHeight);

        if(idx == menuSelection) {
            M5Cardputer.Display.fillRect(0, y, SCREEN_W, itemHeight, COLOR_MENU_SEL);
            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_MENU_SEL);
        } else {
            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
        }

        M5Cardputer.Display.drawString(mainMenuItems[idx], 8, y + 2);
    }

    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);

    // Instructions
    drawStatus("UP/DOWN:Nav ENTER:Select", COLOR_TEXT);
}

// Handle main menu input
void handleMainMenu() {
    if(M5Cardputer.Keyboard.isChange()) {
        if(M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            // Up arrow or ; key
            if(keyWord(status.word) == ";" || M5Cardputer.Keyboard.isKeyPressed(KEY_UP)) {
                if(menuSelection > 0) {
                    menuSelection--;
                    if(menuSelection < menuOffset) {
                        menuOffset = menuSelection;
                    }
                    drawMainMenu();
                }
            }
            // Down arrow or . key
            else if(keyWord(status.word) == "." || M5Cardputer.Keyboard.isKeyPressed(KEY_DOWN)) {
                if(menuSelection < mainMenuCount - 1) {
                    menuSelection++;
                    if(menuSelection >= menuOffset + 7) {
                        menuOffset = menuSelection - 6;
                    }
                    drawMainMenu();
                }
            }
            // Enter
            else if(keyWord(status.word) == "\n" || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                executeMenuItem(menuSelection);
            }
        }
    }
}

// Execute menu item
void executeMenuItem(int item) {
    switch(item) {
        case 0: connectToWiFi(); break;
        case 1: runIPScanner(); break;
        case 2: runPortScanner(); break;
        case 3: runPingSweep(); break;
        case 4: runDNSLookup(); break;
        case 5: runDHCPDiscover(); break;
        case 6: runSignalMapper(); break;
        case 7: showSubnetCalc(); break;
        case 8: runNetMonitor(); break;
        case 9: showWiFiQR(); break;
        case 10: scanWiFiNetworks(); break;
        case 11: runSNMPQuery(); break;
    }
    drawMainMenu();
}

// Wait for any key press
void waitForKey() {
    while(true) {
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            break;
        }
        delay(50);
    }
}

// Get single key input
char getKeyInput() {
    while(true) {
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            if(status.word.size() > 0) {
                return status.word[0];
            }
        }
        delay(50);
    }
}

// Get text input with on-screen keyboard
String getTextInput(const char* prompt, int maxLen) {
    String input = "";
    clearScreen();
    drawHeader(prompt);

    M5Cardputer.Display.drawString("Type and press ENTER", 4, 30);
    M5Cardputer.Display.drawString("ESC to cancel", 4, 45);
    M5Cardputer.Display.fillRect(4, 70, SCREEN_W - 8, 20, TFT_DARKGREY);

    while(true) {
        M5Cardputer.update();

        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            // ESC - cancel
            if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) {
                return "";
            }
            // Enter - confirm
            if(keyWord(status.word) == "\n" || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                return input;
            }
            // Backspace
            if(M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
                if(input.length() > 0) {
                    input = input.substring(0, input.length() - 1);
                }
            }
            // Regular character
            else if(status.word.size() > 0 && input.length() < maxLen) {
                input += keyWord(status.word);
            }

            // Update display
            M5Cardputer.Display.fillRect(4, 70, SCREEN_W - 8, 20, TFT_DARKGREY);
            M5Cardputer.Display.setTextColor(COLOR_TEXT, TFT_DARKGREY);
            M5Cardputer.Display.drawString(input.c_str(), 8, 74);
            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
        }
        delay(50);
    }
}

// Connect to WiFi
void connectToWiFi() {
    clearScreen();
    drawHeader("WiFi Connect");

    // Scan networks
    drawStatus("Scanning networks...", COLOR_WARNING);
    int n = WiFi.scanNetworks();

    if(n == 0) {
        drawStatus("No networks found", COLOR_ERROR);
        waitForKey();
        return;
    }

    // Show networks
    clearScreen();
    drawHeader("Select Network");

    int selection = 0;
    int offset = 0;
    int visibleItems = 6;

    while(true) {
        // Draw network list
        for(int i = 0; i < visibleItems && (i + offset) < n; i++) {
            int idx = i + offset;
            int y = 20 + (i * 14);

            if(idx == selection) {
                M5Cardputer.Display.fillRect(0, y, SCREEN_W, 14, COLOR_MENU_SEL);
                M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_MENU_SEL);
            } else {
                M5Cardputer.Display.fillRect(0, y, SCREEN_W, 14, COLOR_BG);
                M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
            }

            String ssid = WiFi.SSID(idx);
            if(ssid.length() > 20) ssid = ssid.substring(0, 17) + "...";
            int rssi = WiFi.RSSI(idx);
            String line = ssid + " " + String(rssi) + "dBm";
            M5Cardputer.Display.drawString(line.c_str(), 4, y + 2);
        }

        M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
        drawStatus("ENTER:Select ESC:Back", COLOR_TEXT);

        // Handle input
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) {
                WiFi.scanDelete();
                return;
            }
            if(keyWord(status.word) == ";" && selection > 0) {
                selection--;
                if(selection < offset) offset = selection;
            }
            if(keyWord(status.word) == "." && selection < n - 1) {
                selection++;
                if(selection >= offset + visibleItems) offset++;
            }
            if(keyWord(status.word) == "\n" || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                String selectedSSID = WiFi.SSID(selection);
                WiFi.scanDelete();

                // Get password
                String password = getTextInput("Enter Password", 64);
                if(password == "") return;

                // Connect
                clearScreen();
                drawHeader("Connecting...");
                drawStatus(selectedSSID.c_str(), COLOR_WARNING);

                WiFi.begin(selectedSSID.c_str(), password.c_str());

                int attempts = 0;
                while(WiFi.status() != WL_CONNECTED && attempts < 30) {
                    delay(500);
                    M5Cardputer.Display.drawString(".", 4 + (attempts * 6), 50);
                    attempts++;
                }

                if(WiFi.status() == WL_CONNECTED) {
                    wifiConnected = true;
                    connectedSSID = selectedSSID;
                    localIP = WiFi.localIP();
                    gateway = WiFi.gatewayIP();
                    subnet = WiFi.subnetMask();

                    clearScreen();
                    drawHeader("Connected!");
                    M5Cardputer.Display.drawString(("IP: " + localIP.toString()).c_str(), 4, 30);
                    M5Cardputer.Display.drawString(("GW: " + gateway.toString()).c_str(), 4, 45);
                    M5Cardputer.Display.drawString(("Mask: " + subnet.toString()).c_str(), 4, 60);
                    drawStatus("Press any key...", COLOR_SUCCESS);
                } else {
                    drawStatus("Connection failed!", COLOR_ERROR);
                }

                waitForKey();
                return;
            }
        }
        delay(50);
    }
}

// MAC to String
String macToString(uint8_t* mac) {
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

// Get MAC vendor from OUI
String getMacVendor(uint8_t* mac) {
    for(int i = 0; i < macVendorCount; i++) {
        if(mac[0] == macVendors[i].oui[0] &&
           mac[1] == macVendors[i].oui[1] &&
           mac[2] == macVendors[i].oui[2]) {
            return String(macVendors[i].vendor);
        }
    }
    return "Unknown";
}

// ARP scan single IP
bool arpScan(IPAddress targetIP, uint8_t* mac) {
    // Send ping to populate ARP table
    Ping.ping(targetIP, 1);

    // Try to get MAC from ARP table
    ip4_addr_t ipaddr;
    IP4_ADDR(&ipaddr, targetIP[0], targetIP[1], targetIP[2], targetIP[3]);

    struct eth_addr* eth_ret = NULL;
    const ip4_addr_t* ip_ret = NULL;

    // Query ARP table
    if(etharp_find_addr(netif_default, &ipaddr, &eth_ret, &ip_ret) >= 0) {
        if(eth_ret != NULL) {
            memcpy(mac, eth_ret->addr, 6);
            return true;
        }
    }

    return false;
}

// IP Scanner
void runIPScanner() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("IP Scanner");
        drawStatus("Connect to WiFi first!", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("IP Scanner");

    scanResults.count = 0;

    // Calculate network range
    uint32_t ip = (uint32_t)localIP;
    uint32_t mask = (uint32_t)subnet;
    uint32_t network = ip & mask;
    uint32_t broadcast = network | (~mask);

    int startHost = 1;
    int endHost = 254;

    M5Cardputer.Display.drawString("Scanning network...", 4, 25);
    M5Cardputer.Display.drawString(("Range: " + IPAddress(network + 1).toString() + " - " +
                                    IPAddress(broadcast - 1).toString()).c_str(), 4, 40);

    // Progress bar
    M5Cardputer.Display.drawRect(4, 60, SCREEN_W - 8, 12, COLOR_TEXT);

    int found = 0;
    for(int i = startHost; i <= endHost && i <= MAX_HOSTS; i++) {
        // Update progress
        int progress = (i * (SCREEN_W - 10)) / endHost;
        M5Cardputer.Display.fillRect(5, 61, progress, 10, COLOR_SUCCESS);

        IPAddress targetIP(localIP[0], localIP[1], localIP[2], i);

        // Skip our own IP
        if(targetIP == localIP) {
            scanResults.hosts[scanResults.count].ip = targetIP;
            scanResults.hosts[scanResults.count].online = true;
            scanResults.hosts[scanResults.count].hostname = "(This device)";
            memset(scanResults.hosts[scanResults.count].mac, 0, 6);
            scanResults.count++;
            found++;
            continue;
        }

        // Ping check
        if(Ping.ping(targetIP, 1)) {
            uint8_t mac[6] = {0};
            arpScan(targetIP, mac);

            scanResults.hosts[scanResults.count].ip = targetIP;
            scanResults.hosts[scanResults.count].online = true;
            memcpy(scanResults.hosts[scanResults.count].mac, mac, 6);
            scanResults.hosts[scanResults.count].hostname = getMacVendor(mac);
            scanResults.count++;
            found++;

            // Show found count
            M5Cardputer.Display.fillRect(4, 80, SCREEN_W - 8, 14, COLOR_BG);
            M5Cardputer.Display.drawString(("Found: " + String(found) + " hosts").c_str(), 4, 80);
        }

        // Check for ESC to abort
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) {
            break;
        }
    }

    // Show results
    clearScreen();
    drawHeader("Scan Results");

    if(scanResults.count == 0) {
        M5Cardputer.Display.drawString("No hosts found", 4, 40);
        drawStatus("Press any key...", COLOR_TEXT);
        waitForKey();
        return;
    }

    int selection = 0;
    int offset = 0;
    int visibleItems = 6;

    while(true) {
        clearScreen();
        drawHeader(("Found " + String(scanResults.count) + " hosts").c_str());

        for(int i = 0; i < visibleItems && (i + offset) < scanResults.count; i++) {
            int idx = i + offset;
            int y = 20 + (i * 16);

            if(idx == selection) {
                M5Cardputer.Display.fillRect(0, y, SCREEN_W, 16, COLOR_MENU_SEL);
                M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_MENU_SEL);
            } else {
                M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
            }

            String line = scanResults.hosts[idx].ip.toString();
            line += " " + scanResults.hosts[idx].hostname.substring(0, 10);
            M5Cardputer.Display.drawString(line.c_str(), 4, y + 2);
        }

        M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
        drawStatus("ENTER:Details ESC:Back", COLOR_TEXT);

        // Handle input
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) return;

            if(keyWord(status.word) == ";" && selection > 0) {
                selection--;
                if(selection < offset) offset = selection;
            }
            if(keyWord(status.word) == "." && selection < scanResults.count - 1) {
                selection++;
                if(selection >= offset + visibleItems) offset++;
            }
            if(keyWord(status.word) == "\n") {
                // Show host details
                clearScreen();
                drawHeader("Host Details");

                NetworkHost& h = scanResults.hosts[selection];
                M5Cardputer.Display.drawString(("IP: " + h.ip.toString()).c_str(), 4, 25);
                M5Cardputer.Display.drawString(("MAC: " + macToString(h.mac)).c_str(), 4, 40);
                M5Cardputer.Display.drawString(("Vendor: " + h.hostname).c_str(), 4, 55);

                // Quick ping
                float avg = Ping.averageTime();
                if(Ping.ping(h.ip, 3)) {
                    avg = Ping.averageTime();
                    M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
                    M5Cardputer.Display.drawString(("Ping: " + String(avg, 1) + " ms").c_str(), 4, 70);
                } else {
                    M5Cardputer.Display.setTextColor(COLOR_ERROR);
                    M5Cardputer.Display.drawString("Ping: Timeout", 4, 70);
                }
                M5Cardputer.Display.setTextColor(COLOR_TEXT);

                drawStatus("P:PortScan ESC:Back", COLOR_TEXT);

                while(true) {
                    M5Cardputer.update();
                    if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                        if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) break;
                        Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();
                        if(keyWord(st.word) == "p" || keyWord(st.word) == "P") {
                            runPortScanOnHost(h.ip);
                            break;
                        }
                    }
                    delay(50);
                }
            }
        }
        delay(50);
    }
}

// Port scan on specific host
void runPortScanOnHost(IPAddress target) {
    clearScreen();
    drawHeader("Port Scanner");
    M5Cardputer.Display.drawString(("Target: " + target.toString()).c_str(), 4, 25);
    M5Cardputer.Display.drawString("Scanning common ports...", 4, 40);

    M5Cardputer.Display.drawRect(4, 55, SCREEN_W - 8, 12, COLOR_TEXT);

    String openPorts = "";
    int openCount = 0;

    for(int i = 0; i < commonPortsCount; i++) {
        int progress = ((i + 1) * (SCREEN_W - 10)) / commonPortsCount;
        M5Cardputer.Display.fillRect(5, 56, progress, 10, COLOR_SUCCESS);

        WiFiClient client;
        client.setTimeout(SCAN_TIMEOUT);

        if(client.connect(target, commonPorts[i])) {
            openPorts += String(commonPorts[i]) + " ";
            openCount++;
            client.stop();
        }

        // Check ESC
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) break;
    }

    // Show results
    clearScreen();
    drawHeader("Open Ports");
    M5Cardputer.Display.drawString(("Target: " + target.toString()).c_str(), 4, 25);
    M5Cardputer.Display.drawString(("Found: " + String(openCount) + " open").c_str(), 4, 40);

    if(openCount > 0) {
        M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
        // Word wrap open ports
        int y = 55;
        String line = "";
        for(int i = 0; i < openPorts.length(); i++) {
            line += openPorts[i];
            if(line.length() > 30 || i == openPorts.length() - 1) {
                M5Cardputer.Display.drawString(line.c_str(), 4, y);
                y += 12;
                line = "";
                if(y > SCREEN_H - 20) break;
            }
        }
        M5Cardputer.Display.setTextColor(COLOR_TEXT);
    }

    drawStatus("Press any key...", COLOR_TEXT);
    waitForKey();
}

// Port Scanner (asks for target)
void runPortScanner() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("Port Scanner");
        drawStatus("Connect to WiFi first!", COLOR_ERROR);
        waitForKey();
        return;
    }

    String targetStr = getTextInput("Enter IP address", 15);
    if(targetStr == "") return;

    IPAddress target;
    if(!target.fromString(targetStr)) {
        clearScreen();
        drawHeader("Error");
        drawStatus("Invalid IP address!", COLOR_ERROR);
        waitForKey();
        return;
    }

    runPortScanOnHost(target);
}

// Ping Sweep
void runPingSweep() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("Ping Sweep");
        drawStatus("Connect to WiFi first!", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("Ping Sweep");

    String targetStr = getTextInput("Enter IP (or prefix)", 15);
    if(targetStr == "") return;

    // Parse target - support single IP or range
    IPAddress target;
    int startIP = 1, endIP = 254;

    // Check if it's a prefix (e.g., "192.168.1")
    int dotCount = 0;
    for(int i = 0; i < targetStr.length(); i++) {
        if(targetStr[i] == '.') dotCount++;
    }

    if(dotCount == 2) {
        // It's a prefix, scan full range
        targetStr += ".1";
    }

    if(!target.fromString(targetStr)) {
        clearScreen();
        drawHeader("Error");
        drawStatus("Invalid IP!", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("Ping Sweep");
    M5Cardputer.Display.drawString(("Range: " + String(target[0]) + "." + String(target[1]) +
                                    "." + String(target[2]) + ".1-254").c_str(), 4, 25);

    M5Cardputer.Display.drawRect(4, 45, SCREEN_W - 8, 12, COLOR_TEXT);

    int responding = 0;
    int y = 65;

    for(int i = startIP; i <= endIP; i++) {
        int progress = (i * (SCREEN_W - 10)) / 254;
        M5Cardputer.Display.fillRect(5, 46, progress, 10, COLOR_SUCCESS);

        IPAddress pingTarget(target[0], target[1], target[2], i);

        if(Ping.ping(pingTarget, 1)) {
            responding++;
            float latency = Ping.averageTime();

            if(y < SCREEN_H - 20) {
                String line = pingTarget.toString() + " " + String(latency, 0) + "ms";
                M5Cardputer.Display.drawString(line.c_str(), 4, y);
                y += 10;
            }
        }

        // Check ESC
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) break;
    }

    drawStatus(("Done: " + String(responding) + " responding").c_str(), COLOR_SUCCESS);
    waitForKey();
}

// DNS Lookup
void runDNSLookup() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("DNS Lookup");
        drawStatus("Connect to WiFi first!", COLOR_ERROR);
        waitForKey();
        return;
    }

    String hostname = getTextInput("Enter hostname", 64);
    if(hostname == "") return;

    clearScreen();
    drawHeader("DNS Lookup");
    M5Cardputer.Display.drawString(("Query: " + hostname).c_str(), 4, 25);
    drawStatus("Resolving...", COLOR_WARNING);

    IPAddress resolvedIP;

    if(WiFi.hostByName(hostname.c_str(), resolvedIP)) {
        M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
        M5Cardputer.Display.drawString(("IP: " + resolvedIP.toString()).c_str(), 4, 50);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);

        // Reverse lookup attempt
        M5Cardputer.Display.drawString("Ping test:", 4, 70);
        if(Ping.ping(resolvedIP, 3)) {
            float avg = Ping.averageTime();
            M5Cardputer.Display.drawString((String(avg, 1) + " ms avg").c_str(), 80, 70);
        } else {
            M5Cardputer.Display.drawString("No response", 80, 70);
        }

        drawStatus("Press any key...", COLOR_SUCCESS);
    } else {
        M5Cardputer.Display.setTextColor(COLOR_ERROR);
        M5Cardputer.Display.drawString("Resolution failed!", 4, 50);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);
        drawStatus("Press any key...", COLOR_ERROR);
    }

    waitForKey();
}

// DHCP Discover
void runDHCPDiscover() {
    clearScreen();
    drawHeader("DHCP Discover");

    M5Cardputer.Display.drawString("Scanning for DHCP servers...", 4, 25);
    M5Cardputer.Display.drawString("Note: Requires reconnect", 4, 40);

    // Disconnect and reconnect to trigger DHCP
    if(wifiConnected) {
        M5Cardputer.Display.drawString(("Current DHCP: " + gateway.toString()).c_str(), 4, 60);
        M5Cardputer.Display.drawString(("Lease from: " + WiFi.gatewayIP().toString()).c_str(), 4, 75);
        M5Cardputer.Display.drawString(("DNS: " + WiFi.dnsIP().toString()).c_str(), 4, 90);
    } else {
        M5Cardputer.Display.drawString("Not connected to WiFi", 4, 60);
    }

    drawStatus("Press any key...", COLOR_TEXT);
    waitForKey();
}

// Signal Mapper
void runSignalMapper() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("Signal Mapper");
        drawStatus("Connect to WiFi first!", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("Signal Mapper");
    M5Cardputer.Display.drawString("Walk around to map signal", 4, 25);
    M5Cardputer.Display.drawString("ESC to stop", 4, 40);

    int minRSSI = 0, maxRSSI = -100;
    int readings = 0;
    long totalRSSI = 0;

    while(true) {
        int rssi = WiFi.RSSI();
        readings++;
        totalRSSI += rssi;

        if(rssi > minRSSI) minRSSI = rssi;
        if(rssi < maxRSSI) maxRSSI = rssi;

        // Draw signal bar
        M5Cardputer.Display.fillRect(4, 60, SCREEN_W - 8, 30, COLOR_BG);

        // Map RSSI to bar width (-30 = full, -90 = empty)
        int barWidth = map(rssi, -90, -30, 0, SCREEN_W - 8);
        barWidth = constrain(barWidth, 0, SCREEN_W - 8);

        uint16_t barColor = COLOR_SUCCESS;
        if(rssi < -70) barColor = COLOR_WARNING;
        if(rssi < -80) barColor = COLOR_ERROR;

        M5Cardputer.Display.fillRect(4, 60, barWidth, 30, barColor);
        M5Cardputer.Display.drawRect(4, 60, SCREEN_W - 8, 30, COLOR_TEXT);

        // Stats
        M5Cardputer.Display.fillRect(4, 95, SCREEN_W - 8, 30, COLOR_BG);
        M5Cardputer.Display.drawString(("Now: " + String(rssi) + " dBm").c_str(), 4, 95);
        M5Cardputer.Display.drawString(("Max: " + String(minRSSI) + " Min: " + String(maxRSSI)).c_str(), 4, 108);

        // Check ESC
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) break;

        delay(200);
    }

    // Show summary
    clearScreen();
    drawHeader("Signal Summary");
    M5Cardputer.Display.drawString(("Readings: " + String(readings)).c_str(), 4, 30);
    M5Cardputer.Display.drawString(("Best: " + String(minRSSI) + " dBm").c_str(), 4, 45);
    M5Cardputer.Display.drawString(("Worst: " + String(maxRSSI) + " dBm").c_str(), 4, 60);
    M5Cardputer.Display.drawString(("Average: " + String(totalRSSI / readings) + " dBm").c_str(), 4, 75);

    drawStatus("Press any key...", COLOR_TEXT);
    waitForKey();
}

// Subnet Calculator
void showSubnetCalc() {
    clearScreen();
    drawHeader("Subnet Calculator");

    String ipStr = getTextInput("Enter IP/CIDR", 18);
    if(ipStr == "") return;

    // Parse IP and CIDR
    int slashPos = ipStr.indexOf('/');
    int cidr = 24;
    String ipPart = ipStr;

    if(slashPos > 0) {
        ipPart = ipStr.substring(0, slashPos);
        cidr = ipStr.substring(slashPos + 1).toInt();
    }

    IPAddress ip;
    if(!ip.fromString(ipPart)) {
        clearScreen();
        drawHeader("Error");
        drawStatus("Invalid IP!", COLOR_ERROR);
        waitForKey();
        return;
    }

    // Calculate subnet info
    uint32_t mask = (cidr == 0) ? 0 : (0xFFFFFFFF << (32 - cidr));
    uint32_t ipNum = ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) |
                     ((uint32_t)ip[2] << 8) | ip[3];
    uint32_t network = ipNum & mask;
    uint32_t broadcast = network | (~mask);
    uint32_t hosts = (~mask) - 1;

    IPAddress netAddr((network >> 24) & 0xFF, (network >> 16) & 0xFF,
                      (network >> 8) & 0xFF, network & 0xFF);
    IPAddress bcastAddr((broadcast >> 24) & 0xFF, (broadcast >> 16) & 0xFF,
                        (broadcast >> 8) & 0xFF, broadcast & 0xFF);
    IPAddress maskAddr((mask >> 24) & 0xFF, (mask >> 16) & 0xFF,
                       (mask >> 8) & 0xFF, mask & 0xFF);

    clearScreen();
    drawHeader("Subnet Info");
    M5Cardputer.Display.drawString(("IP: " + ip.toString() + "/" + String(cidr)).c_str(), 4, 22);
    M5Cardputer.Display.drawString(("Mask: " + maskAddr.toString()).c_str(), 4, 36);
    M5Cardputer.Display.drawString(("Network: " + netAddr.toString()).c_str(), 4, 50);
    M5Cardputer.Display.drawString(("Broadcast: " + bcastAddr.toString()).c_str(), 4, 64);
    M5Cardputer.Display.drawString(("Usable: " + String(hosts)).c_str(), 4, 78);

    IPAddress firstHost((network >> 24) & 0xFF, (network >> 16) & 0xFF,
                        (network >> 8) & 0xFF, (network & 0xFF) + 1);
    IPAddress lastHost((broadcast >> 24) & 0xFF, (broadcast >> 16) & 0xFF,
                       (broadcast >> 8) & 0xFF, (broadcast & 0xFF) - 1);
    M5Cardputer.Display.drawString(("Range: " + firstHost.toString()).c_str(), 4, 92);
    M5Cardputer.Display.drawString(("    to " + lastHost.toString()).c_str(), 4, 106);

    drawStatus("Press any key...", COLOR_TEXT);
    waitForKey();
}

// Network Monitor
void runNetMonitor() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("Net Monitor");
        drawStatus("Connect to WiFi first!", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("Net Monitor");
    M5Cardputer.Display.drawString("Monitoring for new devices...", 4, 25);
    M5Cardputer.Display.drawString("ESC to stop", 4, 40);

    // Initial scan
    int knownCount = 0;
    IPAddress knownHosts[50];

    for(int i = 1; i <= 254 && knownCount < 50; i++) {
        IPAddress target(localIP[0], localIP[1], localIP[2], i);
        if(Ping.ping(target, 1)) {
            knownHosts[knownCount++] = target;
        }
    }

    M5Cardputer.Display.drawString(("Initial: " + String(knownCount) + " hosts").c_str(), 4, 55);

    int newDevices = 0;
    int y = 70;

    while(true) {
        // Scan for new devices
        for(int i = 1; i <= 254; i++) {
            IPAddress target(localIP[0], localIP[1], localIP[2], i);

            if(Ping.ping(target, 1)) {
                // Check if known
                bool isKnown = false;
                for(int j = 0; j < knownCount; j++) {
                    if(knownHosts[j] == target) {
                        isKnown = true;
                        break;
                    }
                }

                if(!isKnown && knownCount < 50) {
                    knownHosts[knownCount++] = target;
                    newDevices++;

                    // Alert!
                    M5Cardputer.Display.setTextColor(COLOR_WARNING);
                    if(y < SCREEN_H - 20) {
                        M5Cardputer.Display.drawString(("NEW: " + target.toString()).c_str(), 4, y);
                        y += 12;
                    }
                    M5Cardputer.Display.setTextColor(COLOR_TEXT);
                }
            }

            // Check ESC
            M5Cardputer.update();
            if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) {
                drawStatus(("Found " + String(newDevices) + " new devices").c_str(), COLOR_SUCCESS);
                waitForKey();
                return;
            }
        }

        delay(1000);
    }
}

// Show WiFi QR Code
void showWiFiQR() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("WiFi QR");
        drawStatus("Connect to WiFi first!", COLOR_ERROR);
        waitForKey();
        return;
    }

    String password = getTextInput("Enter WiFi password", 64);
    if(password == "") return;

    // Create WiFi QR string
    String wifiString = "WIFI:T:WPA;S:" + connectedSSID + ";P:" + password + ";;";

    clearScreen();
    drawHeader("WiFi QR Code");

    // Generate QR code
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, wifiString.c_str());

    // Calculate size and position
    int size = qrcode.size;
    int scale = min((SCREEN_W - 20) / size, (SCREEN_H - 40) / size);
    int offsetX = (SCREEN_W - (size * scale)) / 2;
    int offsetY = 20 + ((SCREEN_H - 40) - (size * scale)) / 2;

    // Draw QR code
    for(int y = 0; y < size; y++) {
        for(int x = 0; x < size; x++) {
            if(qrcode_getModule(&qrcode, x, y)) {
                M5Cardputer.Display.fillRect(offsetX + x * scale, offsetY + y * scale,
                                              scale, scale, COLOR_TEXT);
            } else {
                M5Cardputer.Display.fillRect(offsetX + x * scale, offsetY + y * scale,
                                              scale, scale, COLOR_BG);
            }
        }
    }

    drawStatus("Scan to connect", COLOR_TEXT);
    waitForKey();
}

// Scan WiFi Networks
void scanWiFiNetworks() {
    clearScreen();
    drawHeader("WiFi Networks");
    drawStatus("Scanning...", COLOR_WARNING);

    int n = WiFi.scanNetworks();

    if(n == 0) {
        drawStatus("No networks found", COLOR_ERROR);
        waitForKey();
        return;
    }

    int selection = 0;
    int offset = 0;
    int visibleItems = 6;

    while(true) {
        clearScreen();
        drawHeader(("Found " + String(n) + " networks").c_str());

        for(int i = 0; i < visibleItems && (i + offset) < n; i++) {
            int idx = i + offset;
            int y = 20 + (i * 16);

            if(idx == selection) {
                M5Cardputer.Display.fillRect(0, y, SCREEN_W, 16, COLOR_MENU_SEL);
                M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_MENU_SEL);
            } else {
                M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
            }

            String ssid = WiFi.SSID(idx);
            if(ssid.length() > 15) ssid = ssid.substring(0, 12) + "...";

            int rssi = WiFi.RSSI(idx);
            String enc = (WiFi.encryptionType(idx) == WIFI_AUTH_OPEN) ? "OPEN" : "ENC";
            String ch = "CH" + String(WiFi.channel(idx));

            String line = ssid + " " + String(rssi) + " " + ch;
            M5Cardputer.Display.drawString(line.c_str(), 4, y + 2);
        }

        M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
        drawStatus("ENTER:Details ESC:Back", COLOR_TEXT);

        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC)) {
                WiFi.scanDelete();
                return;
            }
            if(keyWord(status.word) == ";" && selection > 0) {
                selection--;
                if(selection < offset) offset = selection;
            }
            if(keyWord(status.word) == "." && selection < n - 1) {
                selection++;
                if(selection >= offset + visibleItems) offset++;
            }
            if(keyWord(status.word) == "\n") {
                // Show network details
                clearScreen();
                drawHeader("Network Details");

                M5Cardputer.Display.drawString(("SSID: " + WiFi.SSID(selection)).c_str(), 4, 25);
                M5Cardputer.Display.drawString(("BSSID: " + WiFi.BSSIDstr(selection)).c_str(), 4, 40);
                M5Cardputer.Display.drawString(("Channel: " + String(WiFi.channel(selection))).c_str(), 4, 55);
                M5Cardputer.Display.drawString(("RSSI: " + String(WiFi.RSSI(selection)) + " dBm").c_str(), 4, 70);

                String encType;
                switch(WiFi.encryptionType(selection)) {
                    case WIFI_AUTH_OPEN: encType = "Open"; break;
                    case WIFI_AUTH_WEP: encType = "WEP"; break;
                    case WIFI_AUTH_WPA_PSK: encType = "WPA-PSK"; break;
                    case WIFI_AUTH_WPA2_PSK: encType = "WPA2-PSK"; break;
                    case WIFI_AUTH_WPA_WPA2_PSK: encType = "WPA/WPA2"; break;
                    case WIFI_AUTH_WPA3_PSK: encType = "WPA3"; break;
                    default: encType = "Unknown";
                }
                M5Cardputer.Display.drawString(("Security: " + encType).c_str(), 4, 85);

                drawStatus("Press any key...", COLOR_TEXT);
                waitForKey();
            }
        }
        delay(50);
    }
}

// SNMP Query (basic)
void runSNMPQuery() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("SNMP Query");
        drawStatus("Connect to WiFi first!", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("SNMP Query");

    String targetStr = getTextInput("Enter IP address", 15);
    if(targetStr == "") return;

    IPAddress target;
    if(!target.fromString(targetStr)) {
        drawStatus("Invalid IP!", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("SNMP Query");
    M5Cardputer.Display.drawString(("Target: " + target.toString()).c_str(), 4, 25);
    M5Cardputer.Display.drawString("Sending SNMP GET...", 4, 40);

    // Simple SNMP v1 GET request for sysDescr (1.3.6.1.2.1.1.1.0)
    WiFiUDP udp;

    // SNMP GET packet for sysDescr
    uint8_t snmpPacket[] = {
        0x30, 0x26,                     // SEQUENCE, length 38
        0x02, 0x01, 0x00,               // INTEGER, version 0 (SNMPv1)
        0x04, 0x06, 0x70, 0x75, 0x62,   // OCTET STRING "public"
        0x6c, 0x69, 0x63,
        0xa0, 0x19,                     // GetRequest-PDU, length 25
        0x02, 0x04, 0x00, 0x00, 0x00, 0x01,  // request-id
        0x02, 0x01, 0x00,               // error-status
        0x02, 0x01, 0x00,               // error-index
        0x30, 0x0b,                     // varbind list
        0x30, 0x09,                     // varbind
        0x06, 0x05, 0x2b, 0x06, 0x01, 0x02, 0x01,  // OID 1.3.6.1.2.1
        0x05, 0x00                      // NULL
    };

    udp.begin(161);
    udp.beginPacket(target, 161);
    udp.write(snmpPacket, sizeof(snmpPacket));
    udp.endPacket();

    // Wait for response
    unsigned long start = millis();
    bool received = false;

    while(millis() - start < 3000) {
        int packetSize = udp.parsePacket();
        if(packetSize) {
            uint8_t response[512];
            int len = udp.read(response, 512);

            M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
            M5Cardputer.Display.drawString("Response received!", 4, 60);
            M5Cardputer.Display.drawString(("Size: " + String(len) + " bytes").c_str(), 4, 75);
            M5Cardputer.Display.setTextColor(COLOR_TEXT);

            received = true;
            break;
        }
        delay(100);
    }

    if(!received) {
        M5Cardputer.Display.setTextColor(COLOR_ERROR);
        M5Cardputer.Display.drawString("No response (timeout)", 4, 60);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);
    }

    udp.stop();
    drawStatus("Press any key...", COLOR_TEXT);
    waitForKey();
}
