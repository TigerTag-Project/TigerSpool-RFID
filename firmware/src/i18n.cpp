#include "i18n.h"
#include <Preferences.h>

// One row per string, all languages side by side, in the order of enum Lang:
//   EN, FR, DE, ES, IT, PL, PT-BR, PT-PT
//
// These strings are written WITHOUT diacritics, and that is not an oversight.
// The compiled Montserrat covers 0x20-0x7F plus degree and bullet - ASCII, not
// Latin-1 - so an accent draws as a blank box and LVGL logs nothing. Restoring
// them needs a generated Latin subset font first, and Polish needs Latin
// Extended-A beyond that. scripts/check-ui-fonts.py enforces this, and widens
// by itself once such a font exists.
//
// The screen is 240 px wide. A string that is half again as long in German as
// in English gets cut off, so translations are kept short rather than literal.
struct Row { const char* s[LANG_N]; };

static const Row STR[S_COUNT] = {
/* S_TOUCH_SLOT     */ {{ "tap a slot", "touchez un emplacement", "Slot antippen", "toca una ranura", "tocca uno slot", "dotknij gniazda", "toque um slot", "toque num slot" }},
/* S_SLOT           */ {{ "Slot", "Empl.", "Slot", "Ranura", "Slot", "Gniazdo", "Slot", "Slot" }},
/* S_BRING_TAG      */ {{ "Hold the spool", "Approchez la bobine", "Spule anhalten", "Acerca la bobina", "Avvicina la bobina", "Przyloz szpule", "Aproxime a bobina", "Aproxime a bobine" }},
/* S_TO_READER      */ {{ "against the box", "contre le boitier", "an das Gerat", "a la caja", "alla scatola", "do urzadzenia", "da caixa", "da caixa" }},
/* S_CANCEL         */ {{ "Cancel", "Annuler", "Abbrechen", "Cancelar", "Annulla", "Anuluj", "Cancelar", "Cancelar" }},
/* S_NO             */ {{ "No", "Non", "Nein", "No", "No", "Nie", "Nao", "Nao" }},
/* S_SEND           */ {{ "Send", "Envoyer", "Senden", "Enviar", "Invia", "Wyslij", "Enviar", "Enviar" }},
/* S_SEND_TO        */ {{ "Send to %s?", "Envoyer vers %s ?", "An %s senden?", "Enviar a %s?", "Inviare a %s?", "Wyslac do %s?", "Enviar para %s?", "Enviar para %s?" }},
/* S_NOZZLE         */ {{ "Nozzle", "Buse", "Duse", "Boquilla", "Ugello", "Dysza", "Bico", "Bico" }},
/* S_BED            */ {{ "Bed", "Plateau", "Bett", "Cama", "Piano", "Stol", "Mesa", "Cama" }},
/* S_OK             */ {{ "OK", "OK", "OK", "OK", "OK", "OK", "OK", "OK" }},
/* S_ERR            */ {{ "Error", "Erreur", "Fehler", "Error", "Errore", "Blad", "Erro", "Erro" }},
/* S_TAP_BACK       */ {{ "tap to continue", "touchez pour continuer", "zum Fortfahren tippen", "toca para continuar", "tocca per continuare", "dotknij, aby kontynuowac", "toque para continuar", "toque para continuar" }},
/* S_CONNECTING     */ {{ "Connecting to", "Connexion a", "Verbinde mit", "Conectando a", "Connessione a", "Laczenie z", "Conectando a", "A ligar a" }},
/* S_WIFI_FAIL      */ {{ "Connection failed", "Echec de connexion", "Verbindung fehlgeschlagen", "Fallo de conexion", "Connessione fallita", "Blad polaczenia", "Falha na conexao", "Falha na ligacao" }},
/* S_NO_NETWORK     */ {{ "No network set up", "Aucun reseau configure", "Kein Netzwerk eingerichtet", "Sin red configurada", "Nessuna rete configurata", "Brak skonfigurowanej sieci", "Nenhuma rede configurada", "Sem rede configurada" }},
/* S_CONFIG_HINT    */ {{ "Set up Wi-Fi first", "Configurez le Wi-Fi", "Zuerst WLAN einrichten", "Configura el Wi-Fi", "Configura il Wi-Fi", "Najpierw skonfiguruj Wi-Fi", "Configure o Wi-Fi", "Configure o Wi-Fi" }},
/* S_UPDATED        */ {{ "%s updated", "%s mis a jour", "%s aktualisiert", "%s actualizado", "%s aggiornato", "%s zaktualizowano", "%s atualizado", "%s atualizado" }},
/* S_PRINTER_OFF    */ {{ "Printer unreachable", "Imprimante injoignable", "Drucker nicht erreichbar", "Impresora inaccesible", "Stampante irraggiungibile", "Drukarka niedostepna", "Impressora inacessivel", "Impressora inacessivel" }},
/* S_SEND_FAIL      */ {{ "Could not send", "Envoi impossible", "Senden fehlgeschlagen", "No se pudo enviar", "Invio non riuscito", "Nie mozna wyslac", "Nao foi possivel enviar", "Nao foi possivel enviar" }},
/* S_HOLDER         */ {{ "Ext.", "Ext.", "Ext.", "Ext.", "Est.", "Zew.", "Ext.", "Ext." }},
/* S_CHOOSE_LANG    */ {{ "Choose your language", "Choisissez votre langue", "Sprache wahlen", "Elige tu idioma", "Scegli la lingua", "Wybierz jezyk", "Escolha o idioma", "Escolha o idioma" }},
/* S_READ_UNSTABLE  */ {{ "Move the spool closer", "Rapprochez la bobine", "Spule naher halten", "Acerca mas la bobina", "Avvicina di piu la bobina", "Przysun szpule blizej", "Aproxime mais a bobina", "Aproxime mais a bobine" }},
/* S_BLANK_TAG      */ {{ "Tag is empty", "Etiquette vierge", "Tag ist leer", "Etiqueta vacia", "Tag vuoto", "Pusty tag", "Etiqueta vazia", "Etiqueta vazia" }},
/* S_PRINTER        */ {{ "Printers", "Imprimantes", "Drucker", "Impresoras", "Stampanti", "Drukarki", "Impressoras", "Impressoras" }},
/* S_NO_PRINTERS    */ {{ "No printers yet", "Aucune imprimante", "Noch keine Drucker", "Sin impresoras", "Nessuna stampante", "Brak drukarek", "Nenhuma impressora", "Nenhuma impressora" }},
/* S_ALL_HIDDEN      */ {{ "All printers hidden.\nSettings > Printers", "Toutes masquees.\nReglages > Imprimantes", "Alle ausgeblendet.\nEinstellungen > Drucker", "Todas ocultas.\nAjustes > Impresoras", "Tutte nascoste.\nImpostazioni > Stampanti", "Wszystkie ukryte.\nUstawienia > Drukarki", "Todas ocultas.\nAjustes > Impressoras", "Todas ocultas.\nDefinicoes > Impressoras" }},
/* S_TT_LINKED      */ {{ "Account linked", "Compte connecte", "Konto verbunden", "Cuenta vinculada", "Account collegato", "Konto polaczone", "Conta vinculada", "Conta ligada" }},
/* S_ADD_WEB        */ {{ "Add them in Tiger Studio", "Ajoutez-les dans Tiger Studio", "In Tiger Studio hinzufugen", "Anadelas en Tiger Studio", "Aggiungile in Tiger Studio", "Dodaj je w Tiger Studio", "Adicione no Tiger Studio", "Adicione no Tiger Studio" }},
/* S_CONFIG_WEB     */ {{ "Settings:", "Reglages :", "Einstellungen:", "Ajustes:", "Impostazioni:", "Ustawienia:", "Ajustes:", "Definicoes:" }},
/* S_SETTINGS        */ {{ "Settings", "Reglages", "Einstellungen", "Ajustes", "Impostazioni", "Ustawienia", "Ajustes", "Definicoes" }},
/* S_AP_TITLE       */ {{ "Wi-Fi setup", "Configuration Wi-Fi", "WLAN einrichten", "Configurar Wi-Fi", "Configura Wi-Fi", "Konfiguracja Wi-Fi", "Configurar Wi-Fi", "Configurar Wi-Fi" }},
/* S_CHANGE_NETWORK  */ {{ "Change network", "Changer de reseau", "Netzwerk wechseln", "Cambiar de red", "Cambia rete", "Zmien siec", "Trocar de rede", "Mudar de rede" }},
/* S_AP_JOIN         */ {{ "Scan with your phone", "Scannez avec votre telephone", "Mit dem Handy scannen", "Escanea con el movil", "Inquadra col telefono", "Zeskanuj telefonem", "Escaneie com o celular", "Digitalize com o telemovel" }},
/* S_OR_JOIN         */ {{ "Or join Wi-Fi", "Ou rejoignez le Wi-Fi", "Oder WLAN beitreten", "O conecta al Wi-Fi", "O connettiti al Wi-Fi", "Lub polacz z Wi-Fi", "Ou conecte ao Wi-Fi", "Ou ligue ao Wi-Fi" }},
/* S_OR_OPEN        */ {{ "Or open this address", "Ou ouvrez cette adresse", "Oder diese Adresse offnen", "O abre esta direccion", "O apri questo indirizzo", "Lub otworz ten adres", "Ou abra este endereco", "Ou abra este endereco" }},
/* S_AP_OPEN        */ {{ "Then open", "Puis ouvrez", "Dann offnen", "Luego abre", "Poi apri", "Nastepnie otworz", "Depois abra", "Depois abra" }},
/* S_AP_CHOOSE      */ {{ "and pick your network", "et choisissez votre reseau", "und Netzwerk wahlen", "y elige tu red", "e scegli la rete", "i wybierz siec", "e escolha sua rede", "e escolha a sua rede" }},
/* S_TT_IMPORTING   */ {{ "Importing printers", "Import des imprimantes", "Drucker werden importiert", "Importando impresoras", "Importazione stampanti", "Importowanie drukarek", "Importando impressoras", "A importar impressoras" }},
/* S_TT_ACCOUNT     */ {{ "Account", "Compte", "Konto", "Cuenta", "Account", "Konto", "Conta", "Conta" }},
/* S_ONLINE         */ {{ "online", "en ligne", "online", "en linea", "online", "online", "online", "online" }},
/* S_OFFLINE        */ {{ "offline", "hors ligne", "offline", "sin conexion", "offline", "offline", "offline", "offline" }},
/* S_BACK           */ {{ "Back", "Retour", "Zuruck", "Atras", "Indietro", "Wstecz", "Voltar", "Voltar" }},
/* S_FIND_PRINTERS  */ {{ "Looking for printers", "Recherche d'imprimantes", "Suche nach Druckern", "Buscando impresoras", "Ricerca stampanti", "Szukam drukarek", "Procurando impressoras", "A procurar impressoras" }},
/* S_NO_ONLINE      */ {{ "No printer reachable", "Aucune imprimante joignable", "Kein Drucker erreichbar", "Ninguna impresora accesible", "Nessuna stampante raggiungibile", "Zadna drukarka niedostepna", "Nenhuma impressora acessivel", "Nenhuma impressora acessivel" }},
/* S_WIFI_BAD_PASSWORD */ {{ "Couldn't join. The password may be wrong.", "Echec. Le mot de passe est peut-etre faux.", "Fehlgeschlagen. Passwort evtl. falsch.", "Fallo. La contrasena puede estar mal.", "Fallito. La password potrebbe essere errata.", "Nie udalo sie. Haslo moze byc bledne.", "Falhou. A senha pode estar errada.", "Falhou. A palavra-passe pode estar errada." }},
/* S_ACCOUNT_WHY    */ {{ "Your printers are already in your TigerTag account. Link it and they arrive by themselves.", "Vos imprimantes sont deja dans votre compte TigerTag. Connectez-le et elles arrivent seules.", "Ihre Drucker sind schon in Ihrem TigerTag-Konto. Verbinden und sie erscheinen von selbst.", "Tus impresoras ya estan en tu cuenta TigerTag. Vinculala y apareceran solas.", "Le tue stampanti sono gia nel tuo account TigerTag. Collegalo e arrivano da sole.", "Twoje drukarki sa juz na koncie TigerTag. Polacz je, a pojawia sie same.", "Suas impressoras ja estao na sua conta TigerTag. Vincule e elas chegam sozinhas.", "As suas impressoras ja estao na sua conta TigerTag. Ligue-a e chegam sozinhas." }},
/* S_LINK_ACCOUNT   */ {{ "Link my account", "Connecter mon compte", "Konto verbinden", "Vincular mi cuenta", "Collega il mio account", "Polacz moje konto", "Vincular minha conta", "Ligar a minha conta" }},
/* S_SIGN_IN         */ {{ "Log in to\nyour account", "Connectez\nvotre compte", "Melden Sie\nsich an", "Accede a\ntu cuenta", "Accedi al\ntuo account", "Zaloguj sie\nna konto", "Acesse\nsua conta", "Aceda a\nsua conta" }},
/* S_WAITING         */ {{ "Waiting...", "Patientez...", "Bitte warten...", "Esperando...", "Attendere...", "Czekaj...", "Aguarde...", "Aguarde..." }},
/* S_WITH_EMAIL      */ {{ "Mail & Password", "Mail & mot de passe", "Mail & Passwort", "Mail y contrasena", "Mail e password", "Mail i haslo", "Mail e senha", "Mail e palavra-passe" }},
/* S_WITH_GOOGLE     */ {{ "Continue with Google", "Continuer avec Google", "Mit Google fortfahren", "Continuar con Google", "Continua con Google", "Kontynuuj z Google", "Continuar com o Google", "Continuar com o Google" }},
/* S_SCAN_TO_LINK   */ {{ "Or go to tigersystem.io/pair", "Ou allez sur tigersystem.io/pair", "Oder tigersystem.io/pair offnen", "O ve a tigersystem.io/pair", "O vai su tigersystem.io/pair", "Lub wejdz na tigersystem.io/pair", "Ou acesse tigersystem.io/pair", "Ou va a tigersystem.io/pair" }},
/* S_COLOUR_ADAPTED */ {{ "Colour adapted to the printer's palette", "Couleur adaptee a la palette de l'imprimante", "Farbe an die Druckerpalette angepasst", "Color adaptado a la paleta de la impresora", "Colore adattato alla palette della stampante", "Kolor dopasowany do palety drukarki", "Cor adaptada a paleta da impressora", "Cor adaptada a paleta da impressora" }},
/* S_SCREEN         */ {{ "Screen", "Ecran", "Anzeige", "Pantalla", "Schermo", "Ekran", "Tela", "Ecra" }},
/* S_LANGUAGE       */ {{ "Language", "Langue", "Sprache", "Idioma", "Lingua", "Jezyk", "Idioma", "Idioma" }},
/* S_UPDATE         */ {{ "Update", "Mise a jour", "Update", "Actualizar", "Aggiorna", "Aktualizacja", "Atualizar", "Atualizar" }},
/* S_RESTART        */ {{ "Restart", "Redemarrer", "Neustart", "Reiniciar", "Riavvia", "Uruchom ponownie", "Reiniciar", "Reiniciar" }},
/* S_FACTORY        */ {{ "Factory reset", "Reinit. usine", "Werksreset", "Rest. de fabrica", "Ripristino", "Reset fabryczny", "Restauracao", "Reposicao" }},
/* S_SIGN_OUT       */ {{ "Sign out", "Deconnexion", "Abmelden", "Cerrar sesion", "Esci", "Wyloguj", "Sair", "Terminar sessao" }},
/* S_BRIGHTNESS     */ {{ "Brightness", "Luminosite", "Helligkeit", "Brillo", "Luminosita", "Jasnosc", "Brilho", "Brilho" }},
/* S_SLEEP_AFTER    */ {{ "Sleep after", "Veille apres", "Ruhe nach", "Reposo tras", "Standby dopo", "Uspij po", "Suspender apos", "Suspender apos" }},
/* S_NEVER          */ {{ "Never", "Jamais", "Nie", "Nunca", "Mai", "Nigdy", "Nunca", "Nunca" }},
/* S_INSTALLED      */ {{ "Installed version", "Version installee", "Installierte Version", "Version instalada", "Versione installata", "Zainstalowana wersja", "Versao instalada", "Versao instalada" }},
/* S_OTA_OFF        */ {{ "Over-the-air updates are not enabled on this build.", "Les mises a jour par le reseau ne sont pas actives.", "Updates uber Funk sind in diesem Build nicht aktiv.", "Las actualizaciones por red no estan activas.", "Gli aggiornamenti via rete non sono attivi.", "Aktualizacje przez siec nie sa wlaczone.", "As atualizacoes pela rede nao estao ativas.", "As atualizacoes pela rede nao estao ativas." }},
/* S_RESTART_Q      */ {{ "Restart the box?", "Redemarrer le boitier ?", "Box neu starten?", "Reiniciar la caja?", "Riavviare la scatola?", "Uruchomic ponownie?", "Reiniciar a caixa?", "Reiniciar a caixa?" }},
/* S_RESTART_NOTE   */ {{ "Takes about ten seconds. Nothing is lost.", "Environ dix secondes. Rien n'est perdu.", "Etwa zehn Sekunden. Nichts geht verloren.", "Unos diez segundos. No se pierde nada.", "Circa dieci secondi. Non si perde nulla.", "Okolo dziesieciu sekund. Nic nie ginie.", "Cerca de dez segundos. Nada e perdido.", "Cerca de dez segundos. Nada se perde." }},
/* S_FACTORY_WARN   */ {{ "This erases the network, the account and the printers.", "Ceci efface le reseau, le compte et les imprimantes.", "Loscht Netzwerk, Konto und Drucker.", "Borra la red, la cuenta y las impresoras.", "Cancella rete, account e stampanti.", "Usuwa siec, konto i drukarki.", "Isso apaga a rede, a conta e as impressoras.", "Isto apaga a rede, a conta e as impressoras." }},
/* S_FACTORY_NOTE   */ {{ "Your printers stay in your account.", "Vos imprimantes restent dans votre compte.", "Ihre Drucker bleiben im Konto.", "Tus impresoras siguen en tu cuenta.", "Le stampanti restano nel tuo account.", "Drukarki pozostaja na koncie.", "Suas impressoras ficam na sua conta.", "As suas impressoras ficam na sua conta." }},
/* S_HOLD_ERASE     */ {{ "Hold to erase", "Maintenir pour effacer", "Zum Loschen halten", "Manten para borrar", "Tieni per cancellare", "Przytrzymaj, aby usunac", "Segure para apagar", "Mantenha para apagar" }},
/* S_KEEP_HOLDING   */ {{ "Keep holding...", "Continuez...", "Weiter halten...", "Sigue pulsando...", "Continua a tenere...", "Trzymaj dalej...", "Continue segurando...", "Continue a premir..." }},
/* S_CHECK_UPDATE   */ {{ "Check for updates", "Rechercher une mise a jour", "Nach Updates suchen", "Buscar actualizaciones", "Cerca aggiornamenti", "Sprawdz aktualizacje", "Procurar atualizacoes", "Procurar atualizacoes" }},
/* S_CHECKING       */ {{ "Checking...", "Recherche...", "Suche...", "Buscando...", "Ricerca...", "Sprawdzanie...", "Procurando...", "A procurar..." }},
/* S_UP_TO_DATE     */ {{ "Your TigerSpool is up to date", "Votre TigerSpool est a jour", "Ihr TigerSpool ist aktuell", "Tu TigerSpool esta actualizado", "Il tuo TigerSpool e aggiornato", "Twoj TigerSpool jest aktualny", "Seu TigerSpool esta atualizado", "O seu TigerSpool esta atualizado" }},
/* S_AVAILABLE      */ {{ "Version available", "Version disponible", "Version verfugbar", "Version disponible", "Versione disponibile", "Dostepna wersja", "Versao disponivel", "Versao disponivel" }},
/* S_INSTALL        */ {{ "Install", "Installer", "Installieren", "Instalar", "Installa", "Zainstaluj", "Instalar", "Instalar" }},
/* S_DOWNLOADING    */ {{ "Downloading", "Telechargement", "Wird geladen", "Descargando", "Download", "Pobieranie", "Baixando", "A transferir" }},
/* S_DONT_UNPLUG    */ {{ "Do not unplug the box.", "Ne debranchez pas le boitier.", "Gerat nicht trennen.", "No desconectes la caja.", "Non scollegare la scatola.", "Nie odlaczaj urzadzenia.", "Nao desconecte a caixa.", "Nao desligue a caixa." }},
/* S_UPDATE_KEEPS   */ {{ "Wi-Fi, account and printers are kept.", "Wi-Fi, compte et imprimantes sont conserves.", "WLAN, Konto und Drucker bleiben erhalten.", "Wi-Fi, cuenta e impresoras se conservan.", "Wi-Fi, account e stampanti sono conservati.", "Wi-Fi, konto i drukarki zostaja zachowane.", "Wi-Fi, conta e impressoras sao mantidos.", "Wi-Fi, conta e impressoras sao mantidos." }},
/* S_LATER          */ {{ "Later", "Plus tard", "Spater", "Mas tarde", "Piu tardi", "Pozniej", "Depois", "Mais tarde" }},
/* S_SIGNAL         */ {{ "Signal", "Signal", "Signal", "Senal", "Segnale", "Sygnal", "Sinal", "Sinal" }},
/* S_AUTO           */ {{ "Auto", "Auto", "Auto", "Auto", "Auto", "Auto", "Auto", "Auto" }},
/* S_ORIENTATION   */ {{ "Orientation", "Orientation", "Ausrichtung", "Orientacion", "Orientamento", "Orientacja", "Orientacao", "Orientacao" }},
/* S_RESTARTING     */ {{ "Installed. Restarting...", "Installe. Redemarrage...", "Installiert. Neustart...", "Instalado. Reiniciando...", "Installato. Riavvio...", "Zainstalowano. Restart...", "Instalado. Reiniciando...", "Instalado. A reiniciar..." }},
};

