/**
 * ============================================================
 *  TIMBRATURE QR - La Tienda de Juan
 * ============================================================
 *
 *  Costo: ZERO (Google Sheets + Apps Script)
 *  Anti-imbroglio: GPS + PIN personale
 *  Setup: 10 minuti
 *
 *  ISTRUZIONI:
 *  1. Crea un nuovo Google Sheet
 *  2. Estensioni → Apps Script
 *  3. Incolla questo codice in Code.gs
 *  4. Crea file HTML "Pagina" e incolla Pagina.html
 *  5. Modifica la CONFIGURAZIONE qui sotto
 *  6. Deploy → Nuova distribuzione → App web
 *     - Esegui come: Te stesso
 *     - Chi ha accesso: Chiunque
 *  7. Copia l'URL e facci UN QR code
 */

// ============================================================
//  CONFIGURAZIONE - MODIFICA QUESTI VALORI!
// ============================================================

const CONFIG = {
  // Coordinate GPS del bar
  // Come trovarle: cerca "La Tienda de Juan" su Google Maps
  // Tasto destro → "Che cosa c'è qui?" → copia le coordinate
  BAR_LAT: 45.4642,   // ← SOSTITUISCI con la latitudine del bar
  BAR_LNG: 9.1900,    // ← SOSTITUISCI con la longitudine del bar

  // Distanza massima dal bar (in metri) per poter timbrare
  MAX_DISTANZA_METRI: 100,

  // Dipendenti: chiave = ID, valore = { nome, pin }
  // Il PIN è un codice di 4 cifre scelto dal dipendente
  DIPENDENTI: {
    '1': { nome: 'Mario Rossi',    pin: '1234' },
    '2': { nome: 'Luigi Bianchi',  pin: '5678' },
    '3': { nome: 'Anna Verdi',     pin: '9012' }
  }
};

const NOME_FOGLIO_TIMBRATURE = 'Timbrature';
const NOME_FOGLIO_RIEPILOGO = 'Riepilogo';

// ============================================================
//  FUNZIONI PRINCIPALI - NON MODIFICARE
// ============================================================

/**
 * Serve la pagina quando il dipendente scansiona il QR code.
 * Un solo QR per tutti: la pagina mostra la lista dipendenti.
 */
function doGet() {
  // Costruisci la lista nomi (senza PIN) da passare alla pagina
  var listaDipendenti = {};
  for (var id in CONFIG.DIPENDENTI) {
    listaDipendenti[id] = CONFIG.DIPENDENTI[id].nome;
  }

  var template = HtmlService.createTemplateFromFile('Pagina');
  template.dipendentiJSON = JSON.stringify(listaDipendenti);
  template.barLat = CONFIG.BAR_LAT;
  template.barLng = CONFIG.BAR_LNG;
  template.maxDistanza = CONFIG.MAX_DISTANZA_METRI;

  return template.evaluate()
    .setTitle('Timbratura - La Tienda de Juan')
    .addMetaTag('viewport', 'width=device-width, initial-scale=1')
    .setXFrameOptionsMode(HtmlService.XFrameOptionsMode.ALLOWALL);
}

/**
 * Registra una timbratura nel foglio Google Sheets.
 * Chiamata dal client via google.script.run
 */
function registraTimbratura(data) {
  var dip = CONFIG.DIPENDENTI[data.employeeId];
  if (!dip) {
    return { success: false, errore: 'Dipendente non trovato.' };
  }

  if (data.pin !== dip.pin) {
    return { success: false, errore: 'PIN errato.' };
  }

  var distanza = calcolaDistanza(CONFIG.BAR_LAT, CONFIG.BAR_LNG, data.lat, data.lng);
  if (distanza > CONFIG.MAX_DISTANZA_METRI) {
    return {
      success: false,
      errore: 'Sei troppo lontano (' + Math.round(distanza) + 'm). Devi essere entro ' + CONFIG.MAX_DISTANZA_METRI + 'm dal locale.'
    };
  }

  var tipo = getProssimaAzione(data.employeeId);
  var now = new Date();

  var sheet = getOrCreateSheet(NOME_FOGLIO_TIMBRATURE);
  sheet.appendRow([
    now,
    dip.nome,
    tipo,
    Math.round(distanza),
    data.lat,
    data.lng,
    data.userAgent || 'N/A'
  ]);

  var lastRow = sheet.getLastRow();
  sheet.getRange(lastRow, 1).setNumberFormat('dd/MM/yyyy HH:mm:ss');

  return {
    success: true,
    tipo: tipo,
    ora: Utilities.formatDate(now, 'Europe/Rome', 'dd/MM/yyyy HH:mm:ss'),
    distanza: Math.round(distanza)
  };
}

