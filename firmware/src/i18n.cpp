#include "i18n.h"
#include <Preferences.h>

// One row per string, all languages side by side, in the order of enum Lang:
//   EN, FR, DE, ES, IT, PL, PT-BR, PT-PT, ZH, RU, NL
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
/* S_TOUCH_SLOT     */ {{ "tap a slot", "touchez un emplacement", "Slot antippen", "toca una ranura", "tocca uno slot", "dotknij gniazda", "toque um slot", "toque num slot", "点击槽位" , "Нажмите на слот", "Tik op een slot"}},
/* S_BRING_TAG      */ {{ "Hold the spool", "Approchez la bobine", "Spule anhalten", "Acerca la bobina", "Avvicina la bobina", "Przyloz szpule", "Aproxime a bobina", "Aproxime a bobine", "将料盘" , "Поднесите катушку", "Houd de spoel"}},
/* S_TO_READER      */ {{ "against the box", "contre le boîtier", "an das Gerat", "a la caja", "alla scatola", "do urzadzenia", "da caixa", "da caixa", "靠近设备" , "к устройству", "tegen het apparaat"}},
/* S_CANCEL         */ {{ "Cancel", "Annuler", "Abbrechen", "Cancelar", "Annulla", "Anuluj", "Cancelar", "Cancelar", "取消" , "Отмена", "Annuleren"}},
/* S_SEND_TO        */ {{ "Send to %s?", "Envoyer vers %s ?", "An %s senden?", "Enviar a %s?", "Inviare a %s?", "Wyslac do %s?", "Enviar para %s?", "Enviar para %s?", "发送到 %s?" , "Отправить на %s?", "Verzenden naar %s?"}},
/* S_NOZZLE         */ {{ "Nozzle", "Buse", "Duse", "Boquilla", "Ugello", "Dysza", "Bico", "Bico", "喷嘴" , "Сопло", "Nozzle"}},
/* S_BED            */ {{ "Bed", "Plateau", "Bett", "Cama", "Piano", "Stol", "Mesa", "Cama", "热床" , "Стол", "Bed"}},
/* S_OK             */ {{ "OK", "OK", "OK", "OK", "OK", "OK", "OK", "OK", "确定" , "ОК", "OK"}},
/* S_ERR            */ {{ "Error", "Erreur", "Fehler", "Error", "Errore", "Blad", "Erro", "Erro", "错误" , "Ошибка", "Fout"}},
/* S_TAP_BACK       */ {{ "tap to continue", "touchez pour continuer", "zum Fortfahren tippen", "toca para continuar", "tocca per continuare", "dotknij, aby kontynuowac", "toque para continuar", "toque para continuar", "点击继续" , "нажмите, чтобы продолжить", "tik om verder te gaan"}},
/* S_CONNECTING     */ {{ "Connecting to", "Connexion à", "Verbinde mit", "Conectando a", "Connessione a", "Laczenie z", "Conectando a", "A ligar a", "正在连接" , "Подключение к", "Verbinden met"}},
/* S_WIFI_FAIL      */ {{ "Connection failed", "Échec de connexion", "Verbindung fehlgeschlagen", "Fallo de conexion", "Connessione fallita", "Blad polaczenia", "Falha na conexao", "Falha na ligacao", "连接失败" , "Ошибка подключения", "Verbinding mislukt"}},
/* S_NO_NETWORK     */ {{ "No network set up", "Aucun réseau configuré", "Kein Netzwerk eingerichtet", "Sin red configurada", "Nessuna rete configurata", "Brak skonfigurowanej sieci", "Nenhuma rede configurada", "Sem rede configurada", "未设置网络" , "Сеть не настроена", "Geen netwerk ingesteld"}},
/* S_CONFIG_HINT    */ {{ "Set up Wi-Fi first", "Configurez le Wi-Fi", "Zuerst WLAN einrichten", "Configura el Wi-Fi", "Configura il Wi-Fi", "Najpierw skonfiguruj Wi-Fi", "Configure o Wi-Fi", "Configure o Wi-Fi", "请先设置 Wi-Fi" , "Сначала настройте Wi-Fi", "Stel eerst wifi in"}},
/* S_UPDATED        */ {{ "%s updated", "%s mis à jour", "%s aktualisiert", "%s actualizado", "%s aggiornato", "%s zaktualizowano", "%s atualizado", "%s atualizado", "%s 已更新" , "%s: обновлено", "%s bijgewerkt"}},
/* S_PRINTER_OFF    */ {{ "Printer unreachable", "Imprimante injoignable", "Drucker nicht erreichbar", "Impresora inaccesible", "Stampante irraggiungibile", "Drukarka niedostepna", "Impressora inacessivel", "Impressora inacessivel", "无法连接打印机" , "Принтер недоступен", "Printer onbereikbaar"}},
/* S_SEND_FAIL      */ {{ "Could not send", "Envoi impossible", "Senden fehlgeschlagen", "No se pudo enviar", "Invio non riuscito", "Nie mozna wyslac", "Nao foi possivel enviar", "Nao foi possivel enviar", "发送失败" , "Не удалось отправить", "Verzenden mislukt"}},
/* S_HOLDER         */ {{ "Ext.", "Ext.", "Ext.", "Ext.", "Est.", "Zew.", "Ext.", "Ext.", "外置" , "Внеш.", "Ext."}},
/* S_READ_UNSTABLE  */ {{ "Move the spool closer", "Rapprochez la bobine", "Spule naher halten", "Acerca mas la bobina", "Avvicina di piu la bobina", "Przysun szpule blizej", "Aproxime mais a bobina", "Aproxime mais a bobine", "请将料盘靠近" , "Поднесите катушку ближе", "Houd de spoel dichterbij"}},
/* S_BLANK_TAG      */ {{ "Tag is empty", "Étiquette vierge", "Tag ist leer", "Etiqueta vacia", "Tag vuoto", "Pusty tag", "Etiqueta vazia", "Etiqueta vazia", "标签为空" , "Метка пуста", "Tag is leeg"}},
/* S_PRINTER        */ {{ "Printers", "Imprimantes", "Drucker", "Impresoras", "Stampanti", "Drukarki", "Impressoras", "Impressoras", "打印机" , "Принтеры", "Printers"}},
/* S_NO_PRINTERS    */ {{ "No printers yet", "Aucune imprimante", "Noch keine Drucker", "Sin impresoras", "Nessuna stampante", "Brak drukarek", "Nenhuma impressora", "Nenhuma impressora", "暂无打印机" , "Принтеров пока нет", "Nog geen printers"}},
/* S_TT_LINKED      */ {{ "Account linked", "Compte connecté", "Konto verbunden", "Cuenta vinculada", "Account collegato", "Konto polaczone", "Conta vinculada", "Conta ligada", "账户已绑定" , "Аккаунт привязан", "Account gekoppeld"}},
/* S_ADD_WEB        */ {{ "Add them in Tiger Studio", "Ajoutez-les dans Tiger Studio", "In Tiger Studio hinzufugen", "Anadelas en Tiger Studio", "Aggiungile in Tiger Studio", "Dodaj je w Tiger Studio", "Adicione no Tiger Studio", "Adicione no Tiger Studio", "请在 Tiger Studio 中添加" , "Добавьте их в Tiger Studio", "Voeg ze toe in Tiger Studio"}},
/* S_CONFIG_WEB     */ {{ "Settings:", "Réglages :", "Einstellungen:", "Ajustes:", "Impostazioni:", "Ustawienia:", "Ajustes:", "Definicoes:", "设置:" , "Настройки:", "Instellingen:"}},
/* S_SETTINGS        */ {{ "Settings", "Réglages", "Einstellungen", "Ajustes", "Impostazioni", "Ustawienia", "Ajustes", "Definicoes", "设置" , "Настройки", "Instellingen"}},
/* S_AP_TITLE       */ {{ "Wi-Fi setup", "Configuration Wi-Fi", "WLAN einrichten", "Configurar Wi-Fi", "Configura Wi-Fi", "Konfiguracja Wi-Fi", "Configurar Wi-Fi", "Configurar Wi-Fi", "Wi-Fi 设置" , "Настройка Wi-Fi", "Wifi instellen"}},
/* S_CHANGE_NETWORK  */ {{ "Change network", "Changer de réseau", "Netzwerk wechseln", "Cambiar de red", "Cambia rete", "Zmien siec", "Trocar de rede", "Mudar de rede", "更换网络" , "Сменить сеть", "Netwerk wijzigen"}},
/* S_AP_JOIN         */ {{ "Scan the QR code", "Scanner le QR Code", "QR-Code scannen", "Escanea el código QR", "Inquadra il codice QR", "Zeskanuj kod QR", "Escaneie o QR Code", "Digitalize o código QR", "扫描二维码" , "Отсканируйте QR-код", "Scan de QR-code"}},
/* S_OR_JOIN         */ {{ "Or join Wi-Fi", "Ou rejoignez le Wi-Fi", "Oder WLAN beitreten", "O conecta al Wi-Fi", "O connettiti al Wi-Fi", "Lub polacz z Wi-Fi", "Ou conecte ao Wi-Fi", "Ou ligue ao Wi-Fi", "或连接 Wi-Fi" , "Или подключитесь к Wi-Fi", "Of maak verbinding met wifi"}},
/* S_OR_OPEN        */ {{ "Or open this address", "Ou ouvrez cette adresse", "Oder diese Adresse offnen", "O abre esta direccion", "O apri questo indirizzo", "Lub otworz ten adres", "Ou abra este endereco", "Ou abra este endereco", "或打开此地址" , "Или откройте адрес", "Of open dit adres"}},
/* S_AP_OPEN        */ {{ "Then open", "Puis ouvrez", "Dann offnen", "Luego abre", "Poi apri", "Nastepnie otworz", "Depois abra", "Depois abra", "然后打开" , "Затем откройте", "Open daarna"}},
/* S_AP_CHOOSE      */ {{ "and pick your network", "et choisissez votre réseau", "und Netzwerk wahlen", "y elige tu red", "e scegli la rete", "i wybierz siec", "e escolha sua rede", "e escolha a sua rede", "并选择您的网络" , "и выберите сеть", "en kies je netwerk"}},
/* S_TT_IMPORTING   */ {{ "Importing printers", "Import des imprimantes", "Drucker werden importiert", "Importando impresoras", "Importazione stampanti", "Importowanie drukarek", "Importando impressoras", "A importar impressoras", "正在导入打印机" , "Импорт принтеров", "Printers importeren"}},
/* S_TT_ACCOUNT     */ {{ "Account", "Compte", "Konto", "Cuenta", "Account", "Konto", "Conta", "Conta", "账户" , "Аккаунт", "Account"}},
/* S_ONLINE         */ {{ "online", "en ligne", "online", "en linea", "online", "online", "online", "online", "在线" , "онлайн", "online"}},
/* S_OFFLINE        */ {{ "offline", "hors ligne", "offline", "sin conexion", "offline", "offline", "offline", "offline", "离线" , "офлайн", "offline"}},
/* S_BACK           */ {{ "Back", "Retour", "Zuruck", "Atras", "Indietro", "Wstecz", "Voltar", "Voltar", "返回" , "Назад", "Terug"}},
/* S_FIND_PRINTERS  */ {{ "Looking for printers", "Recherche d'imprimantes", "Suche nach Druckern", "Buscando impresoras", "Ricerca stampanti", "Szukam drukarek", "Procurando impressoras", "A procurar impressoras", "正在查找打印机" , "Поиск принтеров", "Printers zoeken"}},
/* S_NO_ONLINE      */ {{ "No printer reachable", "Aucune imprimante joignable", "Kein Drucker erreichbar", "Ninguna impresora accesible", "Nessuna stampante raggiungibile", "Zadna drukarka niedostepna", "Nenhuma impressora acessivel", "Nenhuma impressora acessivel", "没有可连接的打印机" , "Нет доступных принтеров", "Geen printer bereikbaar"}},
/* S_WIFI_BAD_PASSWORD */ {{ "Couldn't join. The password may be wrong.", "Échec. Le mot de passe est peut-être faux.", "Fehlgeschlagen. Passwort evtl. falsch.", "Fallo. La contrasena puede estar mal.", "Fallito. La password potrebbe essere errata.", "Nie udalo sie. Haslo moze byc bledne.", "Falhou. A senha pode estar errada.", "Falhou. A palavra-passe pode estar errada.", "无法连接, 密码可能有误。" , "Не удалось подключиться. Возможно, неверный пароль.", "Verbinden mislukt. Misschien klopt het wachtwoord niet."}},
/* S_ACCOUNT_WHY    */ {{ "Your printers are already in your TigerTag account. Link it and they arrive by themselves.", "Vos imprimantes sont déjà dans votre compte TigerTag. Connectez-le et elles arrivent seules.", "Ihre Drucker sind schon in Ihrem TigerTag-Konto. Verbinden und sie erscheinen von selbst.", "Tus impresoras ya estan en tu cuenta TigerTag. Vinculala y apareceran solas.", "Le tue stampanti sono gia nel tuo account TigerTag. Collegalo e arrivano da sole.", "Twoje drukarki sa juz na koncie TigerTag. Polacz je, a pojawia sie same.", "Suas impressoras ja estao na sua conta TigerTag. Vincule e elas chegam sozinhas.", "As suas impressoras ja estao na sua conta TigerTag. Ligue-a e chegam sozinhas.", "您的打印机已在 TigerTag 账户中。绑定后将自动导入。" , "Ваши принтеры уже в аккаунте TigerTag. Привяжите его, и они появятся сами.", "Je printers staan al in je TigerTag-account. Koppel het en ze komen vanzelf binnen."}},
/* S_LINK_ACCOUNT   */ {{ "Link my account", "Connecter mon compte", "Konto verbinden", "Vincular mi cuenta", "Collega il mio account", "Polacz moje konto", "Vincular minha conta", "Ligar a minha conta", "绑定我的账户" , "Привязать аккаунт", "Mijn account koppelen"}},
/* S_SIGN_IN         */ {{ "Log in to\nyour account", "Connectez\nvotre compte", "Melden Sie\nsich an", "Accede a\ntu cuenta", "Accedi al\ntuo account", "Zaloguj sie\nna konto", "Acesse\nsua conta", "Aceda a\nsua conta", "登录\n您的账户" , "Войдите в\nаккаунт", "Log in op\nje account"}},
/* S_WAITING         */ {{ "Waiting...", "Patientez...", "Bitte warten...", "Esperando...", "Attendere...", "Czekaj...", "Aguarde...", "Aguarde...", "等待中..." , "Ожидание...", "Wachten..."}},
/* S_WITH_EMAIL      */ {{ "Mail & Password", "Mail & mot de passe", "Mail & Passwort", "Mail y contrasena", "Mail e password", "Mail i haslo", "Mail e senha", "Mail e palavra-passe", "邮箱和密码" , "Почта и пароль", "E-mail en wachtwoord"}},
/* S_WITH_GOOGLE     */ {{ "Continue with Google", "Continuer avec Google", "Mit Google fortfahren", "Continuar con Google", "Continua con Google", "Kontynuuj z Google", "Continuar com o Google", "Continuar com o Google", "使用 Google 继续" , "Продолжить с Google", "Doorgaan met Google"}},
/* S_COLOUR_ADAPTED */ {{ "Colour adapted to the printer's palette", "Couleur adaptée à la palette de l'imprimante", "Farbe an die Druckerpalette angepasst", "Color adaptado a la paleta de la impresora", "Colore adattato alla palette della stampante", "Kolor dopasowany do palety drukarki", "Cor adaptada a paleta da impressora", "Cor adaptada a paleta da impressora", "颜色已适配打印机色板" , "Цвет подобран под палитру принтера", "Kleur aangepast aan het palet van de printer"}},
/* S_SCREEN         */ {{ "Screen", "Écran", "Anzeige", "Pantalla", "Schermo", "Ekran", "Tela", "Ecra", "屏幕" , "Экран", "Scherm"}},
/* S_LANGUAGE       */ {{ "Language", "Langue", "Sprache", "Idioma", "Lingua", "Jezyk", "Idioma", "Idioma", "语言" , "Язык", "Taal"}},
/* S_UPDATE         */ {{ "Update", "Mise à jour", "Update", "Actualizar", "Aggiorna", "Aktualizacja", "Atualizar", "Atualizar", "更新" , "Обновить", "Bijwerken"}},
/* S_RESTART        */ {{"Restart", "Redémarrer", "Neustart", "Reiniciar", "Riavvia", "Uruchom ponownie", "Reiniciar", "Reiniciar", "重启", "Перезапуск", "Herstarten"}},
/* S_FACTORY        */ {{"Factory reset", "Réinit. usine", "Werksreset", "Rest. de fabrica", "Ripristino", "Reset fabryczny", "Restauracao", "Reposicao", "恢复出厂设置", "Сброс", "Fabrieksreset"}},
/* S_SIGN_OUT       */ {{ "Sign out", "Déconnexion", "Abmelden", "Cerrar sesion", "Esci", "Wyloguj", "Sair", "Terminar sessao", "退出登录" , "Выйти", "Uitloggen"}},
/* S_BRIGHTNESS     */ {{ "Brightness", "Luminosité", "Helligkeit", "Brillo", "Luminosita", "Jasnosc", "Brilho", "Brilho", "亮度" , "Яркость", "Helderheid"}},
/* S_SLEEP_AFTER    */ {{ "Sleep after", "Veille après", "Ruhe nach", "Reposo tras", "Standby dopo", "Uspij po", "Suspender apos", "Suspender apos", "休眠时间" , "Спящий режим через", "Slaapstand na"}},
/* S_NEVER          */ {{ "Never", "Jamais", "Nie", "Nunca", "Mai", "Nigdy", "Nunca", "Nunca", "从不" , "Никогда", "Nooit"}},
/* S_INSTALLED      */ {{ "Version", "Version", "Version", "Version", "Versione", "Wersja", "Versao", "Versao", "版本" , "Версия", "Versie"}},
/* S_OTA_OFF        */ {{ "Over-the-air updates are not enabled on this build.", "Les mises à jour par le réseau ne sont pas activées.", "Updates uber Funk sind in diesem Build nicht aktiv.", "Las actualizaciones por red no estan activas.", "Gli aggiornamenti via rete non sono attivi.", "Aktualizacje przez siec nie sa wlaczone.", "As atualizacoes pela rede nao estao ativas.", "As atualizacoes pela rede nao estao ativas.", "此版本未启用在线更新。" , "Обновление по Wi-Fi недоступно в этой сборке.", "Updates via wifi zijn in deze versie niet ingeschakeld."}},
/* S_RESTART_Q      */ {{ "Restart the TigerSpool?", "Redémarrer la TigerSpool ?", "TigerSpool neu starten?", "Reiniciar la TigerSpool?", "Riavviare la TigerSpool?", "Uruchomic ponownie TigerSpool?", "Reiniciar o TigerSpool?", "Reiniciar o TigerSpool?", "重启 TigerSpool?" , "Перезапустить TigerSpool?", "TigerSpool herstarten?"}},
/* S_RESTORE        */ {{ "Restore", "Restaurer", "Wiederherstellen", "Restaurar", "Ripristina", "Przywroc", "Restaurar", "Restaurar", "恢复" , "Восстановить", "Herstellen"}},
/* S_CONFIRM        */ {{ "Confirm", "Valider", "Bestatigen", "Confirmar", "Conferma", "Potwierdz", "Confirmar", "Confirmar", "确认" , "Подтвердить", "Bevestigen"}},
/* S_FACTORY_WARN   */ {{ "Restore factory settings?", "Êtes-vous sûr de vouloir restaurer les paramètres d'usine ?", "Werkseinstellungen wiederherstellen?", "Restaurar los ajustes de fabrica?", "Ripristinare le impostazioni di fabbrica?", "Przywrocic ustawienia fabryczne?", "Restaurar as definicoes de fabrica?", "Restaurar as definicoes de fabrica?", "恢复出厂设置?" , "Сбросить к заводским настройкам?", "Fabrieksinstellingen herstellen?"}},
/* S_HOLD_ERASE     */ {{ "Hold to erase", "Maintenir pour effacer", "Zum Loschen halten", "Manten para borrar", "Tieni per cancellare", "Przytrzymaj, aby usunac", "Segure para apagar", "Mantenha para apagar", "长按清除" , "Удерживайте для стирания", "Houd ingedrukt om te wissen"}},
/* S_KEEP_HOLDING   */ {{ "Keep holding...", "Continuez...", "Weiter halten...", "Sigue pulsando...", "Continua a tenere...", "Trzymaj dalej...", "Continue segurando...", "Continue a premir...", "继续按住..." , "Держите...", "Blijf vasthouden..."}},
/* S_CHECK_UPDATE   */ {{ "Check for updates", "Rechercher une mise à jour", "Nach Updates suchen", "Buscar actualizaciones", "Cerca aggiornamenti", "Sprawdz aktualizacje", "Procurar atualizacoes", "Procurar atualizacoes", "检查更新" , "Проверить обновления", "Controleren op updates"}},
/* S_CHECKING       */ {{ "Checking...", "Recherche...", "Suche...", "Buscando...", "Ricerca...", "Sprawdzanie...", "Procurando...", "A procurar...", "检查中..." , "Проверка...", "Controleren..."}},
/* S_UP_TO_DATE     */ {{ "Your TigerSpool is up to date", "Votre TigerSpool est à jour", "Ihr TigerSpool ist aktuell", "Tu TigerSpool esta actualizado", "Il tuo TigerSpool e aggiornato", "Twoj TigerSpool jest aktualny", "Seu TigerSpool esta atualizado", "O seu TigerSpool esta atualizado", "您的 TigerSpool 已是最新" , "TigerSpool обновлён", "Je TigerSpool is up-to-date"}},
/* S_INSTALL        */ {{ "Install", "Installer", "Installieren", "Instalar", "Installa", "Zainstaluj", "Instalar", "Instalar", "安装" , "Установить", "Installeren"}},
/* S_DOWNLOADING    */ {{ "Downloading", "Téléchargement", "Wird geladen", "Descargando", "Download", "Pobieranie", "Baixando", "A transferir", "下载中" , "Загрузка", "Downloaden"}},
/* S_DONT_UNPLUG    */ {{ "Do not unplug the box.", "Ne débranchez pas le boîtier.", "Gerat nicht trennen.", "No desconectes la caja.", "Non scollegare la scatola.", "Nie odlaczaj urzadzenia.", "Nao desconecte a caixa.", "Nao desligue a caixa.", "请勿断开设备电源。" , "Не отключайте устройство.", "Ontkoppel het apparaat niet."}},
/* S_UPDATE_KEEPS   */ {{ "Wi-Fi, account and printers are kept.", "Wi-Fi, compte et imprimantes sont conservés.", "WLAN, Konto und Drucker bleiben erhalten.", "Wi-Fi, cuenta e impresoras se conservan.", "Wi-Fi, account e stampanti sono conservati.", "Wi-Fi, konto i drukarki zostaja zachowane.", "Wi-Fi, conta e impressoras sao mantidos.", "Wi-Fi, conta e impressoras sao mantidos.", "Wi-Fi、账户和打印机将保留。" , "Wi-Fi, аккаунт и принтеры сохранятся.", "Wifi, account en printers blijven behouden."}},
/* S_LATER          */ {{ "Later", "Plus tard", "Spater", "Mas tarde", "Piu tardi", "Pozniej", "Depois", "Mais tarde", "稍后" , "Позже", "Later"}},
/* S_SIGNAL         */ {{ "Signal", "Signal", "Signal", "Senal", "Segnale", "Sygnal", "Sinal", "Sinal", "信号" , "Сигнал", "Signaal"}},
/* S_AUTO           */ {{ "Auto", "Auto", "Auto", "Auto", "Auto", "Auto", "Auto", "Auto", "自动" , "Авто", "Auto"}},
/* S_TAG_PRODUCT    */ {{ "ID Product", "ID Produit", "ID Produkt", "ID Producto", "ID Prodotto", "ID Produkt", "ID Produto", "ID Produto", "产品 ID" , "ID товара", "Product-ID"}},
/* S_TAG_TYPE       */ {{ "Type", "Type", "Typ", "Tipo", "Tipo", "Typ", "Tipo", "Tipo", "类型" , "Тип", "Type"}},
/* S_TAG_BRAND      */ {{ "Brand", "Marque", "Marke", "Marca", "Marca", "Marka", "Marca", "Marca", "品牌" , "Бренд", "Merk"}},
/* S_TAG_ASPECT     */ {{ "Aspect 1/2", "Aspect 1/2", "Aspekt 1/2", "Aspecto 1/2", "Aspetto 1/2", "Aspekt 1/2", "Aspecto 1/2", "Aspeto 1/2", "外观 1/2" , "Вид 1/2", "Uiterlijk 1/2"}},
/* S_TAG_KIND       */ {{ "Kind", "Nature", "Art", "Tipo", "Genere", "Rodzaj", "Genero", "Genero", "种类" , "Категория", "Soort"}},
/* S_TAG_PROTOCOL   */ {{ "Protocol", "Protocole", "Protokoll", "Protocolo", "Protocollo", "Protokol", "Protocolo", "Protocolo", "协议" , "Протокол", "Protocol"}},
/* S_TAG_STAMP      */ {{ "TimeStamp", "Horodatage", "Zeitstempel", "Marca de tiempo", "Marca temporale", "Znacznik czasu", "Data e hora", "Data e hora", "时间戳" , "Метка времени", "Tijdstempel"}},
/* S_TAG_DRY        */ {{ "Dry temp.", "Séchage temp.", "Trocknung Temp.", "Secado temp.", "Essiccazione temp.", "Suszenie temp.", "Secagem temp.", "Secagem temp.", "烘干温度" , "Темп. сушки", "Droogtemp."}},
/* S_TAG_QTY        */ {{ "Quantity", "Quantité", "Menge", "Cantidad", "Quantita", "Ilosc", "Quantidade", "Quantidade", "数量" , "Количество", "Hoeveelheid"}},
/* S_TAG_LEFT       */ {{ "Remaining", "Restant", "Verbleibend", "Restante", "Rimanente", "Pozostalo", "Restante", "Restante", "剩余" , "Остаток", "Resterend"}},
/* S_TAG_COLOURS    */ {{ "Colours 2/3", "Couleurs 2/3", "Farben 2/3", "Colores 2/3", "Colori 2/3", "Kolory 2/3", "Cores 2/3", "Cores 2/3", "颜色 2/3" , "Цвета 2/3", "Kleuren 2/3"}},
/* S_TAG_TD         */ {{ "HueForge TD", "HueForge TD", "HueForge TD", "HueForge TD", "HueForge TD", "HueForge TD", "HueForge TD", "HueForge TD", "HueForge TD" , "HueForge TD", "HueForge TD"}},
/* S_TAG_MESSAGE    */ {{ "Message", "Message", "Nachricht", "Mensaje", "Messaggio", "Wiadomosc", "Mensagem", "Mensagem", "留言" , "Сообщение", "Bericht"}},
/* S_TAG_SIG        */ {{ "Signature", "Signature", "Signatur", "Firma", "Firma", "Podpis", "Assinatura", "Assinatura", "签名" , "Подпись", "Handtekening"}},
/* S_SIG_VALID      */ {{"Certified", "Certifié", "Zertifiziert", "Certificado", "Certificato", "Certyfikowany", "Certificado", "Certificado", "已认证", "Подлинный", "Gecertificeerd"}},
/* S_SIG_INVALID    */ {{"Not certified", "Non certifié", "Nicht zertifiziert", "No certificado", "Non certificato", "Niecertyfikowany", "Não certificado", "Não certificado", "未认证", "Не подлинный", "Niet gecertificeerd"}},
/* S_SIG_NONE       */ {{ "not signed", "non signée", "nicht signiert", "sin firmar", "non firmata", "niepodpisany", "sem assinatura", "sem assinatura", "未签名" , "не подписан", "niet ondertekend"}},
/* S_SIG_NOKEY      */ {{ "no public key", "pas de clé publique", "kein Schlussel", "sin clave publica", "nessuna chiave", "brak klucza", "sem chave publica", "sem chave publica", "无公钥" , "нет открытого ключа", "geen openbare sleutel"}},
/* S_SIG_UNREAD     */ {{ "not read", "non lue", "nicht gelesen", "no leida", "non letta", "nieodczytany", "nao lida", "nao lida", "未读取" , "не прочитано", "niet gelezen"}},
/* S_LINK_TRY       */ {{ "Connecting...", "Connexion...", "Verbinde...", "Conectando...", "Connessione...", "Laczenie...", "Conectando...", "A ligar...", "连接中..." , "Подключение...", "Verbinden..."}},
/* S_LINK_FETCH     */ {{ "Reading your account", "Récupération des données", "Konto wird gelesen", "Leyendo tu cuenta", "Lettura dell account", "Odczyt konta", "Lendo sua conta", "A ler a sua conta", "正在读取账户" , "Чтение аккаунта", "Account lezen"}},
/* S_LINK_ATTEMPT   */ {{ "Attempt %d/%d", "Tentative %d/%d", "Versuch %d/%d", "Intento %d/%d", "Tentativo %d/%d", "Proba %d/%d", "Tentativa %d/%d", "Tentativa %d/%d", "第 %d/%d 次尝试" , "Попытка %d/%d", "Poging %d/%d"}},
/* S_LINK_FAIL      */ {{ "Connection failed", "Échec de connexion", "Verbindung fehlgeschlagen", "Fallo de conexion", "Connessione fallita", "Blad polaczenia", "Falha na conexao", "Falha na ligacao", "连接失败" , "Ошибка подключения", "Verbinding mislukt"}},
/* S_LF_SCAN        */ {{ "Scan the QR code", "Scanner le QR Code", "QR-Code scannen", "Escanea el codigo QR", "Scansiona il codice QR", "Zeskanuj kod QR", "Escaneie o QR code", "Digitalize o codigo QR", "扫描二维码" , "Отсканируйте QR-код", "Scan de QR-code"}},
/* S_CLOUD_ONLY     */ {{ "Working only with LAN Mode + Dev Mode", "Fonctionne uniquement en Mode LAN + Mode Dev", "Nur mit LAN-Modus + Dev-Modus", "Solo funciona con Modo LAN + Modo Dev", "Funziona solo con Modalita LAN + Modalita Dev", "Dziala tylko z trybem LAN + Dev", "Funciona apenas com Modo LAN + Modo Dev", "Funciona apenas com Modo LAN + Modo Dev", "仅支持局域网模式 + 开发者模式" , "Работает только в режиме LAN + режиме разработчика", "Werkt alleen met LAN-modus + dev-modus"}},
/* S_CLOUD_HOW      */ {{ "Scan for tutorial", "Scanner pour le tutoriel", "Fur die Anleitung scannen", "Escanea para el tutorial", "Scansiona per il tutorial", "Zeskanuj po samouczek", "Escaneie para o tutorial", "Digitalize para o tutorial", "扫码查看教程" , "Сканируйте для инструкции", "Scan voor de handleiding"}},
/* S_READER         */ {{ "NFC Tester", "Testeur NFC", "NFC-Tester", "Probador NFC", "Tester NFC", "Tester NFC", "Testador NFC", "Testador NFC", "NFC 测试" , "NFC-тестер", "NFC-tester"}},
/* S_READ_MODE      */ {{ "Scan", "Scan", "Scan", "Escanear", "Scansione", "Skanuj", "Digitalizar", "Digitalizar", "扫描" , "Скан", "Scan"}},
/* S_READ_HINT      */ {{ "What is this?", "Qu'est-ce que c'est ?", "Was ist das?", "¿Qué es esto?", "Che cos'è?", "Co to jest?", "O que é isto?", "O que é isto?", "这是什么？" , "Что это?", "Wat is dit?"}},
/* S_READ_PUT_SPOOL */ {{ "Put a spool", "Posez une bobine", "Spule auflegen", "Coloca una bobina", "Appoggia una bobina", "Poloz szpule", "Coloque uma bobina", "Coloque uma bobine", "请放置线盘" , "Положите катушку", "Leg een spoel"}},
/* S_READ_ON_READER */ {{ "on the reader", "sur le lecteur", "auf den Leser", "en el lector", "sul lettore", "na czytniku", "no leitor", "no leitor", "在读取器上" , "на ридер", "op de lezer"}},
/* S_DRYING         */ {{ "Drying", "Séchage", "Trocknung", "Secado", "Essiccazione", "Suszenie", "Secagem", "Secagem", "烘干" , "Сушка", "Drogen"}},
/* S_BATT_WHY       */ {{ "This board cannot detect it", "La carte ne sait pas la détecter", "Die Platine erkennt ihn nicht", "La placa no puede detectarla", "La scheda non puo rilevarla", "Plytka nie potrafi jej wykryc", "A placa nao consegue deteta-la", "A placa nao consegue deteta-la", "主板无法检测" , "Плата не может это определить", "Dit bord kan het niet detecteren"}},
/* S_SENT_TO_PRINTER*/ {{ "Sent to the printer", "Envoyé à l'imprimante", "An den Drucker gesendet", "Enviado a la impresora", "Inviato alla stampante", "Wyslano do drukarki", "Enviado para a impressora", "Enviado para a impressora", "已发送到打印机" , "Отправлено на принтер", "Naar de printer verzonden"}},
/* S_INSERT_IN      */ {{ "Insert the spool in", "Insérez la bobine dans", "Spule einlegen in", "Inserta la bobina en", "Inserisci la bobina in", "Wloz szpule do", "Insira a bobina em", "Insira a bobine em", "请将线盘装入" , "Вставьте катушку в", "Plaats de spoel in"}},
/* S_WRITE_MODE     */ {{ "Write", "Écriture", "Schreiben", "Escritura", "Scrittura", "Zapis", "Escrita", "Escrita", "写入" , "Запись", "Schrijven"}},
/* S_SOON           */ {{ "Coming soon", "Bientôt disponible", "Demnächst", "Próximamente", "Prossimamente", "Wkrotce", "Em breve", "Em breve", "即将推出" , "Скоро", "Binnenkort"}},
/* S_REMAINING      */ {{ "Remaining", "Restant", "Rest", "Restante", "Rimanente", "Pozostalo", "Restante", "Restante", "剩余" , "Остаток", "Resterend"}},
/* S_READER_OK      */ {{ "Reader ready", "Lecteur prêt", "Leser bereit", "Lector listo", "Lettore pronto", "Czytnik gotowy", "Leitor pronto", "Leitor pronto", "读卡器就绪" , "Ридер готов", "Lezer gereed"}},
/* S_READER_NONE    */ {{ "Reader not found", "Lecteur introuvable", "Leser nicht gefunden", "Lector no encontrado", "Lettore non trovato", "Nie znaleziono czytnika", "Leitor nao encontrado", "Leitor nao encontrado", "未找到读卡器" , "Ридер не найден", "Lezer niet gevonden"}},
/* S_PRESENT_TAG    */ {{ "Hold a spool against the box", "Approchez une bobine du boîtier", "Spule an das Gerat halten", "Acerca una bobina a la caja", "Avvicina una bobina alla scatola", "Przyloz szpule do urzadzenia", "Aproxime uma bobina da caixa", "Aproxime uma bobina da caixa", "请将料盘靠近设备" , "Поднесите катушку к устройству", "Houd een spoel tegen het apparaat"}},
/* S_ORIENTATION   */ {{ "Orientation", "Orientation", "Ausrichtung", "Orientacion", "Orientamento", "Orientacja", "Orientacao", "Orientacao", "方向" , "Ориентация", "Oriëntatie"}},
/* S_RESTARTING     */ {{ "Installed. Restarting...", "Installé. Redémarrage...", "Installiert. Neustart...", "Instalado. Reiniciando...", "Installato. Riavvio...", "Zainstalowano. Restart...", "Instalado. Reiniciando...", "Instalado. A reiniciar...", "已安装, 重启中..." , "Установлено. Перезапуск...", "Geïnstalleerd. Herstarten..."}},
/* S_CHOOSE_PRINTERS */ {{ "Choose printers", "Vos imprimantes", "Drucker wahlen", "Elige impresoras", "Scegli stampanti", "Wybierz drukarki", "Suas impressoras", "Suas impressoras", "选择打印机" , "Выбор принтеров", "Printers kiezen"}},
/* S_SELECT_PRINTERS */ {{ "Select printers", "Choisir les imprimantes", "Drucker wahlen", "Elegir impresoras", "Scegli stampanti", "Wybierz drukarki", "Escolher impressoras", "Escolher impressoras", "选择打印机" , "Выберите принтеры", "Selecteer printers"}},
/* S_BATTERY        */ {{ "Battery", "Batterie", "Batterie", "Bateria", "Batteria", "Bateria", "Bateria", "Bateria", "电池" , "Батарея", "Batterij"}},
/* S_BATT_VOLTAGE   */ {{ "Voltage", "Tension", "Spannung", "Tension", "Tensione", "Napiecie", "Tensao", "Tensao", "电压" , "Напряжение", "Spanning"}},
/* S_BATT_NOTE      */ {{ "Estimated from the voltage", "Estimation d'après la tension", "Aus der Spannung geschatzt", "Estimado por la tension", "Stimato dalla tensione", "Szacowane z napiecia", "Estimado pela tensao", "Estimado pela tensao", "根据电压估算" , "Оценка по напряжению", "Geschat op basis van de spanning"}},
/* S_BATT_CHARGING  */ {{ "Charging", "En charge", "Ladt", "Cargando", "In carica", "Ladowanie", "Carregando", "A carregar", "充电中" , "Зарядка", "Opladen"}},
/* S_BATT_CHARGE_NOTE */ {{ "Estimated, less exact while charging", "Estimation, moins exacte pendant la charge", "Schatzung, beim Laden ungenauer", "Estimado, menos exacto al cargar", "Stima, meno esatta durante la carica", "Szacowane, mniej dokladne przy ladowaniu", "Estimado, menos exato durante a carga", "Estimado, menos exato durante a carga", "估算值，充电时不够准确" , "Оценка, при зарядке менее точна", "Schatting, minder nauwkeurig tijdens het opladen"}},
/* S_BATT_STATE     */ {{ "State", "État", "Status", "Estado", "Stato", "Stan", "Estado", "Estado", "状态" , "Состояние", "Status"}},
/* S_BATT_ON_BATTERY */ {{ "On battery", "Sur batterie", "Akkubetrieb", "Con bateria", "A batteria", "Na baterii", "Na bateria", "Na bateria", "使用电池" , "От батареи", "Op batterij"}},
/* S_BATT_PLUG_IN   */ {{"Plug it in", "Branchez le câble", "Kabel anschliessen", "Conecta el cable", "Collega il cavo", "Podlacz kabel", "Conecte o cabo", "Ligue o cabo", "请接通电源", "Зарядите", "Sluit aan"}},
/* S_BATT_RUNTIME   */ {{ "Runtime left", "Autonomie", "Restlaufzeit", "Autonomia", "Autonomia", "Pozostaly czas", "Autonomia", "Autonomia", "剩余时间" , "Осталось работать", "Resterende looptijd"}},
/* S_BATT_FULL_IN   */ {{ "Full in", "Charge pleine dans", "Voll in", "Completa en", "Carica in", "Pelna za", "Cheia em", "Cheia em", "充满还需" , "Полная через", "Vol over"}},
/* S_UNIT_MIN       */ {{ "min", "min", "Min", "min", "min", "min", "min", "min", "分钟" , "мин", "min"}},
/* S_UNIT_HOUR      */ {{ "h", "h", "Std", "h", "h", "godz", "h", "h", "小时" , "ч", "u"}},
/* S_WAITING_NFC    */ {{ "Waiting NFC...", "En attente NFC...", "Warte auf NFC...", "Esperando NFC...", "In attesa NFC...", "Oczekiwanie na NFC...", "Aguardando NFC...", "A aguardar NFC...", "等待 NFC..." , "Ожидание NFC...", "Wachten op NFC..."}},
/* S_TAG_DIAMETER   */ {{ "Diameter", "Diamètre", "Durchmesser", "Diametro", "Diametro", "Srednica", "Diametro", "Diametro", "直径" , "Диаметр", "Diameter"}},
/* S_TAG_DRY_TIME   */ {{ "Dry time", "Séchage durée", "Trocknungszeit", "Secado tiempo", "Essiccazione tempo", "Suszenie czas", "Secagem tempo", "Secagem tempo", "烘干时间" , "Время сушки", "Droogtijd"}},
/* ---------------------------------------------------------------------------
   The NFC Tester's field names, identical in every language.

   That screen is a bench instrument, not part of the product's interface: it
   dumps what came off the chip so it can be checked against a reader log, a
   datasheet or the TigerTag SDK - all of which are in English. Two people
   comparing the same spool in two countries have to be looking at the same
   words, and a translated "Weight Available" would have to be translated back
   before it could be compared with anything.

   They go through the table rather than being written into the screen because
   that is where drawn text lives; the rows are identical on purpose, and the
   guard that checks the table against the enum keeps them honest.
   --------------------------------------------------------------------------- */
