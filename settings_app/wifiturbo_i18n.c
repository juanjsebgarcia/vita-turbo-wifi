/*
 * wifiturbo_i18n.c - translation table + helpers for the "Wi-Fi Turbo" Settings
 * sub-menu. See wifiturbo_i18n.h.
 *
 * =====================================================================
 *  MACHINE-TRANSLATED CONTENT. Only English (US/UK) is authoritative; the
 *  other languages are machine translations so the menu is legible in all
 *  20 Vita system languages. Corrections from native speakers are welcome
 *  (see CONTRIBUTING.md).
 * =====================================================================
 *
 * Encoding: this file is UTF-8. The strings for Japanese, Russian, Korean and
 * Chinese contain multi-byte UTF-8; the Settings GetText hook must convert them
 * with wq_i18n_to_u16() (NOT a per-byte ASCII copy).
 *
 * C99. No SDK headers are included so the file is self-contained; the two
 * platform functions it uses are declared below with ABI-compatible prototypes
 * (the enum first-arg of sceAppUtilSystemParamGetInt is passed as int).
 *
 * Vita stubs required when linking the Settings module (weak, per the plugin's
 * link convention - both libraries are resident in NPXS10015):
 *   - SceRegistryMgr_stub_weak     (sceRegMgrGetKeyInt)
 *   - SceAppUtil_stub_weak         (sceAppUtilSystemParamGetInt)
 */
#include "wifiturbo_i18n.h"

/* ---- platform imports (self-contained prototypes) ---------------------- */

/* SceRegMgrForGame: read an int registry key. Canonical path for the system
 * language on 3.60 is category "/CONFIG/SYSTEM", key "language". */
extern int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);

/* SceAppUtil: SDK system-parameter read. paramId 1 == SCE_SYSTEM_PARAM_ID_LANG.
 * Behind the sceAppUtilInit() gate, hence only a fallback here. */
extern int sceAppUtilSystemParamGetInt(int paramId, int *value);

#define WT_SYSTEM_PARAM_ID_LANG 1

/* ---- translation table ------------------------------------------------- */
/*
 * g_tbl[string_id][language_index]. Column order matches enum wq_i18n_lang_id:
 *   0 ja   1 en-US 2 fr    3 es    4 de    5 it    6 nl    7 pt-PT
 *   8 ru   9 ko   10 zhT  11 zhS  12 fi   13 sv   14 da   15 no
 *  16 pl  17 pt-BR 18 en-GB 19 tr
 *
 * "Wi-Fi Turbo" is a product name and is kept verbatim in every language.
 */
