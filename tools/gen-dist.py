# -*- coding: utf-8 -*-
"""The shipped files under dist/ (rule 66, rule 16): the eleven translation files for every TR key in src/Page.cpp,
and the INI with the compiled defaults.

    python tools/gen-dist.py

Keys the Skyrim HUD Position Manager already carries with the same meaning are taken from its eleven files
(6. current wip mods/HUDPositionManager/dist/Interface/Translations); the keys new to the remaster port, or whose
English changed, are in NEW below. The script fails loudly when a key used in the code has no text in a language."""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SKYRIM = r"D:\Claude output\6. current wip mods\HUDPositionManager\dist\Interface\Translations"
OUT = os.path.join(REPO, r"dist\OblivionRemastered\Binaries\Win64\OBSE\Plugins\ApocryphaMenuFramework\Translations")
LANGS = ["english", "japanese", "korean", "chinese", "russian", "german", "french", "spanish", "italian", "polish", "czech"]

REUSE = {"HPM_Intro", "HPM_Enabled", "HPM_NoHud", "HPM_MoveX", "HPM_MoveY", "HPM_Size", "HPM_Hide", "HPM_ResetOne", "HPM_ResetAll",
         "HPM_El_Health", "HPM_El_Magicka", "HPM_El_Compass", "HPM_El_Crosshair", "HPM_El_EnemyHealth", "HPM_MoveWith",
         "HPM_MoveWithNone", "HPM_LinkBars"}

