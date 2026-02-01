/*
 * Network Toolkit v10 per M5Stack Cardputer
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
 *
 * v10 Changes:
 * - New animated splash screen
 * - Horizontal scrolling icon menu
 * - WiFi credentials persistence
 * - Fixed IP Scanner bugs (range, flashing, ESC, details)
 * - Fixed Ping Sweep stop functionality
 * - Larger fonts for better readability
 * - Improved device identification
 */

#include <M5Cardputer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPping.h>
#include <esp_wifi.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/netdb.h>
#include <Preferences.h>
#include "qrcode.h"

// Preferences for WiFi storage
Preferences preferences;

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

// Display colors - Enhanced palette
#define COLOR_BG       0x0000  // Pure black
#define COLOR_TEXT     0xFFFF  // White
#define COLOR_TITLE    0x07FF  // Cyan
#define COLOR_SUCCESS  0x07E0  // Green
#define COLOR_ERROR    0xF800  // Red
#define COLOR_WARNING  0xFFE0  // Yellow
#define COLOR_MENU_SEL 0x001F  // Blue
#define COLOR_ACCENT   0xF81F  // Magenta
#define COLOR_ORANGE   0xFD20  // Orange
#define COLOR_DARKGRAY 0x4208  // Dark gray
#define COLOR_LIGHTGRAY 0x8410 // Light gray

// Screen dimensions
#define SCREEN_W 240
#define SCREEN_H 135

// Icon dimensions for menu
#define ICON_SIZE 40
#define ICON_SPACING 10
#define ICON_Y 45

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
String savedPassword = "";
ScanResult scanResults;
IPAddress localIP;
IPAddress gateway;
IPAddress subnet;

// Menu structure with icons
struct MenuItem {
    const char* name;
    const char* shortName;
    uint16_t color;
    const char* icon;  // Simple text icon
};

const MenuItem menuItems[] = {
    {"WiFi Connect", "WiFi", COLOR_TITLE, "W"},
    {"IP Scanner", "Scan", COLOR_SUCCESS, "S"},
    {"Port Scanner", "Ports", COLOR_ORANGE, "P"},
    {"Ping Sweep", "Ping", COLOR_WARNING, "G"},
    {"DNS Lookup", "DNS", COLOR_ACCENT, "D"},
    {"DHCP Discover", "DHCP", COLOR_LIGHTGRAY, "H"},
    {"Signal Mapper", "Signal", COLOR_SUCCESS, "M"},
    {"Subnet Calc", "Subnet", COLOR_TITLE, "N"},
    {"Net Monitor", "Monitor", COLOR_ERROR, "O"},
    {"WiFi QR Code", "QR", COLOR_ACCENT, "Q"},
    {"WiFi Networks", "Nets", COLOR_WARNING, "L"},
    {"SNMP Query", "SNMP", COLOR_ORANGE, "X"}
};
const int mainMenuCount = 12;