/**
 * Restituisce 'ENTRATA' o 'USCITA' in base all'ultima timbratura
 */
function getProssimaAzione(employeeId) {
  var dip = CONFIG.DIPENDENTI[employeeId];
  if (!dip) return 'ENTRATA';

  var sheet = getOrCreateSheet(NOME_FOGLIO_TIMBRATURE);
  var data = sheet.getDataRange().getValues();

  for (var i = data.length - 1; i >= 1; i--) {
    if (data[i][1] === dip.nome) {
      return data[i][2] === 'ENTRATA' ? 'USCITA' : 'ENTRATA';
    }
  }

  return 'ENTRATA';
}

// ============================================================
//  UTILITÀ
// ============================================================

function calcolaDistanza(lat1, lon1, lat2, lon2) {
  var R = 6371000;
  var dLat = toRad(lat2 - lat1);
  var dLon = toRad(lon2 - lon1);
  var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
          Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) *
          Math.sin(dLon / 2) * Math.sin(dLon / 2);
  var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

function toRad(deg) {
  return deg * (Math.PI / 180);
}

function getOrCreateSheet(nome) {
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var sheet = ss.getSheetByName(nome);

  if (!sheet) {
    sheet = ss.insertSheet(nome);

    if (nome === NOME_FOGLIO_TIMBRATURE) {
      var headers = ['Data/Ora', 'Dipendente', 'Tipo', 'Distanza (m)', 'Latitudine', 'Longitudine', 'Dispositivo'];
      sheet.getRange(1, 1, 1, headers.length).setValues([headers]);
      sheet.getRange(1, 1, 1, headers.length)
        .setFontWeight('bold')
        .setBackground('#4285f4')
        .setFontColor('#ffffff');
      sheet.setColumnWidth(1, 170);
      sheet.setColumnWidth(2, 150);
      sheet.setColumnWidth(3, 100);
      sheet.setColumnWidth(4, 100);
      sheet.setColumnWidth(5, 120);
      sheet.setColumnWidth(6, 120);
      sheet.setColumnWidth(7, 200);
      sheet.setFrozenRows(1);
    }
  }

  return sheet;
}

// ============================================================
//  SETUP E MENU
// ============================================================

function setupIniziale() {
  getOrCreateSheet(NOME_FOGLIO_TIMBRATURE);
  creaFoglioRiepilogo();
  SpreadsheetApp.getUi().alert(
    'Setup completato!\n\n' +
    'Ora fai:\n' +
    '1. Deploy → Nuova distribuzione → App web\n' +
    '2. Esegui come: Te stesso\n' +
    '3. Chi ha accesso: Chiunque\n' +
    '4. Copia l\'URL e facci un QR code'
  );
}

function creaFoglioRiepilogo() {
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var sheet = ss.getSheetByName(NOME_FOGLIO_RIEPILOGO);
  if (sheet) ss.deleteSheet(sheet);

  sheet = ss.insertSheet(NOME_FOGLIO_RIEPILOGO);

  sheet.getRange('A1').setValue('RIEPILOGO ORE LAVORATE - LA TIENDA DE JUAN');
  sheet.getRange('A1').setFontSize(14).setFontWeight('bold');

  sheet.getRange('A3').setValue('Mese/Anno di riferimento:');
  sheet.getRange('B3').setValue(Utilities.formatDate(new Date(), 'Europe/Rome', 'MM/yyyy'));
  sheet.getRange('B3').setFontWeight('bold').setBackground('#fff2cc');

  var headers = ['Dipendente', 'Ore Totali Mese', 'Giorni Lavorati', 'Media Ore/Giorno'];
  sheet.getRange(5, 1, 1, headers.length).setValues([headers]);
  sheet.getRange(5, 1, 1, headers.length)
    .setFontWeight('bold')
    .setBackground('#4285f4')
    .setFontColor('#ffffff');

  var row = 6;
  for (var id in CONFIG.DIPENDENTI) {
    sheet.getRange(row, 1).setValue(CONFIG.DIPENDENTI[id].nome);
    row++;
  }

  sheet.getRange('A' + (row + 1)).setValue('Per aggiornare: menu Timbrature → Aggiorna riepilogo');
  sheet.getRange('A' + (row + 1)).setFontStyle('italic').setFontColor('#666666');

  sheet.setColumnWidth(1, 180);
  sheet.setColumnWidth(2, 150);
  sheet.setColumnWidth(3, 130);
  sheet.setColumnWidth(4, 150);
}

