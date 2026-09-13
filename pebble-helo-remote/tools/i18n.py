#!/usr/bin/env python3
"""Einzige Quelle aller Übersetzungen von HELO Remote. Erzeugt
  src/c/strings_i18n.h   (Texte der Uhr)
  src/pkjs/i18n.js       (Texte der Telefon-Seite und der Konfigurationsseite)
Aufruf aus dem Projektordner: python3 tools/i18n.py
Sprachindex (LANGUAGE-Key): 0 = automatisch, 1..7 = Reihenfolge in LANGS."""
import os

LANGS = ["de", "en", "fr", "es", "it", "pt", "nl"]
NATIVE = {"de": "Deutsch", "en": "English", "fr": "Français", "es": "Español",
          "it": "Italiano", "pt": "Português", "nl": "Nederlands"}

# ------------------------------------------------------------------ Uhr ----
# media_* sind printf-Formate mit %d%% (Prozent). Hinweise müssen auf 148 px
# (emery, Gothic 14) passen, sonst werden sie mit "…" gekürzt.
W = {}
W["de"] = dict(
    st_init="Init", st_idle="Bereit", st_rec="AUFNAHME", st_live="LIVE", st_failed="Fehler", st_off="Aus",
    media_free="Medien %d%% frei", media_short="Medien %d%%", media_none="Medien --",
    no_data="Keine Daten vom Telefon", phone_busy="Telefon beschäftigt...", send_failed="Senden fehlgeschlagen",
    rec_running="Aufnahme läuft", stream_running="Stream läuft", rec_stopped="Aufnahme gestoppt", stream_stopped="Stream gestoppt",
    phone_unreachable="Telefon nicht erreichbar", set_ip_app="IP in App-Einstellungen setzen", helo_not_connected="HELO nicht verbunden",
    stopping_rec="Stoppe Aufnahme...", stopping_stream="Stoppe Stream...", confirm_rec="Stopp? Nochmal OBEN", confirm_stream="Stopp? Nochmal UNTEN",
    starting_rec="Starte Aufnahme...", starting_stream="Starte Stream...", cancelled="Abgebrochen", refreshing="Aktualisiere...",
    waiting_phone="Warte auf Telefon...", helo_unreachable="HELO nicht erreichbar", check_password="HELO: Passwort prüfen",
    set_ip="IP in Einstellungen setzen", sending="Sende Befehl...")
W["en"] = dict(
    st_init="Init", st_idle="Ready", st_rec="RECORDING", st_live="LIVE", st_failed="Error", st_off="Off",
    media_free="Media %d%% free", media_short="Media %d%%", media_none="Media --",
    no_data="No data from phone", phone_busy="Phone busy...", send_failed="Send failed",
    rec_running="Recording", stream_running="Streaming", rec_stopped="Recording stopped", stream_stopped="Stream stopped",
    phone_unreachable="Phone not reachable", set_ip_app="Set IP in app settings", helo_not_connected="HELO not connected",
    stopping_rec="Stopping recording...", stopping_stream="Stopping stream...", confirm_rec="Stop? Press UP again", confirm_stream="Stop? Press DOWN again",
    starting_rec="Starting recording...", starting_stream="Starting stream...", cancelled="Cancelled", refreshing="Refreshing...",
    waiting_phone="Waiting for phone...", helo_unreachable="HELO not reachable", check_password="HELO: check password",
    set_ip="Set IP in settings", sending="Sending command...")
W["fr"] = dict(
    st_init="Init", st_idle="Prêt", st_rec="ENREG.", st_live="LIVE", st_failed="Erreur", st_off="Arrêt",
    media_free="Média %d%% libre", media_short="Média %d%%", media_none="Média --",
    no_data="Pas de données du tél.", phone_busy="Téléphone occupé...", send_failed="Envoi échoué",
    rec_running="Enregistrement en cours", stream_running="Stream en cours", rec_stopped="Enregistrement arrêté", stream_stopped="Stream arrêté",
    phone_unreachable="Téléphone injoignable", set_ip_app="IP dans les réglages", helo_not_connected="HELO non connecté",
    stopping_rec="Arrêt enregistrement...", stopping_stream="Arrêt du stream...", confirm_rec="Stop ? HAUT encore", confirm_stream="Stop ? BAS encore",
    starting_rec="Démarrage enreg....", starting_stream="Démarrage stream...", cancelled="Annulé", refreshing="Actualisation...",
    waiting_phone="Attente du téléphone...", helo_unreachable="HELO injoignable", check_password="HELO : mot de passe ?",
    set_ip="IP dans les réglages", sending="Envoi de la commande...")
