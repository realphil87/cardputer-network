# Timbrature QR - La Tienda de Juan

Sistema di timbratura entrata/uscita dipendenti tramite QR code.
**Costo: ZERO** — usa solo Google Sheets e Google Apps Script (gratis).

---

## Come funziona

1. Ogni dipendente ha un **QR code personale** stampato e appeso nel locale
2. Il dipendente **scansiona il QR** col proprio telefono → si apre una pagina web
3. Inserisce il suo **PIN di 4 cifre**
4. Il sistema verifica che sia **fisicamente al bar** (GPS)
5. Registra **ENTRATA** o **USCITA** automaticamente nel Google Sheet
6. Il titolare vede tutto nel foglio e può esportare in Excel

### Anti-imbroglio
- **Geolocalizzazione GPS**: il dipendente deve trovarsi entro 100m dal bar
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
   - `BAR_LAT` e `BAR_LNG`: le coordinate GPS del bar
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
2. Clicca la rotella ⚙️ → seleziona **App web**
3. Impostazioni:
   - **Descrizione**: "Timbrature"
   - **Esegui come**: Il tuo account (te stesso)
   - **Chi ha accesso**: **Chiunque**
4. Clicca **Esegui deployment**
5. **Autorizza** l'app quando richiesto (clicca "Avanzate" → "Vai a..." se appare un avviso)
6. **Copia l'URL** che ti viene dato — ti serve per i QR code

### Passo 5: Setup iniziale del foglio

1. Torna al Google Sheet
2. Ricarica la pagina (F5)
3. Apparirà un menu **⏰ Timbrature** nella barra dei menu
4. Clicca **⏰ Timbrature → Setup iniziale**
5. Verranno creati i fogli "Timbrature" e "Riepilogo"

### Passo 6: Genera e stampa i QR code

1. Apri il file `genera-qr.html` nel tuo browser (doppio click)
2. Incolla l'URL della Web App copiato al Passo 4
3. Inserisci i nomi dei dipendenti (devono corrispondere a quelli in Code.gs)
4. Clicca **"Genera QR Code"**
5. Clicca **"Stampa"** e stampa il foglio
6. Ritaglia i QR code e appendili vicino all'ingresso del locale

---

## Uso quotidiano

### Per i dipendenti
1. Arrivando al lavoro: scansiona il tuo QR code → inserisci PIN → premi TIMBRA ENTRATA
2. Andando via: scansiona di nuovo → inserisci PIN → premi TIMBRA USCITA

### Per il titolare
- Apri il Google Sheet per vedere tutte le timbrature in tempo reale
- Usa **⏰ Timbrature → Aggiorna riepilogo** per vedere le ore totali del mese
- Cambia il mese nel foglio "Riepilogo" (cella B3) per vedere mesi diversi
- Per esportare in Excel: **File → Scarica → Microsoft Excel (.xlsx)**

---

## Struttura file

```
timbrature-qr/
├── Code.gs          ← Codice backend (da incollare in Apps Script)
├── Pagina.html      ← Pagina web dipendenti (da incollare in Apps Script)
├── genera-qr.html   ← Generatore QR code (da aprire nel browser)
└── README.md        ← Questo file
```

---

## Domande frequenti

**D: Costa qualcosa?**
R: No, è completamente gratuito. Usa Google Sheets (gratis) e Google Apps Script (gratis).

**D: Serve un server?**
R: No. Google fa tutto. Il cliente ha bisogno solo di un account Google.

**D: I dipendenti possono timbrare da casa?**
R: No. Il GPS verifica che siano entro 100 metri dal bar. Se sono più lontani, la timbratura viene rifiutata.

**D: Un dipendente può timbrare per un altro?**
R: Deve conoscere il PIN dell'altro E avere il suo QR code E essere fisicamente al bar. Per 3 dipendenti in un bar piccolo, è praticamente impossibile senza farsi notare.

**D: Cosa succede se un dipendente dimentica di timbrare l'uscita?**
R: La prossima volta che timbra, il sistema gli proporrà "USCITA" invece di "ENTRATA". Il titolare può anche correggere manualmente nel foglio.

**D: Posso aggiungere altri dipendenti?**
R: Sì. Modifica la sezione DIPENDENTI in Code.gs, aggiungi il nuovo dipendente, e rifai il deploy (Deploy → Gestisci distribuzioni → modifica → Nuova versione).

**D: Funziona su iPhone e Android?**
R: Sì, su qualsiasi smartphone con fotocamera e browser.
