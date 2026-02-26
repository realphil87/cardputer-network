/**
 * ============================================================
 *  TIMBRATURE QR - Sistema di Timbratura per Piccole Attività
 * ============================================================
 *
 *  Costo: ZERO (usa Google Sheets + Apps Script)
 *  Anti-imbroglio: Geolocalizzazione + PIN personale
 *  Setup: 10 minuti
 *
 *  ISTRUZIONI:
 *  1. Crea un nuovo Google Sheet
 *  2. Vai su Estensioni → Apps Script
 *  3. Incolla questo codice in Code.gs
 *  4. Crea un file HTML chiamato "Pagina" e incolla il codice di Pagina.html
 *  5. Modifica la CONFIGURAZIONE qui sotto
 *  6. Fai Deploy → Nuova distribuzione → App web
 *     - Esegui come: Te stesso
 *     - Chi ha accesso: Chiunque
 *  7. Copia l'URL e generaci i QR code
 */

// ============================================================
//  CONFIGURAZIONE - MODIFICA QUESTI VALORI!
// ============================================================

const CONFIG = {
  // Coordinate GPS del bar
  // Come trovarle: cerca il bar su Google Maps → tasto destro → "Che cosa c'è qui?"
  // Clicca sulle coordinate che appaiono in basso e copiale
  BAR_LAT: 45.4642,   // ← SOSTITUISCI con la latitudine del bar
  BAR_LNG: 9.1900,    // ← SOSTITUISCI con la longitudine del bar

  // Distanza massima dal bar (in metri) per poter timbrare
  MAX_DISTANZA_METRI: 100,

  // Dipendenti: chiave = ID (usato nel QR), valore = { nome, pin }
  // Il PIN è un codice di 4 cifre che il dipendente deve inserire
  DIPENDENTI: {
    '1': { nome: 'Mario Rossi',    pin: '1234' },
    '2': { nome: 'Luigi Bianchi',  pin: '5678' },
    '3': { nome: 'Anna Verdi',     pin: '9012' }
  }
};

// Nome del foglio dove scrivere le timbrature (viene creato automaticamente)
const NOME_FOGLIO_TIMBRATURE = 'Timbrature';
const NOME_FOGLIO_RIEPILOGO = 'Riepilogo';

// ============================================================
//  FUNZIONI PRINCIPALI - NON MODIFICARE
// ============================================================

/**
 * Gestisce le richieste GET (quando il dipendente apre il link dal QR code)
 */
function doGet(e) {
  const id = e && e.parameter && e.parameter.id ? e.parameter.id : null;

  if (!id || !CONFIG.DIPENDENTI[id]) {
    return HtmlService.createHtmlOutput(
      '<html><body style="font-family:sans-serif;text-align:center;padding:40px;">' +
      '<h1>⚠️ Errore</h1><p>QR code non valido o dipendente non trovato.</p>' +
      '<p>Contatta il tuo datore di lavoro.</p></body></html>'
    );
  }

  const template = HtmlService.createTemplateFromFile('Pagina');
  template.employeeId = id;
  template.employeeName = CONFIG.DIPENDENTI[id].nome;
  template.barLat = CONFIG.BAR_LAT;
  template.barLng = CONFIG.BAR_LNG;
  template.maxDistanza = CONFIG.MAX_DISTANZA_METRI;

  return template.evaluate()
    .setTitle('Timbratura - ' + CONFIG.DIPENDENTI[id].nome)
    .addMetaTag('viewport', 'width=device-width, initial-scale=1')
    .setXFrameOptionsMode(HtmlService.XFrameOptionsMode.ALLOWALL);
}

/**
 * Registra una timbratura nel foglio Google Sheets
 * Chiamata dal client via google.script.run
 */
function registraTimbratura(data) {
  // Validazione dipendente
  const dip = CONFIG.DIPENDENTI[data.employeeId];
  if (!dip) {
    return { success: false, errore: 'Dipendente non trovato.' };
  }

  // Validazione PIN
  if (data.pin !== dip.pin) {
    return { success: false, errore: 'PIN errato.' };
  }

  // Validazione distanza
  const distanza = calcolaDistanza(CONFIG.BAR_LAT, CONFIG.BAR_LNG, data.lat, data.lng);
  if (distanza > CONFIG.MAX_DISTANZA_METRI) {
    return {
      success: false,
      errore: 'Sei troppo lontano dal bar (' + Math.round(distanza) + 'm). ' +
              'Devi essere entro ' + CONFIG.MAX_DISTANZA_METRI + 'm.'
    };
  }

  // Determina se è ENTRATA o USCITA
  const tipo = getProssimaAzione(data.employeeId);
  const now = new Date();

  // Scrivi nel foglio
  const sheet = getOrCreateSheet(NOME_FOGLIO_TIMBRATURE);
  sheet.appendRow([
    now,                           // A: Data/Ora
    dip.nome,                      // B: Dipendente
    tipo,                          // C: Tipo
    Math.round(distanza),          // D: Distanza (m)
    data.lat,                      // E: Latitudine
    data.lng,                      // F: Longitudine
    data.userAgent || 'N/A'        // G: Dispositivo
  ]);

  // Formatta la data nella colonna A
  const lastRow = sheet.getLastRow();
  sheet.getRange(lastRow, 1).setNumberFormat('dd/MM/yyyy HH:mm:ss');

  return {
    success: true,
    tipo: tipo,
    ora: Utilities.formatDate(now, 'Europe/Rome', 'dd/MM/yyyy HH:mm:ss'),
    distanza: Math.round(distanza)
  };
}