static const char *const g_tbl[WT_I18N_COUNT][WT_LANG_COUNT] = {

/* WT_I18N_TITLE */
{
	"Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo",
	"Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo",
	"Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo",
	"Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo",
	"Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo", "Wi-Fi Turbo"
},

/* WT_I18N_ROW_KEEP - "Keep Wi-Fi on" */
{
	"Wi-Fiを常にオンにする",
	"Keep Wi-Fi on",
	"Garder le Wi-Fi activé",
	"Mantener el Wi-Fi activado",
	"WLAN eingeschaltet lassen",
	"Mantieni il Wi-Fi attivo",
	"Wi-Fi ingeschakeld houden",
	"Manter o Wi-Fi ligado",
	"Держать Wi-Fi включённым",
	"Wi-Fi 항상 켜기",
	"保持 Wi-Fi 開啟",
	"保持 Wi-Fi 开启",
	"Pidä Wi-Fi päällä",
	"Håll Wi-Fi på",
	"Hold Wi-Fi tændt",
	"Hold Wi-Fi på",
	"Nie wyłączaj Wi-Fi",
	"Manter o Wi-Fi ligado",
	"Keep Wi-Fi on",
	"Wi-Fi'yi açık tut"
},

/* WT_I18N_ROW_HUD - "Wi-Fi HUD overlay" */
{
	"Wi-Fi HUDオーバーレイ",
	"Wi-Fi HUD overlay",
	"Superposition HUD Wi-Fi",
	"Superposición HUD Wi-Fi",
	"Wi-Fi-HUD-Overlay",
	"Overlay HUD Wi-Fi",
	"Wi-Fi HUD-overlay",
	"Sobreposição HUD Wi-Fi",
	"Наложение Wi-Fi HUD",
	"Wi-Fi HUD 오버레이",
	"Wi-Fi HUD 疊加顯示",
	"Wi-Fi HUD 叠加显示",
	"Wi-Fi HUD -peittokuva",
	"Wi-Fi HUD-överlägg",
	"Wi-Fi HUD-overlay",
	"Wi-Fi HUD-overlegg",
	"Nakładka HUD Wi-Fi",
	"Sobreposição HUD Wi-Fi",
	"Wi-Fi HUD overlay",
	"Wi-Fi HUD katmanı"
},

/* WT_I18N_ROW_RECONNECT - "Auto-reconnect on drop" */
{
	"切断時に自動再接続",
	"Auto-reconnect on drop",
	"Reconnexion auto après coupure",
	"Reconexión automática al caerse",
	"Automatisch neu verbinden bei Abbruch",
	"Riconnessione automatica alla caduta",
	"Automatisch opnieuw verbinden bij uitval",
	"Reconexão automática ao cair",
	"Автоподключение при обрыве",
	"끊길 때 자동 재연결",
	"斷線時自動重新連線",
	"断线时自动重新连接",
	"Yhdistä uudelleen katketessa",
	"Återanslut automatiskt vid avbrott",
	"Gentilslut automatisk ved afbrydelse",
	"Koble til igjen automatisk ved brudd",
	"Automatyczne łączenie po zerwaniu",
	"Reconexão automática ao cair",
	"Auto-reconnect on drop",
	"Koptuğunda otomatik yeniden bağlan"
},

/* WT_I18N_ROW_ROAM - "Smart Mesh Roaming" */
{
	"スマートメッシュローミング",
	"Smart Mesh Roaming",
	"Itinérance maillée intelligente",
	"Roaming mesh inteligente",
	"Intelligentes Mesh-Roaming",
	"Roaming mesh intelligente",
	"Slimme mesh-roaming",
	"Roaming mesh inteligente",
	"Умный mesh-роуминг",
	"스마트 메시 로밍",
	"智慧 Mesh 漫遊",
	"智能 Mesh 漫游",
	"Älykäs mesh-verkkovierailu",
	"Smart mesh-roaming",
	"Smart mesh-roaming",
	"Smart mesh-roaming",
	"Inteligentny roaming mesh",
	"Roaming mesh inteligente",
	"Smart Mesh Roaming",
	"Akıllı mesh dolaşımı"
},

/* WT_I18N_ROW_HT40 - "40 MHz" */
{
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 МГц",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz",
	"40 MHz"
},

/* WT_I18N_ROW_POWER - "Power profile" */
{
	"電力プロファイル",
	"Power profile",
	"Profil d'alimentation",
	"Perfil de energía",
	"Energieprofil",
	"Profilo di alimentazione",
	"Energieprofiel",
	"Perfil de energia",
	"Профиль питания",
	"전원 프로필",
	"電源設定檔",
	"电源配置文件",
	"Virtaprofiili",
	"Energiprofil",
	"Strømprofil",
	"Strømprofil",
	"Profil zasilania",
	"Perfil de energia",
	"Power profile",
	"Güç profili"
},

/* WT_I18N_ITEM_MAXPERF - "Max performance" */
{
	"最大パフォーマンス",
	"Max performance",
	"Performances maximales",
	"Máximo rendimiento",
	"Maximale Leistung",
	"Prestazioni massime",
	"Maximale prestaties",
	"Desempenho máximo",
	"Макс. производительность",
	"최대 성능",
	"最高效能",
	"最高性能",
	"Paras suorituskyky",
	"Maximal prestanda",
	"Maksimal ydeevne",
	"Maksimal ytelse",
	"Maksymalna wydajność",
	"Desempenho máximo",
	"Max performance",
	"Maksimum performans"
},

/* WT_I18N_ITEM_BALANCED - "Balanced" */
{
	"バランス",
	"Balanced",
	"Équilibré",
	"Equilibrado",
	"Ausgeglichen",
	"Bilanciato",
	"Gebalanceerd",
	"Equilibrado",
	"Сбалансированный",
	"균형",
	"平衡",
	"平衡",
	"Tasapainotettu",
	"Balanserad",
	"Balanceret",
	"Balansert",
	"Zrównoważony",
	"Equilibrado",
	"Balanced",
	"Dengeli"
},

/* WT_I18N_ITEM_BATTERY - "Battery saver" */
{
	"バッテリーセーバー",
	"Battery saver",
	"Économie de batterie",
	"Ahorro de batería",
	"Energiesparmodus",
	"Risparmio batteria",
	"Batterijbesparing",
	"Poupança de bateria",
	"Экономия заряда",
	"배터리 절약",
	"省電模式",
	"省电模式",
	"Virransäästö",
	"Batterisparläge",
	"Batterisparefunktion",
	"Batterisparing",
	"Oszczędzanie baterii",
	"Economia de bateria",
	"Battery saver",
	"Pil tasarrufu"
},

/* WT_I18N_HINT_KEEP */
{
	"本体がスタンバイに入るのを防ぎ、アイドル中の接続がルーターに切断されないようにします。ダウンロードやリモート操作が途切れません。バッテリー消費が増えます。",
	"Stops the console going into standby and keeps the router from dropping an idle connection, so downloads and remote sessions keep running. Uses more battery.",
	"Empêche la console de passer en veille et évite que le routeur coupe une connexion inactive : les téléchargements et sessions à distance continuent. Consomme plus de batterie.",
	"Evita que la consola entre en reposo y que el router corte una conexión inactiva, para que las descargas y sesiones remotas sigan activas. Consume más batería.",
	"Verhindert den Standby der Konsole und dass der Router eine inaktive Verbindung trennt, damit Downloads und Fernsitzungen weiterlaufen. Verbraucht mehr Akku.",
	"Impedisce alla console di andare in standby e al router di chiudere una connessione inattiva, così download e sessioni remote proseguono. Consuma più batteria.",
	"Voorkomt dat de console in stand-by gaat en dat de router een inactieve verbinding verbreekt, zodat downloads en externe sessies doorlopen. Verbruikt meer batterij.",
	"Impede a consola de entrar em espera e o router de cortar uma ligação inativa, para que transferências e sessões remotas continuem. Gasta mais bateria.",
	"Не даёт консоли уйти в режим ожидания, а роутеру — разорвать неактивное соединение, чтобы загрузки и удалённые сеансы не прерывались. Расходует больше заряда.",
	"본체가 대기 모드로 전환되지 않게 하고 라우터가 유휴 연결을 끊지 않도록 하여 다운로드와 원격 세션이 계속 유지됩니다. 배터리 소모가 늘어납니다.",
	"防止主機進入待機，並避免路由器中斷閒置連線，讓下載與遠端工作階段持續進行。會增加電池消耗。",
	"防止主机进入待机，并避免路由器断开空闲连接，让下载和远程会话持续进行。会增加电池消耗。",
	"Estää konsolia siirtymästä valmiustilaan ja reititintä katkaisemasta käyttämätöntä yhteyttä, joten lataukset ja etäistunnot jatkuvat. Kuluttaa enemmän akkua.",
	"Hindrar konsolen från att gå i vänteläge och routern från att koppla bort en inaktiv anslutning, så att nedladdningar och fjärrsessioner fortsätter. Drar mer batteri.",
	"Forhindrer konsollen i at gå på standby og routeren i at afbryde en inaktiv forbindelse, så downloads og fjernsessioner fortsætter. Bruger mere batteri.",
	"Hindrer konsollen i å gå i ventemodus og ruteren i å koble fra en inaktiv tilkobling, slik at nedlastinger og fjernøkter fortsetter. Bruker mer batteri.",
	"Zapobiega przejściu konsoli w tryb czuwania i rozłączaniu bezczynnego połączenia przez router, więc pobieranie i sesje zdalne trwają dalej. Zużywa więcej baterii.",
	"Impede o console de entrar em espera e o roteador de derrubar uma conexão ociosa, para que downloads e sessões remotas continuem. Consome mais bateria.",
	"Stops the console going into standby and keeps the router from dropping an idle connection, so downloads and remote sessions keep running. Uses more battery.",
	"Konsolun bekleme moduna geçmesini ve yönlendiricinin boştaki bağlantıyı kesmesini önler; indirmeler ve uzak oturumlar sürer. Daha fazla pil tüketir."
},

/* WT_I18N_HINT_HUD */
{
	"Wi-Fiの状態を画面上にリアルタイム表示します。コンパクト：ネットワーク、電波強度、チャンネルを1行で表示。拡張：リンク速度、BSSID、電力プロファイル、カスタムMACアドレス使用時はそのアドレスも表示。",
	"Show live Wi-Fi status over the screen. Compact: one line with the network, signal and channel. Extended: adds link speed, BSSID, power profile and, with a custom MAC address, the address.",
	"Affiche l'état du Wi-Fi en temps réel à l'écran. Compact : une ligne avec le réseau, le signal et le canal. Étendu : ajoute le débit, le BSSID, le profil d'énergie et, avec une adresse MAC personnalisée, l'adresse.",
	"Muestra el estado del Wi-Fi en tiempo real en pantalla. Compacto: una línea con la red, la señal y el canal. Extendido: añade la velocidad del enlace, el BSSID, el perfil de energía y, con una dirección MAC personalizada, la dirección.",
	"Zeigt den WLAN-Status live auf dem Bildschirm. Kompakt: eine Zeile mit Netzwerk, Signal und Kanal. Erweitert: zusätzlich Verbindungsgeschwindigkeit, BSSID, Energieprofil und bei benutzerdefinierter MAC-Adresse die Adresse.",
	"Mostra lo stato del Wi-Fi in tempo reale sullo schermo. Compatto: una riga con rete, segnale e canale. Esteso: aggiunge velocità del collegamento, BSSID, profilo energetico e, con un indirizzo MAC personalizzato, l'indirizzo.",
	"Toont de wifistatus live op het scherm. Compact: één regel met netwerk, signaal en kanaal. Uitgebreid: voegt verbindingssnelheid, BSSID, energieprofiel en, bij een aangepast MAC-adres, het adres toe.",
	"Mostra o estado do Wi-Fi em tempo real no ecrã. Compacto: uma linha com a rede, o sinal e o canal. Alargado: acrescenta a velocidade da ligação, o BSSID, o perfil de energia e, com um endereço MAC personalizado, o endereço.",
	"Показывает состояние Wi-Fi на экране в реальном времени. Компактно: одна строка с сетью, сигналом и каналом. Расширенно: добавляет скорость соединения, BSSID, профиль питания и, если задан свой MAC-адрес, этот адрес.",
	"화면에 Wi-Fi 상태를 실시간으로 표시합니다. 간단히: 네트워크, 신호, 채널을 한 줄로 표시. 자세히: 링크 속도, BSSID, 전원 프로필, 사용자 지정 MAC 주소 사용 시 해당 주소를 추가로 표시.",
	"在畫面上即時顯示 Wi-Fi 狀態。精簡：以一行顯示網路、訊號與頻道。詳細：另外顯示連線速度、BSSID、電源設定檔，以及使用自訂 MAC 位址時的位址。",
	"在屏幕上实时显示 Wi-Fi 状态。精简：以一行显示网络、信号和频道。详细：另外显示连接速度、BSSID、电源配置，以及使用自定义 MAC 地址时的地址。",
	"Näyttää Wi-Fi-tilan reaaliajassa näytöllä. Tiivis: yksi rivi verkosta, signaalista ja kanavasta. Laaja: lisää yhteysnopeuden, BSSID:n, virtaprofiilin ja mukautetun MAC-osoitteen ollessa käytössä osoitteen.",
	"Visar Wi-Fi-status i realtid på skärmen. Kompakt: en rad med nätverk, signal och kanal. Utökad: lägger till länkhastighet, BSSID, energiprofil och, med en anpassad MAC-adress, adressen.",
	"Viser Wi-Fi-status live på skærmen. Kompakt: én linje med netværk, signal og kanal. Udvidet: tilføjer forbindelseshastighed, BSSID, strømprofil og, med en brugerdefineret MAC-adresse, adressen.",
	"Viser Wi-Fi-status live på skjermen. Kompakt: én linje med nettverk, signal og kanal. Utvidet: legger til tilkoblingshastighet, BSSID, strømprofil og, med en egendefinert MAC-adresse, adressen.",
	"Pokazuje stan Wi-Fi na ekranie na żywo. Kompaktowy: jeden wiersz z siecią, sygnałem i kanałem. Rozszerzony: dodaje szybkość łącza, BSSID, profil zasilania oraz, przy własnym adresie MAC, ten adres.",
	"Mostra o status do Wi-Fi em tempo real na tela. Compacto: uma linha com a rede, o sinal e o canal. Estendido: adiciona a velocidade do link, o BSSID, o perfil de energia e, com um endereço MAC personalizado, o endereço.",
	"Show live Wi-Fi status over the screen. Compact: one line with the network, signal and channel. Extended: adds link speed, BSSID, power profile and, with a custom MAC address, the address.",
	"Wi-Fi durumunu ekranda canlı gösterir. Kompakt: ağ, sinyal ve kanal tek satırda. Genişletilmiş: bağlantı hızı, BSSID, güç profili ve özel bir MAC adresi kullanılıyorsa bu adresi ekler."
},

/* WT_I18N_HINT_RECONNECT */
{
	"リンクが予期せず切断されたとき、直前のアクセスポイントに自動的に再接続します。",
	"Automatically reconnect to the last access point whenever the link is unexpectedly lost.",
	"Se reconnecte automatiquement au dernier point d'accès lorsque la connexion est perdue de façon inattendue.",
	"Se reconecta automáticamente al último punto de acceso cuando se pierde la conexión de forma inesperada.",
	"Stellt automatisch die Verbindung zum letzten Zugangspunkt wieder her, wenn die Verbindung unerwartet abbricht.",
	"Si riconnette automaticamente all'ultimo punto di accesso quando il collegamento si interrompe inaspettatamente.",
	"Maakt automatisch opnieuw verbinding met het laatste toegangspunt wanneer de verbinding onverwacht wegvalt.",
	"Liga-se automaticamente ao último ponto de acesso sempre que a ligação é perdida inesperadamente.",
	"Автоматически переподключается к последней точке доступа при внезапном обрыве связи.",
	"연결이 예기치 않게 끊기면 마지막 액세스 포인트에 자동으로 다시 연결합니다.",
	"當連線意外中斷時，自動重新連接到最後使用的存取點。",
	"当连接意外中断时，自动重新连接到最后使用的接入点。",
	"Yhdistää automaattisesti viimeisimpään tukiasemaan, kun yhteys katkeaa odottamatta.",
	"Återansluter automatiskt till den senaste åtkomstpunkten när anslutningen oväntat bryts.",
	"Opretter automatisk forbindelse til det seneste adgangspunkt igen, når forbindelsen uventet mistes.",
	"Kobler automatisk til det siste tilgangspunktet igjen når tilkoblingen uventet mistes.",
	"Automatycznie ponownie łączy się z ostatnim punktem dostępu, gdy połączenie zostanie nieoczekiwanie utracone.",
	"Reconecta-se automaticamente ao último ponto de acesso sempre que a conexão cai inesperadamente.",
	"Automatically reconnect to the last access point whenever the link is unexpectedly lost.",
	"Bağlantı beklenmedik şekilde koptuğunda son erişim noktasına otomatik olarak yeniden bağlanır."
},

/* WT_I18N_HINT_ROAM */
{
	"移動中に同じネットワークのより電波の強いアクセスポイントへ自動的に切り替え、最良の信号を維持します。",
	"Switch to a stronger access point for the same network as you move, to keep the best signal.",
	"Bascule vers un point d'accès plus puissant du même réseau lorsque vous vous déplacez, pour conserver le meilleur signal.",
	"Cambia a un punto de acceso más potente de la misma red a medida que te mueves, para mantener la mejor señal.",
	"Wechselt beim Bewegen zu einem stärkeren Zugangspunkt desselben Netzwerks, um das beste Signal zu behalten.",
	"Passa a un punto di accesso più potente della stessa rete mentre ti sposti, per mantenere il segnale migliore.",
	"Schakelt tijdens het bewegen over naar een sterker toegangspunt van hetzelfde netwerk om het beste signaal te behouden.",
	"Muda para um ponto de acesso mais forte da mesma rede à medida que se desloca, para manter o melhor sinal.",
	"Переключается на более сильную точку доступа той же сети при перемещении, чтобы сохранить лучший сигнал.",
	"이동 중에 같은 네트워크의 더 강한 액세스 포인트로 전환하여 최상의 신호를 유지합니다.",
	"移動時自動切換到同一網路中訊號較強的存取點，以維持最佳訊號。",
	"移动时自动切换到同一网络中信号较强的接入点，以保持最佳信号。",
	"Vaihtaa liikkuessasi saman verkon vahvempaan tukiasemaan parhaan signaalin säilyttämiseksi.",
	"Växlar till en starkare åtkomstpunkt i samma nätverk när du rör dig, för att behålla bästa signal.",
	"Skifter til et stærkere adgangspunkt på samme netværk, når du bevæger dig, for at bevare det bedste signal.",
	"Bytter til et sterkere tilgangspunkt på samme nettverk mens du beveger deg, for å beholde best mulig signal.",
	"Przełącza na silniejszy punkt dostępu tej samej sieci podczas przemieszczania się, aby zachować najlepszy sygnał.",
	"Muda para um ponto de acesso mais forte da mesma rede conforme você se move, para manter o melhor sinal.",
	"Switch to a stronger access point for the same network as you move, to keep the best signal.",
	"Hareket ederken en iyi sinyali korumak için aynı ağdaki daha güçlü bir erişim noktasına geçer."
},

/* WT_I18N_HINT_HT40 */
{
	"2.4 GHz帯で40 MHz幅のチャンネルを許可してスループットを高めます。40 MHzに設定されたアクセスポイントが必要です。",
	"Allow 40 MHz-wide channels on 2.4 GHz for higher throughput. Needs an access point set to 40 MHz.",
	"Autorise les canaux de 40 MHz de large sur 2,4 GHz pour un débit plus élevé. Nécessite un point d'accès réglé sur 40 MHz.",
	"Permite canales de 40 MHz de ancho en 2,4 GHz para mayor velocidad. Requiere un punto de acceso configurado a 40 MHz.",
	"Erlaubt 40 MHz breite Kanäle im 2,4-GHz-Band für höheren Durchsatz. Erfordert einen auf 40 MHz eingestellten Access Point.",
	"Consente canali larghi 40 MHz sui 2,4 GHz per un throughput maggiore. Richiede un access point impostato a 40 MHz.",
	"Staat 40 MHz-brede kanalen op 2,4 GHz toe voor een hogere doorvoer. Vereist een access point dat op 40 MHz is ingesteld.",
	"Permite canais de 40 MHz de largura em 2,4 GHz para maior débito. Requer um ponto de acesso configurado para 40 MHz.",
	"Разрешает каналы шириной 40 МГц в диапазоне 2,4 ГГц для большей скорости. Требуется точка доступа, настроенная на 40 МГц.",
	"더 높은 처리량을 위해 2.4 GHz에서 40 MHz 폭 채널을 허용합니다. 40 MHz로 설정된 액세스 포인트가 필요합니다.",
	"在 2.4 GHz 上允許 40 MHz 寬頻道以提升傳輸量。需要設定為 40 MHz 的無線基地台。",
	"在 2.4 GHz 上允许 40 MHz 宽信道以提升吞吐量。需要设置为 40 MHz 的无线接入点。",
	"Sallii 40 MHz:n levyiset kanavat 2,4 GHz:n taajuudella suuremman suoritustehon saavuttamiseksi. Vaatii 40 MHz:iin asetetun tukiaseman.",
	"Tillåter 40 MHz breda kanaler på 2,4 GHz för högre genomströmning. Kräver en åtkomstpunkt inställd på 40 MHz.",
	"Tillader 40 MHz brede kanaler på 2,4 GHz for højere gennemløb. Kræver et adgangspunkt indstillet til 40 MHz.",
	"Tillater 40 MHz brede kanaler på 2,4 GHz for høyere gjennomstrømning. Krever et tilgangspunkt satt til 40 MHz.",
	"Zezwala na kanały o szerokości 40 MHz w paśmie 2,4 GHz dla większej przepustowości. Wymaga punktu dostępowego ustawionego na 40 MHz.",
	"Permite canais de 40 MHz de largura em 2,4 GHz para maior taxa de transferência. Requer um ponto de acesso configurado para 40 MHz.",
	"Allow 40 MHz-wide channels on 2.4 GHz for higher throughput. Needs an access point set to 40 MHz.",
	"Daha yüksek verim için 2,4 GHz'de 40 MHz genişliğinde kanallara izin verir. 40 MHz'e ayarlı bir erişim noktası gerektirir."
},

/* WT_I18N_HINT_POWER */
{
	"Wi-Fiがバッテリー駆動時間と性能のどちらをどの程度優先するかを選びます。",
	"Choose how aggressively Wi-Fi trades battery life for performance.",
	"Choisissez dans quelle mesure le Wi-Fi privilégie les performances au détriment de l'autonomie.",
	"Elige con qué intensidad el Wi-Fi sacrifica la duración de la batería en favor del rendimiento.",
	"Legt fest, wie stark das WLAN die Akkulaufzeit zugunsten der Leistung eintauscht.",
	"Scegli quanto il Wi-Fi sacrifica la durata della batteria a favore delle prestazioni.",
	"Kies in welke mate Wi-Fi accuduur inruilt voor prestaties.",
	"Escolha até que ponto o Wi-Fi troca a autonomia da bateria por desempenho.",
	"Выберите, насколько активно Wi-Fi жертвует временем работы от батареи ради производительности.",
	"Wi-Fi가 배터리 수명과 성능 사이에서 어느 쪽을 얼마나 우선할지 선택합니다.",
	"選擇 Wi-Fi 以多大程度犧牲電池續航來換取效能。",
	"选择 Wi-Fi 以多大程度牺牲电池续航来换取性能。",
	"Valitse, kuinka voimakkaasti Wi-Fi tinkii akunkestosta suorituskyvyn hyväksi.",
	"Välj hur mycket Wi-Fi offrar batteritid för prestanda.",
	"Vælg, hvor meget Wi-Fi bytter batterilevetid for ydeevne.",
	"Velg hvor mye Wi-Fi bytter batteritid mot ytelse.",
	"Wybierz, jak mocno Wi-Fi poświęca czas pracy baterii na rzecz wydajności.",
	"Escolha o quanto o Wi-Fi troca a duração da bateria por desempenho.",
	"Choose how aggressively Wi-Fi trades battery life for performance.",
	"Wi-Fi'nin performans için pil ömründen ne kadar ödün vereceğini seçin."
},

/* WT_I18N_ROW_BTWARN - "Bluetooth impacts Wi-Fi performance" */
{
	"BluetoothはWi-Fiの性能に影響します",
	"Bluetooth impacts Wi-Fi performance",
	"Le Bluetooth réduit les performances Wi-Fi",
	"El Bluetooth afecta al rendimiento del Wi-Fi",
	"Bluetooth beeinträchtigt die WLAN-Leistung",
	"Il Bluetooth influisce sulle prestazioni del Wi-Fi",
	"Bluetooth beïnvloedt de Wi-Fi-prestaties",
	"O Bluetooth afeta o desempenho do Wi-Fi",
	"Bluetooth снижает производительность Wi-Fi",
	"블루투스는 Wi-Fi 성능에 영향을 줍니다",
	"藍牙會影響 Wi-Fi 效能",
	"蓝牙会影响 Wi-Fi 性能",
	"Bluetooth heikentää Wi-Fi-suorituskykyä",
	"Bluetooth påverkar Wi-Fi-prestandan",
	"Bluetooth påvirker Wi-Fi-ydeevnen",
	"Bluetooth påvirker Wi-Fi-ytelsen",
	"Bluetooth obniża wydajność Wi-Fi",
	"O Bluetooth afeta o desempenho do Wi-Fi",
	"Bluetooth impacts Wi-Fi performance",
	"Bluetooth, Wi-Fi performansını etkiler"
},

/* WT_I18N_HINT_BTWARN */
{
	"Wi-FiとBluetoothは同じ無線を共有しています。Wi-Fiの速度と応答性を最大にするには、Bluetoothをオフにしてください。",
	"Wi-Fi and Bluetooth share one radio. Turn Bluetooth off for the best Wi-Fi speed and latency.",
	"Le Wi-Fi et le Bluetooth partagent la même radio. Désactivez le Bluetooth pour une vitesse et une latence Wi-Fi optimales.",
	"El Wi-Fi y el Bluetooth comparten la misma radio. Desactiva el Bluetooth para obtener la mejor velocidad y latencia del Wi-Fi.",
	"WLAN und Bluetooth teilen sich ein Funkmodul. Schalte Bluetooth aus, um die beste WLAN-Geschwindigkeit und Latenz zu erhalten.",
	"Wi-Fi e Bluetooth condividono la stessa radio. Disattiva il Bluetooth per ottenere la migliore velocità e latenza del Wi-Fi.",
	"Wi-Fi en Bluetooth delen één radio. Schakel Bluetooth uit voor de beste Wi-Fi-snelheid en -latentie.",
	"O Wi-Fi e o Bluetooth partilham o mesmo rádio. Desligue o Bluetooth para obter a melhor velocidade e latência do Wi-Fi.",
	"Wi-Fi и Bluetooth используют один радиомодуль. Отключите Bluetooth для максимальной скорости и минимальной задержки Wi-Fi.",
	"Wi-Fi와 블루투스는 하나의 무선 장치를 공유합니다. 최상의 Wi-Fi 속도와 지연 시간을 위해 블루투스를 끄세요.",
	"Wi-Fi 與藍牙共用同一個無線電。關閉藍牙可獲得最佳的 Wi-Fi 速度與延遲。",
	"Wi-Fi 与蓝牙共用同一个无线电。关闭蓝牙可获得最佳的 Wi-Fi 速度与延迟。",
	"Wi-Fi ja Bluetooth jakavat saman radion. Poista Bluetooth käytöstä parhaan Wi-Fi-nopeuden ja -viiveen saavuttamiseksi.",
	"Wi-Fi och Bluetooth delar på samma radio. Stäng av Bluetooth för bästa Wi-Fi-hastighet och latens.",
	"Wi-Fi og Bluetooth deler den samme radio. Slå Bluetooth fra for at få den bedste Wi-Fi-hastighed og latens.",
	"Wi-Fi og Bluetooth deler den samme radioen. Slå av Bluetooth for best mulig Wi-Fi-hastighet og forsinkelse.",
	"Wi-Fi i Bluetooth korzystają z tego samego modułu radiowego. Wyłącz Bluetooth, aby uzyskać najlepszą szybkość i opóźnienia Wi-Fi.",
	"O Wi-Fi e o Bluetooth compartilham o mesmo rádio. Desative o Bluetooth para obter a melhor velocidade e latência do Wi-Fi.",
	"Wi-Fi and Bluetooth share one radio. Turn Bluetooth off for the best Wi-Fi speed and latency.",
	"Wi-Fi ve Bluetooth aynı radyoyu paylaşır. En iyi Wi-Fi hızı ve gecikmesi için Bluetooth'u kapatın."
},

/* WT_I18N_ROW_DEVMODE - "Developer mode (remote control)" */
{
	"開発者モード（リモート操作）",
	"Developer mode (remote control)",
	"Mode développeur (contrôle à distance)",
	"Modo desarrollador (control remoto)",
	"Entwicklermodus (Fernsteuerung)",
	"Modalità sviluppatore (controllo remoto)",
	"Ontwikkelaarsmodus (bediening op afstand)",
	"Modo de programador (controlo remoto)",
	"Режим разработчика (удалённое управление)",
	"개발자 모드 (원격 제어)",
	"開發者模式（遠端控制）",
	"开发者模式（远程控制）",
	"Kehittäjätila (etäohjaus)",
	"Utvecklarläge (fjärrstyrning)",
	"Udviklertilstand (fjernstyring)",
	"Utviklermodus (fjernstyring)",
	"Tryb dewelopera (zdalne sterowanie)",
	"Modo desenvolvedor (controle remoto)",
	"Developer mode (remote control)",
	"Geliştirici modu (uzaktan kontrol)"
},

/* WT_I18N_HINT_DEVMODE */
{
	"スクリプトやテスト用に、ネットワーク上でリモート操作ポート（TCP 1339）を開きます。同じネットワーク上の誰でも使用できるため、必要なとき以外はオフにしてください。",
	"Opens a remote-control port (TCP 1339) on your network for scripting and testing. Anyone on the same network can use it, so leave this off unless you need it.",
	"Ouvre un port de contrôle à distance (TCP 1339) sur votre réseau pour les scripts et les tests. Toute personne sur le même réseau peut l'utiliser : laissez-le désactivé sauf si vous en avez besoin.",
	"Abre un puerto de control remoto (TCP 1339) en tu red para scripts y pruebas. Cualquiera en la misma red puede usarlo, así que déjalo desactivado salvo que lo necesites.",
	"Öffnet einen Fernsteuerungs-Port (TCP 1339) in deinem Netzwerk für Skripte und Tests. Jeder im selben Netzwerk kann ihn nutzen, lass ihn daher aus, wenn du ihn nicht brauchst.",
	"Apre una porta di controllo remoto (TCP 1339) sulla tua rete per script e test. Chiunque sulla stessa rete può usarla, quindi lasciala disattivata se non ti serve.",
	"Opent een poort voor bediening op afstand (TCP 1339) op je netwerk voor scripts en tests. Iedereen op hetzelfde netwerk kan hem gebruiken, dus laat dit uit tenzij je het nodig hebt.",
	"Abre uma porta de controlo remoto (TCP 1339) na sua rede para scripts e testes. Qualquer pessoa na mesma rede pode usá-la, por isso mantenha desligado salvo se precisar.",
	"Открывает порт удалённого управления (TCP 1339) в вашей сети для скриптов и тестирования. Им может воспользоваться любой в той же сети, поэтому держите его выключенным, если он не нужен.",
	"스크립트와 테스트를 위해 네트워크에 원격 제어 포트(TCP 1339)를 엽니다. 같은 네트워크의 누구나 사용할 수 있으므로 필요할 때가 아니면 꺼 두세요.",
	"在網路上開啟遠端控制連接埠（TCP 1339），用於指令碼與測試。同一網路上的任何人都能使用，因此不需要時請保持關閉。",
	"在网络上打开远程控制端口（TCP 1339），用于脚本和测试。同一网络中的任何人都能使用，因此不需要时请保持关闭。",
	"Avaa etäohjausportin (TCP 1339) verkkoosi skriptejä ja testausta varten. Kuka tahansa samassa verkossa voi käyttää sitä, joten pidä se pois päältä, ellet tarvitse sitä.",
	"Öppnar en port för fjärrstyrning (TCP 1339) i ditt nätverk för skript och tester. Alla i samma nätverk kan använda den, så låt den vara av om du inte behöver den.",
	"Åbner en port til fjernstyring (TCP 1339) på dit netværk til scripts og test. Alle på samme netværk kan bruge den, så lad den være slået fra, medmindre du har brug for den.",
	"Åpner en port for fjernstyring (TCP 1339) på nettverket ditt for skript og testing. Alle på samme nettverk kan bruke den, så la den være av med mindre du trenger den.",
	"Otwiera port zdalnego sterowania (TCP 1339) w Twojej sieci do skryptów i testów. Każdy w tej samej sieci może z niego korzystać, więc pozostaw go wyłączonego, jeśli go nie potrzebujesz.",
	"Abre uma porta de controle remoto (TCP 1339) na sua rede para scripts e testes. Qualquer pessoa na mesma rede pode usá-la, então deixe desativado, a menos que precise.",
	"Opens a remote-control port (TCP 1339) on your network for scripting and testing. Anyone on the same network can use it, so leave this off unless you need it.",
	"Betikler ve testler için ağınızda bir uzaktan kontrol bağlantı noktası (TCP 1339) açar. Aynı ağdaki herkes kullanabilir, bu yüzden gerekmedikçe kapalı tutun."
},

/* WT_I18N_ROW_RXRING - "Wi-Fi receive buffer" */
{
	"Wi-Fi受信バッファ",
	"Wi-Fi receive buffer",
	"Tampon de réception Wi-Fi",
	"Búfer de recepción Wi-Fi",
	"Wi-Fi-Empfangspuffer",
	"Buffer di ricezione Wi-Fi",
	"Wi-Fi-ontvangstbuffer",
	"Buffer de receção Wi-Fi",
	"Буфер приёма Wi-Fi",
	"Wi-Fi 수신 버퍼",
	"Wi-Fi 接收緩衝區",
	"Wi-Fi 接收缓冲区",
	"Wi-Fi-vastaanottopuskuri",
	"Wi-Fi-mottagningsbuffert",
	"Wi-Fi-modtagebuffer",
	"Wi-Fi-mottaksbuffer",
	"Bufor odbioru Wi-Fi",
	"Buffer de recepção Wi-Fi",
	"Wi-Fi receive buffer",
	"Wi-Fi alma arabelleği"
},

/* WT_I18N_ITEM_STOCK - "Default" */
{
	"標準",
	"Default",
	"Par défaut",
	"Predeterminado",
	"Standard",
	"Predefinito",
	"Standaard",
	"Predefinido",
	"По умолчанию",
	"기본",
	"預設",
	"默认",
	"Oletus",
	"Standard",
	"Standard",
	"Standard",
	"Domyślny",
	"Padrão",
	"Default",
	"Varsayılan"
},

/* WT_I18N_ITEM_ENHANCED - "Enhanced" */
{
	"強化",
	"Enhanced",
	"Amélioré",
	"Mejorado",
	"Erweitert",
	"Migliorato",
	"Verbeterd",
	"Melhorado",
	"Улучшенный",
	"향상",
	"增強",
	"增强",
	"Tehostettu",
	"Förbättrad",
	"Forbedret",
	"Forbedret",
	"Rozszerzony",
	"Aprimorado",
	"Enhanced",
	"Gelişmiş"
},

/* WT_I18N_ITEM_MAXIMUM - "Maximum" */
{
	"最大",
	"Maximum",
	"Maximum",
	"Máximo",
	"Maximal",
	"Massimo",
	"Maximaal",
	"Máximo",
	"Максимальный",
	"최대",
	"最大",
	"最大",
	"Maksimi",
	"Maximal",
	"Maksimal",
	"Maksimal",
	"Maksymalny",
	"Máximo",
	"Maximum",
	"Maksimum"
},

/* WT_I18N_HINT_RXRING */
{
	"試験的機能。Wi-Fiチップの受信用メモリを増やし、ダウンロードを高速化します。変更するとWi-Fi（オンの場合はBluetoothも）が約20秒間オフになり、再びオンになります。",
	"Experimental. Gives the Wi-Fi chip more room for incoming data, for faster downloads. Changing it turns Wi-Fi (and Bluetooth, if on) off and on for about 20 seconds.",
	"Expérimental. Donne plus de place à la puce Wi-Fi pour les données entrantes, pour des téléchargements plus rapides. La modification éteint puis rallume le Wi-Fi (et le Bluetooth s'il est activé) pendant environ 20 secondes.",
	"Experimental. Da al chip Wi-Fi más espacio para los datos entrantes, para descargas más rápidas. Al cambiarlo, el Wi-Fi (y el Bluetooth, si está activado) se apaga y se vuelve a encender durante unos 20 segundos.",
	"Experimentell. Gibt dem Wi-Fi-Chip mehr Platz für eingehende Daten und beschleunigt Downloads. Beim Ändern werden Wi-Fi (und Bluetooth, falls aktiv) für etwa 20 Sekunden aus- und wieder eingeschaltet.",
	"Sperimentale. Dà al chip Wi-Fi più spazio per i dati in arrivo, per download più veloci. Modificarlo spegne e riaccende il Wi-Fi (e il Bluetooth, se attivo) per circa 20 secondi.",
	"Experimenteel. Geeft de Wi-Fi-chip meer ruimte voor binnenkomende gegevens, voor snellere downloads. Bij wijzigen gaan Wi-Fi (en Bluetooth, als het aan staat) ongeveer 20 seconden uit en weer aan.",
	"Experimental. Dá ao chip Wi-Fi mais espaço para os dados recebidos, para transferências mais rápidas. Ao alterar, o Wi-Fi (e o Bluetooth, se ligado) desliga-se e volta a ligar durante cerca de 20 segundos.",
	"Экспериментальная функция. Даёт чипу Wi-Fi больше места для входящих данных, ускоряя загрузку. При изменении Wi-Fi (и Bluetooth, если он включён) выключается и снова включается примерно на 20 секунд.",
	"실험적 기능입니다. Wi-Fi 칩에 수신 데이터 공간을 더 주어 다운로드를 빠르게 합니다. 변경하면 Wi-Fi(켜져 있으면 Bluetooth도)가 약 20초 동안 꺼졌다가 다시 켜집니다.",
	"實驗性功能。為 Wi-Fi 晶片提供更多接收資料的空間，加快下載速度。變更時，Wi-Fi（以及開啟中的藍牙）會關閉約 20 秒後重新開啟。",
	"实验性功能。为 Wi-Fi 芯片提供更多接收数据的空间，加快下载速度。更改时，Wi-Fi（以及已开启的蓝牙）会关闭约 20 秒后重新开启。",
	"Kokeellinen. Antaa Wi-Fi-sirulle enemmän tilaa saapuvalle datalle nopeampia latauksia varten. Muuttaminen sammuttaa ja käynnistää Wi-Fin (ja Bluetoothin, jos se on päällä) noin 20 sekunnin ajaksi.",
	"Experimentell. Ger Wi-Fi-kretsen mer utrymme för inkommande data, för snabbare nedladdningar. När du ändrar den stängs Wi-Fi (och Bluetooth, om det är på) av och slås på igen under cirka 20 sekunder.",
	"Eksperimentel. Giver Wi-Fi-chippen mere plads til indgående data for hurtigere downloads. Når du ændrer den, slukkes Wi-Fi (og Bluetooth, hvis det er tændt) og tændes igen i ca. 20 sekunder.",
	"Eksperimentell. Gir Wi-Fi-brikken mer plass til innkommende data, for raskere nedlastinger. Når du endrer den, slås Wi-Fi (og Bluetooth, hvis det er på) av og på igjen i omtrent 20 sekunder.",
	"Eksperymentalne. Daje układowi Wi-Fi więcej miejsca na dane przychodzące, przyspieszając pobieranie. Zmiana wyłącza i ponownie włącza Wi-Fi (oraz Bluetooth, jeśli jest włączony) na około 20 sekund.",
	"Experimental. Dá ao chip Wi-Fi mais espaço para os dados recebidos, para downloads mais rápidos. Ao alterar, o Wi-Fi (e o Bluetooth, se estiver ligado) desliga e liga de novo por cerca de 20 segundos.",
	"Experimental. Gives the Wi-Fi chip more room for incoming data, for faster downloads. Changing it turns Wi-Fi (and Bluetooth, if on) off and on for about 20 seconds.",
	"Deneysel. Wi-Fi yongasına gelen veriler için daha fazla alan vererek indirmeleri hızlandırır. Değiştirildiğinde Wi-Fi (ve açıksa Bluetooth) yaklaşık 20 saniye kapanıp yeniden açılır."
},

/* WT_I18N_ROW_HUDPOS - "HUD position" */
{
	"HUDの位置",
	"HUD position",
	"Position du HUD",
	"Posición del HUD",
	"HUD-Position",
	"Posizione HUD",
	"HUD-positie",
	"Posição do HUD",
	"Положение HUD",
	"HUD 위치",
	"HUD 位置",
	"HUD 位置",
	"HUD:n sijainti",
	"HUD-position",
	"HUD-placering",
	"HUD-plassering",
	"Położenie HUD",
	"Posição do HUD",
	"HUD position",
	"HUD konumu"
},

/* WT_I18N_HINT_HUDPOS - "Which corner of the screen the HUD sits in." */
{
	"HUDを表示する画面の隅。",
	"Which corner of the screen the HUD sits in.",
	"Le coin de l'écran où s'affiche le HUD.",
	"En qué esquina de la pantalla aparece el HUD.",
	"In welcher Bildschirmecke das HUD erscheint.",
	"In quale angolo dello schermo appare l'HUD.",
	"In welke hoek van het scherm de HUD staat.",
	"Em que canto do ecrã aparece o HUD.",
	"В каком углу экрана отображается HUD.",
	"HUD가 표시될 화면 모서리.",
	"HUD 顯示在螢幕的哪個角落。",
	"HUD 显示在屏幕的哪个角落。",
	"Missä näytön kulmassa HUD näkyy.",
	"I vilket hörn av skärmen HUD:en visas.",
	"Hvilket hjørne af skærmen HUD'en vises i.",
	"Hvilket hjørne av skjermen HUD-en vises i.",
	"W którym rogu ekranu pojawia się HUD.",
	"Em qual canto da tela o HUD aparece.",
	"Which corner of the screen the HUD sits in.",
	"HUD'nin ekranın hangi köşesinde görüneceği."
},

/* WT_I18N_ITEM_POS_TL - "Top left" */
{
	"左上",
	"Top left",
	"En haut à gauche",
	"Arriba a la izquierda",
	"Oben links",
	"In alto a sinistra",
	"Linksboven",
	"Canto superior esquerdo",
	"Слева сверху",
	"왼쪽 위",
	"左上",
	"左上",
	"Ylävasen",
	"Uppe till vänster",
	"Øverst til venstre",
	"Øverst til venstre",
	"Lewy górny",
	"Canto superior esquerdo",
	"Top left",
	"Sol üst"
},

/* WT_I18N_ITEM_POS_TR - "Top right" */
{
	"右上",
	"Top right",
	"En haut à droite",
	"Arriba a la derecha",
	"Oben rechts",
	"In alto a destra",
	"Rechtsboven",
	"Canto superior direito",
	"Справа сверху",
	"오른쪽 위",
	"右上",
	"右上",
	"Yläoikea",
	"Uppe till höger",
	"Øverst til højre",
	"Øverst til høyre",
	"Prawy górny",
	"Canto superior direito",
	"Top right",
	"Sağ üst"
},

/* WT_I18N_ITEM_POS_BL - "Bottom left" */
{
	"左下",
	"Bottom left",
	"En bas à gauche",
	"Abajo a la izquierda",
	"Unten links",
	"In basso a sinistra",
	"Linksonder",
	"Canto inferior esquerdo",
	"Слева снизу",
	"왼쪽 아래",
	"左下",
	"左下",
	"Alavasen",
	"Nere till vänster",
	"Nederst til venstre",
	"Nederst til venstre",
	"Lewy dolny",
	"Canto inferior esquerdo",
	"Bottom left",
	"Sol alt"
},

/* WT_I18N_ITEM_POS_BR - "Bottom right" */
{
	"右下",
	"Bottom right",
	"En bas à droite",
	"Abajo a la derecha",
	"Unten rechts",
	"In basso a destra",
	"Rechtsonder",
	"Canto inferior direito",
	"Справа снизу",
	"오른쪽 아래",
	"右下",
	"右下",
	"Alaoikea",
	"Nere till höger",
	"Nederst til højre",
	"Nederst til høyre",
	"Prawy dolny",
	"Canto inferior direito",
	"Bottom right",
	"Sağ alt"
},

/* WT_I18N_ROW_HUDGAME - "In-game HUD" */
{
	"ゲーム中のHUD",
	"In-game HUD",
	"HUD en jeu",
	"HUD durante el juego",
	"HUD im Spiel",
	"HUD in gioco",
	"HUD tijdens spelen",
	"HUD durante o jogo",
	"HUD в играх",
	"게임 중 HUD",
	"遊戲中顯示 HUD",
	"游戏中显示 HUD",
	"HUD pelin aikana",
	"HUD i spel",
	"HUD i spil",
	"HUD i spill",
	"HUD w grach",
	"HUD durante o jogo",
	"In-game HUD",
	"Oyun içi HUD"
},

/* WT_I18N_HINT_HUDGAME - "Also show the HUD while a game or app is running..." */
{
	"ゲームやアプリの実行中もHUDを表示します。オフにすると、ホーム画面、設定、クイックメニュー（PSボタン）でのみ表示されます。",
	"Also show the HUD while a game or app is running. When off, it only shows on the Home screen, in Settings and in the quick menu (press the PS button).",
	"Affiche aussi le HUD pendant un jeu ou une application. Désactivé, il n'apparaît que sur l'écran d'accueil, dans les Paramètres et dans le menu rapide (touche PS).",
	"Muestra también el HUD mientras se ejecuta un juego o una aplicación. Si está desactivado, solo aparece en la pantalla de inicio, en Ajustes y en el menú rápido (botón PS).",
	"Zeigt das HUD auch während eines Spiels oder einer App. Wenn aus, erscheint es nur auf dem Startbildschirm, in den Einstellungen und im Schnellmenü (PS-Taste).",
	"Mostra l'HUD anche durante un gioco o un'applicazione. Se disattivato, appare solo nella schermata Home, nelle Impostazioni e nel menu rapido (tasto PS).",
	"Toont de HUD ook tijdens een game of app. Als dit uit staat, verschijnt de HUD alleen op het startscherm, in Instellingen en in het snelmenu (PS-toets).",
	"Mostra também o HUD durante um jogo ou aplicação. Desativado, só aparece no ecrã inicial, nas Definições e no menu rápido (botão PS).",
	"Показывать HUD и во время игры или приложения. Если выключено, HUD виден только на главном экране, в настройках и в быстром меню (кнопка PS).",
	"게임이나 앱 실행 중에도 HUD를 표시합니다. 끄면 홈 화면, 설정, 퀵 메뉴(PS 버튼)에서만 표시됩니다.",
	"執行遊戲或應用程式時也顯示 HUD。關閉時，只在主畫面、設定及快速選單（PS 按鈕）中顯示。",
	"运行游戏或应用程序时也显示 HUD。关闭时，仅在主屏幕、设置和快捷菜单（PS 按钮）中显示。",
	"Näyttää HUD:n myös pelin tai sovelluksen aikana. Pois päältä se näkyy vain aloitusnäytössä, asetuksissa ja pikavalikossa (PS-näppäin).",
	"Visar HUD:en även när ett spel eller en app körs. Av: den visas bara på hemskärmen, i Inställningar och i snabbmenyn (PS-knappen).",
	"Viser også HUD'en, mens et spil eller en app kører. Fra: den vises kun på startskærmen, i Indstillinger og i hurtigmenuen (PS-knappen).",
	"Viser også HUD-en mens et spill eller en app kjører. Av: den vises bare på startskjermen, i Innstillinger og i hurtigmenyen (PS-knappen).",
	"Pokazuje HUD także podczas gry lub aplikacji. Gdy wyłączone, HUD jest widoczny tylko na ekranie głównym, w Ustawieniach i w szybkim menu (przycisk PS).",
	"Mostra o HUD também durante um jogo ou aplicativo. Desligado, ele só aparece na tela inicial, em Configurações e no menu rápido (botão PS).",
	"Also show the HUD while a game or app is running. When off, it only shows on the Home screen, in Settings and in the quick menu (press the PS button).",
	"HUD'yi bir oyun veya uygulama çalışırken de gösterir. Kapalıyken yalnızca ana ekranda, Ayarlar'da ve hızlı menüde (PS düğmesi) görünür."
},

/* WT_I18N_QM_MAXPERF - short "Max performance" for the quick menu button (fits 260 px) */
{
	"最大性能",
	"Max performance",
	"Performance",
	"Rendimiento",
	"Leistung",
	"Prestazioni",
	"Prestaties",
	"Desempenho",
	"Максимум",
	"최대 성능",
	"最高效能",
	"最高性能",
	"Suorituskyky",
	"Prestanda",
	"Ydeevne",
	"Ytelse",
	"Wydajność",
	"Desempenho",
	"Max performance",
	"Performans"
},

/* WT_I18N_QM_BALANCED - short "Balanced" for the quick menu button */
{
	"バランス",
	"Balanced",
	"Équilibré",
	"Equilibrado",
	"Ausgeglichen",
	"Bilanciato",
	"Gebalanceerd",
	"Equilibrado",
	"Баланс",
	"균형",
	"平衡",
	"平衡",
	"Tasapaino",
	"Balanserad",
	"Balanceret",
	"Balansert",
	"Zrównoważony",
	"Equilibrado",
	"Balanced",
	"Dengeli"
},

/* WT_I18N_QM_BATTERY - short "Battery saver" for the quick menu button */
{
	"省電力",
	"Battery saver",
	"Économie",
	"Ahorro",
	"Sparen",
	"Risparmio",
	"Besparing",
	"Poupança",
	"Экономия",
	"배터리 절약",
	"省電模式",
	"省电模式",
	"Virransäästö",
	"Batterispar",
	"Batterispar",
	"Strømsparing",
	"Oszczędzanie",
	"Economia",
	"Battery saver",
	"Tasarruf"
},

/* WT_I18N_ROW_MACMODE - "MAC address" */
{
	"MACアドレス",
	"MAC address",
	"Adresse MAC",
	"Dirección MAC",
	"MAC-Adresse",
	"Indirizzo MAC",
	"MAC-adres",
	"Endereço MAC",
	"MAC-адрес",
	"MAC 주소",
	"MAC 位址",
	"MAC 地址",
	"MAC-osoite",
	"MAC-adress",
	"MAC-adresse",
	"MAC-adresse",
	"Adres MAC",
	"Endereço MAC",
	"MAC address",
	"MAC adresi"
},
/* WT_I18N_ITEM_CUSTOM - "Custom" */
{
	"カスタム",
	"Custom",
	"Personnalisée",
	"Personalizada",
	"Benutzerdefiniert",
	"Personalizzato",
	"Aangepast",
	"Personalizado",
	"Своё",
	"사용자 지정",
	"自訂",
	"自定义",
	"Mukautettu",
	"Anpassad",
	"Brugerdefineret",
	"Egendefinert",
	"Własny",
	"Personalizado",
	"Custom",
	"Özel"
},
/* WT_I18N_HINT_MACMODE - "Default uses this system's own Wi-Fi MAC address. Custom uses the address below; choosing Custom makes a new random one. Wi-Fi disconnects and reconnects for a few seconds after a change." */
{
	"標準ではこの本体のWi-Fi MACアドレスを使います。カスタムでは下のアドレスを使い、カスタムを選ぶと新しいランダムなアドレスが作られます。変更後、Wi-Fiは数秒間切断して再接続します。",
	"Default uses this system's own Wi-Fi MAC address. Custom uses the address below; choosing Custom makes a new random one. Wi-Fi disconnects and reconnects for a few seconds after a change.",
	"Par défaut utilise l'adresse MAC Wi-Fi propre au système. Personnalisée utilise l'adresse ci-dessous ; choisir Personnalisée en crée une nouvelle au hasard. Après un changement, le Wi-Fi se déconnecte puis se reconnecte en quelques secondes.",
	"Predeterminado usa la dirección MAC Wi-Fi propia del sistema. Personalizada usa la dirección de abajo; al elegir Personalizada se crea una nueva aleatoria. Tras un cambio, el Wi-Fi se desconecta y se vuelve a conectar en unos segundos.",
	"Standard verwendet die eigene WLAN-MAC-Adresse des Systems. Benutzerdefiniert verwendet die Adresse darunter; bei Auswahl wird eine neue zufällige erzeugt. Nach einer Änderung trennt sich das WLAN für einige Sekunden und verbindet sich neu.",
	"Predefinito usa l'indirizzo MAC Wi-Fi del sistema. Personalizzato usa l'indirizzo qui sotto; sceglierlo ne crea uno nuovo casuale. Dopo una modifica il Wi-Fi si disconnette e si riconnette in pochi secondi.",
	"Standaard gebruikt het eigen wifi-MAC-adres van het systeem. Aangepast gebruikt het adres hieronder; als je Aangepast kiest, wordt er een nieuw willekeurig adres gemaakt. Na een wijziging verbreekt wifi enkele seconden de verbinding en maakt daarna opnieuw verbinding.",
	"Predefinido usa o endereço MAC Wi-Fi do próprio sistema. Personalizado usa o endereço abaixo; escolher Personalizado cria um novo aleatório. Após uma alteração, o Wi-Fi desliga-se e volta a ligar-se em alguns segundos.",
	"«По умолчанию» — собственный MAC-адрес Wi-Fi системы. «Своё» — адрес ниже; при выборе создаётся новый случайный. После изменения Wi-Fi на несколько секунд отключится и подключится снова.",
	"기본값은 이 시스템의 Wi-Fi MAC 주소를 사용합니다. 사용자 지정은 아래 주소를 사용하며, 선택하면 새 무작위 주소가 만들어집니다. 변경 후 Wi-Fi가 몇 초간 연결이 끊겼다가 다시 연결됩니다.",
	"預設會使用本主機的 Wi-Fi MAC 位址。自訂會使用下方的位址；選擇自訂時會產生新的隨機位址。變更後 Wi-Fi 會中斷並在幾秒內重新連線。",
	"默认使用本主机的 Wi-Fi MAC 地址。自定义使用下方的地址；选择自定义时会生成新的随机地址。更改后 Wi-Fi 会断开并在几秒内重新连接。",
	"Oletus käyttää järjestelmän omaa Wi-Fi-MAC-osoitetta. Mukautettu käyttää alla olevaa osoitetta; sen valitseminen luo uuden satunnaisen. Muutoksen jälkeen Wi-Fi katkeaa ja yhdistää uudelleen muutamassa sekunnissa.",
	"Standard använder systemets egen Wi-Fi-MAC-adress. Anpassad använder adressen nedan; väljer du Anpassad skapas en ny slumpmässig. Efter en ändring kopplas Wi-Fi från och ansluter igen inom några sekunder.",
	"Standard bruger systemets egen Wi-Fi-MAC-adresse. Brugerdefineret bruger adressen nedenfor; vælger du det, laves en ny tilfældig. Efter en ændring afbrydes Wi-Fi og forbinder igen i løbet af et par sekunder.",
	"Standard bruker systemets egen Wi-Fi-MAC-adresse. Egendefinert bruker adressen nedenfor; velger du det, lages en ny tilfeldig. Etter en endring kobles Wi-Fi fra og kobler til igjen i løpet av noen sekunder.",
	"Domyślny używa własnego adresu MAC Wi-Fi systemu. Własny używa adresu poniżej; jego wybranie tworzy nowy losowy. Po zmianie Wi-Fi rozłączy się i połączy ponownie w ciągu kilku sekund.",
	"Padrão usa o endereço MAC Wi-Fi do próprio sistema. Personalizado usa o endereço abaixo; escolher Personalizado cria um novo aleatório. Após uma alteração, o Wi-Fi desconecta e reconecta em alguns segundos.",
	"Default uses this system's own Wi-Fi MAC address. Custom uses the address below; choosing Custom makes a new random one. Wi-Fi disconnects and reconnects for a few seconds after a change.",
	"Varsayılan, sistemin kendi Wi-Fi MAC adresini kullanır. Özel, aşağıdaki adresi kullanır; Özel'i seçmek yeni rastgele bir adres oluşturur. Değişiklikten sonra Wi-Fi birkaç saniyeliğine bağlantıyı kesip yeniden bağlanır."
},
/* WT_I18N_ROW_MACADDR - "Custom MAC address" */
{
	"カスタムMACアドレス",
	"Custom MAC address",
	"Adresse MAC personnalisée",
	"Dirección MAC personalizada",
	"Benutzerdefinierte MAC-Adresse",
	"Indirizzo MAC personalizzato",
	"Aangepast MAC-adres",
	"Endereço MAC personalizado",
	"Своё значение MAC-адреса",
	"사용자 지정 MAC 주소",
	"自訂 MAC 位址",
	"自定义 MAC 地址",
	"Mukautettu MAC-osoite",
	"Anpassad MAC-adress",
	"Brugerdefineret MAC-adresse",
	"Egendefinert MAC-adresse",
	"Własny adres MAC",
	"Endereço MAC personalizado",
	"Custom MAC address",
	"Özel MAC adresi"
},
/* WT_I18N_HINT_MACADDR - "The MAC address used on Wi-Fi. With MAC address set to Custom, press X to type one: 12 hexadecimal digits (0-9, A-F), with or without colons. Addresses that cannot be used are refused." */
{
	"Wi-Fiで使うMACアドレスです。MACアドレスがカスタムのとき、×ボタンで入力できます：16進数12桁（0～9、A～F）、コロンはあってもなくてもかまいません。使えないアドレスは受け付けません。",
	"The MAC address used on Wi-Fi. With MAC address set to Custom, press X to type one: 12 hexadecimal digits (0-9, A-F), with or without colons. Addresses that cannot be used are refused.",
	"L'adresse MAC utilisée en Wi-Fi. Avec l'adresse MAC sur Personnalisée, appuyez sur X pour en saisir une : 12 chiffres hexadécimaux (0-9, A-F), avec ou sans deux-points. Les adresses inutilisables sont refusées.",
	"La dirección MAC que se usa en Wi-Fi. Con la dirección MAC en Personalizada, pulsa X para escribir una: 12 dígitos hexadecimales (0-9, A-F), con o sin dos puntos. Las direcciones que no se pueden usar se rechazan.",
	"Die im WLAN verwendete MAC-Adresse. Ist die MAC-Adresse auf Benutzerdefiniert gestellt, X drücken und eine eingeben: 12 Hexadezimalziffern (0-9, A-F), mit oder ohne Doppelpunkte. Unbrauchbare Adressen werden abgelehnt.",
	"L'indirizzo MAC usato sul Wi-Fi. Con l'indirizzo MAC su Personalizzato, premi X per inserirne uno: 12 cifre esadecimali (0-9, A-F), con o senza due punti. Gli indirizzi non utilizzabili vengono rifiutati.",
	"Het MAC-adres dat op wifi wordt gebruikt. Staat het MAC-adres op Aangepast, druk dan op X om er een in te voeren: 12 hexadecimale cijfers (0-9, A-F), met of zonder dubbele punten. Onbruikbare adressen worden geweigerd.",
	"O endereço MAC usado no Wi-Fi. Com o endereço MAC em Personalizado, prima X para escrever um: 12 dígitos hexadecimais (0-9, A-F), com ou sem dois pontos. Endereços que não podem ser usados são recusados.",
	"MAC-адрес, используемый в Wi-Fi. Когда для MAC-адреса выбрано «Своё», нажмите X, чтобы ввести адрес: 12 шестнадцатеричных цифр (0-9, A-F), с двоеточиями или без. Непригодные адреса не принимаются.",
	"Wi-Fi에서 사용하는 MAC 주소입니다. MAC 주소가 사용자 지정일 때 X 버튼을 눌러 입력하세요: 16진수 12자리(0-9, A-F), 콜론은 있어도 없어도 됩니다. 사용할 수 없는 주소는 거부됩니다.",
	"Wi-Fi 使用的 MAC 位址。MAC 位址設為自訂時，按 X 鈕輸入：12 位十六進位數字（0-9、A-F），可含或不含冒號。無法使用的位址會被拒絕。",
	"Wi-Fi 使用的 MAC 地址。MAC 地址设为自定义时，按 X 键输入：12 位十六进制数字（0-9、A-F），可带或不带冒号。无法使用的地址会被拒绝。",
	"Wi-Fi-yhteydessä käytettävä MAC-osoite. Kun MAC-osoite on Mukautettu, kirjoita osoite painamalla X: 12 heksadesimaalinumeroa (0-9, A-F), kaksoispisteillä tai ilman. Käyttökelvottomat osoitteet hylätään.",
	"MAC-adressen som används på Wi-Fi. När MAC-adress är Anpassad trycker du på X för att skriva en: 12 hexadecimala siffror (0-9, A-F), med eller utan kolon. Adresser som inte kan användas avvisas.",
	"MAC-adressen, der bruges på Wi-Fi. Når MAC-adresse er Brugerdefineret, tryk på X for at skrive en: 12 hexadecimale cifre (0-9, A-F), med eller uden koloner. Adresser, der ikke kan bruges, afvises.",
	"MAC-adressen som brukes på Wi-Fi. Når MAC-adresse er Egendefinert, trykk X for å skrive en: 12 heksadesimale sifre (0-9, A-F), med eller uten kolon. Adresser som ikke kan brukes, avvises.",
	"Adres MAC używany w Wi-Fi. Gdy adres MAC ustawiono na Własny, naciśnij X, aby wpisać adres: 12 cyfr szesnastkowych (0-9, A-F), z dwukropkami lub bez. Adresy, których nie można użyć, są odrzucane.",
	"O endereço MAC usado no Wi-Fi. Com o endereço MAC em Personalizado, pressione X para digitar um: 12 dígitos hexadecimais (0-9, A-F), com ou sem dois-pontos. Endereços que não podem ser usados são recusados.",
	"The MAC address used on Wi-Fi. With MAC address set to Custom, press X to type one: 12 hexadecimal digits (0-9, A-F), with or without colons. Addresses that cannot be used are refused.",
	"Wi-Fi'da kullanılan MAC adresi. MAC adresi Özel olarak ayarlıyken bir adres yazmak için X'e basın: 12 onaltılık rakam (0-9, A-F), iki nokta üst üste ile ya da olmadan. Kullanılamayan adresler reddedilir."
},
/* WT_I18N_ROW_MACRAND - "Randomize MAC address" */
{
	"MACアドレスをランダムにする",
	"Randomize MAC address",
	"Adresse MAC aléatoire",
	"Dirección MAC aleatoria",
	"MAC-Adresse zufällig wählen",
	"Indirizzo MAC casuale",
	"Willekeurig MAC-adres",
	"Endereço MAC aleatório",
	"Случайный MAC-адрес",
	"MAC 주소 무작위로 만들기",
	"隨機產生 MAC 位址",
	"随机生成 MAC 地址",
	"Satunnainen MAC-osoite",
	"Slumpa MAC-adress",
	"Tilfældig MAC-adresse",
	"Tilfeldig MAC-adresse",
	"Losowy adres MAC",
	"Endereço MAC aleatório",
	"Randomise MAC address",
	"Rastgele MAC adresi"
},
/* WT_I18N_HINT_MACRAND - "Makes a new custom MAC address: the first half (the maker) stays the same as this system's own, the second half is random. Available with MAC address set to Custom." */
{
	"新しいカスタムMACアドレスを作ります：前半（メーカー部分）はこの本体と同じまま、後半がランダムになります。MACアドレスがカスタムのときに使えます。",
	"Makes a new custom MAC address: the first half (the maker) stays the same as this system's own, the second half is random. Available with MAC address set to Custom.",
	"Crée une nouvelle adresse MAC personnalisée : la première moitié (le fabricant) reste celle du système, la seconde est aléatoire. Disponible avec l'adresse MAC sur Personnalisée.",
	"Crea una nueva dirección MAC personalizada: la primera mitad (el fabricante) sigue siendo la del sistema y la segunda es aleatoria. Disponible con la dirección MAC en Personalizada.",
	"Erzeugt eine neue benutzerdefinierte MAC-Adresse: Die erste Hälfte (der Hersteller) bleibt die des Systems, die zweite ist zufällig. Verfügbar, wenn die MAC-Adresse auf Benutzerdefiniert steht.",
	"Crea un nuovo indirizzo MAC personalizzato: la prima metà (il produttore) resta quella del sistema, la seconda è casuale. Disponibile con l'indirizzo MAC su Personalizzato.",
	"Maakt een nieuw aangepast MAC-adres: de eerste helft (de fabrikant) blijft die van het systeem, de tweede helft is willekeurig. Beschikbaar als het MAC-adres op Aangepast staat.",
	"Cria um novo endereço MAC personalizado: a primeira metade (o fabricante) fica igual à do sistema e a segunda é aleatória. Disponível com o endereço MAC em Personalizado.",
	"Создаёт новый MAC-адрес: первая половина (производитель) остаётся как у системы, вторая — случайная. Доступно, когда для MAC-адреса выбрано «Своё».",
	"새 사용자 지정 MAC 주소를 만듭니다: 앞쪽 절반(제조사)은 이 시스템과 같게 유지되고 뒤쪽 절반은 무작위입니다. MAC 주소가 사용자 지정일 때 사용할 수 있습니다.",
	"產生新的自訂 MAC 位址：前半（製造商）與本主機相同，後半為隨機。MAC 位址設為自訂時可使用。",
	"生成新的自定义 MAC 地址：前半（制造商）与本主机相同，后半为随机。MAC 地址设为自定义时可用。",
	"Luo uuden mukautetun MAC-osoitteen: ensimmäinen puolisko (valmistaja) pysyy järjestelmän omana, toinen on satunnainen. Käytettävissä, kun MAC-osoite on Mukautettu.",
	"Skapar en ny anpassad MAC-adress: första halvan (tillverkaren) förblir systemets egen, andra halvan är slumpmässig. Tillgängligt när MAC-adress är Anpassad.",
	"Laver en ny brugerdefineret MAC-adresse: første halvdel (producenten) forbliver systemets egen, anden halvdel er tilfældig. Tilgængelig, når MAC-adresse er Brugerdefineret.",
	"Lager en ny egendefinert MAC-adresse: første halvdel (produsenten) forblir systemets egen, andre halvdel er tilfeldig. Tilgjengelig når MAC-adresse er Egendefinert.",
	"Tworzy nowy własny adres MAC: pierwsza połowa (producent) pozostaje taka jak w systemie, druga jest losowa. Dostępne, gdy adres MAC ustawiono na Własny.",
	"Cria um novo endereço MAC personalizado: a primeira metade (o fabricante) continua igual à do sistema e a segunda é aleatória. Disponível com o endereço MAC em Personalizado.",
	"Makes a new custom MAC address: the first half (the maker) stays the same as this system's own, the second half is random. Available with MAC address set to Custom.",
	"Yeni bir özel MAC adresi oluşturur: ilk yarı (üretici) sistemin kendi adresiyle aynı kalır, ikinci yarı rastgeledir. MAC adresi Özel olarak ayarlıyken kullanılabilir."
},

/* WT_I18N_ROW_MACCUR - "Current MAC address" */
{
	"現在のMACアドレス",
	"Current MAC address",
	"Adresse MAC actuelle",
	"Dirección MAC actual",
	"Aktuelle MAC-Adresse",
	"Indirizzo MAC attuale",
	"Huidig MAC-adres",
	"Endereço MAC atual",
	"Текущий MAC-адрес",
	"현재 MAC 주소",
	"目前的 MAC 位址",
	"当前 MAC 地址",
	"Nykyinen MAC-osoite",
	"Aktuell MAC-adress",
	"Aktuel MAC-adresse",
	"Gjeldende MAC-adresse",
	"Bieżący adres MAC",
	"Endereço MAC atual",
	"Current MAC address",
	"Geçerli MAC adresi"
},

/* WT_I18N_HINT_MACCUR - "The MAC address this system is using on Wi-Fi right now. If it isn't the custom address, that one isn't in use yet. Shown for reference only." */
{
	"この本体が現在Wi-Fiで使っているMACアドレスです。カスタムアドレスと違う場合、カスタムアドレスはまだ使われていません。表示のみです。",
	"The MAC address this system is using on Wi-Fi right now. If it isn't the custom address, that one isn't in use yet. Shown for reference only.",
	"L'adresse MAC que le système utilise en ce moment en Wi-Fi. Si ce n'est pas l'adresse personnalisée, celle-ci n'est pas encore utilisée. Affichée à titre d'information uniquement.",
	"La dirección MAC que el sistema usa ahora mismo en Wi-Fi. Si no es la personalizada, esa todavía no se está usando. Solo se muestra como referencia.",
	"Die MAC-Adresse, die das System gerade im WLAN verwendet. Ist es nicht die benutzerdefinierte, wird diese noch nicht verwendet. Nur zur Information.",
	"L'indirizzo MAC che il sistema sta usando ora sul Wi-Fi. Se non è quello personalizzato, quest'ultimo non è ancora in uso. Solo a titolo informativo.",
	"Het MAC-adres dat het systeem nu op wifi gebruikt. Is dit niet het aangepaste adres, dan is dat nog niet in gebruik. Alleen ter informatie.",
	"O endereço MAC que o sistema está a usar agora no Wi-Fi. Se não for o personalizado, esse ainda não está em uso. Apenas para consulta.",
	"MAC-адрес, который система сейчас использует в Wi-Fi. Если это не заданный адрес, он ещё не применён. Только для справки.",
	"이 시스템이 지금 Wi-Fi에서 사용 중인 MAC 주소입니다. 사용자 지정 주소와 다르면 사용자 지정 주소는 아직 사용되지 않고 있습니다. 참고용으로만 표시됩니다.",
	"本主機目前在 Wi-Fi 上使用的 MAC 位址。若不是自訂位址，表示自訂位址尚未使用。僅供參考。",
	"本主机当前在 Wi-Fi 上使用的 MAC 地址。若不是自定义地址，表示自定义地址尚未使用。仅供参考。",
	"MAC-osoite, jota järjestelmä käyttää Wi-Fi-yhteydessä juuri nyt. Jos se ei ole mukautettu osoite, mukautettu ei ole vielä käytössä. Vain tiedoksi.",
	"MAC-adressen som systemet använder på Wi-Fi just nu. Om det inte är den anpassade adressen används den inte än. Visas endast som information.",
	"MAC-adressen, som systemet bruger på Wi-Fi lige nu. Hvis det ikke er den brugerdefinerede adresse, er den ikke i brug endnu. Vises kun til orientering.",
	"MAC-adressen systemet bruker på Wi-Fi akkurat nå. Hvis det ikke er den egendefinerte adressen, er den ikke i bruk ennå. Vises kun til informasjon.",
	"Adres MAC, którego system używa teraz w Wi-Fi. Jeśli to nie jest adres własny, nie jest on jeszcze używany. Tylko do wglądu.",
	"O endereço MAC que o sistema está usando agora no Wi-Fi. Se não for o personalizado, ele ainda não está em uso. Apenas para referência.",
	"The MAC address this system is using on Wi-Fi right now. If it isn't the custom address, that one isn't in use yet. Shown for reference only.",
	"Sistemin şu anda Wi-Fi'da kullandığı MAC adresi. Özel adres değilse, özel adres henüz kullanılmıyor. Yalnızca bilgi amaçlı gösterilir."
},

/* WT_I18N_ITEM_HUD_OFF - "Off" */
{
	"オフ",
	"Off",
	"Désactivé",
	"Desactivado",
	"Aus",
	"Disattivato",
	"Uit",
	"Desligado",
	"Выкл.",
	"끔",
	"關閉",
	"关闭",
	"Pois",
	"Av",
	"Fra",
	"Av",
	"Wyłączony",
	"Desligado",
	"Off",
	"Kapalı"
},

/* WT_I18N_ITEM_HUD_COMPACT - "Compact" */
{
	"コンパクト",
	"Compact",
	"Compact",
	"Compacto",
	"Kompakt",
	"Compatto",
	"Compact",
	"Compacto",
	"Компактно",
	"간단히",
	"精簡",
	"精简",
	"Tiivis",
	"Kompakt",
	"Kompakt",
	"Kompakt",
	"Kompaktowy",
	"Compacto",
	"Compact",
	"Kompakt"
},

/* WT_I18N_ITEM_HUD_EXTENDED - "Extended" */
{
	"拡張",
	"Extended",
	"Étendu",
	"Extendido",
	"Erweitert",
	"Esteso",
	"Uitgebreid",
	"Alargado",
	"Расширенно",
	"자세히",
	"詳細",
	"详细",
	"Laaja",
	"Utökad",
	"Udvidet",
	"Utvidet",
	"Rozszerzony",
	"Estendido",
	"Extended",
	"Genişletilmiş"
},
/* WT_I18N_MAC_PENDING - "pending" */
{
	"保留中",
	"pending",
	"en attente",
	"pendiente",
	"ausstehend",
	"in attesa",
	"bezig",
	"pendente",
	"ожидание",
	"대기 중",
	"等待中",
	"等待中",
	"odottaa",
	"väntar",
	"afventer",
	"venter",
	"oczekuje",
	"pendente",
	"pending",
	"bekliyor"
},
/* WT_I18N_MAC_NOTAPPLIED - "not applied" */
{
	"未適用",
	"not applied",
	"non appliquée",
	"no aplicada",
	"nicht übernommen",
	"non applicato",
	"niet toegepast",
	"não aplicado",
	"не применён",
	"적용 안 됨",
	"未套用",
	"未应用",
	"ei otettu käyttöön",
	"inte tillämpad",
	"ikke anvendt",
	"ikke tatt i bruk",
	"nie zastosowano",
	"não aplicado",
	"not applied",
	"uygulanmadı"
}
};

