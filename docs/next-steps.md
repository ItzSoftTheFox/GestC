# Předání práce: FPS kamery a GPU

Zapsáno 2026-09-28. Zatím pouze poznámky, bez implementace těchto změn.

## Nejnovější zadání uživatele

Po přestavbě aplikace podle uživatele nejspíš fungovala dobře, ale kamera
momentálně dosahuje maximálně přibližně **10 fps**. Další požadavky:

- Přidat do nastavení **volbu 30 / 60 fps**.
- Vyřešit skutečnou rychlost snímání, nikoli pouze přidat přepínač.
- Prověřit **výpočet rozpoznávání přes GPU**, pokud přinese zlepšení.
- V této fázi požadavky jen zapsat; implementaci zahájit až na další pokyn.

## Výchozí stav

- Proběhl overhaul UI, snímání, gest, systémového vstupu a struktury projektu.
- Gesta vycházejí z `gesta.txt`; zachovat jejich význam.
- Prostředí: Arch Linux, Hyprland 0.56.2 / Wayland, Qt 6.11.2, OpenCV 5.0.0.
- Poslední ověření: 8/8 automatických testů prošlo. Test finálního pracovního
  vlákna kamery zpracoval 40 snímků za přibližně 5 sekund, všechny s detekcí ruky.
  Samotná inference průměrně trvala 21,4 ms. To nepotvrzuje 30 fps celé aplikace.
- `src/tracking/camera_worker.cpp` nyní žádá 640×480 a 30 fps přes V4L2.
  Výsledek `camera.set(...)` ani skutečně vyjednaný režim se nekontroluje.
  Formát přenosu není explicitně vybraný. Hodnota FPS není uživatelské nastavení.
- Kamera potřebuje úvodní `grab()` pro zahájení streamování; až potom funguje
  `waitAny()` a `retrieve()`. Tento již opravený postup zachovat.
- Čtení, inference a příprava náhledu běží v jednom pracovním vlákně.
  Mechanismus `framePending_` omezuje frontu výsledků, aby kurzor nereagoval
  na nahromaděné staré snímky.
- FPS v UI se odvozuje od výsledků doručených do `Controller::receive`,
  nikoli od všech snímků skutečně dodaných kamerou.
- `src/tracking/hand_tracker.cpp` používá CPU se dvěma OpenCV vlákny.
  Na OpenCV 5 explicitně volí `ENGINE_CLASSIC`: automaticky zvolený nový
  engine při předchozím ověření nevracel všechny potřebné výstupy modelů.
- Starý GPU přepínač byl odstraněn, protože nebyla ověřená kompatibilita
  ani reálné zrychlení. Nevracet ho pouze jako kosmetickou volbu.

## Doporučený postup při pokračování

### 1. Najít příčinu limitu kolem 10 fps

- Zjistit konkrétní kameru a její podporované kombinace rozlišení, formátu
  a snímkové frekvence, například přes `v4l2-ctl --list-formats-ext`.
- Změřit samotné snímání bez inference a náhledu. Zvlášť měřit čekání na kameru,
  dekódování, detekci, předání výsledku a vykreslení UI.
- Zkontrolovat skutečně vyjednaný režim a výsledky nastavování parametrů.
- Prověřit formát MJPEG oproti nekomprimovanému přenosu, expozici a osvětlení,
  případně omezení připojení. Jsou to možné příčiny, nikoli potvrzená diagnóza.
- Prověřit vliv `framePending_` a společné smyčky snímání/inference.
  Optimalizace nesmí zvětšit frontu ani latenci ovládání.

### 2. Volba 30 / 60 fps

- Přidat uloženou volbu do `AppSettings`, datového `Settings` a QML nastavení.
- Nabízet režimy s ohledem na možnosti vybrané kamery. Pokud 60 fps vyžaduje
  jiné rozlišení nebo není podporováno, srozumitelně to zobrazit.
- Změnu režimu aplikovat řízeným restartem snímání a ověřit její přijetí.
- Rozlišit požadované FPS, skutečné FPS kamery a frekvenci rozpoznávání/náhledu.
  Nezobrazovat požadovaných 60 fps jako naměřený výkon.
- Zachovat časování gest nezávislé na FPS, plynulost kurzoru, odmítání starých
  snímků a uvolnění vstupu při zastavení nebo ztrátě ruky.

### 3. GPU inference

- Nejprve zjistit dostupnou grafiku, ovladače a podporované výpočetní backendy.
- Ověřit kompatibilitu obou přiložených TFLite modelů a všech potřebných
  výstupů; případnou změnu runtime nebo formátu modelu podložit testem.
- Porovnat CPU/GPU po zahřátí na stejných datech: inference, celková latence,
  propustnost a stabilita sledování. Započítat i přenosy dat.
- Pro 60 rozpoznaných snímků/s je rozpočet celé sériové pipeline přibližně
  16,7 ms na snímek; dříve naměřených 21,4 ms CPU inference samo nestačí.
  Plynulý 60fps náhled a 60fps rozpoznávání jsou dva odlišné cíle.
- Nabídnout pouze dostupné a ověřené backendy, skutečně použitý backend
  zobrazit a při selhání umožnit návrat na CPU s vysvětlením.

## Soubory pro navázání

- `src/tracking/camera_worker.*` — vyjednání režimu kamery, smyčka a předávání snímků.
- `src/tracking/hand_tracker.*` — modely, inference a sledování ruky.
- `src/gestures/gesture_engine.*` — nastavení a časově řízené vyhodnocení gest.
- `src/app/settings.*` — validace a ukládání voleb.
- `src/app/controller.*` — revize nastavení, stav, metriky a watchdog.
- `src/ui/main.qml` — nastavení kamery a zobrazení výkonu.
- `tests/camera_check.cpp` — opt-in test skutečného snímacího vlákna bez
  systémového vstupu a bez ukládání obrazu.
- `tests/model_tests.cpp` — kontrola modelů, volitelně reálný obrázek ruky.
- `docs/architecture.md` a `README.md` — současná architektura a spuštění.

## Ověření budoucí změny

- Automatické testy a měření kamery v každém podporovaném režimu 30/60 fps.
- Korektní reakce na nepodporované FPS a odpojení kamery.
- Zachování klikání, tažení, Super + tažení, scrollovací kotvy a pauzy při
  různých frekvencích snímků; žádné zaseknuté tlačítko nebo modifikátor.
- GPU označit za funkční až po ověření skutečného backendu a výkonu.

Lokální socket a skutečná zařízení mohou být v sandboxu nedostupné. Předchozí
testy IPC a kamery proto vyžadovaly běh mimo sandbox; nezaměňovat tento limit
prostředí za chybu aplikace.