W["es"] = dict(
    st_init="Init", st_idle="Listo", st_rec="GRABANDO", st_live="LIVE", st_failed="Error", st_off="Apagado",
    media_free="Medios %d%% libres", media_short="Medios %d%%", media_none="Medios --",
    no_data="Sin datos del móvil", phone_busy="Móvil ocupado...", send_failed="Envío fallido",
    rec_running="Grabando", stream_running="Stream en curso", rec_stopped="Grabación detenida", stream_stopped="Stream detenido",
    phone_unreachable="Móvil no accesible", set_ip_app="IP en ajustes de la app", helo_not_connected="HELO no conectado",
    stopping_rec="Parando grabación...", stopping_stream="Parando stream...", confirm_rec="¿Parar? ARRIBA otra vez", confirm_stream="¿Parar? ABAJO otra vez",
    starting_rec="Iniciando grabación...", starting_stream="Iniciando stream...", cancelled="Cancelado", refreshing="Actualizando...",
    waiting_phone="Esperando al móvil...", helo_unreachable="HELO no accesible", check_password="HELO: revisa contraseña",
    set_ip="IP en ajustes", sending="Enviando comando...")
W["it"] = dict(
    st_init="Init", st_idle="Pronto", st_rec="REGISTRA", st_live="LIVE", st_failed="Errore", st_off="Spento",
    media_free="Media %d%% liberi", media_short="Media %d%%", media_none="Media --",
    no_data="Nessun dato dal telefono", phone_busy="Telefono occupato...", send_failed="Invio fallito",
    rec_running="Registrazione in corso", stream_running="Stream in corso", rec_stopped="Registrazione fermata", stream_stopped="Stream fermato",
    phone_unreachable="Telefono non raggiungibile", set_ip_app="IP nelle impostazioni app", helo_not_connected="HELO non connesso",
    stopping_rec="Fermo registrazione...", stopping_stream="Fermo stream...", confirm_rec="Stop? Ancora SU", confirm_stream="Stop? Ancora GIÙ",
    starting_rec="Avvio registrazione...", starting_stream="Avvio stream...", cancelled="Annullato", refreshing="Aggiornamento...",
    waiting_phone="Attendo il telefono...", helo_unreachable="HELO non raggiungibile", check_password="HELO: controlla password",
    set_ip="IP nelle impostazioni", sending="Invio comando...")
W["pt"] = dict(
    st_init="Init", st_idle="Pronto", st_rec="A GRAVAR", st_live="LIVE", st_failed="Erro", st_off="Desligado",
    media_free="Média %d%% livre", media_short="Média %d%%", media_none="Média --",
    no_data="Sem dados do telemóvel", phone_busy="Telemóvel ocupado...", send_failed="Envio falhou",
    rec_running="A gravar", stream_running="Stream a decorrer", rec_stopped="Gravação parada", stream_stopped="Stream parado",
    phone_unreachable="Telemóvel inacessível", set_ip_app="IP nas definições da app", helo_not_connected="HELO não ligado",
    stopping_rec="A parar gravação...", stopping_stream="A parar stream...", confirm_rec="Parar? CIMA outra vez", confirm_stream="Parar? BAIXO outra vez",
    starting_rec="A iniciar gravação...", starting_stream="A iniciar stream...", cancelled="Cancelado", refreshing="A atualizar...",
    waiting_phone="À espera do telemóvel...", helo_unreachable="HELO inacessível", check_password="HELO: verificar senha",
    set_ip="IP nas definições", sending="A enviar comando...")