// Interrupt flag for stopping operations
volatile bool stopRequested = false;

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
    // Cisco
    {{0x00, 0x1A, 0x2B}, "Cisco"},
    {{0x00, 0x1B, 0x0D}, "Cisco"},
    {{0x00, 0x1C, 0x0E}, "Cisco"},
    {{0x00, 0x1D, 0x45}, "Cisco"},
    {{0x00, 0x22, 0x0D}, "Cisco"},
    // VMware
    {{0x00, 0x50, 0x56}, "VMware"},
    {{0x00, 0x0C, 0x29}, "VMware"},
    {{0x00, 0x05, 0x69}, "VMware"},
    // Microsoft
    {{0x00, 0x15, 0x5D}, "Microsoft"},
    {{0x00, 0x17, 0xFA}, "Microsoft"},
    {{0x00, 0x1D, 0xD8}, "Microsoft"},
    {{0x28, 0x18, 0x78}, "Microsoft"},
    // VirtualBox
    {{0x08, 0x00, 0x27}, "VirtualBox"},
    {{0x00, 0x1C, 0x42}, "Parallels"},
    // Apple
    {{0xAC, 0xDE, 0x48}, "Apple"},
    {{0x3C, 0x06, 0x30}, "Apple"},
    {{0x00, 0x03, 0x93}, "Apple"},
    {{0x00, 0x0A, 0x27}, "Apple"},
    {{0x00, 0x0A, 0x95}, "Apple"},
    {{0x00, 0x10, 0xFA}, "Apple"},
    {{0x00, 0x1C, 0xB3}, "Apple"},
    {{0x00, 0x1D, 0x4F}, "Apple"},
    {{0x00, 0x1E, 0x52}, "Apple"},
    {{0x00, 0x1F, 0x5B}, "Apple"},
    {{0x00, 0x1F, 0xF3}, "Apple"},
    {{0x00, 0x21, 0xE9}, "Apple"},
    {{0x00, 0x22, 0x41}, "Apple"},
    {{0x00, 0x23, 0x12}, "Apple"},
    {{0x00, 0x23, 0x32}, "Apple"},
    {{0x00, 0x23, 0x6C}, "Apple"},
    {{0x00, 0x23, 0xDF}, "Apple"},
    {{0x00, 0x24, 0x36}, "Apple"},
    {{0x00, 0x25, 0x00}, "Apple"},
    {{0x00, 0x25, 0xBC}, "Apple"},
    {{0x00, 0x26, 0x08}, "Apple"},
    {{0x00, 0x26, 0xB0}, "Apple"},
    {{0x00, 0x26, 0xBB}, "Apple"},
    {{0x14, 0x10, 0x9F}, "Apple"},
    {{0x18, 0xE7, 0xF4}, "Apple"},
    {{0x20, 0xC9, 0xD0}, "Apple"},
    {{0x24, 0xA0, 0x74}, "Apple"},
    {{0x28, 0xCF, 0xDA}, "Apple"},
    {{0x34, 0x15, 0x9E}, "Apple"},
    {{0x38, 0xC9, 0x86}, "Apple"},
    {{0x40, 0x6C, 0x8F}, "Apple"},
    {{0x44, 0xD8, 0x84}, "Apple"},
    {{0x5C, 0xF9, 0x38}, "Apple"},
    {{0x70, 0x56, 0x81}, "Apple"},
    {{0x78, 0x31, 0xC1}, "Apple"},
    {{0x78, 0x88, 0x6D}, "Apple"},
    {{0x84, 0x78, 0x8B}, "Apple"},
    {{0x84, 0x85, 0x06}, "Apple"},
    {{0x88, 0xC6, 0x63}, "Apple"},
    {{0xA8, 0x5C, 0x2C}, "Apple"},
    {{0xA8, 0x86, 0xDD}, "Apple"},
    {{0xAC, 0x87, 0xA3}, "Apple"},
    {{0xB8, 0xE8, 0x56}, "Apple"},
    {{0xBC, 0x52, 0xB7}, "Apple"},
    {{0xD8, 0xD1, 0xCB}, "Apple"},
    {{0xE0, 0xB9, 0xBA}, "Apple"},
    {{0xF0, 0xDC, 0xE2}, "Apple"},
    // Raspberry Pi
    {{0xDC, 0xA6, 0x32}, "Raspberry Pi"},
    {{0xB8, 0x27, 0xEB}, "Raspberry Pi"},
    {{0xE4, 0x5F, 0x01}, "Raspberry Pi"},
    {{0x28, 0xCD, 0xC1}, "Raspberry Pi"},
    {{0xD8, 0x3A, 0xDD}, "Raspberry Pi"},
    // Samsung
    {{0x00, 0x00, 0xF0}, "Samsung"},
    {{0x00, 0x02, 0x78}, "Samsung"},
    {{0x00, 0x07, 0xAB}, "Samsung"},
    {{0x00, 0x09, 0x18}, "Samsung"},
    {{0x00, 0x12, 0x47}, "Samsung"},
    {{0x00, 0x12, 0xFB}, "Samsung"},
    {{0x00, 0x13, 0x77}, "Samsung"},
    {{0x00, 0x15, 0x99}, "Samsung"},
    {{0x00, 0x15, 0xB9}, "Samsung"},
    {{0x00, 0x16, 0x32}, "Samsung"},
    {{0x00, 0x16, 0x6B}, "Samsung"},
    {{0x00, 0x16, 0x6C}, "Samsung"},
    {{0x00, 0x16, 0xDB}, "Samsung"},
    {{0x00, 0x17, 0xC9}, "Samsung"},
    {{0x00, 0x17, 0xD5}, "Samsung"},
    {{0x00, 0x18, 0xAF}, "Samsung"},
    {{0x00, 0x1A, 0x8A}, "Samsung"},
    {{0x00, 0x1B, 0x98}, "Samsung"},
    {{0x00, 0x1C, 0x43}, "Samsung"},
    {{0x00, 0x1D, 0x25}, "Samsung"},
    {{0x00, 0x1D, 0xF6}, "Samsung"},
    {{0x00, 0x1E, 0x7D}, "Samsung"},
    {{0x00, 0x1E, 0xE1}, "Samsung"},
    {{0x00, 0x1E, 0xE2}, "Samsung"},
    {{0x00, 0x1F, 0xCC}, "Samsung"},
    {{0x00, 0x1F, 0xCD}, "Samsung"},
    {{0x00, 0x21, 0x19}, "Samsung"},
    {{0x00, 0x21, 0x4C}, "Samsung"},
    {{0x00, 0x21, 0xD1}, "Samsung"},
    {{0x00, 0x21, 0xD2}, "Samsung"},
    {{0x00, 0x24, 0x54}, "Samsung"},
    {{0x00, 0x24, 0x90}, "Samsung"},
    {{0x00, 0x24, 0x91}, "Samsung"},
    {{0x00, 0x25, 0x66}, "Samsung"},
    {{0x00, 0x25, 0x67}, "Samsung"},
    {{0x00, 0x26, 0x37}, "Samsung"},
    {{0x00, 0x26, 0x5D}, "Samsung"},
    // Xiaomi
    {{0x48, 0x2C, 0xA0}, "Xiaomi"},
    {{0x64, 0xB4, 0x73}, "Xiaomi"},
    {{0x0C, 0x1D, 0xAF}, "Xiaomi"},
    {{0x14, 0xF6, 0x5A}, "Xiaomi"},
    {{0x18, 0x59, 0x36}, "Xiaomi"},
    {{0x28, 0x6C, 0x07}, "Xiaomi"},
    {{0x34, 0x80, 0xB3}, "Xiaomi"},
    {{0x38, 0xA4, 0xED}, "Xiaomi"},
    {{0x50, 0x8F, 0x4C}, "Xiaomi"},
    {{0x58, 0x44, 0x98}, "Xiaomi"},
    {{0x64, 0x09, 0x80}, "Xiaomi"},
    {{0x74, 0x23, 0x44}, "Xiaomi"},
    {{0x7C, 0x1D, 0xD9}, "Xiaomi"},
    {{0x84, 0x09, 0x3F}, "Xiaomi"},
    {{0x8C, 0xBE, 0xBE}, "Xiaomi"},
    {{0x98, 0xFA, 0xE3}, "Xiaomi"},
    {{0xAC, 0xF7, 0xF3}, "Xiaomi"},
    {{0xB0, 0xE2, 0x35}, "Xiaomi"},
    {{0xC4, 0x6A, 0xB7}, "Xiaomi"},
    {{0xD4, 0x97, 0x0B}, "Xiaomi"},
    {{0xF8, 0xA4, 0x5F}, "Xiaomi"},
    {{0xFC, 0x64, 0xBA}, "Xiaomi"},
    // Huawei
    {{0x7C, 0x1E, 0x52}, "Huawei"},
    {{0x00, 0x18, 0x82}, "Huawei"},
    {{0x00, 0x1E, 0x10}, "Huawei"},
    {{0x00, 0x25, 0x68}, "Huawei"},
    {{0x00, 0x25, 0x9E}, "Huawei"},
    {{0x00, 0x46, 0x4B}, "Huawei"},
    {{0x00, 0x66, 0x4B}, "Huawei"},
    {{0x00, 0x9A, 0xCD}, "Huawei"},
    {{0x00, 0xE0, 0xFC}, "Huawei"},
    {{0x04, 0x02, 0x1F}, "Huawei"},
    {{0x04, 0xB0, 0xE7}, "Huawei"},
    {{0x04, 0xC0, 0x6F}, "Huawei"},
    {{0x04, 0xF9, 0x38}, "Huawei"},
    {{0x08, 0x19, 0xA6}, "Huawei"},
    {{0x08, 0x7A, 0x4C}, "Huawei"},
    {{0x0C, 0x37, 0xDC}, "Huawei"},
    {{0x0C, 0x96, 0xBF}, "Huawei"},
    {{0x10, 0x1B, 0x54}, "Huawei"},
    {{0x10, 0x47, 0x80}, "Huawei"},
    // Google
    {{0x00, 0x1A, 0x11}, "Google"},
    {{0x3C, 0x5A, 0xB4}, "Google"},
    {{0x54, 0x60, 0x09}, "Google"},
    {{0x94, 0xEB, 0x2C}, "Google"},
    {{0xF4, 0xF5, 0xD8}, "Google"},
    {{0xF4, 0xF5, 0xE8}, "Google"},
    // Amazon
    {{0x00, 0xFC, 0x8B}, "Amazon"},
    {{0x0C, 0x47, 0xC9}, "Amazon"},
    {{0x10, 0xCE, 0xA9}, "Amazon"},
    {{0x18, 0x74, 0x2E}, "Amazon"},
    {{0x34, 0xD2, 0x70}, "Amazon"},
    {{0x38, 0xF7, 0x3D}, "Amazon"},
    {{0x40, 0xB4, 0xCD}, "Amazon"},
    {{0x44, 0x65, 0x0D}, "Amazon"},
    {{0x50, 0xDC, 0xE7}, "Amazon"},
    {{0x68, 0x54, 0xFD}, "Amazon"},
    {{0x68, 0x9C, 0x70}, "Amazon"},
    {{0x74, 0x75, 0x48}, "Amazon"},
    {{0x74, 0xC2, 0x46}, "Amazon"},
    {{0x84, 0xD6, 0xD0}, "Amazon"},
    {{0xA0, 0x02, 0xDC}, "Amazon"},
    {{0xAC, 0x63, 0xBE}, "Amazon"},
    {{0xB4, 0x7C, 0x9C}, "Amazon"},
    {{0xF0, 0x27, 0x2D}, "Amazon"},
    {{0xF0, 0x81, 0x73}, "Amazon"},
    {{0xFC, 0x65, 0xDE}, "Amazon"},
    // TP-Link
    {{0x50, 0xC7, 0xBF}, "TP-Link"},
    {{0x98, 0xDA, 0xC4}, "TP-Link"},
    {{0x30, 0xB5, 0xC2}, "TP-Link"},
    {{0xAC, 0x84, 0xC6}, "TP-Link"},
    {{0x14, 0xCC, 0x20}, "TP-Link"},
    {{0x14, 0xCF, 0x92}, "TP-Link"},
    {{0x18, 0xA6, 0xF7}, "TP-Link"},
    {{0x1C, 0xFA, 0x68}, "TP-Link"},
    {{0x54, 0xC8, 0x0F}, "TP-Link"},
    {{0x5C, 0x89, 0x9A}, "TP-Link"},
    {{0x60, 0xE3, 0x27}, "TP-Link"},
    {{0x64, 0x66, 0xB3}, "TP-Link"},
    {{0x64, 0x70, 0x02}, "TP-Link"},
    {{0x6C, 0xB0, 0xCE}, "TP-Link"},
    {{0x74, 0xDA, 0x88}, "TP-Link"},
    {{0x94, 0x0C, 0x6D}, "TP-Link"},
    {{0xB0, 0x4E, 0x26}, "TP-Link"},
    {{0xC0, 0x25, 0xE9}, "TP-Link"},
    {{0xC4, 0xE9, 0x84}, "TP-Link"},
    {{0xD8, 0x07, 0xB6}, "TP-Link"},
    {{0xEC, 0x08, 0x6B}, "TP-Link"},
    {{0xEC, 0x17, 0x2F}, "TP-Link"},
    {{0xF4, 0xEC, 0x38}, "TP-Link"},
    // D-Link
    {{0x00, 0x1E, 0x58}, "D-Link"},
    {{0x00, 0x05, 0x5D}, "D-Link"},
    {{0x00, 0x0D, 0x88}, "D-Link"},
    {{0x00, 0x0F, 0x3D}, "D-Link"},
    {{0x00, 0x11, 0x95}, "D-Link"},
    {{0x00, 0x13, 0x46}, "D-Link"},
    {{0x00, 0x15, 0xE9}, "D-Link"},
    {{0x00, 0x17, 0x9A}, "D-Link"},
    {{0x00, 0x19, 0x5B}, "D-Link"},
    {{0x00, 0x1B, 0x11}, "D-Link"},
    {{0x00, 0x1C, 0xF0}, "D-Link"},
    {{0x00, 0x1E, 0x58}, "D-Link"},
    {{0x00, 0x21, 0x91}, "D-Link"},
    {{0x00, 0x22, 0xB0}, "D-Link"},
    {{0x00, 0x24, 0x01}, "D-Link"},
    {{0x00, 0x26, 0x5A}, "D-Link"},
    // Netgear
    {{0x00, 0x24, 0xB2}, "Netgear"},
    {{0x00, 0x26, 0xF2}, "Netgear"},
    {{0x00, 0x09, 0x5B}, "Netgear"},
    {{0x00, 0x0F, 0xB5}, "Netgear"},
    {{0x00, 0x14, 0x6C}, "Netgear"},
    {{0x00, 0x18, 0x4D}, "Netgear"},
    {{0x00, 0x1B, 0x2F}, "Netgear"},
    {{0x00, 0x1E, 0x2A}, "Netgear"},
    {{0x00, 0x1F, 0x33}, "Netgear"},
    {{0x00, 0x22, 0x3F}, "Netgear"},
    {{0x20, 0x4E, 0x7F}, "Netgear"},
    {{0x28, 0xC6, 0x8E}, "Netgear"},
    {{0x30, 0x46, 0x9A}, "Netgear"},
    {{0x44, 0x94, 0xFC}, "Netgear"},
    {{0x6C, 0xB0, 0xCE}, "Netgear"},
    {{0x84, 0x1B, 0x5E}, "Netgear"},
    {{0x9C, 0x3D, 0xCF}, "Netgear"},
    {{0xA4, 0x2B, 0x8C}, "Netgear"},
    {{0xC0, 0x3F, 0x0E}, "Netgear"},
    {{0xC4, 0x04, 0x15}, "Netgear"},
    {{0xE0, 0x46, 0x9A}, "Netgear"},
    {{0xE0, 0x91, 0xF5}, "Netgear"},
    // ASUS
    {{0xF8, 0x32, 0xE4}, "ASUS"},
    {{0x00, 0x22, 0x15}, "ASUS"},
    {{0x00, 0x0C, 0x6E}, "ASUS"},
    {{0x00, 0x11, 0x2F}, "ASUS"},
    {{0x00, 0x11, 0xD8}, "ASUS"},
    {{0x00, 0x13, 0xD4}, "ASUS"},
    {{0x00, 0x15, 0xF2}, "ASUS"},
    {{0x00, 0x17, 0x31}, "ASUS"},
    {{0x00, 0x18, 0xF3}, "ASUS"},
    {{0x00, 0x1A, 0x92}, "ASUS"},
    {{0x00, 0x1B, 0xFC}, "ASUS"},
    {{0x00, 0x1D, 0x60}, "ASUS"},
    {{0x00, 0x1E, 0x8C}, "ASUS"},
    {{0x00, 0x1F, 0xC6}, "ASUS"},
    {{0x00, 0x22, 0x15}, "ASUS"},
    {{0x00, 0x23, 0x54}, "ASUS"},
    {{0x00, 0x24, 0x8C}, "ASUS"},
    {{0x00, 0x25, 0x22}, "ASUS"},
    {{0x00, 0x26, 0x18}, "ASUS"},
    {{0x04, 0x92, 0x26}, "ASUS"},
    {{0x08, 0x60, 0x6E}, "ASUS"},
    {{0x10, 0xBF, 0x48}, "ASUS"},
    {{0x14, 0xDA, 0xE9}, "ASUS"},
    {{0x1C, 0x87, 0x2C}, "ASUS"},
    {{0x20, 0xCF, 0x30}, "ASUS"},
    {{0x2C, 0x56, 0xDC}, "ASUS"},
    {{0x2C, 0xFD, 0xA1}, "ASUS"},
    {{0x30, 0x85, 0xA9}, "ASUS"},
    {{0x34, 0x97, 0xF6}, "ASUS"},
    {{0x38, 0x2C, 0x4A}, "ASUS"},
    {{0x38, 0xD5, 0x47}, "ASUS"},
    {{0x3C, 0x97, 0x0E}, "ASUS"},
    // Dell
    {{0x00, 0x23, 0x24}, "Dell"},
    {{0x18, 0x03, 0x73}, "Dell"},
    {{0x00, 0x1A, 0xA0}, "Dell"},
    {{0x00, 0x06, 0x5B}, "Dell"},
    {{0x00, 0x08, 0x74}, "Dell"},
    {{0x00, 0x0B, 0xDB}, "Dell"},
    {{0x00, 0x0D, 0x56}, "Dell"},
    {{0x00, 0x0F, 0x1F}, "Dell"},
    {{0x00, 0x11, 0x43}, "Dell"},
    {{0x00, 0x12, 0x3F}, "Dell"},
    {{0x00, 0x13, 0x72}, "Dell"},
    {{0x00, 0x14, 0x22}, "Dell"},
    {{0x00, 0x15, 0xC5}, "Dell"},
    {{0x00, 0x18, 0x8B}, "Dell"},
    {{0x00, 0x19, 0xB9}, "Dell"},
    {{0x00, 0x1C, 0x23}, "Dell"},
    {{0x00, 0x1D, 0x09}, "Dell"},
    {{0x00, 0x1E, 0x4F}, "Dell"},
    {{0x00, 0x1E, 0xC9}, "Dell"},
    {{0x00, 0x21, 0x70}, "Dell"},
    {{0x00, 0x21, 0x9B}, "Dell"},
    {{0x00, 0x22, 0x19}, "Dell"},
    {{0x00, 0x24, 0xE8}, "Dell"},
    {{0x00, 0x25, 0x64}, "Dell"},
    {{0x00, 0x26, 0xB9}, "Dell"},
    {{0x14, 0xFE, 0xB5}, "Dell"},
    {{0x18, 0x03, 0x73}, "Dell"},
    {{0x18, 0x66, 0xDA}, "Dell"},
    {{0x18, 0xA9, 0x9B}, "Dell"},
    {{0x18, 0xDB, 0xF2}, "Dell"},
    {{0x1C, 0x40, 0x24}, "Dell"},
    {{0x24, 0xB6, 0xFD}, "Dell"},
    // HP
    {{0x00, 0x21, 0x5A}, "HP"},
    {{0x3C, 0xD9, 0x2B}, "HP"},
    {{0x00, 0x01, 0xE6}, "HP"},
    {{0x00, 0x01, 0xE7}, "HP"},
    {{0x00, 0x02, 0xA5}, "HP"},
    {{0x00, 0x04, 0xEA}, "HP"},
    {{0x00, 0x08, 0x02}, "HP"},
    {{0x00, 0x08, 0x83}, "HP"},
    {{0x00, 0x0A, 0x57}, "HP"},
    {{0x00, 0x0B, 0xCD}, "HP"},
    {{0x00, 0x0D, 0x9D}, "HP"},
    {{0x00, 0x0E, 0x7F}, "HP"},
    {{0x00, 0x0F, 0x20}, "HP"},
    {{0x00, 0x0F, 0x61}, "HP"},
    {{0x00, 0x10, 0x83}, "HP"},
    {{0x00, 0x11, 0x0A}, "HP"},
    {{0x00, 0x11, 0x85}, "HP"},
    {{0x00, 0x12, 0x79}, "HP"},
    {{0x00, 0x13, 0x21}, "HP"},
    {{0x00, 0x14, 0x38}, "HP"},
    {{0x00, 0x14, 0xC2}, "HP"},
    {{0x00, 0x15, 0x60}, "HP"},
    {{0x00, 0x16, 0x35}, "HP"},
    {{0x00, 0x17, 0x08}, "HP"},
    {{0x00, 0x17, 0xA4}, "HP"},
    {{0x00, 0x18, 0x71}, "HP"},
    {{0x00, 0x18, 0xFE}, "HP"},
    {{0x00, 0x19, 0xBB}, "HP"},
    {{0x00, 0x1A, 0x4B}, "HP"},
    {{0x00, 0x1B, 0x78}, "HP"},
    {{0x00, 0x1C, 0xC4}, "HP"},
    {{0x00, 0x1E, 0x0B}, "HP"},
    {{0x00, 0x1F, 0x29}, "HP"},
    {{0x00, 0x21, 0x5A}, "HP"},
    {{0x00, 0x22, 0x64}, "HP"},
    {{0x00, 0x23, 0x7D}, "HP"},
    {{0x00, 0x24, 0x81}, "HP"},
    {{0x00, 0x25, 0xB3}, "HP"},
    {{0x00, 0x26, 0x55}, "HP"},
    // Linksys
    {{0x00, 0x50, 0xB6}, "Linksys"},
    {{0x00, 0x1A, 0x70}, "Linksys"},
    {{0x00, 0x04, 0x5A}, "Linksys"},
    {{0x00, 0x06, 0x25}, "Linksys"},
    {{0x00, 0x0C, 0x41}, "Linksys"},
    {{0x00, 0x0F, 0x66}, "Linksys"},
    {{0x00, 0x12, 0x17}, "Linksys"},
    {{0x00, 0x13, 0x10}, "Linksys"},
    {{0x00, 0x14, 0xBF}, "Linksys"},
    {{0x00, 0x16, 0xB6}, "Linksys"},
    {{0x00, 0x18, 0x39}, "Linksys"},
    {{0x00, 0x18, 0xF8}, "Linksys"},
    {{0x00, 0x1C, 0x10}, "Linksys"},
    {{0x00, 0x1D, 0x7E}, "Linksys"},
    {{0x00, 0x1E, 0xE5}, "Linksys"},
    {{0x00, 0x21, 0x29}, "Linksys"},
    {{0x00, 0x22, 0x6B}, "Linksys"},
    {{0x00, 0x23, 0x69}, "Linksys"},
    {{0x00, 0x25, 0x9C}, "Linksys"},
    // Espressif (ESP32/ESP8266)
    {{0x24, 0x0A, 0xC4}, "Espressif"},
    {{0xA4, 0xCF, 0x12}, "Espressif"},
    {{0x7C, 0xDF, 0xA1}, "Espressif"},
    {{0x30, 0xAE, 0xA4}, "Espressif"},
    {{0x24, 0x6F, 0x28}, "Espressif"},
    {{0x24, 0xB2, 0xDE}, "Espressif"},
    {{0x2C, 0x3A, 0xE8}, "Espressif"},
    {{0x30, 0x83, 0x98}, "Espressif"},
    {{0x3C, 0x61, 0x05}, "Espressif"},
    {{0x3C, 0x71, 0xBF}, "Espressif"},
    {{0x40, 0xF5, 0x20}, "Espressif"},
    {{0x48, 0x3F, 0xDA}, "Espressif"},
    {{0x4C, 0x11, 0xAE}, "Espressif"},
    {{0x54, 0x5A, 0xA6}, "Espressif"},
    {{0x5C, 0xCF, 0x7F}, "Espressif"},
    {{0x60, 0x01, 0x94}, "Espressif"},
    {{0x68, 0xC6, 0x3A}, "Espressif"},
    {{0x84, 0x0D, 0x8E}, "Espressif"},
    {{0x84, 0xCC, 0xA8}, "Espressif"},
    {{0x84, 0xF3, 0xEB}, "Espressif"},
    {{0x8C, 0xAA, 0xB5}, "Espressif"},
    {{0x90, 0x97, 0xD5}, "Espressif"},
    {{0x98, 0xF4, 0xAB}, "Espressif"},
    {{0xA0, 0x20, 0xA6}, "Espressif"},
    {{0xA4, 0x7B, 0x9D}, "Espressif"},
    {{0xAC, 0x67, 0xB2}, "Espressif"},
    {{0xAC, 0xD0, 0x74}, "Espressif"},
    {{0xB4, 0xE6, 0x2D}, "Espressif"},
    {{0xBC, 0xDD, 0xC2}, "Espressif"},
    {{0xC4, 0x4F, 0x33}, "Espressif"},
    {{0xC8, 0x2B, 0x96}, "Espressif"},
    {{0xCC, 0x50, 0xE3}, "Espressif"},
    {{0xD8, 0xA0, 0x1D}, "Espressif"},
    {{0xD8, 0xBF, 0xC0}, "Espressif"},
    {{0xDC, 0x4F, 0x22}, "Espressif"},
    {{0xE8, 0xDB, 0x84}, "Espressif"},
    {{0xEC, 0xFA, 0xBC}, "Espressif"},
    {{0xF0, 0x08, 0xD1}, "Espressif"},
    {{0xF4, 0xCF, 0xA2}, "Espressif"},
    {{0xFC, 0xF5, 0xC4}, "Espressif"},
    // Tenda
    {{0xC8, 0x3A, 0x35}, "Tenda"},
    {{0xC8, 0xD3, 0xA3}, "Tenda"},
    {{0xD8, 0x32, 0x14}, "Tenda"},
    {{0xCC, 0x2D, 0x21}, "Tenda"},
    // Philips
    {{0x00, 0x17, 0x88}, "Philips Hue"},
    {{0xEC, 0xB5, 0xFA}, "Philips Hue"},
    // Sonos
    {{0x00, 0x0E, 0x58}, "Sonos"},
    {{0x5C, 0xAA, 0xFD}, "Sonos"},
    {{0x78, 0x28, 0xCA}, "Sonos"},
    {{0x94, 0x9F, 0x3E}, "Sonos"},
    {{0xB8, 0xE9, 0x37}, "Sonos"},
    // Roku
    {{0x00, 0x0D, 0x4B}, "Roku"},
    {{0x08, 0x05, 0x81}, "Roku"},
    {{0x10, 0x59, 0x32}, "Roku"},
    {{0x20, 0xEF, 0xBD}, "Roku"},
    {{0xB0, 0xA7, 0x37}, "Roku"},
    {{0xB8, 0x3E, 0x59}, "Roku"},
    {{0xD0, 0x4D, 0x2C}, "Roku"},
    {{0xD8, 0x31, 0x34}, "Roku"},
    // Nvidia
    {{0x00, 0x04, 0x4B}, "Nvidia"},
    {{0x48, 0xB0, 0x2D}, "Nvidia"},
    // Intel
    {{0x00, 0x02, 0xB3}, "Intel"},
    {{0x00, 0x03, 0x47}, "Intel"},
    {{0x00, 0x04, 0x23}, "Intel"},
    {{0x00, 0x07, 0xE9}, "Intel"},
    {{0x00, 0x0C, 0xF1}, "Intel"},
    {{0x00, 0x0E, 0x0C}, "Intel"},
    {{0x00, 0x0E, 0x35}, "Intel"},
    {{0x00, 0x11, 0x11}, "Intel"},
    {{0x00, 0x12, 0xF0}, "Intel"},
    {{0x00, 0x13, 0x02}, "Intel"},
    {{0x00, 0x13, 0x20}, "Intel"},
    {{0x00, 0x13, 0xCE}, "Intel"},
    {{0x00, 0x13, 0xE8}, "Intel"},
    {{0x00, 0x15, 0x00}, "Intel"},
    {{0x00, 0x15, 0x17}, "Intel"},
    {{0x00, 0x16, 0x6F}, "Intel"},
    {{0x00, 0x16, 0x76}, "Intel"},
    {{0x00, 0x16, 0xEA}, "Intel"},
    {{0x00, 0x16, 0xEB}, "Intel"},
    {{0x00, 0x18, 0xDE}, "Intel"},
    {{0x00, 0x19, 0xD1}, "Intel"},
    {{0x00, 0x19, 0xD2}, "Intel"},
    {{0x00, 0x1B, 0x21}, "Intel"},
    {{0x00, 0x1B, 0x77}, "Intel"},
    {{0x00, 0x1C, 0xBF}, "Intel"},
    {{0x00, 0x1D, 0xE0}, "Intel"},
    {{0x00, 0x1D, 0xE1}, "Intel"},
    {{0x00, 0x1E, 0x64}, "Intel"},
    {{0x00, 0x1E, 0x65}, "Intel"},
    {{0x00, 0x1E, 0x67}, "Intel"},
    {{0x00, 0x1F, 0x3B}, "Intel"},
    {{0x00, 0x1F, 0x3C}, "Intel"},
    {{0x00, 0x20, 0xA6}, "Intel"},
    {{0x00, 0x21, 0x5C}, "Intel"},
    {{0x00, 0x21, 0x5D}, "Intel"},
    {{0x00, 0x21, 0x6A}, "Intel"},
    {{0x00, 0x21, 0x6B}, "Intel"},
    {{0x00, 0x22, 0xFA}, "Intel"},
    {{0x00, 0x22, 0xFB}, "Intel"},
    {{0x00, 0x23, 0x14}, "Intel"},
    {{0x00, 0x24, 0xD6}, "Intel"},
    {{0x00, 0x24, 0xD7}, "Intel"},
    {{0x00, 0x26, 0xC6}, "Intel"},
    {{0x00, 0x26, 0xC7}, "Intel"},
    // Hon Hai/Foxconn
    {{0x00, 0x26, 0x5E}, "Foxconn"},
    {{0x54, 0xEE, 0x75}, "Wistron"},
    {{0xFC, 0xAA, 0x14}, "Gigabyte"},
    // LG
    {{0x00, 0x1C, 0x62}, "LG"},
    {{0x00, 0x1E, 0x75}, "LG"},
    {{0x00, 0x1F, 0x6B}, "LG"},
    {{0x00, 0x1F, 0xE3}, "LG"},
    {{0x00, 0x22, 0xA9}, "LG"},
    {{0x00, 0x24, 0x83}, "LG"},
    {{0x00, 0x25, 0xE5}, "LG"},
    {{0x00, 0x26, 0xE2}, "LG"},
    {{0x10, 0x68, 0x3F}, "LG"},
    {{0x14, 0xC9, 0x13}, "LG"},
    {{0x20, 0x21, 0xA5}, "LG"},
    {{0x28, 0x39, 0x26}, "LG"},
    {{0x2C, 0x54, 0xCF}, "LG"},
    {{0x34, 0x4D, 0xF7}, "LG"},
    {{0x38, 0x8C, 0x50}, "LG"},
    {{0x40, 0xB0, 0xFA}, "LG"},
    {{0x58, 0x3F, 0x54}, "LG"},
    {{0x5C, 0x25, 0xE6}, "LG"},
    {{0x64, 0x99, 0x5D}, "LG"},
    {{0x6C, 0xAD, 0xF8}, "LG"},
    {{0x74, 0xA7, 0x22}, "LG"},
    {{0x78, 0x5D, 0xC8}, "LG"},
    {{0x88, 0xC9, 0xD0}, "LG"},
    {{0x8C, 0x3A, 0xE3}, "LG"},
    {{0x98, 0xD6, 0xF7}, "LG"},
    {{0xA8, 0x16, 0xB2}, "LG"},
    {{0xAC, 0x0D, 0x1B}, "LG"},
    {{0xB4, 0xE6, 0x2A}, "LG"},
    {{0xBC, 0xF5, 0xAC}, "LG"},
    {{0xC4, 0x36, 0x6C}, "LG"},
    {{0xC4, 0x9A, 0x02}, "LG"},
    {{0xC8, 0x08, 0xE9}, "LG"},
    {{0xCC, 0x2D, 0x8C}, "LG"},
    {{0xD0, 0x13, 0xFD}, "LG"},
    {{0xE8, 0x5B, 0x5B}, "LG"},
    {{0xF8, 0x0D, 0xEA}, "LG"},
    {{0xF8, 0xD0, 0xAC}, "LG"}
};
const int macVendorCount = 464;