/* ---- language detection ------------------------------------------------ */

int wq_i18n_lang(void)
{
	int lang = -1;

	/* Primary (reliable for NPXS10015): the system registry. SceSettings
	 * already links RegMgr; /CONFIG/SYSTEM/language is the canonical store and
	 * has no init gate. */
	if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "language", &lang) >= 0 &&
	    lang >= 0 && lang < WT_LANG_COUNT)
		return lang;

	/* Fallback: SDK system param. Only works once the host app has called
	 * sceAppUtilInit(); otherwise returns SCE_APPUTIL_ERROR_NOT_INITIALIZED. */
	lang = -1;
	if (sceAppUtilSystemParamGetInt(WT_SYSTEM_PARAM_ID_LANG, &lang) >= 0 &&
	    lang >= 0 && lang < WT_LANG_COUNT)
		return lang;

	return WT_LANG_ENGLISH_US;
}

/* ---- lookup ------------------------------------------------------------ */

const char *wq_i18n_for(int id, int lang)
{
	const char *s;

	if (id < 0 || id >= WT_I18N_COUNT)
		return "";
	if (lang < 0 || lang >= WT_LANG_COUNT)
		lang = WT_LANG_ENGLISH_US;

	s = g_tbl[id][lang];
	if (s == 0)
		s = g_tbl[id][WT_LANG_ENGLISH_US];   /* fallback */
	if (s == 0)
		s = "";
	return s;
}

