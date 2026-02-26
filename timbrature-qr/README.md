# Timbrature QR - La Tienda de Juan

Sistema di timbratura entrata/uscita dipendenti tramite QR code.
**Costo: ZERO** — usa solo Google Sheets e Google Apps Script (gratis).

---

## Come funziona

1. **Un unico QR code** stampato e appeso vicino all'ingresso del locale
2. Il dipendente **scansiona il QR** col proprio telefono
3. **Seleziona il suo nome** dalla lista
4. Inserisce il suo **PIN di 4 cifre**
5. Il sistema verifica che sia **fisicamente al bar** (GPS)
6. Registra **ENTRATA** o **USCITA** automaticamente nel Google Sheet
7. Il titolare vede tutto nel foglio e può esportare in Excel

### Anti-imbroglio
- **Geolocalizzazione GPS**: il dipendente deve trovarsi entro 100m dal locale
- **PIN personale**: ogni dipendente ha un codice segreto di 4 cifre
- **Registro dispositivo**: viene salvato il tipo di telefono usato
- **Alternanza automatica**: il sistema alterna ENTRATA/USCITA, non si può timbrare due entrate di fila

---

## Setup (10 minuti)

### Passo 1: Crea il Google Sheet

1. Vai su [Google Sheets](https://sheets.google.com) e crea un **nuovo foglio vuoto**
2. Rinominalo "Timbrature La Tienda de Juan" (o come preferisci)

### Passo 2: Aggiungi il codice Apps Script

1. Nel Google Sheet vai su **Estensioni → Apps Script**
2. Si apre l'editor di codice. Cancella tutto quello che c'è
3. Copia e incolla il contenuto del file `Code.gs` di questa cartella
4. **MODIFICA la sezione CONFIGURAZIONE** in cima al file:
   - `BAR_LAT` e `BAR_LNG`: le coordinate GPS del locale
     - Per trovarle: cerca "La Tienda de Juan Milano" su Google Maps
     - Tasto destro sulla posizione → "Che cosa c'è qui?"
     - Clicca sulle coordinate che appaiono in basso e copiale
   - `DIPENDENTI`: modifica i nomi e i PIN dei 3 dipendenti

### Passo 3: Aggiungi la pagina HTML

1. Nell'editor di Apps Script, clicca su **+ → HTML**
2. Nomina il file **Pagina** (senza .html, lo aggiunge automaticamente)
3. Cancella il contenuto predefinito
4. Copia e incolla il contenuto del file `Pagina.html` di questa cartella

### Passo 4: Deploy (pubblica la web app)

1. Clicca **Deploy → Nuova distribuzione**
2. Clicca la rotella → seleziona **App web**
3. Impostazioni:
   - **Descrizione**: "Timbrature"
   - **Esegui come**: Il tuo account (te stesso)
   - **Chi ha accesso**: **Chiunque**
4. Clicca **Esegui deployment**
5. **Autorizza** l'app quando richiesto (clicca "Avanzate" → "Vai a..." se appare un avviso)
6. **Copia l'URL** che ti viene dato

### Passo 5: Setup iniziale del foglio

1. Torna al Google Sheet
2. Ricarica la pagina (F5)
3. Apparirà un menu **Timbrature** nella barra dei menu
4. Clicca **Timbrature → Setup iniziale**
5. Verranno creati i fogli "Timbrature" e "Riepilogo"

### Passo 6: Genera e stampa il QR code

1. Apri il file `genera-qr.html` nel tuo browser (doppio click)
2. Incolla l'URL della Web App copiato al Passo 4
3. Clicca **"Genera QR Code"**
4. Clicca **"Stampa"**
5. Appendi il foglio vicino all'ingresso del locale

---

## Uso quotidiano

### Per i dipendenti
1. Scansiona il QR code col telefono
2. Tocca il tuo nome
3. Inserisci il tuo PIN di 4 cifre
4. Premi **TIMBRA** (il sistema sa da solo se è ENTRATA o USCITA)

### Per il titolare
- Apri il Google Sheet per vedere tutte le timbrature in tempo reale
- Usa **Timbrature → Aggiorna riepilogo** per vedere le ore totali del mese
- Cambia il mese nel foglio "Riepilogo" (cella B3) per vedere mesi diversi
- Per esportare in Excel: **File → Scarica → Microsoft Excel (.xlsx)**

---

## Struttura file

```
timbrature-qr/
├── Code.gs          ← Backend (da incollare in Apps Script)
├── Pagina.html      ← Pagina web (da incollare in Apps Script)
├── genera-qr.html   ← Generatore QR code (apri nel browser per stampare)
└── README.md        ← Questo file
```

---

## Domande frequenti

**D: Costa qualcosa?**
R: No. Google Sheets e Apps Script sono gratuiti.

**D: Serve un server?**
R: No. Google fa tutto. Serve solo un account Google.

**D: I dipendenti possono timbrare da casa?**
R: No. Il GPS verifica che siano entro 100 metri dal locale.

**D: Un dipendente può timbrare per un altro?**
R: Dovrebbe conoscere il suo PIN ed essere fisicamente al bar. Per 3 dipendenti in un locale piccolo, il titolare se ne accorgerebbe.

**D: Cosa succede se uno dimentica di timbrare l'uscita?**
R: La prossima volta il sistema proporrà "USCITA". Il titolare può anche correggere manualmente nel foglio.

**D: Posso aggiungere dipendenti?**
R: Sì. Modifica DIPENDENTI in Code.gs, rifai il deploy (Deploy → Gestisci distribuzioni → Modifica → Nuova versione).

**D: Funziona su iPhone e Android?**
R: Sì, su qualsiasi smartphone con fotocamera e browser.