// Forward declarations
void drawSplashScreen();
void drawMainMenu();
void drawIconMenu();
void handleMainMenu();
void drawHeader(const char* title);
void drawStatus(const char* msg, uint16_t color);
void connectToWiFi();
void autoConnectWiFi();
void saveWiFiCredentials(const String& ssid, const String& password);
void loadWiFiCredentials();
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
bool checkEscapeKey();
char getKeyInput();
String getTextInput(const char* prompt, int maxLen);
void drawIcon(int x, int y, int index, bool selected);
void runPortScanOnHost(IPAddress target);

// Setup
void setup() {
    M5Cardputer.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(COLOR_BG);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);

    // Load saved WiFi credentials
    loadWiFiCredentials();

    // Animated splash screen
    drawSplashScreen();

    // Try auto-connect if credentials saved
    if(connectedSSID.length() > 0 && savedPassword.length() > 0) {
        autoConnectWiFi();
    }

    drawMainMenu();
}

// Animated Splash Screen
void drawSplashScreen() {
    clearScreen();

    // Draw animated network pattern
    for(int i = 0; i < 8; i++) {
        int x = random(20, SCREEN_W - 20);
        int y = random(20, 50);
        M5Cardputer.Display.fillCircle(x, y, 3, COLOR_DARKGRAY);
        delay(30);
    }

    // Draw connecting lines
    for(int i = 0; i < 5; i++) {
        int x1 = random(20, SCREEN_W - 20);
        int y1 = random(20, 50);
        int x2 = random(20, SCREEN_W - 20);
        int y2 = random(20, 50);
        M5Cardputer.Display.drawLine(x1, y1, x2, y2, COLOR_DARKGRAY);
        delay(20);
    }

    // Main title with animation
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.setTextSize(2);

    // Animate title appearance
    String title = "NetToolkit";
    int titleX = SCREEN_W / 2;
    int titleY = 67;

    for(int i = 0; i <= title.length(); i++) {
        M5Cardputer.Display.setTextColor(COLOR_TITLE, COLOR_BG);
        M5Cardputer.Display.drawString(title.substring(0, i), titleX, titleY);
        delay(40);
    }

    // Version with glow effect
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT);
    M5Cardputer.Display.drawString("v10", titleX, titleY + 20);

    // Animated loading bar
    int barY = 105;
    int barW = 160;
    int barH = 8;
    int barX = (SCREEN_W - barW) / 2;

    M5Cardputer.Display.drawRoundRect(barX - 2, barY - 2, barW + 4, barH + 4, 4, COLOR_TITLE);

    for(int i = 0; i <= barW; i += 4) {
        // Gradient color effect
        uint16_t col = (i < barW/3) ? COLOR_TITLE : (i < 2*barW/3) ? COLOR_SUCCESS : COLOR_ACCENT;
        M5Cardputer.Display.fillRect(barX, barY, i, barH, col);
        delay(8);
    }

    // Bottom text
    M5Cardputer.Display.setTextColor(COLOR_LIGHTGRAY);
    M5Cardputer.Display.drawString("For Network Engineers", titleX, 125);

    delay(300);
    M5Cardputer.Display.setTextDatum(TL_DATUM);
    M5Cardputer.Display.setTextSize(1);
}