W["nl"] = dict(
    st_init="Init", st_idle="Gereed", st_rec="OPNAME", st_live="LIVE", st_failed="Fout", st_off="Uit",
    media_free="Media %d%% vrij", media_short="Media %d%%", media_none="Media --",
    no_data="Geen data van telefoon", phone_busy="Telefoon bezig...", send_failed="Verzenden mislukt",
    rec_running="Opname loopt", stream_running="Stream loopt", rec_stopped="Opname gestopt", stream_stopped="Stream gestopt",
    phone_unreachable="Telefoon niet bereikbaar", set_ip_app="IP in app-instellingen zetten", helo_not_connected="HELO niet verbonden",
    stopping_rec="Opname stoppen...", stopping_stream="Stream stoppen...", confirm_rec="Stop? Nogmaals OMHOOG", confirm_stream="Stop? Nogmaals OMLAAG",
    starting_rec="Opname starten...", starting_stream="Stream starten...", cancelled="Geannuleerd", refreshing="Vernieuwen...",
    waiting_phone="Wachten op telefoon...", helo_unreachable="HELO niet bereikbaar", check_password="HELO: wachtwoord nakijken",
    set_ip="IP in instellingen zetten", sending="Opdracht verzenden...")

# ------------------------------------------------------------ Telefon ----
BMC = '<a href="https://buymeacoffee.com/SIDEffects" target="_blank">buymeacoffee.com/SIDEffects</a>'
P = {}
P["de"] = dict(
    msg_check_password="HELO: Passwort prüfen", msg_timeout="HELO: Timeout ", msg_unreachable="HELO nicht erreichbar: ",
    msg_cmd_sent="Befehl gesendet", msg_cmd_failed="Befehl fehlgeschlagen",
    intro="Verbindung zum AJA HELO. Das Telefon muss im selben Netzwerk wie der HELO sein.",
    section_helo="HELO", host_label="IP-Adresse oder Hostname", host_placeholder="z. B. 192.168.1.50",
    port_label="HTTP-Port", password_label="Passwort (nur falls Benutzer-Auth am HELO aktiv ist)", password_placeholder="leer lassen, wenn keine Auth",
    section_monitoring="Überwachung", poll_label="Abfrage-Intervall (Sekunden)", vibrate_label="Vibrieren bei Start/Stopp",
    section_display="Anzeige", language_label="Sprache", language_auto="Automatisch (wie die Uhr)",
    keys_text="Tasten: OBEN = Aufnahme, MITTE = Aktualisieren, UNTEN = Stream. Stoppen muss durch einen zweiten Druck innerhalb von 4 Sekunden bestätigt werden.",
    save="Speichern", support="Unterstützen",
    support_text="Die App ist kostenlos und ohne Werbung. Wenn sie dir Freude macht, freue ich mich über einen Kaffee: " + BMC,
    donate="☕ Buy me a coffee")
P["en"] = dict(
    msg_check_password="HELO: check password", msg_timeout="HELO: timeout ", msg_unreachable="HELO not reachable: ",
    msg_cmd_sent="Command sent", msg_cmd_failed="Command failed",
    intro="Connection to the AJA HELO. The phone must be on the same network as the HELO.",
    section_helo="HELO", host_label="IP address or hostname", host_placeholder="e.g. 192.168.1.50",
    port_label="HTTP port", password_label="Password (only if user authentication is enabled on the HELO)", password_placeholder="leave empty if no auth",
    section_monitoring="Monitoring", poll_label="Polling interval (seconds)", vibrate_label="Vibrate on start/stop",
    section_display="Display", language_label="Language", language_auto="Automatic (same as watch)",
    keys_text="Buttons: UP = record, SELECT = refresh, DOWN = stream. Stopping must be confirmed with a second press within 4 seconds.",
    save="Save", support="Support",
    support_text="The app is free and has no ads. If you enjoy it, a coffee is much appreciated: " + BMC,
    donate="☕ Buy me a coffee")
