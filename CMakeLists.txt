cmake_minimum_required(VERSION 3.20)
project(GestC VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Gui Qml Quick REQUIRED)

# Změna: Vyžádáme si výhradně tyto tři specifické moduly OpenCV
find_package(OpenCV REQUIRED COMPONENTS core videoio imgproc)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/gesture_scanner.cpp
    src/ui/video_item.cpp
    src/core/mouse_controller.cpp
)

# Zapneme podporu pro stahování obsahu z internetu
include(FetchContent)

find_package(OpenCV REQUIRED COMPONENTS core videoio imgproc dnn)

# Definujeme zdroj pro C++ knihovnu MediaPipe
FetchContent_Declare(
    mediapipe_cpp
    GIT_REPOSITORY https://github.com/google/mediapipe.git
    GIT_TAG        master

    # Zabrání stahování historie repozitáře, stáhne jen špičku
    GIT_SHALLOW    ON
    # Zobrazí průběh stahování v terminálu, abys viděl práci na pozadí
    GIT_PROGRESS   ON
)

# Provedeme stažení a zpřístupnění knihovny v projektu
FetchContent_MakeAvailable(mediapipe_cpp)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# CMake nyní automaticky vloží do proměnné OpenCV_LIBS pouze ty moduly, které jsme si vyžádali výše
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    ${OpenCV_LIBS}
)