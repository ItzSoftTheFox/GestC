# Architektura a rozhodnutí

## Změna 0.4

Původní GestureScanner míchal kameru, DNN, gesta a systémový vstup v hlavním
vlákně. Nahradily jej samostatné části:

- `CameraWorker` vlastní kameru a modely. Čeká na V4L2 snímky s omezenou dobou
  čekání, reaguje na přerušení a předává maximálně jeden nepotvrzený snímek.
- `HandTracker` provádí letterbox 192×192 pro detekci dlaně a natočený výřez
  224×224 pro body ruky. Další snímek používá oblast předchozí ruky; při ztrátě
  jistoty se vrací k detektoru dlaně. Náhled se kreslí až po inferenci.
- `GestureEngine` je nezávislý na Qt, kameře a Linuxu. Vstupem jsou body a čas,
  výstupem požadovaný stav tlačítek a relativní pohyb. Gesta z `gesta.txt`
  jsou vzájemně výlučná. Pěst má přednost před náhodným kontaktem prstů uvnitř.
- `InputDevice` vytváří oddělenou myš a klávesnici, aby libinput rozpoznal Super.
  Při tažení jde Super dolů před tlačítkem; při uvolnění jde tlačítko nahoru
  před Super. Chyba zápisu odstraní zařízení.
- `Controller` aplikuje výsledky, drží stav pro UI a hlídá chybějící snímky.
  Snímky starší 250 ms odmítá; po 350 ms bez výsledku uvolní vstup. Změna
  konfigurace má revizi, takže starý snímek nepoužije nové parametry.
- `AppSettings` validuje rozsahy a ukládá hodnoty přes QSettings.

## Modely

Používají se původní `hand_detector.tflite` a `hand_landmarks_detector.tflite`.
Rozměry i názvy výstupů byly ověřeny proti FlatBuffer metadatům modelů:

| Model | Vstup NHWC v souboru | Výstupy |
| --- | --- | --- |
| Dlaň | 1×192×192×3 | Identity: 2016×18; Identity_1: 2016×1 logits |
| Ruka | 1×224×224×3 | Identity: 63 obrazových souřadnic; Identity_1: přítomnost; Identity_2: levá/pravá; Identity_3: světové souřadnice |

OpenCV zpracovává RGB blob v NCHW. Konkrétní výstupy se vyžadují jménem,
neodhadují se podle velikosti nebo podle maximálního skóre. Při startu se obě
sítě zahřejí a zkontrolují se rozměry, typ i konečnost hodnot.

Na zdejším OpenCV 5.0 nový automatický engine vracel pouze poslední výstup.
Proto se explicitně volí `ENGINE_CLASSIC`; OpenCV 4 používá jeho standardní API.
Původní GPU přepínač byl odstraněn: neprokazoval skutečné zrychlení ani
nezajišťoval kompatibilitu. Nyní je výchozí CPU se dvěma OpenCV vlákny.

Soubor `hand_landmarker.task` je původní nepoužívaný balíček; aplikace jej
nenačítá ani neinstaluje. Nezavádíme závislost na celém MediaPipe repozitáři.

## Gesta a pohyb

Souřadnice x i y jsou dělené šířkou obrazu, takže mají stejnou metriku.
Detekce otevřeného prstu kombinuje úhel kloubu a vzdálenost od zápěstí.
Dotyk se měří v poměru k délce dlaně. Přijetí nového gesta čeká 65 ms;
neznámé gesto, pauza a ztráta ruky uvolňují vstup ihned.

Pohyb je relativní, s uchováním zlomků pixelu a EMA vyhlazováním podle času.
Při přechodu mezi gesty se výchozí bod obnoví. Pěst drží scrollovací kotvu,
rychlost roste s odchylkou a má mrtvou zónu. Pauza vyžaduje 500 ms a západku,
aby dlouho držené gesto nepřepínalo stav opakovaně.

## Rozhraní a distribuce

QML je zabalené do Qt resources. CMake nestahuje nic ze sítě a instaluje pouze
binárku, potřebné modely a desktop soubor. Žádná cesta k původní ploše vývojáře.

Alfa okna umožňuje skutečný backdrop blur v Hyprlandu. Dokumentované pravidlo
neztlumuje neprůhlednost textu a nepřepisuje celou konfiguraci uživatele.
Lokální IPC slouží pro jedinou instanci a globální zkratky. Socket omezuje
přístup na stejného uživatele; příkazy nepřijímají shell kód.

## Omezení a ruční kontrola

- Linux / V4L2 / uinput; Windows a macOS zatím nejsou implementované.
- Snímá se jedna ruka. Víc rukou nebo její zakrytí může změnit cíl sledování.
- Heuristiky gest vyžadují rozumný pohled na dlaň. Reálné světlo, úhel a délka
  prstů vyžadují doladit citlivost; nejde o univerzálně vyhodnocený klasifikátor.
- Cílových 30 fps se pouze požaduje od kamery; skutečná frekvence závisí na
  zařízení a expozici. Výkon DNN a rychlost kamery se ukazují zvlášť.
- Rychlost relativní myši ovlivňuje i profil akcelerace kompozitoru.
- Při normálním ukončení se zařízení uvolní. Watchdog nechrání před kompletním
  zamrznutím celého procesu; k nouzovému ukončení slouží i správce procesů.
- Rozostření skla vyžaduje konfiguraci kompozitoru. Offscreen screenshot
  kontroluje layout, nikoli vzhled pozadí vykresleného Hyprlandem.

Ověření 2026-09-28: Release/RelWithDebInfo build na Qt 6.11.2, OpenCV 5.0.0;
modelová inference na prázdných snímcích i reálném testovacím obrázku
[MediaPipe right_hands.jpg](https://storage.googleapis.com/mediapipe-assets/right_hands.jpg).
Testovací obrázek není součástí repozitáře. Test finálního pracovního vlákna
kamery zpracoval za pět sekund 40 snímků, všechny s detekovanou rukou,
s průměrnou inferencí 21,4 ms a čistým zastavením. Neodesílal systémový vstup. Celý scénář přesunu okna gestem je třeba
prakticky ověřit v uživatelské session po zapojení pravidel.
