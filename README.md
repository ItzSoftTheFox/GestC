# HandMouse

Lokální ovládání myši rukou přes webkameru pro Linux, včetně Hyprlandu / Waylandu.
C++17, Qt 6 / QML, OpenCV DNN a přiložené MediaPipe modely. Bez cloudové služby,
bez Python runtime a bez stahování závislostí při konfiguraci CMake.

## Spuštění

Na Arch Linuxu jsou potřeba `base-devel`, `cmake`, `ninja`, `qt6-base`,
`qt6-declarative`, `qt6-wayland` a `opencv`. Qt alespoň 6.5 a OpenCV alespoň 4.9
s podporou TFLite v DNN; ověřeno s Qt 6.11.2 a OpenCV 5.0.0.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/GestC
```

Aplikaci lze spustit z libovolného pracovního adresáře. QML je součástí binárky.
Modely hledá vedle instalace v `share/handmouse/models`, v instalačním adresáři
a nakonec v původním zdrojovém stromu. Vlastní umístění:

```sh
./build/GestC --model-dir /cesta/k/modelum
```

Volitelná instalace do uživatelského profilu:

```sh
cmake --install build --prefix "$HOME/.local"
```

Pro spouštění příkazem `GestC` musí být `~/.local/bin` v `PATH`.
Instalace obsahuje i položku pro nabídku aplikací.

## První použití

1. V **Nastavení** vyber kameru. Změna kamery zastaví aktuální snímání.
2. V **Živém náhledu** spusť kameru. První spuštění používá **Pouze náhled**.
3. Nech ruku celou v záběru, čelem ke kameře, s rovnoměrným osvětlením.
   Ověř kresbu bodů ruky a text rozpoznaného gesta.
4. Zapni **Ovládat kurzor**. Pokud chybí přístup k `/dev/uinput`, aplikace
   zobrazí chybu; náhled lze dál používat bez oprávnění k ovládání myši.
5. Nastav rychlost a vyhlazování podle sebe. Větší vyhlazování znamená
   klidnější pohyb za cenu pomalejší odezvy.

Obraz se neukládá ani neposílá po síti. Výběr režimu, kamery a ostatní nastavení
se ukládají přes QSettings, obvykle do `~/.config/HandMouse/HandMouse.conf`.
Kamera se při dalším spuštění sama nezapíná.

## Gesta

Implementace vychází z uživatelského zadání v [gesta.txt](gesta.txt).

| Gesto | Akce |
| --- | --- |
| Rozevřená ruka | Relativní pohyb kurzoru |
| Pěst | Scrollování od kotvy zachycené při sevření; nahoru/dolů podle posunu |
| Palec + ukazováček | Krátký dotyk klikne; držený dotyk táhne |
| Palec + malíček | Drží Super a levé tlačítko pro přesun okna |
| Palec + prsteníček | Jedno kliknutí kolečkem; další až po oddělení prstů |
| Jen prostředníček nahoře, ostatní schované | Po 0,5 s přepne pauzu; další přepnutí až po uvolnění gesta |

Dotyk prstů drž přibližně alespoň 70 ms. Ostatní prsty při něm nech volné;
kompletně zavřená pěst má přednost jako scrollování. Přechody mezi gesty
neposílají pohyb kurzoru. Pravý klik není součástí tohoto zadání.

Ztráta ruky, pauza, zastavení, změna nastavení nebo chyba vstupu uvolní tlačítka
i Super. Při nedostatku nových snímků hlídá vstup časovač. Při opětovném
zachycení ruky vznikne nový počátek pohybu, takže kurzor nepřeskočí.

## Nastavení

- **Rychlost kurzoru:** 0,2–3×; relativní pohyb funguje bez pevného rozlišení monitoru.
- **Vyhlazování:** 0–100 %, časově řízený filtr nezávislý na frekvenci snímků.
- **Rychlost scrollování:** rychlost roste se vzdáleností od kotvy; malý posun má mrtvou zónu.
- **Kamera, zrcadlení a kresba bodů:** pro výběr a kontrolu obrazu.
- **Citlivost dotyku prstů:** vzdálenost vztažená k velikosti dlaně, s hysterezí při držení.
- **Jistota rozpoznání:** práh skutečné přítomnosti ruky, nikoli skóre levé/pravé ruky.
- **Krytí skla:** 60–100 %; text a ovladače zůstávají neprůhledné.

## Hyprland a skutečné sklo

Okno používá skutečný alfa kanál a vrstvené průsvitné panely. Rozostření plochy
za oknem provádí kompozitor; aplikace nepředstírá blur barevným obrázkem.
Bez zapnutého blur v Hyprlandu bude pozadí průhledné, ale ostré. V jiných
kompozitorech závisí blur na jejich podpoře. Nejde o nativní macOS Liquid Glass.

Pro **Hyprland 0.55+** použij [docs/hyprland.lua](docs/hyprland.lua), ověřený
parserem Hyprlandu 0.56.2. Nastav v něm cestu k `GestC` a načti jej na konci své
konfigurace, po tématu / HyDE. Například při vývoji v tomto checkoutu:

```lua
-- V docs/hyprland.lua nejprve uprav local handmouse na cestu k build/GestC.
dofile("/home/Fox/Projects/GestC/docs/hyprland.lua")
```

Doplněk nastavuje pravidla pouze pro okno `handmouse`. Celkové nastavení blur
ponechává tématu; ukázka jeho zapnutí je v souboru zakomentovaná.
Pro starší Hyprland **0.53–0.54** je připraven [docs/hyprland.conf](docs/hyprland.conf).
Obě varianty nepoužívej současně. Existující vazbu Super + levé tlačítko
neduplikuj; pokud už přesouvá okna, ponech ji.

| Zkratka | Dosah | Akce |
| --- | --- | --- |
| Esc | Aktivní okno HandMouse | Zastavit kameru a uvolnit vstup |
| Mezerník | Náhled / průvodce v HandMouse | Přepnout pauzu |
| Ctrl+, | HandMouse | Otevřít nastavení |
| Ctrl+Q | HandMouse | Ukončit aplikaci |
| Super+Shift+F9 | Globální, po zapojení doplňku | Přepnout pauzu |
| Super+Shift+F10 | Globální, po zapojení doplňku | Zastavit snímání |

Globální zkratky používají lokální socket přístupný pouze stejnému uživateli:

```sh
./build/GestC --toggle-pause
./build/GestC --stop
```

Zkontroluj případný konflikt F9/F10 se svými vazbami. Druhé běžné spuštění
aktivuje existující instanci. Systémové nastavení rychlosti/akcelerace myši se
uplatní i na virtuální myš `HandMouse Virtual Pointer`.

Reference: [Hyprland vazby](https://wiki.hypr.land/Configuring/Basics/Binds/),
[pravidla oken](https://wiki.hypr.land/Configuring/Basics/Window-Rules/).

## Přístup ke vstupu

Ovládání používá `/dev/uinput`, samostatnou virtuální myš a klávesnici pro Super.
Aplikaci nespouštěj jako root. Pokud zařízení chybí, načti modul `uinput`.
Pro přístup aktivního lokálního uživatele je připravené pravidlo
[packaging/70-handmouse-uinput.rules](packaging/70-handmouse-uinput.rules).
Instalace vyžaduje správcovská oprávnění:

```sh
sudo modprobe uinput
sudo install -m 644 packaging/70-handmouse-uinput.rules /etc/udev/rules.d/70-handmouse-uinput.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=misc --sysname-match=uinput
```

Případně se odhlas a znovu přihlas. Pravidlo používá přístup aktivní session;
neotvírá zařízení všem uživatelům pomocí `chmod 666`. Přístup ke kameře závisí
na oprávnění `/dev/videoN` a na tom, zda ji právě nepoužívá jiný program.

## Vývoj a ověřování

```text
src/app/       životní cyklus aplikace, nastavení, stav pro QML
src/tracking/  pracovní vlákno kamery, detektor dlaně a bodů ruky
src/gestures/  čisté vyhodnocení gest, časování, vyhlazování
src/platform/  linuxový uinput, pořadí stisků a uvolnění
src/ui/        QML obrazovky, sdílené ovladače, vykreslení videa
tests/         gesta, vstup, nastavení, modely a opt-in test kamery
docs/          Hyprland doplňky, architektura a omezení
```

Automatické testy neotevírají kameru ani skutečná vstupní zařízení. Ověřují
časování gest, ztrátu ruky, pauzu, relativní pohyb, pořadí Super/klik,
uvolnění při chybě, persistenci nastavení, oba modely a všechny stránky QML.

Ruční test skutečné kamery, na pět sekund, bez systémového vstupu a ukládání obrazu:

```sh
./build/camera_check models 0
```

Test detekce a sledování na vlastním obrázku ruky:

```sh
./build/model_tests models /cesta/ruka.jpg
```

Tento obrazový test uloží anotovaný kontrolní snímek do `/tmp/handmouse-model-check.jpg`.
Offscreen snímek rozhraní (nezachytí rozostření skutečné plochy kompozitorem):

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/GestC --screenshot /tmp/handmouse.png --page settings
```

Technické podrobnosti a známá omezení jsou v [docs/architecture.md](docs/architecture.md).