P["fr"] = dict(
    msg_check_password="HELO : vérifiez le mot de passe", msg_timeout="HELO : délai dépassé ", msg_unreachable="HELO injoignable : ",
    msg_cmd_sent="Commande envoyée", msg_cmd_failed="Commande échouée",
    intro="Connexion au AJA HELO. Le téléphone doit être sur le même réseau que le HELO.",
    section_helo="HELO", host_label="Adresse IP ou nom d'hôte", host_placeholder="ex. 192.168.1.50",
    port_label="Port HTTP", password_label="Mot de passe (seulement si l'authentification est activée sur le HELO)", password_placeholder="laisser vide sans authentification",
    section_monitoring="Surveillance", poll_label="Intervalle d'interrogation (secondes)", vibrate_label="Vibrer au démarrage/à l'arrêt",
    section_display="Affichage", language_label="Langue", language_auto="Automatique (comme la montre)",
    keys_text="Boutons : HAUT = enregistrement, SELECT = actualiser, BAS = stream. L'arrêt doit être confirmé par un second appui dans les 4 secondes.",
    save="Enregistrer", support="Soutenir",
    support_text="L'app est gratuite et sans publicité. Si elle vous plaît, un café fait plaisir : " + BMC,
    donate="☕ Buy me a coffee")
P["es"] = dict(
    msg_check_password="HELO: revisa la contraseña", msg_timeout="HELO: tiempo agotado ", msg_unreachable="HELO no accesible: ",
    msg_cmd_sent="Comando enviado", msg_cmd_failed="Comando fallido",
    intro="Conexión con el AJA HELO. El móvil debe estar en la misma red que el HELO.",
    section_helo="HELO", host_label="Dirección IP o nombre de host", host_placeholder="p. ej. 192.168.1.50",
    port_label="Puerto HTTP", password_label="Contraseña (solo si la autenticación de usuario está activa en el HELO)", password_placeholder="dejar vacío sin autenticación",
    section_monitoring="Supervisión", poll_label="Intervalo de consulta (segundos)", vibrate_label="Vibrar al iniciar/parar",
    section_display="Pantalla", language_label="Idioma", language_auto="Automático (como el reloj)",
    keys_text="Botones: ARRIBA = grabar, SELECT = actualizar, ABAJO = stream. Para parar hay que confirmar con una segunda pulsación en 4 segundos.",
    save="Guardar", support="Apoyar",
    support_text="La app es gratuita y sin anuncios. Si te gusta, un café se agradece mucho: " + BMC,
    donate="☕ Buy me a coffee")
P["it"] = dict(
    msg_check_password="HELO: controlla la password", msg_timeout="HELO: timeout ", msg_unreachable="HELO non raggiungibile: ",
    msg_cmd_sent="Comando inviato", msg_cmd_failed="Comando fallito",
    intro="Connessione all'AJA HELO. Il telefono deve essere sulla stessa rete dell'HELO.",
    section_helo="HELO", host_label="Indirizzo IP o nome host", host_placeholder="es. 192.168.1.50",
    port_label="Porta HTTP", password_label="Password (solo se l'autenticazione utente è attiva sull'HELO)", password_placeholder="lascia vuoto senza autenticazione",
    section_monitoring="Monitoraggio", poll_label="Intervallo di aggiornamento (secondi)", vibrate_label="Vibra a start/stop",
    section_display="Schermo", language_label="Lingua", language_auto="Automatica (come l'orologio)",
    keys_text="Tasti: SU = registrazione, SELECT = aggiorna, GIÙ = stream. Lo stop va confermato con una seconda pressione entro 4 secondi.",
    save="Salva", support="Sostieni",
    support_text="L'app è gratuita e senza pubblicità. Se ti piace, un caffè è molto gradito: " + BMC,
    donate="☕ Buy me a coffee")