/**
 * Restituisce la prossima azione per un dipendente (ENTRATA o USCITA)
 * Chiamata anche dal client per mostrare il bottone giusto
 */
function getProssimaAzione(employeeId) {
  const dip = CONFIG.DIPENDENTI[employeeId];
  if (!dip) return 'ENTRATA';

  const sheet = getOrCreateSheet(NOME_FOGLIO_TIMBRATURE);
  const data = sheet.getDataRange().getValues();

  // Cerca l'ultima azione di questo dipendente (parti dal basso)
  for (let i = data.length - 1; i >= 1; i--) {
    if (data[i][1] === dip.nome) {
      return data[i][2] === 'ENTRATA' ? 'USCITA' : 'ENTRATA';
    }
  }

  return 'ENTRATA';
}

/**
 * Restituisce il nome del dipendente dato il suo ID
 * Usato dal client per conferma visuale
 */
function getNomeDipendente(employeeId) {
  const dip = CONFIG.DIPENDENTI[employeeId];
  return dip ? dip.nome : null;
}

// ============================================================
//  FUNZIONI DI UTILITÀ
// ============================================================

/**
 * Calcola la distanza in metri tra due coordinate GPS (formula Haversine)
 */
function calcolaDistanza(lat1, lon1, lat2, lon2) {
  const R = 6371000; // Raggio della Terra in metri
  const dLat = toRad(lat2 - lat1);
  const dLon = toRad(lon2 - lon1);
  const a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

function toRad(deg) {
  return deg * (Math.PI / 180);
}

/**
 * Ottiene o crea il foglio specificato con le intestazioni
 */
function getOrCreateSheet(nome) {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let sheet = ss.getSheetByName(nome);

  if (!sheet) {
    sheet = ss.insertSheet(nome);

    if (nome === NOME_FOGLIO_TIMBRATURE) {
      // Intestazioni
      const headers = ['Data/Ora', 'Dipendente', 'Tipo', 'Distanza (m)', 'Latitudine', 'Longitudine', 'Dispositivo'];
      sheet.getRange(1, 1, 1, headers.length).setValues([headers]);

      // Formattazione intestazioni
      sheet.getRange(1, 1, 1, headers.length)
        .setFontWeight('bold')
        .setBackground('#4285f4')
        .setFontColor('#ffffff');

      // Larghezza colonne
      sheet.setColumnWidth(1, 170); // Data/Ora
      sheet.setColumnWidth(2, 150); // Dipendente
      sheet.setColumnWidth(3, 100); // Tipo
      sheet.setColumnWidth(4, 100); // Distanza
      sheet.setColumnWidth(5, 120); // Lat
      sheet.setColumnWidth(6, 120); // Lng
      sheet.setColumnWidth(7, 200); // Dispositivo

      // Blocca riga intestazione
      sheet.setFrozenRows(1);
    }
  }

  return sheet;
}

/**
 * Funzione di setup iniziale - eseguila una volta per creare i fogli
 * Menu: Esegui → setupIniziale
 */
function setupIniziale() {
  getOrCreateSheet(NOME_FOGLIO_TIMBRATURE);
  creaFoglioRiepilogo();
  SpreadsheetApp.getUi().alert(
    'Setup completato!\n\n' +
    'Ora fai:\n' +
    '1. Deploy → Nuova distribuzione → App web\n' +
    '2. Esegui come: Te stesso\n' +
    '3. Chi ha accesso: Chiunque\n' +
    '4. Copia l\'URL generato'
  );
}

/**
 * Crea il foglio riepilogo con le formule per calcolare le ore
 */
function creaFoglioRiepilogo() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let sheet = ss.getSheetByName(NOME_FOGLIO_RIEPILOGO);
  if (sheet) ss.deleteSheet(sheet);

  sheet = ss.insertSheet(NOME_FOGLIO_RIEPILOGO);

  // Intestazione
  sheet.getRange('A1').setValue('RIEPILOGO ORE LAVORATE');
  sheet.getRange('A1').setFontSize(14).setFontWeight('bold');

  sheet.getRange('A3').setValue('Mese/Anno di riferimento:');
  sheet.getRange('B3').setValue(Utilities.formatDate(new Date(), 'Europe/Rome', 'MM/yyyy'));
  sheet.getRange('B3').setFontWeight('bold').setBackground('#fff2cc');

  // Tabella riepilogo per dipendente
  const headers = ['Dipendente', 'Ore Totali Mese', 'Giorni Lavorati', 'Media Ore/Giorno'];
  sheet.getRange(5, 1, 1, headers.length).setValues([headers]);
  sheet.getRange(5, 1, 1, headers.length)
    .setFontWeight('bold')
    .setBackground('#4285f4')
    .setFontColor('#ffffff');

  let row = 6;
  for (const id in CONFIG.DIPENDENTI) {
    const nome = CONFIG.DIPENDENTI[id].nome;
    sheet.getRange(row, 1).setValue(nome);
    // Le ore verranno calcolate con la funzione aggiornaRiepilogo()
    row++;
  }

  sheet.getRange('A' + (row + 1)).setValue('→ Per aggiornare: menu Timbrature → Aggiorna riepilogo');
  sheet.getRange('A' + (row + 1)).setFontStyle('italic').setFontColor('#666666');

  // Larghezze colonne
  sheet.setColumnWidth(1, 180);
  sheet.setColumnWidth(2, 150);
  sheet.setColumnWidth(3, 130);
  sheet.setColumnWidth(4, 150);
}