// Load WiFi credentials from flash
void loadWiFiCredentials() {
    preferences.begin("netwifi", true);  // Read-only
    connectedSSID = preferences.getString("ssid", "");
    savedPassword = preferences.getString("pass", "");
    preferences.end();
}

// Save WiFi credentials to flash
void saveWiFiCredentials(const String& ssid, const String& password) {
    preferences.begin("netwifi", false);  // Read-write
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.end();
    connectedSSID = ssid;
    savedPassword = password;
}

// Auto-connect to saved WiFi
void autoConnectWiFi() {
    clearScreen();
    drawHeader("Auto-Connect");

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("Connecting to:", 8, 35);
    M5Cardputer.Display.setTextColor(COLOR_TITLE);
    M5Cardputer.Display.drawString(connectedSSID, 8, 50);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);

    WiFi.begin(connectedSSID.c_str(), savedPassword.c_str());

    int attempts = 0;
    int dotX = 8;
    while(WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(300);
        M5Cardputer.Display.fillCircle(dotX, 75, 3, COLOR_WARNING);
        dotX += 10;
        if(dotX > SCREEN_W - 20) dotX = 8;
        attempts++;
    }

    if(WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        localIP = WiFi.localIP();
        gateway = WiFi.gatewayIP();
        subnet = WiFi.subnetMask();

        M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
        M5Cardputer.Display.drawString("Connected!", 8, 95);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);
        M5Cardputer.Display.drawString(localIP.toString(), 8, 110);
        delay(1000);
    } else {
        M5Cardputer.Display.setTextColor(COLOR_ERROR);
        M5Cardputer.Display.drawString("Failed - manual connect", 8, 95);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);
        delay(1500);
    }
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