# key -> [english, japanese, korean, chinese, russian, german, french, spanish, italian, polish, czech]
NEW = {
    "HPM_EnabledHint": [
        "Off: every element goes back to where the game puts it.",
        "オフ: すべての要素がゲーム本来の位置に戻ります。",
        "끄면 모든 요소가 게임이 정한 위치로 돌아갑니다.",
        "关闭：每个元素都回到游戏默认的位置。",
        "Выкл.: каждый элемент возвращается туда, где его размещает игра.",
        "Aus: jedes Element kehrt dorthin zurück, wo das Spiel es platziert.",
        "Désactivé : chaque élément revient là où le jeu le place.",
        "Desactivado: cada elemento vuelve a donde lo coloca el juego.",
        "Disattivato: ogni elemento torna dove lo colloca il gioco.",
        "Wyłączone: każdy element wraca tam, gdzie umieszcza go gra.",
        "Vypnuto: každý prvek se vrátí tam, kam ho umisťuje hra.",
    ],
    "HPM_NotFound": [
        "Not in your HUD right now: it may appear later, or the HUD you use may not have it. Its settings are kept.",
        "現在HUDにありません。後で表示されるか、使用中のHUDには存在しない可能性があります。設定は保持されます。",
        "지금 HUD에 없습니다. 나중에 나타나거나, 사용 중인 HUD에 없을 수 있습니다. 설정은 유지됩니다.",
        "当前不在你的HUD中：它可能稍后出现，或你使用的HUD没有它。其设置会保留。",
        "Сейчас его нет в вашем HUD: он может появиться позже, или в вашем HUD его нет. Настройки сохраняются.",
        "Derzeit nicht in deinem HUD: es kann später erscheinen, oder dein HUD hat es nicht. Die Einstellungen bleiben erhalten.",
        "Absent de votre ATH pour l'instant : il peut apparaître plus tard, ou votre ATH ne l'a pas. Ses réglages sont conservés.",
        "Ahora no está en tu HUD: puede aparecer más tarde, o el HUD que usas no lo tiene. Sus ajustes se conservan.",
        "Ora non è nel tuo HUD: potrebbe comparire più tardi, o l'HUD che usi non lo ha. Le sue impostazioni sono conservate.",
        "Teraz nie ma go w twoim HUD: może pojawić się później albo używany HUD go nie ma. Ustawienia są zachowane.",
        "Teď není ve vašem HUD: může se objevit později, nebo ho váš HUD nemá. Nastavení zůstává zachováno.",
    ],
    "HPM_Found": [
        "In your HUD. Changes show at once.",
        "HUDにあります。変更はすぐに反映されます。",
        "HUD에 있습니다. 변경 사항이 바로 표시됩니다.",
        "在你的HUD中。更改会立即显示。",
        "Есть в вашем HUD. Изменения видны сразу.",
        "In deinem HUD. Änderungen sind sofort sichtbar.",
        "Dans votre ATH. Les changements sont visibles aussitôt.",
        "En tu HUD. Los cambios se ven al instante.",
        "Nel tuo HUD. Le modifiche si vedono subito.",
        "W twoim HUD. Zmiany widać od razu.",
        "Ve vašem HUD. Změny se projeví hned.",
    ],
    "HPM_El_Fatigue": ["Fatigue", "疲労", "피로", "疲劳", "Усталость", "Ausdauer", "Fatigue", "Fatiga", "Fatica", "Zmęczenie", "Únava"],
    "HPM_El_WeaponIcon": ["Weapon icon", "武器アイコン", "무기 아이콘", "武器图标", "Значок оружия", "Waffensymbol", "Icône d'arme", "Icono de arma", "Icona dell'arma", "Ikona broni", "Ikona zbraně"],
    "HPM_El_MagicIcon": ["Spell icon", "呪文アイコン", "주문 아이콘", "法术图标", "Значок заклинания", "Zaubersymbol", "Icône de sort", "Icono de hechizo", "Icona dell'incantesimo", "Ikona zaklęcia", "Ikona kouzla"],
    "HPM_El_EffectIcons": ["Active effects", "有効な効果", "활성 효과", "生效效果", "Активные эффекты", "Aktive Effekte", "Effets actifs", "Efectos activos", "Effetti attivi", "Aktywne efekty", "Aktivní efekty"],
    "HPM_El_SneakEye": ["Sneak eye", "隠密の目", "은신 눈", "潜行之眼", "Глаз скрытности", "Schleichauge", "Œil de furtivité", "Ojo de sigilo", "Occhio furtivo", "Oko skradania", "Oko plížení"],
    "HPM_El_LevelUp": ["Level-up gauge", "レベルアップゲージ", "레벨 업 게이지", "升级计量条", "Индикатор уровня", "Aufstiegsanzeige", "Jauge de niveau", "Indicador de nivel", "Indicatore di livello", "Wskaźnik awansu", "Ukazatel postupu"],
    "HPM_El_Info": ["Target name and value", "対象の名前と価値", "대상 이름과 가치", "目标名称与价值", "Имя и цена цели", "Name und Wert des Ziels", "Nom et valeur de la cible", "Nombre y valor del objetivo", "Nome e valore del bersaglio", "Nazwa i wartość celu", "Název a hodnota cíle"],
    "HPM_El_Subtitles": ["Subtitles and notifications", "字幕と通知", "자막과 알림", "字幕与通知", "Субтитры и уведомления", "Untertitel und Meldungen", "Sous-titres et notifications", "Subtítulos y notificaciones", "Sottotitoli e notifiche", "Napisy i powiadomienia", "Titulky a oznámení"],
    "HPM_El_Breath": ["Breath meter", "息メーター", "숨 게이지", "屏息计量条", "Индикатор дыхания", "Atemanzeige", "Jauge de souffle", "Indicador de aire", "Indicatore del respiro", "Wskaźnik oddechu", "Ukazatel dechu"],
    "HPM_El_Location": ["Location name", "場所の名前", "지역 이름", "地点名称", "Название места", "Ortsname", "Nom du lieu", "Nombre del lugar", "Nome del luogo", "Nazwa miejsca", "Název místa"],
    "HPM_El_DamageIndicators": ["Damage direction", "ダメージの方向", "피해 방향", "伤害方向", "Направление урона", "Schadensrichtung", "Direction des dégâts", "Dirección del daño", "Direzione del danno", "Kierunek obrażeń", "Směr poškození"],
    "HPM_El_Notifications": ["Pop-up notifications", "ポップアップ通知", "팝업 알림", "弹出通知", "Всплывающие уведомления", "Einblendmeldungen", "Notifications contextuelles", "Notificaciones emergentes", "Notifiche a comparsa", "Powiadomienia wyskakujące", "Vyskakovací oznámení"],
    "HPM_El_Tutorial": ["Tutorial messages", "チュートリアルメッセージ", "튜토리얼 메시지", "教程提示", "Сообщения обучения", "Tutorial-Meldungen", "Messages du tutoriel", "Mensajes del tutorial", "Messaggi del tutorial", "Komunikaty samouczka", "Zprávy výuky"],
    "HPM_AlwaysOne": ["Always visible", "常に表示", "항상 표시", "始终显示", "Всегда видно", "Immer sichtbar", "Toujours visible", "Siempre visible", "Sempre visibile", "Zawsze widoczne", "Vždy viditelné"],
    "HPM_AlwaysOneHint": [
        "The game fades this out on its own. On: it stays shown while you play.",
        "ゲームはこれを自動で薄くします。オン: プレイ中は表示されたままになります。",
        "게임이 이것을 스스로 흐리게 합니다. 켜면 플레이 중에 계속 표시됩니다.",
        "游戏会自行将其淡出。开启：游戏中保持显示。",
        "Игра сама плавно скрывает это. Вкл.: остаётся видимым во время игры.",
        "Das Spiel blendet es von selbst aus. An: es bleibt beim Spielen sichtbar.",
        "Le jeu l'estompe de lui-même. Activé : il reste affiché pendant le jeu.",
        "El juego lo atenúa por sí solo. Activado: permanece visible mientras juegas.",
        "Il gioco lo dissolve da solo. Attivato: resta visibile mentre giochi.",
        "Gra sama go wygasza. Włączone: pozostaje widoczny podczas gry.",
        "Hra ho sama nechá zmizet. Zapnuto: zůstane zobrazený během hry.",
    ],
    "HPM_LinkedHint": [
        "Moves with Health while \"Move the three bars together\" is on.",
        "「3つのバーを一緒に動かす」がオンの間は体力と一緒に動きます。",
        "\"세 막대를 함께 이동\"이 켜져 있는 동안 체력과 함께 움직입니다.",
        "开启“三条栏一起移动”时随生命值一起移动。",
        "Движется вместе со здоровьем, пока включено «Двигать три полосы вместе».",
        "Bewegt sich mit der Gesundheit, solange „Die drei Leisten zusammen bewegen“ an ist.",
        "Se déplace avec la santé tant que « Déplacer les trois barres ensemble » est activé.",
        "Se mueve con la salud mientras \"Mover las tres barras juntas\" está activado.",
        "Si muove con la salute finché \"Sposta le tre barre insieme\" è attivo.",
        "Porusza się razem ze zdrowiem, gdy włączone jest „Przesuwaj trzy paski razem”.",
        "Pohybuje se se zdravím, dokud je zapnuto „Posouvat tři lišty společně“.",
    ],
    "HPM_LinkBarsHint": [
        "Magicka and Fatigue move with Health.",
        "マジカと疲労は体力と一緒に動きます。",
        "매지카와 피로가 체력과 함께 움직입니다.",
        "魔力与疲劳随生命值一起移动。",
        "Магия и усталость движутся вместе со здоровьем.",
        "Magicka und Ausdauer bewegen sich mit der Gesundheit.",
        "La magie et la fatigue se déplacent avec la santé.",
        "La magia y la fatiga se mueven con la salud.",
        "Magicka e fatica si muovono con la salute.",
        "Magia i zmęczenie poruszają się razem ze zdrowiem.",
        "Magie a únava se pohybují se zdravím.",
    ],
    "HPM_GroupVisibility": ["HUD visibility", "HUDの表示", "HUD 표시", "HUD显示", "Видимость HUD", "HUD-Sichtbarkeit", "Visibilité de l'ATH", "Visibilidad del HUD", "Visibilità dell'HUD", "Widoczność HUD", "Viditelnost HUD"],
    "HPM_AlwaysAll": ["Always visible", "常に表示", "항상 표시", "始终显示", "Всегда видно", "Immer sichtbar", "Toujours visible", "Siempre visible", "Sempre visibile", "Zawsze widoczne", "Vždy viditelné"],
    "HPM_AlwaysAllOnHint": [
        "The bars stay shown while you play. Menus, dialogue and loading screens still hide the HUD.",
        "プレイ中はバーが表示されたままになります。メニュー、会話、ロード画面ではHUDは引き続き隠れます。",
        "플레이 중에는 막대가 계속 표시됩니다. 메뉴, 대화, 로딩 화면에서는 HUD가 여전히 숨겨집니다.",
        "游戏中各栏保持显示。菜单、对话和加载画面仍会隐藏HUD。",
        "Полосы остаются видимыми во время игры. В меню, диалогах и на экранах загрузки HUD по-прежнему скрыт.",
        "Die Leisten bleiben beim Spielen sichtbar. Menüs, Gespräche und Ladebildschirme blenden das HUD weiterhin aus.",
        "Les barres restent affichées pendant le jeu. Les menus, les dialogues et les écrans de chargement masquent toujours l'ATH.",
        "Las barras permanecen visibles mientras juegas. Los menús, los diálogos y las pantallas de carga siguen ocultando el HUD.",
        "Le barre restano visibili mentre giochi. Menu, dialoghi e schermate di caricamento nascondono comunque l'HUD.",
        "Paski pozostają widoczne podczas gry. Menu, dialogi i ekrany ładowania nadal ukrywają HUD.",
        "Lišty zůstávají zobrazené během hry. Nabídky, dialogy a načítací obrazovky HUD nadále skrývají.",
    ],
    "HPM_AlwaysAllOffHint": [
        "Off: the game decides. The bars fade out when they are full.",
        "オフ: ゲームが決めます。バーは満タンになると薄くなります。",
        "끄면 게임이 결정합니다. 막대는 가득 차면 흐려집니다.",
        "关闭：由游戏决定。各栏满时会淡出。",
        "Выкл.: решает игра. Полосы плавно исчезают, когда заполнены.",
        "Aus: das Spiel entscheidet. Die Leisten blenden aus, wenn sie voll sind.",
        "Désactivé : le jeu décide. Les barres s'estompent quand elles sont pleines.",
        "Desactivado: decide el juego. Las barras se atenúan cuando están llenas.",
        "Disattivato: decide il gioco. Le barre si dissolvono quando sono piene.",
        "Wyłączone: decyduje gra. Paski gasną, gdy są pełne.",
        "Vypnuto: rozhoduje hra. Lišty zmizí, když jsou plné.",
    ],
}


