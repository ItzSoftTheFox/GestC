# FPS kamery a GPU — výsledek práce

Aktualizováno 2026-09-29. Zadání z 2026-09-28 je implementováno a ověřeno
v rozsahu dostupné kamery. Podrobné měření je v [camera-performance.md](camera-performance.md).

## Doplnění: rozlišení a ochrana pohybu

- Přidaná uložená volba rozlišení včetně 1280×720, nabídka podle kamery a
  řízený restart. Výchozí automatika zůstává poblíž 640×480.
- 720p MJPEG ověřeno: kamera přibližně 30 fps, výsledky přibližně 29 fps.
- Při ztrátě ruky, skoku bodů či nejistém přechodu z pěsti se vstup zastaví.
  Pokračování vyžaduje otevřenou klidnou dlaň po dobu 0,2 s. Automatické
  znovuspuštění scrollu po výpadku je zakázané; scroll je vyhlazený a omezený.
- Rozšířené testy simulují výpadky pěsti, falešný pinch, skoky a chvění při
  15/30/60 fps. Přesnost rozpoznávání konkrétní uživatelovy pěsti je potřeba
  vyzkoušet v živém náhledu; vyšší rozlišení samo ji nezaručuje.

## Hotovo

- Nalezena hlavní příčina nízkých FPS: jediný V4L2 buffer. Samotné snímání
  dosahovalo přibližně 14 fps; dva buffery umožňují souběh čtení a snímání.
- Kamera a inference běží odděleně. Mezi nimi je jediný přepisovaný snímek,
  do UI se stále předává nejvýš jeden nepotvrzený výsledek.
- Uložená volba 30/60 fps v `Settings`, `AppSettings` a QML. Dostupnost a
  potřebné rozlišení se zjišťují z V4L2. Nepodporovaná volba je označena;
  uložený požadavek 60 na 30fps kameře přejde na skutečných 30 s vysvětlením.
- Změna kamery/FPS řízeně restartuje snímání; nouzové zastavení ruší restart.
- Ověřují se návratové hodnoty nastavování a čte skutečně vyjednaný režim.
  Volí se MJPEG nebo YUYV podle podporovaných režimů.
- Po dobu snímání se podle možností kamery zakáže snižování FPS automatickou
  expozicí, původní hodnota se při ukončení obnoví. Expozice zůstává automatická.
- UI rozlišuje FPS kamery, FPS doručeného rozpoznávání/náhledu, inferenci,
  režim kamery a odezvu od převzetí snímku; zobrazuje používaný CPU backend.
- Rozšířený `camera_check` měří čekání, dekódování, inferenci, přípravu náhledu,
  předání výsledku a zahazování snímků. `backend_check` porovnává oba modely
  proti CPU bez přidání neověřeného GPU přepínače do aplikace.

## Ověřené výsledky

- Integrovaná SunplusIT kamera nabízí nejvýš 30 fps, včetně nižších rozlišení.
- Finální pracovní vlákno: přibližně **30 fps**, MJPEG 640×480. Při požadavku
  60 fps správně vyjednává a hlásí 30 fps.
- Při zpoždění potvrzení výsledků o 150 ms: kamera stále přibližně 30 fps,
  výsledky přibližně 5 fps, maximální naměřené stáří výsledku pod 50 ms.
  Zahazování snímků nezpůsobilo frontu starých výsledků.
- CPU zůstává vhodnější: OpenCL na RTX 3050 byl několikanásobně pomalejší
  a hlásil chybu kompilace kernelu; CUDA backend v nainstalovaném OpenCV chybí.
  Vulkan nedokončil první inferenci a diagnostika byla ukončena.
- Všech 10 automatických testů prošlo (IPC mimo sandbox). Testy pokrývají nastavení, výběr režimů, restart/zrušení restartu,
  odpojení, staré výsledky, gesta při 15/30/60 fps, systémový vstup, modely,
  IPC a stránky UI. Detekce a stabilita sledování prošly i na testovacím obrázku ruky.

## Co vyžaduje další hardware nebo ruční ověření

- Skutečné 60fps snímání lze fyzicky ověřit až na kameře podporující 60 fps.
  Výběr nižšího rozlišení pro 60 fps je pokryt automatickým testem.
- Rychlost rozpoznávání závisí na CPU a scéně; volba 60fps snímání neslibuje
  60 rozpoznaných výsledků/s. Náhled je nadále navázaný na rozpoznávání.
- Přesun skutečného okna a dlouhodobé ovládání v Hyprlandu vyžadují ruční
  kontrolu. Automatické a kamerové testy neodesílají systémový vstup.
- Případné budoucí CUDA/OpenVINO řešení vyžaduje jiný runtime/build a nové
  měření obou modelů, stability ruky a celé pipeline. Aktuální GPU cesty
  nejsou důvodem ke změně výchozího CPU.

Spuštění nové sestavené aplikace: `./build/GestC`. Uživatelská instalace
v `~/.local` se tímto během práce nepřepisovala.