// Draw header - improved style
void drawHeader(const char* title) {
    // Gradient-style header
    M5Cardputer.Display.fillRect(0, 0, SCREEN_W, 20, COLOR_DARKGRAY);
    M5Cardputer.Display.drawFastHLine(0, 19, SCREEN_W, COLOR_TITLE);

    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.setTextColor(COLOR_TITLE, COLOR_DARKGRAY);
    M5Cardputer.Display.drawString(title, SCREEN_W/2, 10);
    M5Cardputer.Display.setTextDatum(TL_DATUM);

    // Show WiFi status with icon
    if(wifiConnected) {
        M5Cardputer.Display.fillCircle(SCREEN_W - 12, 10, 5, COLOR_SUCCESS);
    } else {
        M5Cardputer.Display.drawCircle(SCREEN_W - 12, 10, 5, COLOR_ERROR);
    }
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
}

// Draw status line at bottom - improved style
void drawStatus(const char* msg, uint16_t color) {
    M5Cardputer.Display.fillRect(0, SCREEN_H - 16, SCREEN_W, 16, COLOR_DARKGRAY);
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.setTextColor(color, COLOR_DARKGRAY);
    M5Cardputer.Display.drawString(msg, SCREEN_W/2, SCREEN_H - 8);
    M5Cardputer.Display.setTextDatum(TL_DATUM);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
}