def read_translation(path):
    raw = open(path, "rb").read()
    text = raw[2:].decode("utf-16-le") if raw[:2] == b"\xff\xfe" else raw.decode("utf-8-sig")
    out = {}
    for line in text.splitlines():
        if line.startswith("$") and "\t" in line:
            k, v = line[1:].split("\t", 1)
            out[k] = v
    return out


code = open(os.path.join(REPO, "src", "Page.cpp"), encoding="utf-8").read()
used = sorted(set(re.findall(r'\bTR\s*\(\s*"([A-Za-z0-9_]+)"', code)))
english_in_code = dict(re.findall(r'\bTR\s*\(\s*"([A-Za-z0-9_]+)"\s*,\s*"((?:[^"\\]|\\.)*)"', code))
os.makedirs(OUT, exist_ok=True)
for li, lang in enumerate(LANGS):
    sky = read_translation(os.path.join(SKYRIM, "HUDPositionManager_%s.txt" % lang))
    lines = []
    for key in used:
        if key in NEW:
            text = NEW[key][li]
        elif key in REUSE and key in sky:
            text = sky[key] if lang != "english" else english_in_code.get(key, sky[key]).replace('\\"', '"')
        else:
            raise SystemExit("no %s text for %s" % (lang, key))
        lines.append("$%s\t%s" % (key, text))
    with open(os.path.join(OUT, "HUDPositionManager_%s.txt" % lang), "wb") as f:
        f.write(b"\xff\xfe" + ("\r\n".join(lines) + "\r\n").encode("utf-16-le"))