/**
 * Aggiorna il riepilogo ore per il mese specificato nel foglio Riepilogo
 */
function aggiornaRiepilogo() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const sheetTimb = ss.getSheetByName(NOME_FOGLIO_TIMBRATURE);
  const sheetRiep = ss.getSheetByName(NOME_FOGLIO_RIEPILOGO);

  if (!sheetTimb || !sheetRiep) {
    SpreadsheetApp.getUi().alert('Errore: fogli non trovati. Esegui prima setupIniziale().');
    return;
  }

  // Leggi il mese di riferimento
  const meseAnnoStr = sheetRiep.getRange('B3').getValue().toString();
  const parts = meseAnnoStr.split('/');
  const meseRif = parseInt(parts[0]) - 1; // 0-indexed
  const annoRif = parseInt(parts[1]);

  // Leggi tutte le timbrature
  const timbrature = sheetTimb.getDataRange().getValues();

  // Calcola ore per ogni dipendente
  let row = 6;
  for (const id in CONFIG.DIPENDENTI) {
    const nome = CONFIG.DIPENDENTI[id].nome;

    // Filtra timbrature di questo dipendente nel mese
    const timbDip = [];
    for (let i = 1; i < timbrature.length; i++) {
      const data = new Date(timbrature[i][0]);
      if (timbrature[i][1] === nome &&
          data.getMonth() === meseRif &&
          data.getFullYear() === annoRif) {
        timbDip.push({
          data: data,
          tipo: timbrature[i][2]
        });
      }
    }

    // Ordina per data
    timbDip.sort(function(a, b) { return a.data - b.data; });

    // Calcola ore: accoppia ENTRATA-USCITA
    let oreTotali = 0;
    const giorniSet = {};

    for (let i = 0; i < timbDip.length - 1; i++) {
      if (timbDip[i].tipo === 'ENTRATA' && timbDip[i + 1].tipo === 'USCITA') {
        const diff = (timbDip[i + 1].data - timbDip[i].data) / (1000 * 60 * 60);
        // Ignora sessioni superiori a 16 ore (probabile errore)
        if (diff > 0 && diff <= 16) {
          oreTotali += diff;
          const giorno = Utilities.formatDate(timbDip[i].data, 'Europe/Rome', 'yyyy-MM-dd');
          giorniSet[giorno] = true;
        }
        i++; // Salta l'uscita
      }
    }

    const giorniLavorati = Object.keys(giorniSet).length;
    const mediaOre = giorniLavorati > 0 ? oreTotali / giorniLavorati : 0;

    sheetRiep.getRange(row, 2).setValue(Math.round(oreTotali * 100) / 100);
    sheetRiep.getRange(row, 3).setValue(giorniLavorati);
    sheetRiep.getRange(row, 4).setValue(Math.round(mediaOre * 100) / 100);

    row++;
  }

  // Formatta numeri
  sheetRiep.getRange(6, 2, row - 6, 1).setNumberFormat('0.00');
  sheetRiep.getRange(6, 4, row - 6, 1).setNumberFormat('0.00');

  SpreadsheetApp.getUi().alert('Riepilogo aggiornato per ' + meseAnnoStr + '!');
}

/**
 * Aggiunge menu personalizzato al Google Sheet
 */
function onOpen() {
  SpreadsheetApp.getUi()
    .createMenu('⏰ Timbrature')
    .addItem('Setup iniziale', 'setupIniziale')
    .addItem('Aggiorna riepilogo', 'aggiornaRiepilogo')
    .addItem('Mostra URL per QR code', 'mostraURL')
    .addToUi();
}

/**
 * Mostra l'URL della web app per generare i QR code
 */
function mostraURL() {
  const url = ScriptApp.getService().getUrl();
  let msg = 'URL base della web app:\n' + url + '\n\n';
  msg += 'URL per ogni dipendente (da trasformare in QR code):\n\n';

  for (const id in CONFIG.DIPENDENTI) {
    msg += CONFIG.DIPENDENTI[id].nome + ':\n';
    msg += url + '?id=' + id + '\n\n';
  }

  SpreadsheetApp.getUi().alert(msg);
}