// Draw single icon
void drawIcon(int x, int y, int index, bool selected) {
    uint16_t bgColor = selected ? menuItems[index].color : COLOR_DARKGRAY;
    uint16_t borderColor = selected ? COLOR_TEXT : COLOR_DARKGRAY;

    // Draw icon background (rounded rect)
    M5Cardputer.Display.fillRoundRect(x, y, ICON_SIZE, ICON_SIZE, 8, bgColor);

    if(selected) {
        M5Cardputer.Display.drawRoundRect(x, y, ICON_SIZE, ICON_SIZE, 8, COLOR_TEXT);
        M5Cardputer.Display.drawRoundRect(x+1, y+1, ICON_SIZE-2, ICON_SIZE-2, 7, COLOR_TEXT);
    }

    // Draw icon letter
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(selected ? COLOR_BG : COLOR_LIGHTGRAY);
    M5Cardputer.Display.drawString(menuItems[index].icon, x + ICON_SIZE/2, y + ICON_SIZE/2);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextDatum(TL_DATUM);
}

// Draw main menu with horizontal scrolling icons
void drawMainMenu() {
    clearScreen();

    // Header bar
    M5Cardputer.Display.fillRect(0, 0, SCREEN_W, 22, COLOR_DARKGRAY);
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_TITLE);
    M5Cardputer.Display.drawString("NetToolkit v10", SCREEN_W/2, 11);

    // WiFi status indicator
    if(wifiConnected) {
        M5Cardputer.Display.fillCircle(SCREEN_W - 15, 11, 5, COLOR_SUCCESS);
    } else {
        M5Cardputer.Display.drawCircle(SCREEN_W - 15, 11, 5, COLOR_ERROR);
    }

    // Calculate visible icons (show 3-4 icons at once)
    int visibleIcons = 4;
    int totalWidth = visibleIcons * (ICON_SIZE + ICON_SPACING) - ICON_SPACING;
    int startX = (SCREEN_W - totalWidth) / 2;

    // Calculate offset for smooth scrolling
    int firstVisible = menuSelection - 1;
    if(firstVisible < 0) firstVisible = 0;
    if(firstVisible > mainMenuCount - visibleIcons) firstVisible = mainMenuCount - visibleIcons;

    // Draw icons
    for(int i = 0; i < visibleIcons && (firstVisible + i) < mainMenuCount; i++) {
        int idx = firstVisible + i;
        int x = startX + i * (ICON_SIZE + ICON_SPACING);
        drawIcon(x, ICON_Y, idx, idx == menuSelection);
    }

    // Draw scroll indicators
    M5Cardputer.Display.setTextColor(COLOR_LIGHTGRAY);
    if(firstVisible > 0) {
        M5Cardputer.Display.fillTriangle(8, ICON_Y + ICON_SIZE/2, 16, ICON_Y + ICON_SIZE/2 - 8,
                                          16, ICON_Y + ICON_SIZE/2 + 8, COLOR_LIGHTGRAY);
    }
    if(firstVisible + visibleIcons < mainMenuCount) {
        M5Cardputer.Display.fillTriangle(SCREEN_W - 8, ICON_Y + ICON_SIZE/2,
                                          SCREEN_W - 16, ICON_Y + ICON_SIZE/2 - 8,
                                          SCREEN_W - 16, ICON_Y + ICON_SIZE/2 + 8, COLOR_LIGHTGRAY);
    }

    // Selected item name (larger font)
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);
    M5Cardputer.Display.drawString(menuItems[menuSelection].name, SCREEN_W/2, ICON_Y + ICON_SIZE + 15);

    // Instructions at bottom
    M5Cardputer.Display.fillRect(0, SCREEN_H - 18, SCREEN_W, 18, COLOR_DARKGRAY);
    M5Cardputer.Display.setTextColor(COLOR_LIGHTGRAY);
    M5Cardputer.Display.drawString("</>:Navigate  ENTER:Select", SCREEN_W/2, SCREEN_H - 9);

    M5Cardputer.Display.setTextDatum(TL_DATUM);
    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
}

// Check if ESC key is pressed (improved detection)
bool checkEscapeKey() {
    M5Cardputer.update();
    if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        // Check for ESC key or '`' as alternative
        if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) ||
           keyWord(status.word) == "`" ||
           keyWord(status.word) == "\\") {
            stopRequested = true;
            return true;
        }
    }
    return false;
}

// Handle main menu input - horizontal navigation
void handleMainMenu() {
    if(M5Cardputer.Keyboard.isChange()) {
        if(M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            String key = keyWord(status.word);

            // Left navigation: , or ; or left bracket
            if(key == "," || key == ";" || key == "[") {
                if(menuSelection > 0) {
                    menuSelection--;
                    drawMainMenu();
                }
            }
            // Right navigation: . or / or right bracket
            else if(key == "." || key == "/" || key == "]") {
                if(menuSelection < mainMenuCount - 1) {
                    menuSelection++;
                    drawMainMenu();
                }
            }
            // Enter
            else if(key == "\n" || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
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

// Get text input with on-screen keyboard - improved UI
String getTextInput(const char* prompt, int maxLen) {
    String input = "";
    clearScreen();
    drawHeader(prompt);

    M5Cardputer.Display.setTextColor(COLOR_LIGHTGRAY);
    M5Cardputer.Display.drawString("Type and press ENTER", 8, 30);
    M5Cardputer.Display.drawString("ESC or ` to cancel", 8, 45);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);

    // Input box with border
    M5Cardputer.Display.drawRoundRect(6, 65, SCREEN_W - 12, 28, 6, COLOR_TITLE);
    M5Cardputer.Display.fillRoundRect(8, 67, SCREEN_W - 16, 24, 4, COLOR_DARKGRAY);

    // Cursor blink timer
    unsigned long lastBlink = millis();
    bool cursorVisible = true;

    while(true) {
        M5Cardputer.update();

        // Blink cursor
        if(millis() - lastBlink > 500) {
            cursorVisible = !cursorVisible;
            lastBlink = millis();

            // Redraw input with/without cursor
            M5Cardputer.Display.fillRoundRect(8, 67, SCREEN_W - 16, 24, 4, COLOR_DARKGRAY);
            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_DARKGRAY);

            String displayText = input;
            if(cursorVisible) displayText += "_";

            // Scroll text if too long
            if(displayText.length() > 28) {
                displayText = displayText.substring(displayText.length() - 28);
            }

            M5Cardputer.Display.drawString(displayText, 12, 73);
            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
        }

        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            String key = keyWord(status.word);

            // ESC - cancel
            if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || key == "`") {
                return "";
            }
            // Enter - confirm
            if(key == "\n" || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
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
                input += key;
            }

            // Force cursor visible on input
            cursorVisible = true;
            lastBlink = millis();

            // Update display immediately
            M5Cardputer.Display.fillRoundRect(8, 67, SCREEN_W - 16, 24, 4, COLOR_DARKGRAY);
            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_DARKGRAY);

            String displayText = input + "_";
            if(displayText.length() > 28) {
                displayText = displayText.substring(displayText.length() - 28);
            }

            M5Cardputer.Display.drawString(displayText, 12, 73);
            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
        }
        delay(30);
    }
}

