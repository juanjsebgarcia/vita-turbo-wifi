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
	"リンク速度、電波強度、接続状態をリアルタイムで表示する小さなオーバーレイを画面に表示します。",
	"Show a small on-screen overlay with live link speed, signal strength and connection status.",
	"Affiche une petite superposition à l'écran avec le débit, la puissance du signal et l'état de la connexion en temps réel.",
	"Muestra una pequeña superposición en pantalla con la velocidad del enlace, la intensidad de la señal y el estado de la conexión en tiempo real.",
	"Zeigt ein kleines Overlay auf dem Bildschirm mit Live-Verbindungsgeschwindigkeit, Signalstärke und Verbindungsstatus.",
	"Mostra una piccola sovrapposizione a schermo con velocità del collegamento, intensità del segnale e stato della connessione in tempo reale.",
	"Toont een kleine overlay op het scherm met live verbindingssnelheid, signaalsterkte en verbindingsstatus.",
	"Mostra uma pequena sobreposição no ecrã com a velocidade da ligação, a intensidade do sinal e o estado da ligação em tempo real.",
	"Показывает небольшое наложение на экране с текущей скоростью, уровнем сигнала и состоянием подключения.",
	"링크 속도, 신호 강도, 연결 상태를 실시간으로 표시하는 작은 오버레이를 화면에 표시합니다.",
	"在螢幕上顯示一個小型疊加層，即時呈現連線速度、訊號強度與連線狀態。",
	"在屏幕上显示一个小型叠加层，实时呈现连接速度、信号强度与连接状态。",
	"Näyttää näytöllä pienen peittokuvan, jossa näkyvät reaaliaikainen yhteysnopeus, signaalin voimakkuus ja yhteyden tila.",
	"Visar ett litet överlägg på skärmen med länkhastighet, signalstyrka och anslutningsstatus i realtid.",
	"Viser et lille overlay på skærmen med live-forbindelseshastighed, signalstyrke og forbindelsesstatus.",
	"Viser et lite overlegg på skjermen med sanntids tilkoblingshastighet, signalstyrke og tilkoblingsstatus.",
	"Wyświetla na ekranie małą nakładkę z aktualną prędkością łącza, siłą sygnału i stanem połączenia.",
	"Mostra uma pequena sobreposição na tela com velocidade do link, intensidade do sinal e status da conexão em tempo real.",
	"Show a small on-screen overlay with live link speed, signal strength and connection status.",
	"Ekranda anlık bağlantı hızı, sinyal gücü ve bağlantı durumunu gösteren küçük bir katman görüntüler."
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