print("%d keys x %d languages -> %s" % (len(used), len(LANGS), OUT))

# the INI with the compiled defaults (Settings.h / Elements.h)
ELEMENTS = [
    ("Health", True), ("Magicka", True), ("Fatigue", True), ("Compass", False), ("Crosshair", False), ("WeaponIcon", False),
    ("MagicIcon", False), ("EffectIcons", False), ("EnemyHealth", False), ("SneakEye", False), ("LevelUp", False), ("Info", False),
    ("Subtitles", False), ("Breath", False), ("Location", False), ("DamageIndicators", False), ("Notifications", False), ("Tutorial", False),
]
ini = [
    "; HUD Position Manager for Oblivion - settings. The settings page in Apocrypha Menu Framework writes this file;",
    "; edit it by hand only with the game closed. Offsets are in pixels at the game's UI scale; scale is 1.00 = the game's size.",
    "",
    "[General]",
    "; Apply my layout: 0 puts every element back where the game places it.",
    "bEnabled=1",
    "; Move the three bars together: Magicka and Fatigue move with Health.",
    "bLinkBars=1",
    "; HUD visibility: 0 = the game decides (the bars fade out when full), 1 = the bars stay shown while you play.",
    "bAlwaysVisible=0",
    "",
]
for key, fades in ELEMENTS:
    ini += ["[%s]" % key, "fX=0", "fY=0", "fScale=1.00", "bHide=0"]
    if fades:
        ini.append("bAlwaysVisible=0")
    ini += ["sMoveWith=", ""]
ini += ["[Log]", "; 0 = trace ... 6 = off. Shipped at 2 (info).", "uLogLevel=2", ""]
ini_path = os.path.join(REPO, r"dist\OblivionRemastered\Binaries\Win64\OBSE\Plugins\HUDPositionManager.ini")
with open(ini_path, "w", encoding="utf-8", newline="\r\n") as f:
    f.write("\n".join(ini))
print("INI ->", ini_path)