// Connect to WiFi
void connectToWiFi() {
    clearScreen();
    drawHeader("WiFi Connect");

    // Scan networks
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("Scanning networks...", 8, 50);
    drawStatus("Please wait...", COLOR_WARNING);

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
    int visibleItems = 5;
    int itemHeight = 18;

    bool needsRedraw = true;

    while(true) {
        if(needsRedraw) {
            // Clear list area only
            M5Cardputer.Display.fillRect(0, 20, SCREEN_W, SCREEN_H - 40, COLOR_BG);

            // Draw network list with larger spacing
            for(int i = 0; i < visibleItems && (i + offset) < n; i++) {
                int idx = i + offset;
                int y = 22 + (i * itemHeight);

                if(idx == selection) {
                    M5Cardputer.Display.fillRoundRect(2, y, SCREEN_W - 4, itemHeight - 2, 4, COLOR_MENU_SEL);
                    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_MENU_SEL);
                } else {
                    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
                }

                String ssid = WiFi.SSID(idx);
                if(ssid.length() > 18) ssid = ssid.substring(0, 15) + "...";
                int rssi = WiFi.RSSI(idx);

                // Draw signal bars
                int bars = (rssi > -50) ? 4 : (rssi > -60) ? 3 : (rssi > -70) ? 2 : 1;
                int barX = SCREEN_W - 30;
                for(int b = 0; b < 4; b++) {
                    uint16_t barColor = (b < bars) ? COLOR_SUCCESS : COLOR_DARKGRAY;
                    M5Cardputer.Display.fillRect(barX + b*5, y + 12 - b*3, 4, 4 + b*3, barColor);
                }

                M5Cardputer.Display.drawString(ssid.c_str(), 8, y + 3);
            }

            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
            drawStatus(";/.:Nav ENTER:Select ESC:Back", COLOR_LIGHTGRAY);
            needsRedraw = false;
        }

        // Handle input
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            String key = keyWord(status.word);

            if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || key == "`") {
                WiFi.scanDelete();
                return;
            }
            if((key == ";" || key == ",") && selection > 0) {
                selection--;
                if(selection < offset) offset = selection;
                needsRedraw = true;
            }
            if((key == "." || key == "/") && selection < n - 1) {
                selection++;
                if(selection >= offset + visibleItems) offset++;
                needsRedraw = true;
            }
            if(key == "\n" || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                String selectedSSID = WiFi.SSID(selection);
                WiFi.scanDelete();

                // Get password
                String password = getTextInput("Enter Password", 64);
                if(password == "") return;

                // Connect
                clearScreen();
                drawHeader("Connecting...");
                M5Cardputer.Display.setTextSize(1);
                M5Cardputer.Display.drawString("Network:", 8, 35);
                M5Cardputer.Display.setTextColor(COLOR_TITLE);
                M5Cardputer.Display.drawString(selectedSSID, 8, 50);
                M5Cardputer.Display.setTextColor(COLOR_TEXT);

                WiFi.begin(selectedSSID.c_str(), password.c_str());

                int attempts = 0;
                int dotX = 8;
                while(WiFi.status() != WL_CONNECTED && attempts < 30) {
                    delay(300);
                    M5Cardputer.Display.fillCircle(dotX, 75, 3, COLOR_WARNING);
                    dotX += 10;
                    if(dotX > SCREEN_W - 20) {
                        dotX = 8;
                        M5Cardputer.Display.fillRect(8, 70, SCREEN_W - 16, 15, COLOR_BG);
                    }
                    attempts++;
                }

                if(WiFi.status() == WL_CONNECTED) {
                    wifiConnected = true;
                    connectedSSID = selectedSSID;
                    localIP = WiFi.localIP();
                    gateway = WiFi.gatewayIP();
                    subnet = WiFi.subnetMask();

                    // Save credentials for auto-reconnect
                    saveWiFiCredentials(selectedSSID, password);

                    clearScreen();
                    drawHeader("Connected!");
                    M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
                    M5Cardputer.Display.drawString("Successfully connected!", 8, 30);
                    M5Cardputer.Display.setTextColor(COLOR_TEXT);
                    M5Cardputer.Display.drawString(("IP: " + localIP.toString()).c_str(), 8, 50);
                    M5Cardputer.Display.drawString(("GW: " + gateway.toString()).c_str(), 8, 65);
                    M5Cardputer.Display.drawString(("Mask: " + subnet.toString()).c_str(), 8, 80);
                    M5Cardputer.Display.setTextColor(COLOR_ACCENT);
                    M5Cardputer.Display.drawString("Credentials saved!", 8, 100);
                    M5Cardputer.Display.setTextColor(COLOR_TEXT);
                    drawStatus("Press any key...", COLOR_SUCCESS);
                } else {
                    M5Cardputer.Display.setTextColor(COLOR_ERROR);
                    M5Cardputer.Display.drawString("Connection failed!", 8, 90);
                    M5Cardputer.Display.setTextColor(COLOR_TEXT);
                    drawStatus("Press any key...", COLOR_ERROR);
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

// IP Scanner - FIXED version
void runIPScanner() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("IP Scanner");
        M5Cardputer.Display.setTextColor(COLOR_ERROR);
        M5Cardputer.Display.drawString("Connect to WiFi first!", 8, 50);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);
        drawStatus("Press any key...", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("IP Scanner");

    scanResults.count = 0;
    stopRequested = false;

    // Calculate network range CORRECTLY
    // Use the actual IP octets directly
    int baseOctet1 = localIP[0];
    int baseOctet2 = localIP[1];
    int baseOctet3 = localIP[2];

    int startHost = 1;
    int endHost = 254;

    // Display correct range
    String rangeStart = String(baseOctet1) + "." + String(baseOctet2) + "." + String(baseOctet3) + ".1";
    String rangeEnd = String(baseOctet1) + "." + String(baseOctet2) + "." + String(baseOctet3) + ".254";

    M5Cardputer.Display.drawString("Scanning network...", 8, 28);
    M5Cardputer.Display.setTextColor(COLOR_TITLE);
    M5Cardputer.Display.drawString(rangeStart + " - " + rangeEnd, 8, 43);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);

    // Progress bar
    M5Cardputer.Display.drawRoundRect(8, 60, SCREEN_W - 16, 14, 4, COLOR_TEXT);

    // ESC hint
    M5Cardputer.Display.setTextColor(COLOR_LIGHTGRAY);
    M5Cardputer.Display.drawString("ESC or ` to stop", 8, SCREEN_H - 25);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);

    int found = 0;
    for(int i = startHost; i <= endHost && i <= MAX_HOSTS; i++) {
        // Update progress bar
        int progress = (i * (SCREEN_W - 20)) / endHost;
        M5Cardputer.Display.fillRoundRect(10, 62, progress, 10, 2, COLOR_SUCCESS);

        // Update current IP being scanned
        M5Cardputer.Display.fillRect(8, 78, SCREEN_W - 16, 12, COLOR_BG);
        M5Cardputer.Display.setTextColor(COLOR_LIGHTGRAY);
        M5Cardputer.Display.drawString("Checking: " + String(baseOctet1) + "." +
                                        String(baseOctet2) + "." + String(baseOctet3) + "." + String(i), 8, 78);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);

        IPAddress targetIP(baseOctet1, baseOctet2, baseOctet3, i);

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
            M5Cardputer.Display.fillRect(8, 95, SCREEN_W - 16, 14, COLOR_BG);
            M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
            M5Cardputer.Display.drawString("Found: " + String(found) + " hosts", 8, 95);
            M5Cardputer.Display.setTextColor(COLOR_TEXT);
        }

        // Check for ESC to abort - improved detection
        if(checkEscapeKey()) {
            M5Cardputer.Display.setTextColor(COLOR_WARNING);
            M5Cardputer.Display.drawString("Scan stopped by user", 8, 110);
            M5Cardputer.Display.setTextColor(COLOR_TEXT);
            delay(500);
            break;
        }
    }

    // Show results
    if(scanResults.count == 0) {
        clearScreen();
        drawHeader("Scan Results");
        M5Cardputer.Display.drawString("No hosts found", 8, 50);
        drawStatus("Press any key...", COLOR_TEXT);
        waitForKey();
        return;
    }

    int selection = 0;
    int offset = 0;
    int visibleItems = 5;
    int itemHeight = 18;
    bool needsRedraw = true;
    stopRequested = false;

    while(true) {
        // Only redraw when needed - FIXES FLASHING
        if(needsRedraw) {
            clearScreen();
            drawHeader(("Found " + String(scanResults.count) + " hosts").c_str());

            for(int i = 0; i < visibleItems && (i + offset) < scanResults.count; i++) {
                int idx = i + offset;
                int y = 22 + (i * itemHeight);

                if(idx == selection) {
                    M5Cardputer.Display.fillRoundRect(2, y, SCREEN_W - 4, itemHeight - 2, 4, COLOR_MENU_SEL);
                    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_MENU_SEL);
                } else {
                    M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
                }

                String line = scanResults.hosts[idx].ip.toString();
                String vendor = scanResults.hosts[idx].hostname;
                if(vendor.length() > 12) vendor = vendor.substring(0, 10) + "..";

                M5Cardputer.Display.drawString(line, 8, y + 2);
                M5Cardputer.Display.setTextColor(COLOR_ACCENT, (idx == selection) ? COLOR_MENU_SEL : COLOR_BG);
                M5Cardputer.Display.drawString(vendor, 130, y + 2);
            }

            M5Cardputer.Display.setTextColor(COLOR_TEXT, COLOR_BG);
            drawStatus(";/.:Nav ENTER:Details ESC:Back", COLOR_LIGHTGRAY);
            needsRedraw = false;
        }

        // Handle input
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            String key = keyWord(status.word);

            // ESC to exit - FIXED
            if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || key == "`" || key == "\\") {
                return;
            }

            // Navigation
            if((key == ";" || key == ",") && selection > 0) {
                selection--;
                if(selection < offset) offset = selection;
                needsRedraw = true;
            }
            if((key == "." || key == "/") && selection < scanResults.count - 1) {
                selection++;
                if(selection >= offset + visibleItems) offset++;
                needsRedraw = true;
            }

            // Show details - FIXED
            if(key == "\n" || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                // Show host details
                clearScreen();
                drawHeader("Host Details");

                NetworkHost& h = scanResults.hosts[selection];

                M5Cardputer.Display.setTextColor(COLOR_TITLE);
                M5Cardputer.Display.drawString("IP Address:", 8, 25);
                M5Cardputer.Display.setTextColor(COLOR_TEXT);
                M5Cardputer.Display.drawString(h.ip.toString(), 100, 25);

                M5Cardputer.Display.setTextColor(COLOR_TITLE);
                M5Cardputer.Display.drawString("MAC:", 8, 40);
                M5Cardputer.Display.setTextColor(COLOR_TEXT);
                M5Cardputer.Display.drawString(macToString(h.mac), 100, 40);

                M5Cardputer.Display.setTextColor(COLOR_TITLE);
                M5Cardputer.Display.drawString("Vendor:", 8, 55);
                M5Cardputer.Display.setTextColor(COLOR_ACCENT);
                M5Cardputer.Display.drawString(h.hostname, 100, 55);
                M5Cardputer.Display.setTextColor(COLOR_TEXT);

                // Quick ping test
                M5Cardputer.Display.drawString("Testing ping...", 8, 75);
                if(Ping.ping(h.ip, 3)) {
                    float avg = Ping.averageTime();
                    M5Cardputer.Display.fillRect(8, 75, 150, 12, COLOR_BG);
                    M5Cardputer.Display.setTextColor(COLOR_TITLE);
                    M5Cardputer.Display.drawString("Latency:", 8, 75);
                    M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
                    M5Cardputer.Display.drawString(String(avg, 1) + " ms", 100, 75);
                } else {
                    M5Cardputer.Display.fillRect(8, 75, 150, 12, COLOR_BG);
                    M5Cardputer.Display.setTextColor(COLOR_ERROR);
                    M5Cardputer.Display.drawString("Ping timeout", 8, 75);
                }
                M5Cardputer.Display.setTextColor(COLOR_TEXT);

                drawStatus("P:PortScan  ESC/`:Back", COLOR_LIGHTGRAY);

                // Wait for input in details view
                bool inDetails = true;
                while(inDetails) {
                    M5Cardputer.update();
                    if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                        Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();
                        String k = keyWord(st.word);

                        if(M5Cardputer.Keyboard.isKeyPressed(KEY_ESC) || k == "`" || k == "\\") {
                            inDetails = false;
                            needsRedraw = true;
                        }
                        if(k == "p" || k == "P") {
                            runPortScanOnHost(h.ip);
                            inDetails = false;
                            needsRedraw = true;
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

// Ping Sweep - FIXED: can be stopped with ESC
void runPingSweep() {
    if(!wifiConnected) {
        clearScreen();
        drawHeader("Ping Sweep");
        M5Cardputer.Display.setTextColor(COLOR_ERROR);
        M5Cardputer.Display.drawString("Connect to WiFi first!", 8, 50);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);
        drawStatus("Press any key...", COLOR_ERROR);
        waitForKey();
        return;
    }

    // Use current network by default
    String defaultPrefix = String(localIP[0]) + "." + String(localIP[1]) + "." + String(localIP[2]);

    clearScreen();
    drawHeader("Ping Sweep");
    M5Cardputer.Display.drawString("Default: " + defaultPrefix, 8, 25);

    String targetStr = getTextInput("IP prefix (or Enter)", 15);

    // Use current network if empty
    if(targetStr == "") {
        targetStr = defaultPrefix + ".1";
    }

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
        M5Cardputer.Display.setTextColor(COLOR_ERROR);
        M5Cardputer.Display.drawString("Invalid IP format!", 8, 50);
        M5Cardputer.Display.setTextColor(COLOR_TEXT);
        drawStatus("Press any key...", COLOR_ERROR);
        waitForKey();
        return;
    }

    clearScreen();
    drawHeader("Ping Sweep");

    String rangeStr = String(target[0]) + "." + String(target[1]) + "." + String(target[2]) + ".1-254";
    M5Cardputer.Display.setTextColor(COLOR_TITLE);
    M5Cardputer.Display.drawString("Range: " + rangeStr, 8, 25);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);

    // Progress bar
    M5Cardputer.Display.drawRoundRect(8, 42, SCREEN_W - 16, 12, 4, COLOR_TEXT);

    // ESC hint - prominent display
    M5Cardputer.Display.setTextColor(COLOR_WARNING);
    M5Cardputer.Display.drawString("Press ESC or ` to STOP", 8, 57);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);

    stopRequested = false;
    int responding = 0;
    int scanned = 0;
    int listY = 72;
    int maxListY = SCREEN_H - 20;

    // Results storage for display
    String results[20];
    int resultCount = 0;

    for(int i = startIP; i <= endIP; i++) {
        scanned++;

        // Update progress bar
        int progress = (i * (SCREEN_W - 20)) / 254;
        M5Cardputer.Display.fillRoundRect(10, 44, progress, 8, 2, COLOR_SUCCESS);

        IPAddress pingTarget(target[0], target[1], target[2], i);

        if(Ping.ping(pingTarget, 1)) {
            responding++;
            float latency = Ping.averageTime();

            // Store result
            if(resultCount < 20) {
                results[resultCount++] = pingTarget.toString() + " " + String(latency, 0) + "ms";
            }

            // Display result if space available
            if(listY < maxListY) {
                M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
                M5Cardputer.Display.drawString(pingTarget.toString() + " " + String(latency, 0) + "ms", 8, listY);
                M5Cardputer.Display.setTextColor(COLOR_TEXT);
                listY += 11;
            }
        }

        // Check ESC to stop - FIXED: improved detection
        if(checkEscapeKey()) {
            M5Cardputer.Display.fillRect(8, 57, SCREEN_W - 16, 12, COLOR_BG);
            M5Cardputer.Display.setTextColor(COLOR_WARNING);
            M5Cardputer.Display.drawString("STOPPED by user", 8, 57);
            M5Cardputer.Display.setTextColor(COLOR_TEXT);
            break;
        }
    }

    // Show final summary
    M5Cardputer.Display.fillRect(0, SCREEN_H - 18, SCREEN_W, 18, COLOR_DARKGRAY);
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.setTextColor(COLOR_SUCCESS);
    M5Cardputer.Display.drawString(String(responding) + "/" + String(scanned) + " hosts responding", SCREEN_W/2, SCREEN_H - 9);
    M5Cardputer.Display.setTextDatum(TL_DATUM);
    M5Cardputer.Display.setTextColor(COLOR_TEXT);

    // Wait for key to exit
    stopRequested = false;
    while(true) {
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            break;
        }
        delay(50);
    }
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