P["pt"] = dict(
    msg_check_password="HELO: verificar senha", msg_timeout="HELO: tempo esgotado ", msg_unreachable="HELO inacessível: ",
    msg_cmd_sent="Comando enviado", msg_cmd_failed="Comando falhou",
    intro="Ligação ao AJA HELO. O telemóvel tem de estar na mesma rede que o HELO.",
    section_helo="HELO", host_label="Endereço IP ou nome do host", host_placeholder="p. ex. 192.168.1.50",
    port_label="Porta HTTP", password_label="Senha (só se a autenticação de utilizador estiver ativa no HELO)", password_placeholder="deixar vazio sem autenticação",
    section_monitoring="Monitorização", poll_label="Intervalo de consulta (segundos)", vibrate_label="Vibrar ao iniciar/parar",
    section_display="Ecrã", language_label="Idioma", language_auto="Automático (como o relógio)",
    keys_text="Botões: CIMA = gravar, SELECT = atualizar, BAIXO = stream. Parar tem de ser confirmado com um segundo toque em 4 segundos.",
    save="Guardar", support="Apoiar",
    support_text="A app é gratuita e sem anúncios. Se gostares, um café é muito bem-vindo: " + BMC,
    donate="☕ Buy me a coffee")
P["nl"] = dict(
    msg_check_password="HELO: wachtwoord nakijken", msg_timeout="HELO: time-out ", msg_unreachable="HELO niet bereikbaar: ",
    msg_cmd_sent="Opdracht verzonden", msg_cmd_failed="Opdracht mislukt",
    intro="Verbinding met de AJA HELO. De telefoon moet in hetzelfde netwerk zitten als de HELO.",
    section_helo="HELO", host_label="IP-adres of hostnaam", host_placeholder="bv. 192.168.1.50",
    port_label="HTTP-poort", password_label="Wachtwoord (alleen als gebruikersauthenticatie op de HELO aanstaat)", password_placeholder="leeg laten zonder authenticatie",
    section_monitoring="Bewaking", poll_label="Pollinginterval (seconden)", vibrate_label="Trillen bij start/stop",
    section_display="Weergave", language_label="Taal", language_auto="Automatisch (zoals het horloge)",
    keys_text="Knoppen: OMHOOG = opname, SELECT = vernieuwen, OMLAAG = stream. Stoppen moet binnen 4 seconden met een tweede druk worden bevestigd.",
    save="Opslaan", support="Steunen",
    support_text="De app is gratis en zonder reclame. Vind je hem leuk, dan is een koffie zeer welkom: " + BMC,
    donate="☕ Buy me a coffee")


def c_str(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    keys = list(W["en"].keys())
    for l in LANGS:
        assert set(W[l]) == set(keys), l
        assert set(P[l]) == set(P["en"]), l
    lines = ["#pragma once", "// GENERIERT von tools/i18n.py. Nicht von Hand aendern.", "",
             "#define LANG_COUNT_REAL %d" % len(LANGS),
             "static const char *LANG_CODES[LANG_COUNT_REAL] = { %s };" % ", ".join(c_str(l) for l in LANGS), "",
             "typedef struct {"]
    lines += ["  const char *%s;" % k for k in keys]
    lines += ["} Strings;", "", "static const Strings STRINGS[LANG_COUNT_REAL] = {"]
    for l in LANGS:
        lines.append("  { // %s" % l)
        lines += ["    .%s = %s," % (k, c_str(W[l][k])) for k in keys]
        lines.append("  },")
    lines.append("};")
    open(os.path.join(root, "src", "c", "strings_i18n.h"), "w", encoding="utf-8").write("\n".join(lines) + "\n")

    import json
    js = {"langs": LANGS, "native": NATIVE, "phone": P}
    open(os.path.join(root, "src", "pkjs", "i18n.js"), "w", encoding="utf-8").write(
        "// GENERIERT von tools/i18n.py. Nicht von Hand aendern.\nmodule.exports = "
        + json.dumps(js, ensure_ascii=False, indent=2) + ";\n")
    print("strings_i18n.h und i18n.js erzeugt:", len(LANGS), "Sprachen,", len(keys), "Uhr-Texte,", len(P["en"]), "Telefon-Texte")


if __name__ == "__main__":
    main()