// A mismatch here is silent at runtime and reads as garbled text on screen, so
// let the compiler catch it instead.
static_assert(sizeof(STR) / sizeof(STR[0]) == S_COUNT,
              "i18n table and StrId enum are out of step");

// Each language written in itself: someone looking for Portugues should not
// have to recognise the English word "Portuguese" first.
static const char* const NAMES[LANG_N] = {
    "English", "Francais", "Deutsch", "Espanol",
    "Italiano", "Polski", "Portugues (BR)", "Portugues (PT)"
};

namespace {
    Lang g_lang  = LANG_EN;
    bool g_chosen = false;
}

namespace i18n {

// The stored value is an index into enum Lang, so it only means anything to the
// enum that wrote it. The prototype ordered its languages PT, EN, ES, FR; this
// firmware orders them EN, FR, DE, ES, ... A value carried across without a
// marker silently selects a different language - a device set to French came up
// in Spanish, which is how this was found.
//
// Bump LANG_SCHEMA whenever enum Lang is reordered or has entries removed. An
// index from an older schema is discarded, the device falls back to English and
// asks once. Adding a language at the END does not need a bump.
static constexpr int LANG_SCHEMA = 2;

void begin() {
    Preferences p;
    p.begin("tigerspool", true);
    int schema = p.getInt("langVer", 0);
    int v      = p.getInt("lang", -1);
    p.end();

    if (schema != LANG_SCHEMA) {
        if (v >= 0) Serial.printf("[i18n] language index %d written by schema %d "
                                  "- discarding, asking again\n", v, schema);
        return;                       // stays English, stays unchosen
    }
    if (v >= 0 && v < LANG_N) { g_lang = (Lang)v; g_chosen = true; }
}

bool chosen()  { return g_chosen; }
Lang current() { return g_lang; }

void set(Lang l) {
    if (l >= LANG_N) return;
    g_lang = l; g_chosen = true;
    Preferences p;
    p.begin("tigerspool", false);
    p.putInt("lang", (int)l);
    p.putInt("langVer", LANG_SCHEMA);   // stamp what wrote it
    p.end();
}

// Falls back to English rather than returning null: a missing translation
// should show the English word, never crash a screen mid-draw.
const char* T(StrId id) {
    if (id >= S_COUNT) return "";
    const char* s = STR[id].s[g_lang];
    return (s && *s) ? s : STR[id].s[LANG_EN];
}

const char* name(Lang l) { return l < LANG_N ? NAMES[l] : NAMES[LANG_EN]; }

}  // namespace i18n
