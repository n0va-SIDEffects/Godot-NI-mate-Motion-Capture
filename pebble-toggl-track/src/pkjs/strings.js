/*
 * Phone-side strings (messages sent to the watch and the settings page).
 * The language follows the watch's system language; English is the fallback.
 */

var TABLES = {
  en: {
    noToken: 'No API token. Please enter it in the Pebble app.',
    savedSettings: 'Settings saved',
    refreshing: 'Refreshing…', pleaseWait: 'Please wait…', noPrevious: 'No previous entry',
    noWorkspace: 'No workspace found',
    errToken: 'API token invalid', errPlan: 'Toggl: not in your plan', errNotFound: 'Toggl: not found',
    errRate: 'Toggl: too many requests', errHttp: 'Toggl error (HTTP %s)', errJson: 'Toggl: invalid reply',
    errOffline: 'No internet connection', errTimeout: 'Toggl not responding',
    // settings page
    title: 'Toggl Track', tagline: 'Start and stop timers right from your Pebble.',
    account: 'Account', token: 'API token', tokenPlaceholder: '32 characters (or “demo” to try it out)',
    tokenHint: 'Found at the bottom of <a href="https://track.toggl.com/profile" target="_blank">track.toggl.com/profile</a> ("API Token").',
    workspace: 'Workspace ID (optional)', workspacePlaceholder: 'default workspace',
    favorites: 'Favourites', favoritesText: 'Up to four tiles on the watch: description and project. One tap starts the entry.',
    favoritesNoProjects: 'Projects show up here once the app has connected to Toggl. Save the token first, then add favourites.',
    favoritePlaceholder: 'Favourite %s: description', noProject: 'No project',
    reminders: 'Reminders', remindRunning: 'Vibrate when a timer was forgotten', afterHours: 'after hours',
    orAtHour: 'or from hour', remindNoTimer: 'Remind on weekdays when no timer is running', fromHour: 'from hour',
    remindHint: 'The watch wakes the app for the reminder even when it is closed.',
    rounding: 'Rounding', roundLabel: 'When stopping, round to', roundOff: 'do not round', minutes: '%s minutes',
    save: 'Save',
    support: 'Support', supportText: 'This app is free and has no ads. If it makes your day a little easier, a coffee is much appreciated:',
    timeline: 'Timeline', timelinePin: 'Show the running timer as a pin on the timeline',
    timelineHint: 'The pin offers "Stop timer" right from the timeline.',
    pinRunning: 'Running', pinBody: 'Toggl timer running', pinStop: 'Stop timer', pinOpen: 'Open Toggl Timer',
    rateLimited: 'Toggl limit reached, pausing 5 min',
    coffee: '☕ Buy me a coffee'
  },
  de: {
    noToken: 'Kein API-Token. Bitte in der Pebble-App eintragen.',
    savedSettings: 'Einstellungen gespeichert',
    refreshing: 'Aktualisiere…', pleaseWait: 'Bitte warten…', noPrevious: 'Kein vorheriger Eintrag',
    noWorkspace: 'Kein Workspace gefunden',
    errToken: 'API-Token ungültig', errPlan: 'Toggl: Funktion nicht im Plan', errNotFound: 'Toggl: nicht gefunden',
    errRate: 'Toggl: zu viele Anfragen', errHttp: 'Toggl-Fehler (HTTP %s)', errJson: 'Toggl: ungültige Antwort',
    errOffline: 'Keine Internetverbindung', errTimeout: 'Toggl antwortet nicht',
    title: 'Toggl Track', tagline: 'Timer direkt von der Pebble starten und stoppen.',
    account: 'Konto', token: 'API-Token', tokenPlaceholder: '32 Zeichen (oder „demo“ zum Ausprobieren)',
    tokenHint: 'Zu finden unter <a href="https://track.toggl.com/profile" target="_blank">track.toggl.com/profile</a> ganz unten ("API Token").',
    workspace: 'Workspace-ID (optional)', workspacePlaceholder: 'Standard-Workspace',
    favorites: 'Favoriten', favoritesText: 'Bis zu vier Kacheln auf der Uhr: Beschreibung und Projekt. Ein Tipp startet den Eintrag.',
    favoritesNoProjects: 'Projekte erscheinen hier, sobald die App einmal mit Toggl verbunden war. Erst Token speichern, dann Favoriten anlegen.',
    favoritePlaceholder: 'Favorit %s: Beschreibung', noProject: 'Ohne Projekt',
    reminders: 'Erinnerungen', remindRunning: 'Vibrieren, wenn ein Timer vergessen wurde', afterHours: 'nach Stunden',
    orAtHour: 'oder ab Uhrzeit', remindNoTimer: 'Werktags erinnern, wenn kein Timer läuft', fromHour: 'ab Uhrzeit',
    remindHint: 'Die Uhr weckt die App zur Erinnerung auch, wenn sie geschlossen ist.',
    rounding: 'Rundung', roundLabel: 'Beim Stoppen runden auf', roundOff: 'nicht runden', minutes: '%s Minuten',
    save: 'Speichern',
    support: 'Unterstützen', supportText: 'Die App ist kostenlos und ohne Werbung. Wenn sie deinen Tag ein bisschen leichter macht, freue ich mich über einen Kaffee:',
    timeline: 'Timeline', timelinePin: 'Laufenden Timer als Pin auf der Timeline zeigen',
    timelineHint: 'Der Pin bietet "Timer stoppen" direkt aus der Timeline.',
    pinRunning: 'Läuft', pinBody: 'Toggl-Timer läuft', pinStop: 'Timer stoppen', pinOpen: 'Toggl Timer öffnen',
    rateLimited: 'Toggl-Limit erreicht, 5 Min Pause',
    coffee: '☕ Buy me a coffee'
  },
  fr: {
    noToken: 'Pas de jeton API. Saisissez-le dans l’app Pebble.',
    savedSettings: 'Réglages enregistrés',
    refreshing: 'Actualisation…', pleaseWait: 'Patientez…', noPrevious: 'Pas d’entrée précédente',
    noWorkspace: 'Aucun espace de travail',
    errToken: 'Jeton API invalide', errPlan: 'Toggl : pas dans votre offre', errNotFound: 'Toggl : introuvable',
    errRate: 'Toggl : trop de requêtes', errHttp: 'Erreur Toggl (HTTP %s)', errJson: 'Toggl : réponse invalide',
    errOffline: 'Pas de connexion Internet', errTimeout: 'Toggl ne répond pas',
    title: 'Toggl Track', tagline: 'Démarrez et arrêtez vos minuteurs depuis la Pebble.',
    account: 'Compte', token: 'Jeton API', tokenPlaceholder: '32 caractères (ou « demo » pour essayer)',
    tokenHint: 'En bas de <a href="https://track.toggl.com/profile" target="_blank">track.toggl.com/profile</a> ("API Token").',
    workspace: 'ID d’espace de travail (optionnel)', workspacePlaceholder: 'espace par défaut',
    favorites: 'Favoris', favoritesText: 'Jusqu’à quatre tuiles sur la montre : description et projet. Un tap démarre l’entrée.',
    favoritesNoProjects: 'Les projets apparaissent ici après la première connexion à Toggl. Enregistrez d’abord le jeton.',
    favoritePlaceholder: 'Favori %s : description', noProject: 'Sans projet',
    reminders: 'Rappels', remindRunning: 'Vibrer si un minuteur a été oublié', afterHours: 'après (heures)',
    orAtHour: 'ou à partir de (heure)', remindNoTimer: 'Rappeler en semaine si aucun minuteur ne tourne', fromHour: 'à partir de (heure)',
    remindHint: 'La montre réveille l’app pour le rappel même si elle est fermée.',
    rounding: 'Arrondi', roundLabel: 'À l’arrêt, arrondir à', roundOff: 'ne pas arrondir', minutes: '%s minutes',
    save: 'Enregistrer',
    support: 'Soutenir', supportText: 'L’app est gratuite et sans publicité. Si elle vous facilite la journée, un café fait plaisir :',
    timeline: 'Timeline', timelinePin: 'Afficher le minuteur en cours comme pin sur la timeline',
    timelineHint: 'Le pin propose « Arrêter » directement depuis la timeline.',
    pinRunning: 'En cours', pinBody: 'Minuteur Toggl en cours', pinStop: 'Arrêter le minuteur', pinOpen: 'Ouvrir Toggl Timer',
    rateLimited: 'Limite Toggl atteinte, pause 5 min',
    coffee: '☕ Buy me a coffee'
  },
  it: {
    noToken: 'Nessun token API. Inseriscilo nell’app Pebble.',
    savedSettings: 'Impostazioni salvate',
    refreshing: 'Aggiornamento…', pleaseWait: 'Attendere…', noPrevious: 'Nessuna voce precedente',
    noWorkspace: 'Nessun workspace trovato',
    errToken: 'Token API non valido', errPlan: 'Toggl: non incluso nel piano', errNotFound: 'Toggl: non trovato',
    errRate: 'Toggl: troppe richieste', errHttp: 'Errore Toggl (HTTP %s)', errJson: 'Toggl: risposta non valida',
    errOffline: 'Nessuna connessione Internet', errTimeout: 'Toggl non risponde',
    title: 'Toggl Track', tagline: 'Avvia e ferma i timer direttamente dal Pebble.',
    account: 'Account', token: 'Token API', tokenPlaceholder: '32 caratteri (o “demo” per provare)',
    tokenHint: 'In fondo a <a href="https://track.toggl.com/profile" target="_blank">track.toggl.com/profile</a> ("API Token").',
    workspace: 'ID workspace (opzionale)', workspacePlaceholder: 'workspace predefinito',
    favorites: 'Preferiti', favoritesText: 'Fino a quattro riquadri sull’orologio: descrizione e progetto. Un tocco avvia la voce.',
    favoritesNoProjects: 'I progetti compaiono qui dopo la prima connessione a Toggl. Salva prima il token.',
    favoritePlaceholder: 'Preferito %s: descrizione', noProject: 'Senza progetto',
    reminders: 'Promemoria', remindRunning: 'Vibra se un timer è stato dimenticato', afterHours: 'dopo ore',
    orAtHour: 'o dalle ore', remindNoTimer: 'Nei giorni feriali ricorda se nessun timer è attivo', fromHour: 'dalle ore',
    remindHint: 'L’orologio riattiva l’app per il promemoria anche se è chiusa.',
    rounding: 'Arrotondamento', roundLabel: 'Allo stop arrotonda a', roundOff: 'non arrotondare', minutes: '%s minuti',
    save: 'Salva',
    support: 'Sostieni', supportText: 'L’app è gratuita e senza pubblicità. Se ti semplifica la giornata, un caffè è molto gradito:',
    timeline: 'Timeline', timelinePin: 'Mostra il timer attivo come pin sulla timeline',
    timelineHint: 'Il pin offre "Ferma timer" direttamente dalla timeline.',
    pinRunning: 'In corso', pinBody: 'Timer Toggl attivo', pinStop: 'Ferma timer', pinOpen: 'Apri Toggl Timer',
    rateLimited: 'Limite Toggl raggiunto, pausa 5 min',
    coffee: '☕ Buy me a coffee'
  },
  es: {
    noToken: 'Sin token de API. Introdúcelo en la app de Pebble.',
    savedSettings: 'Ajustes guardados',
    refreshing: 'Actualizando…', pleaseWait: 'Espera…', noPrevious: 'No hay entrada anterior',
    noWorkspace: 'No se encontró workspace',
    errToken: 'Token de API no válido', errPlan: 'Toggl: no incluido en tu plan', errNotFound: 'Toggl: no encontrado',
    errRate: 'Toggl: demasiadas solicitudes', errHttp: 'Error de Toggl (HTTP %s)', errJson: 'Toggl: respuesta no válida',
    errOffline: 'Sin conexión a Internet', errTimeout: 'Toggl no responde',
    title: 'Toggl Track', tagline: 'Inicia y detén temporizadores desde tu Pebble.',
    account: 'Cuenta', token: 'Token de API', tokenPlaceholder: '32 caracteres (o “demo” para probar)',
    tokenHint: 'Al final de <a href="https://track.toggl.com/profile" target="_blank">track.toggl.com/profile</a> ("API Token").',
    workspace: 'ID de workspace (opcional)', workspacePlaceholder: 'workspace predeterminado',
    favorites: 'Favoritos', favoritesText: 'Hasta cuatro mosaicos en el reloj: descripción y proyecto. Un toque inicia la entrada.',
    favoritesNoProjects: 'Los proyectos aparecen aquí tras la primera conexión con Toggl. Guarda primero el token.',
    favoritePlaceholder: 'Favorito %s: descripción', noProject: 'Sin proyecto',
    reminders: 'Recordatorios', remindRunning: 'Vibrar si se olvidó un temporizador', afterHours: 'tras horas',
    orAtHour: 'o a partir de la hora', remindNoTimer: 'Recordar entre semana si no hay temporizador', fromHour: 'a partir de la hora',
    remindHint: 'El reloj despierta la app para el recordatorio aunque esté cerrada.',
    rounding: 'Redondeo', roundLabel: 'Al parar, redondear a', roundOff: 'no redondear', minutes: '%s minutos',
    save: 'Guardar',
    support: 'Apoyar', supportText: 'La app es gratuita y sin anuncios. Si te facilita el día, un café se agradece:',
    timeline: 'Timeline', timelinePin: 'Mostrar el temporizador activo como pin en la timeline',
    timelineHint: 'El pin ofrece "Parar" directamente desde la timeline.',
    pinRunning: 'Activo', pinBody: 'Temporizador Toggl activo', pinStop: 'Parar temporizador', pinOpen: 'Abrir Toggl Timer',
    rateLimited: 'Límite de Toggl, pausa de 5 min',
    coffee: '☕ Buy me a coffee'
  }
};

var current = TABLES.en;

// "de_DE", "fr", "en-GB" -> table; unknown -> English
function pickLanguage(code) {
  var short = String(code || '').slice(0, 2).toLowerCase();
  current = TABLES[short] || TABLES.en;
  return current;
}

// Language of the watch first, then the phone.
function detectLanguage() {
  var code = '';
  try { code = (Pebble.getActiveWatchInfo() || {}).language || ''; } catch (e) {}
  if (!code) {
    try { code = (typeof navigator !== 'undefined' && navigator.language) || ''; } catch (e) {}
  }
  return pickLanguage(code);
}

function currentCode() {
  for (var k in TABLES) { if (TABLES[k] === current) { return k; } }
  return 'en';
}

function t(key, arg) {
  var s = current[key] !== undefined ? current[key] : (TABLES.en[key] || key);
  return arg === undefined ? s : s.replace('%s', arg);
}

module.exports = { t: t, pickLanguage: pickLanguage, detectLanguage: detectLanguage, current: currentCode, TABLES: TABLES };