function aggiornaRiepilogo() {
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var sheetTimb = ss.getSheetByName(NOME_FOGLIO_TIMBRATURE);
  var sheetRiep = ss.getSheetByName(NOME_FOGLIO_RIEPILOGO);

  if (!sheetTimb || !sheetRiep) {
    SpreadsheetApp.getUi().alert('Errore: fogli non trovati. Esegui prima setupIniziale().');
    return;
  }

  var meseAnnoStr = sheetRiep.getRange('B3').getValue().toString();
  var parts = meseAnnoStr.split('/');
  var meseRif = parseInt(parts[0]) - 1;
  var annoRif = parseInt(parts[1]);

  var timbrature = sheetTimb.getDataRange().getValues();

  var row = 6;
  for (var id in CONFIG.DIPENDENTI) {
    var nome = CONFIG.DIPENDENTI[id].nome;

    var timbDip = [];
    for (var i = 1; i < timbrature.length; i++) {
      var data = new Date(timbrature[i][0]);
      if (timbrature[i][1] === nome &&
          data.getMonth() === meseRif &&
          data.getFullYear() === annoRif) {
        timbDip.push({ data: data, tipo: timbrature[i][2] });
      }
    }

    timbDip.sort(function(a, b) { return a.data - b.data; });

    var oreTotali = 0;
    var giorniSet = {};

    for (var i = 0; i < timbDip.length - 1; i++) {
      if (timbDip[i].tipo === 'ENTRATA' && timbDip[i + 1].tipo === 'USCITA') {
        var diff = (timbDip[i + 1].data - timbDip[i].data) / (1000 * 60 * 60);
        if (diff > 0 && diff <= 16) {
          oreTotali += diff;
          var giorno = Utilities.formatDate(timbDip[i].data, 'Europe/Rome', 'yyyy-MM-dd');
          giorniSet[giorno] = true;
        }
        i++;
      }
    }

    var giorniLavorati = Object.keys(giorniSet).length;
    var mediaOre = giorniLavorati > 0 ? oreTotali / giorniLavorati : 0;

    sheetRiep.getRange(row, 2).setValue(Math.round(oreTotali * 100) / 100);
    sheetRiep.getRange(row, 3).setValue(giorniLavorati);
    sheetRiep.getRange(row, 4).setValue(Math.round(mediaOre * 100) / 100);

    row++;
  }

  sheetRiep.getRange(6, 2, row - 6, 1).setNumberFormat('0.00');
  sheetRiep.getRange(6, 4, row - 6, 1).setNumberFormat('0.00');

  SpreadsheetApp.getUi().alert('Riepilogo aggiornato per ' + meseAnnoStr + '!');
}

function onOpen() {
  SpreadsheetApp.getUi()
    .createMenu('Timbrature')
    .addItem('Setup iniziale', 'setupIniziale')
    .addItem('Aggiorna riepilogo', 'aggiornaRiepilogo')
    .addItem('Mostra URL per QR code', 'mostraURL')
    .addToUi();
}

function mostraURL() {
  var url = ScriptApp.getService().getUrl();
  SpreadsheetApp.getUi().alert(
    'URL della web app (per il QR code):\n\n' + url + '\n\n' +
    'Fai un QR code con questo URL e appendilo nel locale.\n' +
    'I dipendenti lo scansionano, scelgono il loro nome e inseriscono il PIN.'
  );
}
