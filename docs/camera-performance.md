# Snímání a inference — měření 2026-09-29

Prostředí: Arch Linux, Qt 6.11.2, OpenCV 5.0.0, dva OpenCV CPU thready,
RelWithDebInfo. Integrovaná USB SunplusIT kamera, Intel UHD a NVIDIA RTX 3050
6GB Laptop GPU, ovladač NVIDIA 615.71.09.

## Příčina nízkých FPS

V4L2 nabízí MJPEG při 30 fps v 1280×720, 640×480, 640×360 a 320×240.
YUYV nabízí v 1280×720 jen 10 fps, v nižších rozlišeních 30 fps.
**Žádný 60fps režim není dostupný.** Původní worker však skutečně vyjednával
640×480 YUYV / 30 fps, takže 10fps režim 720p nebyl jeho příčinou.

Samotné čtení bez inference s jedním driver bufferem dosahovalo zhruba
14,2–14,8 fps v obou formátech. Vypnutí `exposure_dynamic_framerate`
samo nepomohlo. Dva buffery zvýšily stejný test na 28,1–28,4 fps včetně
počátečního rozběhu; tři buffery přinesly podobných 28,6 fps.
Jediný buffer bránil souběhu přenosu/čtení a další akvizice.

Implementace používá dva driver buffery. Snímání běží průběžně, nezávisle
na DNN, a přepisuje jedinou čekající položku. Pomalý odběratel výsledků
nezpomaluje kameru a nevytváří FIFO. Úvodní `grab()` před `waitAny()` zůstal.

## Celé pracovní vlákno

Pětisekundové běhy `camera_check`, bez ukládání obrazu a systémového vstupu:

| Požadavek | Vyjednaný režim | Kamera (poslední okno) | Doručené výsledky | Průměr inference | Max. stáří výsledku |
| --- | --- | --- | --- | --- | --- |
| 30 fps | 640×480 MJPEG / 30 | 30,39 fps | 29,98 fps | 19,73 ms | 35,71 ms |
| 60 fps | 640×480 MJPEG / 30 | 30,40 fps | 30,25 fps | 18,35 ms | 32,50 ms |
| 30 fps, potvrzení opožděné o 150 ms | 640×480 MJPEG / 30 | 30,39 fps | 5,17 fps | 21,61 ms | 49,65 ms |

První dva běhy nezachytily ruku, třetí zachytil ruku ve 12 z 28 výsledků.
Samostatný test reálného obrázku ověřil detekci, opakované sledování, ztrátu,
opětovné nalezení a zrcadlenou/otočenou ruku. Tyto údaje nejsou zárukou
30 fps s každým gestem, osvětlením či zatížením stroje.

První běh: čekání na snímek 31,43 ms, dekódování 1,27 ms, příprava náhledu
0,16 ms, ostatní práce/předání 0,20 ms. Čekání kamery a inference běží
souběžně, proto se jejich časy nesčítají jako sériová pipeline.
Pomalý odběr vynechal 131 snímků, aniž by vytvářel zpožděnou frontu.

FPS kamery měří přijaté snímky v přibližně sekundovém okně; FPS rozpoznávání
měří čerstvé výsledky přijaté controllerem. Krátké okno může mírně překročit
nominálních 30. Náhled sdílí frekvenci výsledků; nejde o měření fyzické obnovy
monitoru. „Odezva“ začíná převzetím snímku z V4L2, nezahrnuje expozici ani
latenci displeje. Čas kreslení měří `QPainter::drawImage`, nikoli celý kompozitor.

Po ukončení kamerových testů byl přes V4L2 ověřen návrat
`exposure_dynamic_framerate` na původní hodnotu 1. Všech 10 automatických
testů prošlo; IPC bylo kvůli omezení lokálních socketů ověřeno mimo sandbox.
Rozhraní bylo zkontrolováno offscreen na stránkách živého náhledu i nastavení,
včetně načtení skutečných možností kamery.

## Vyšší rozlišení (doplnění)

`./build/camera_check models 0 30 0 1280x720` vyjednalo 1280×720 MJPEG / 30 fps.
Naměřeno 30,39 fps kamery a 29,08 fps výsledků; inference 21,47 ms,
dekódování 3,98 ms, náhled 0,61 ms, maximální stáří výsledku 58,82 ms.
152 výsledků, 7 přeskočených snímků, žádná detekovaná ruka v tomto běhu.
Toto měření ověřuje režim a výkon, nikoli přesnost detekce sevřené pěsti.

## GPU

`backend_check` zahřeje oba modely (ENGINE_CLASSIC), porovná všechny potřebné
pojmenované výstupy s CPU na stejných deterministických datech a měří 30
opakování včetně `setInput`/`forward` a návratu výstupů na CPU:

| Backend | Detekce dlaně, průměr / p95 | Body ruky, průměr / p95 | Výsledek |
| --- | --- | --- | --- |
| CPU | 20,26 / 23,12 ms | 13,28 / 16,05 ms | Ověřený výchozí backend |
| OpenCL, RTX 3050 | 117,54 / 121,52 ms | 166,25 / 174,35 ms | Výrazně pomalejší |
| OpenCL FP16 požadavek | 114,12 / 118,90 ms | 159,57 / 163,40 ms | Výrazně pomalejší; skutečné FP16 nelze z tohoto výsledku potvrdit |
| CUDA | — | — | `getAvailableTargets` nevrací CUDA |
| Vulkan | — | — | První inference se nevrátila, diagnostický proces ukončen |

OpenCL graf hlásil OCL uzly, ale runtime hlásil chybu kompilace
`dnn/activations` (`std::pow` v OpenCL kernelu). Proto samotný dump grafu
**neprokazuje výhradně GPU provedení všech operací**. Výstupní tenzory měly
správné rozměry a maximální absolutní rozdíl proti CPU přibližně 0,00011;
to nenahrazuje test stability sledování skutečné ruky.

Kvůli výraznému zpomalení a chybě kernelu se GPU v aplikaci nenabízí.
Modely ani jejich runtime se nemění. CUDA podpora v ovladači/NVIDIA-SMI
neznamená, že CUDA podporuje nainstalované OpenCV.

## Opakování měření

```sh
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/camera_check models 0 30
./build/camera_check models 0 60
./build/camera_check models 0 30 150
# Každý backend měřit postupně, nikoli souběžně.
timeout 45s ./build/backend_check models cpu
timeout 45s ./build/backend_check models opencl
timeout 45s ./build/backend_check models opencl-fp16
./build/backend_check models cuda
```

GPU test je opt-in, není součástí automatického CTest. Návratový kód 3
znamená nedostupný backend; nedojde k tichému přepnutí testu na CPU.
Kamera, GPU a IPC mohou vyžadovat běh mimo sandbox.

Ovládání expozice vychází z [V4L2 Camera Control Reference](https://kernel.org/doc/html/v5.7/media/uapi/v4l/ext-ctrls-camera.html).
Dostupnost DNN backendů a sestavení viz [OpenCV DNN](https://docs.opencv.org/4.13.0/d6/d0f/group__dnn.html)
a [OpenCV configuration](https://docs.opencv.org/doc/doxygen/html/db/d05/tutorial_config_reference.html).
