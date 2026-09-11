#include "i18n.h"

#include <pebble.h>

enum { L_EN = 0, L_DE, L_FR, L_IT, L_ES, L_COUNT };

static int s_lang = L_EN;

static const char *const s_strings[S_COUNT][L_COUNT] = {
  //                  en                              de                                   fr                                   it                                   es
  [S_APP_NAME]        = { "Toggl Track", "Toggl Track", "Toggl Track", "Toggl Track", "Toggl Track" },
  [S_NO_PROJECT]      = { "No project", "Kein Projekt", "Sans projet", "Senza progetto", "Sin proyecto" },
  [S_NO_TIMER]        = { "No timer running", "Kein Timer läuft", "Aucun minuteur", "Nessun timer attivo", "Sin temporizador" },
  [S_CONNECTING]      = { "Connecting to phone…", "Verbinde mit Handy…", "Connexion au téléphone…", "Connessione al telefono…", "Conectando al móvil…" },
  [S_NOT_CONNECTED]   = { "Not connected", "Nicht verbunden", "Non connecté", "Non connesso", "Sin conexión" },
  [S_NO_DESCRIPTION]  = { "(no description)", "(ohne Beschreibung)", "(sans description)", "(senza descrizione)", "(sin descripción)" },
  [S_SELECT_TO_START] = { "SELECT: start timer", "SELECT: Timer starten", "SELECT : démarrer", "SELECT: avvia timer", "SELECT: iniciar" },
  [S_SINCE]           = { "since %s", "seit %s", "depuis %s", "dalle %s", "desde %s" },
  [S_TODAY_H]         = { "today %s h", "heute %s h", "auj. %s h", "oggi %s h", "hoy %s h" },
  [S_TODAY_BOOKED]    = { "Today %s h tracked", "Heute %s h gebucht", "Aujourd'hui %s h", "Oggi %s h", "Hoy %s h" },
  [S_REFRESHING]      = { "Refreshing…", "Aktualisiere…", "Actualisation…", "Aggiornamento…", "Actualizando…" },
  [S_STOPPING]        = { "Stopping…", "Stoppe…", "Arrêt…", "Arresto…", "Deteniendo…" },
  [S_STARTING]        = { "Starting…", "Starte…", "Démarrage…", "Avvio…", "Iniciando…" },
  [S_PLEASE_WAIT]     = { "Please wait…", "Bitte warten…", "Patientez…", "Attendere…", "Espera…" },
  [S_SEND_ERROR]      = { "Send error", "Sendefehler", "Erreur d'envoi", "Errore di invio", "Error de envío" },
  [S_NO_PHONE]        = { "No connection to phone", "Keine Verbindung zum Handy", "Pas de connexion au téléphone", "Nessuna connessione al telefono", "Sin conexión con el móvil" },
  [S_NO_REPLY]        = { "No reply from phone", "Keine Antwort vom Handy", "Pas de réponse du téléphone", "Nessuna risposta dal telefono", "Sin respuesta del móvil" },
  [S_STARTED]         = { "Started: %s", "Gestartet: %s", "Démarré : %s", "Avviato: %s", "Iniciado: %s" },
  [S_STOPPED]         = { "Stopped", "Gestoppt", "Arrêté", "Fermato", "Detenido" },
  [S_TIMER]           = { "Timer", "Timer", "Minuteur", "Timer", "Temporizador" },
  [S_PREVIOUS_ENTRY]  = { "Previous entry…", "Vorheriger Eintrag…", "Entrée précédente…", "Voce precedente…", "Entrada anterior…" },
  [S_RECENT]          = { "Recent", "Zuletzt", "Récents", "Recenti", "Recientes" },
  [S_PROJECTS]        = { "Projects", "Projekte", "Projets", "Progetti", "Proyectos" },
  [S_PICK_PROJECT]    = { "Pick a project", "Projekt wählen", "Choisir un projet", "Scegli progetto", "Elegir proyecto" },
  [S_START_EMPTY]     = { "Start an empty timer", "Leeren Timer starten", "Démarrer sans projet", "Avvia timer vuoto", "Iniciar sin proyecto" },
  [S_DICTATE]         = { "Dictate", "Diktieren", "Dicter", "Dettare", "Dictar" },
  [S_LIST]            = { "List", "Liste", "Liste", "Elenco", "Lista" },
  [S_FAVORITE]        = { "Favourite", "Favorit", "Favori", "Preferito", "Favorito" },
  [S_NO_FAVORITES]    = { "Add favourites in the settings of the Pebble app", "Favoriten in den Einstellungen der Pebble-App anlegen", "Ajoutez des favoris dans les réglages de l'app Pebble", "Aggiungi preferiti nelle impostazioni dell'app Pebble", "Añade favoritos en los ajustes de la app Pebble" },
  [S_DICT_NO_CONNECTION] = { "Dictation: no connection", "Diktat: keine Verbindung", "Dictée : pas de connexion", "Dettatura: nessuna connessione", "Dictado: sin conexión" },
  [S_DICT_DISABLED]   = { "Dictation is disabled", "Diktat ist deaktiviert", "Dictée désactivée", "Dettatura disattivata", "Dictado desactivado" },
  [S_DICT_NOTHING]    = { "Nothing understood", "Nichts verstanden", "Rien compris", "Non capito", "No se entendió" },
  [S_DICT_CANCELLED]  = { "Dictation cancelled", "Diktat abgebrochen", "Dictée annulée", "Dettatura annullata", "Dictado cancelado" },
  [S_DICT_UNAVAILABLE]= { "Dictation unavailable", "Diktat nicht verfügbar", "Dictée indisponible", "Dettatura non disponibile", "Dictado no disponible" },
  [S_NO_MICROPHONE]   = { "This watch has no microphone", "Diese Uhr hat kein Mikrofon", "Cette montre n'a pas de micro", "Questo orologio non ha microfono", "Este reloj no tiene micrófono" },
  [S_HINT_STILL_RUNNING] = { "Still running? SELECT = stop", "Läuft noch? SELECT = stoppen", "Toujours en cours ? SELECT = stop", "Ancora attivo? SELECT = stop", "¿Sigue activo? SELECT = parar" },
  [S_HINT_NO_TIMER]   = { "No timer running. Start one?", "Kein Timer läuft. Starten?", "Aucun minuteur. Démarrer ?", "Nessun timer. Avviare?", "Sin temporizador. ¿Iniciar?" },
  [S_GLANCE_IDLE]     = { "No timer running", "Kein Timer läuft", "Aucun minuteur", "Nessun timer attivo", "Sin temporizador" },
};

void i18n_init(void) {
  const char *locale = i18n_get_system_locale();   // e.g. "de_DE"
  s_lang = L_EN;
  if (locale && locale[0] && locale[1]) {
    if (locale[0] == 'd' && locale[1] == 'e') s_lang = L_DE;
    else if (locale[0] == 'f' && locale[1] == 'r') s_lang = L_FR;
    else if (locale[0] == 'i' && locale[1] == 't') s_lang = L_IT;
    else if (locale[0] == 'e' && locale[1] == 's') s_lang = L_ES;
  }
}

const char *STR(StringId id) {
  if ((unsigned)id >= S_COUNT) {
    return "";
  }
  const char *s = s_strings[id][s_lang];
  return s ? s : s_strings[id][L_EN];
}
