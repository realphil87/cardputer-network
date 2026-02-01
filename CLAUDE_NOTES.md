# Note di Sviluppo - Network Toolkit per M5Stack Cardputer

## Repository
- **GitHub**: https://github.com/realphil87/cardputer-network
- **Branch principale**: main

## Struttura Progetto
```
.
├── NetworkToolkit.ino      # Codice principale (~43KB)
├── platformio.ini          # Configurazione PlatformIO
├── README.md               # Documentazione utente
├── CLAUDE_NOTES.md         # Questo file
└── .github/workflows/
    └── build.yml           # GitHub Actions per build automatica
```

## Build Automatica (GitHub Actions)

### Come funziona
- **Trigger**: push su main, PR su main, o manuale (workflow_dispatch)
- **Output**: `NetworkToolkit.bin` scaricabile
- **Release**: creata automaticamente su ogni push a main

### Download firmware
1. Vai su: https://github.com/realphil87/cardputer-network/actions
2. Clicca sull'ultima build riuscita
3. Scarica da **Artifacts** → `NetworkToolkit-firmware`
4. Oppure dalla sezione **Releases**

### Configurazione importante nel workflow
```yaml
permissions:
  contents: write  # Necessario per creare release
```

## PlatformIO

### Configurazione chiave (`platformio.ini`)
```ini
[platformio]
src_dir = .  # IMPORTANTE: il .ino è nella root, non in src/

[env:m5stack-cardputer]
platform = espressif32
board = m5stack-stamps3
framework = arduino
```

### Librerie
```ini
lib_deps =
    m5stack/M5Cardputer@^1.0.1      # NON forzare versioni di M5Unified/M5GFX
    https://github.com/dvarrel/ESPping.git  # Per ping (usa ESPping.h)
    ricmoo/QRCode@^0.0.1
```

**ATTENZIONE**: Non specificare `m5stack/M5Unified` o `m5stack/M5GFX` con versioni forzate - lasciare che M5Cardputer gestisca le sue dipendenze per evitare conflitti.

## Compatibilità M5Cardputer 1.1.x

### Cambio API: KeysState.word
Nella versione 1.1.x, `Keyboard_Class::KeysState::word` è cambiato da `String` a `vector<char>`.

**Soluzione**: funzione helper nel codice:
```cpp
String keyWord(const std::vector<char>& word) {
    if(word.empty()) return "";
    return String(word.data(), word.size());
}
```

Usare `keyWord(status.word)` invece di `status.word` per i confronti.

### Costanti tastiera mancanti
Definire manualmente se non presenti:
```cpp
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
```

## SSH Setup (Mac locale)

### Chiave SSH per GitHub
- **File**: `~/.ssh/github_cardputer` (ed25519)
- **Config** (`~/.ssh/config`):
```
Host github.com
    HostName github.com
    User git
    IdentityFile ~/.ssh/github_cardputer
    IdentitiesOnly yes
```

### Comandi utili
```bash
# Push modifiche
git add . && git commit -m "messaggio" && git push

# Vedere stato
git status
git log --oneline -5
```

## Problemi Risolti

| Problema | Soluzione |
|----------|-----------|
| `Nothing to build` | Aggiungere `src_dir = .` in sezione `[platformio]` |
| `ESPping.h not found` | Usare `https://github.com/dvarrel/ESPping.git` |
| `status.word == "..."` non compila | Usare `keyWord(status.word) == "..."` |
| `KEY_ESC/UP/DOWN not declared` | Definire le costanti manualmente |
| `M5.getButton` not found | Non forzare versioni vecchie di M5Unified |
| Release 403 error | Aggiungere `permissions: contents: write` |

## Flash sul Cardputer

1. Scarica `NetworkToolkit.bin` dalle Actions/Releases
2. Collega Cardputer via USB
3. Usa esptool o M5Burner:
```bash
esptool.py --chip esp32s3 --port /dev/tty.usbmodem* write_flash 0x0 NetworkToolkit.bin
```

Oppure con PlatformIO locale:
```bash
pio run -t upload
```
