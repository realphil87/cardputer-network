# Network Toolkit per M5Stack Cardputer

Tool completo per network engineering su M5Stack Cardputer (ESP32-S3).

## Funzionalità

### Discovery & Inventory
- **IP Scanner** - Scansione ARP della rete locale con identificazione vendor MAC
- **Port Scanner** - Scan delle porte comuni (21, 22, 80, 443, 3389, etc.)
- **WiFi Networks** - Lista reti WiFi con BSSID, canale, sicurezza, RSSI

### Diagnostica
- **Ping Sweep** - Ping massivo su range IP con latenza
- **DNS Lookup** - Risoluzione hostname con test ping
- **DHCP Discover** - Mostra info DHCP correnti

### Monitoring
- **Signal Mapper** - Mappa segnale WiFi in tempo reale (dBm)
- **Net Monitor** - Rileva nuovi dispositivi in rete (alert)

### Utility
- **Subnet Calculator** - Calcola network, broadcast, range da IP/CIDR
- **WiFi QR Code** - Genera QR code per condividere credenziali WiFi
- **SNMP Query** - Query SNMP v1 base (sysDescr)

## Compilazione

### Metodo 1: PlatformIO (Consigliato)

1. Installa [VS Code](https://code.visualstudio.com/)
2. Installa l'estensione PlatformIO
3. Apri la cartella `cardputer-network-toolkit`
4. Clicca su **Build** (icona check)
5. Clicca su **Upload** (icona freccia)

### Metodo 2: Arduino IDE

1. Installa [Arduino IDE](https://www.arduino.cc/en/software)
2. Aggiungi board ESP32: File → Preferences → Additional Boards Manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Installa "ESP32" da Boards Manager
4. Installa librerie da Library Manager:
   - M5Cardputer
   - M5Unified
   - M5GFX
   - ESP32Ping
   - QRCode by Richard Moore
5. Seleziona board: **M5Stack-STAMPS3**
6. Compila e carica

## Controlli

| Tasto | Funzione |
|-------|----------|
| `;` o UP | Naviga su |
| `.` o DOWN | Naviga giù |
| ENTER | Seleziona/Conferma |
| ESC | Indietro/Annulla |
| BACKSPACE | Cancella carattere |
| P | Port scan (da dettaglio host) |

## Database MAC Vendor

Include OUI per i vendor più comuni:
- Cisco, VMware, Microsoft, Parallels, VirtualBox
- Apple, Raspberry Pi
- D-Link, Tenda, TP-Link, Netgear, ASUS, Linksys
- Dell, HP
- Espressif (ESP32/ESP8266)
- Xiaomi, Huawei, Philips

## Porte Scansionate

Port scanner controlla le porte più comuni:
```
21 (FTP), 22 (SSH), 23 (Telnet), 25 (SMTP), 53 (DNS)
80 (HTTP), 110 (POP3), 135 (RPC), 139 (NetBIOS), 143 (IMAP)
443 (HTTPS), 445 (SMB), 993 (IMAPS), 995 (POP3S)
1433 (MSSQL), 1521 (Oracle), 3306 (MySQL), 3389 (RDP)
5432 (PostgreSQL), 5900 (VNC), 8080 (HTTP-Alt), 8443 (HTTPS-Alt)
```

## Note

- Il Cardputer deve essere connesso a WiFi per la maggior parte delle funzioni
- L'asterisco (*) in alto a destra indica connessione WiFi attiva
- Lo scan IP può richiedere 1-2 minuti per subnet /24
- Premi ESC durante gli scan per interrompere

## Limitazioni Hardware

- RAM limitata: max ~50 host in memoria per monitoring
- Display piccolo: alcune info vengono troncate
- Singola interfaccia WiFi: no sniffing promiscuo senza modifiche firmware

## Licenza

MIT - Usa liberamente per network engineering e security testing autorizzato.