const char *wq_i18n(int id)
{
	return wq_i18n_for(id, wq_i18n_lang());
}

/* ---- UTF-8 -> UTF-16 decoder ------------------------------------------- */

int wq_i18n_to_u16(const char *utf8, unsigned short *out, int out_max)
{
	const unsigned char *s = (const unsigned char *)utf8;
	int n = 0;

	if (out == 0 || out_max <= 0)
		return 0;
	if (s == 0) {
		out[0] = 0;
		return 0;
	}

	while (*s) {
		unsigned int cp;
		unsigned char c = *s;

		if (c < 0x80) {                         /* 1 byte: ASCII */
			cp = c;
			s += 1;
		} else if ((c & 0xE0) == 0xC0) {        /* 2 bytes */
			if ((s[1] & 0xC0) != 0x80) { s += 1; continue; }
			cp = ((unsigned int)(c & 0x1F) << 6) |
			     (unsigned int)(s[1] & 0x3F);
			s += 2;
		} else if ((c & 0xF0) == 0xE0) {        /* 3 bytes (BMP) */
			if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
				s += 1;
				continue;
			}
			cp = ((unsigned int)(c & 0x0F) << 12) |
			     ((unsigned int)(s[1] & 0x3F) << 6) |
			     (unsigned int)(s[2] & 0x3F);
			s += 3;
		} else if ((c & 0xF8) == 0xF0) {        /* 4 bytes: astral - skip */
			if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 ||
			    (s[3] & 0xC0) != 0x80) {
				s += 1;
				continue;
			}
			s += 4;                 /* best-effort: drop (needs surrogates) */
			continue;
		} else {                                /* stray continuation / invalid */
			s += 1;
			continue;
		}

		if (cp == 0)
			break;
		if (n >= out_max - 1)                   /* leave room for NUL */
			break;
		out[n++] = (unsigned short)cp;
	}

	out[n] = 0;
	return n;
}