/* S_NT_BRAND       */ {{ "Brand", "Brand", "Brand", "Brand", "Brand", "Brand", "Brand", "Brand", "Brand" , "Brand", "Brand"}},
/* S_NT_TYPE        */ {{ "Type", "Type", "Type", "Type", "Type", "Type", "Type", "Type", "Type" , "Type", "Type"}},
/* S_NT_MATERIAL    */ {{ "Material", "Material", "Material", "Material", "Material", "Material", "Material", "Material", "Material" , "Material", "Material"}},
/* S_NT_MESSAGE     */ {{ "Message", "Message", "Message", "Message", "Message", "Message", "Message", "Message", "Message" , "Message", "Message"}},
/* S_NT_ASPECT      */ {{ "Aspect 1/2", "Aspect 1/2", "Aspect 1/2", "Aspect 1/2", "Aspect 1/2", "Aspect 1/2", "Aspect 1/2", "Aspect 1/2", "Aspect 1/2" , "Aspect 1/2", "Aspect 1/2"}},
/* S_NT_KIND        */ {{ "Kind", "Kind", "Kind", "Kind", "Kind", "Kind", "Kind", "Kind", "Kind" , "Kind", "Kind"}},
/* S_NT_WTOTAL      */ {{ "Weight Total", "Weight Total", "Weight Total", "Weight Total", "Weight Total", "Weight Total", "Weight Total", "Weight Total", "Weight Total" , "Weight Total", "Weight Total"}},
/* S_NT_WAVAIL      */ {{ "Weight Available", "Weight Available", "Weight Available", "Weight Available", "Weight Available", "Weight Available", "Weight Available", "Weight Available", "Weight Available" , "Weight Available", "Weight Available"}},
/* S_NT_DIAM        */ {{ "Diam.", "Diam.", "Diam.", "Diam.", "Diam.", "Diam.", "Diam.", "Diam.", "Diam." , "Diam.", "Diam."}},
/* S_NT_NOZZLE      */ {{ "Nozzle Temp", "Nozzle Temp", "Nozzle Temp", "Nozzle Temp", "Nozzle Temp", "Nozzle Temp", "Nozzle Temp", "Nozzle Temp", "Nozzle Temp" , "Nozzle Temp", "Nozzle Temp"}},
/* S_NT_BED         */ {{ "Bed Temp.", "Bed Temp.", "Bed Temp.", "Bed Temp.", "Bed Temp.", "Bed Temp.", "Bed Temp.", "Bed Temp.", "Bed Temp." , "Bed Temp.", "Bed Temp."}},
/* S_NT_DRY         */ {{ "Drying Temp. / Time", "Drying Temp. / Time", "Drying Temp. / Time", "Drying Temp. / Time", "Drying Temp. / Time", "Drying Temp. / Time", "Drying Temp. / Time", "Drying Temp. / Time", "Drying Temp. / Time" , "Drying Temp. / Time", "Drying Temp. / Time"}},
/* S_NT_STAMP       */ {{ "Timestamp", "Timestamp", "Timestamp", "Timestamp", "Timestamp", "Timestamp", "Timestamp", "Timestamp", "Timestamp" , "Timestamp", "Timestamp"}},
/* S_NT_DATE        */ {{ "Date", "Date", "Date", "Date", "Date", "Date", "Date", "Date", "Date" , "Date", "Date"}},
/* S_NT_COLOR1      */ {{ "Color 1 (RGBA)", "Color 1 (RGBA)", "Color 1 (RGBA)", "Color 1 (RGBA)", "Color 1 (RGBA)", "Color 1 (RGBA)", "Color 1 (RGBA)", "Color 1 (RGBA)", "Color 1 (RGBA)" , "Color 1 (RGBA)", "Color 1 (RGBA)"}},
/* S_NT_COLOR2      */ {{ "Color 2 (RGB)", "Color 2 (RGB)", "Color 2 (RGB)", "Color 2 (RGB)", "Color 2 (RGB)", "Color 2 (RGB)", "Color 2 (RGB)", "Color 2 (RGB)", "Color 2 (RGB)" , "Color 2 (RGB)", "Color 2 (RGB)"}},
/* S_NT_COLOR3      */ {{ "Color 3 (RGB)", "Color 3 (RGB)", "Color 3 (RGB)", "Color 3 (RGB)", "Color 3 (RGB)", "Color 3 (RGB)", "Color 3 (RGB)", "Color 3 (RGB)", "Color 3 (RGB)" , "Color 3 (RGB)", "Color 3 (RGB)"}},
/* S_NT_TD          */ {{ "TD HueForge", "TD HueForge", "TD HueForge", "TD HueForge", "TD HueForge", "TD HueForge", "TD HueForge", "TD HueForge", "TD HueForge" , "TD HueForge", "TD HueForge"}},
/* S_NT_CERT        */ {{ "Certified", "Certified", "Certified", "Certified", "Certified", "Certified", "Certified", "Certified", "Certified" , "Certified", "Certified"}},
/* S_NT_HEX         */ {{ "HEX Code", "HEX Code", "HEX Code", "HEX Code", "HEX Code", "HEX Code", "HEX Code", "HEX Code", "HEX Code" , "HEX Code", "HEX Code"}},
/* S_NT_PAGE        */ {{ "Page", "Page", "Page", "Page", "Page", "Page", "Page", "Page", "Page" , "Page", "Page"}},
/* S_NT_INVALID     */ {{ "INVALID", "INVALID", "INVALID", "INVALID", "INVALID", "INVALID", "INVALID", "INVALID", "INVALID" , "INVALID", "INVALID"}},
/* S_NT_NONE        */ {{ "NONE", "NONE", "NONE", "NONE", "NONE", "NONE", "NONE", "NONE", "NONE" , "NONE", "NONE"}},
/* S_NT_NOKEY       */ {{ "NO KEY", "NO KEY", "NO KEY", "NO KEY", "NO KEY", "NO KEY", "NO KEY", "NO KEY", "NO KEY" , "NO KEY", "NO KEY"}},
/* S_LOAD           */ {{"Load", "Charge", "Auslastung", "Carga", "Carico", "Obciążenie", "Carga", "Carga", "负载", "Нагрузка", "Belasting"}},
/* S_NO_ROOM        */ {{ "Not enough room", "Plus assez de place", "Kein Platz mehr", "No queda espacio", "Spazio esaurito", "Brak miejsca", "Sem espaço", "Sem espaço", "空间不足" , "Недостаточно места", "Onvoldoende ruimte"}},
/* S_SIG_EXCELLENT  */ {{ "Excellent", "Excellent", "Ausgezeichnet", "Excelente", "Eccellente", "Doskonały", "Excelente", "Excelente", "极佳" , "Отличный", "Uitstekend"}},
/* S_SIG_GOOD       */ {{ "Good", "Bon", "Gut", "Buena", "Buono", "Dobry", "Bom", "Bom", "良好" , "Хороший", "Goed"}},
/* S_SIG_FAIR       */ {{ "Fair", "Moyen", "Mittel", "Media", "Medio", "Średni", "Médio", "Médio", "一般" , "Средний", "Redelijk"}},
/* S_SIG_WEAK       */ {{ "Weak", "Faible", "Schwach", "Débil", "Debole", "Słaby", "Fraco", "Fraco", "较弱" , "Слабый", "Zwak"}},
/* S_CHANNEL        */ {{ "Channel", "Canal", "Kanal", "Canal", "Canale", "Kanał", "Canal", "Canal", "信道" , "Канал", "Kanaal"}},
/* S_POWER_OFF      */ {{ "Turn off", "Éteindre", "Ausschalten", "Apagar", "Spegni", "Wyłącz", "Desligar", "Desligar", "关机" , "Выключить", "Uitschakelen"}},
/* S_POWER_OFF_Q    */ {{ "Turn the TigerSpool off?", "Éteindre la TigerSpool ?", "TigerSpool ausschalten?", "Apagar la TigerSpool?", "Spegnere la TigerSpool?", "Wyłączyć TigerSpool?", "Desligar o TigerSpool?", "Desligar o TigerSpool?", "关闭 TigerSpool?" , "Выключить TigerSpool?", "TigerSpool uitschakelen?"}},
/* S_POWER_OFF_HINT */ {{ "To turn it back on, unplug the USB cable and plug it back in.", "Pour la rallumer, débranchez le câble USB puis rebranchez-le.", "Zum Einschalten das USB-Kabel ab- und wieder anstecken.", "Para encenderla, desconecte el cable USB y vuelva a conectarlo.", "Per riaccenderla, scollega il cavo USB e ricollegalo.", "Aby włączyć, odłącz kabel USB i podłącz go ponownie.", "Para ligar de novo, desconecte o cabo USB e conecte-o novamente.", "Para voltar a ligar, desligue o cabo USB e volte a ligá-lo.", "重新插拔 USB 线即可开机。" , "Чтобы включить снова, отключите USB-кабель и подключите заново.", "Om hem weer aan te zetten, haal je de USB-kabel eruit en steek je hem er weer in."}},
/* S_WEIGHT_KEPT    */ {{ "Weight not updated - the printer keeps its previous value", "Poids non mis à jour - l'imprimante garde l'ancienne valeur", "Gewicht nicht aktualisiert - der Drucker behält den alten Wert", "Peso no actualizado - la impresora conserva el valor anterior", "Peso non aggiornato - la stampante mantiene il valore precedente", "Waga nie zaktualizowana - drukarka zachowuje starą wartość", "Peso não atualizado - a impressora mantém o valor anterior", "Peso não atualizado - a impressora mantém o valor anterior", "重量未更新" , "Вес не обновлён - принтер сохраняет прежнее значение", "Gewicht niet bijgewerkt - de printer houdt de vorige waarde"}},
/* S_WEIGHT_FROM    */ {{ "Weight from", "Poids depuis", "Gewicht aus", "Peso desde", "Peso da", "Waga z", "Peso de", "Peso de", "重量" , "Вес из", "Gewicht van"}},
/* S_SRC_CHIP       */ {{ "Chip", "Puce", "Chip", "Chip", "Chip", "Chip", "Chip", "Chip", "标签" , "Чип", "Chip"}},
/* S_SRC_CLOUD      */ {{ "Cloud", "Cloud", "Cloud", "Nube", "Cloud", "Chmura", "Nuvem", "Nuvem", "账户" , "Облако", "Cloud"}},
};

// A mismatch here is silent at runtime and reads as garbled text on screen, so
// let the compiler catch it instead.
static_assert(sizeof(STR) / sizeof(STR[0]) == S_COUNT,
              "i18n table and StrId enum are out of step");

// Each language written in itself: someone looking for Portugues should not
// have to recognise the English word "Portuguese" first.
static const char* const NAMES[LANG_N] = {
    "English", "Français", "Deutsch", "Español",
    "Italiano", "Polski", "Português (BR)", "Português (PT)", "中文",
    "Русский", "Nederlands"
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
