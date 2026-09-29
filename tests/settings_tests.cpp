#include "app/settings.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <limits>
#include <iostream>
#include <stdexcept>
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    app.setOrganizationName("HandMouseTests"); app.setApplicationName("Settings");
    try {
        { AppSettings settings;
          require(settings.snapshot().previewOnly, "first launch is preview only");
          settings.setValue("speed", 2.2); settings.setValue("smoothing", 0.85);
          settings.setValue("scrollSpeed", -5); require(settings.snapshot().scrollSpeed == 2, "clamp invalid range");
          settings.setValue("speed", std::numeric_limits<double>::quiet_NaN());
          require(settings.snapshot().speed == 2.2, "ignore nonfinite settings");
          require(settings.snapshot().cameraFps == 30, "default camera FPS");
          settings.setValue("cameraFps", 60);
          for (double invalid : {0., 24., 59.94, 90., std::numeric_limits<double>::infinity()}) settings.setValue("cameraFps", invalid);
          require(settings.snapshot().cameraFps == 60, "only 30/60 FPS are accepted");
          settings.setValue("cameraResolution", "1280x720");
          for (const auto* invalid : {"", "0x720", "1280x0", "-1x720", "9999x720", "1280x720junk"}) settings.setValue("cameraResolution", invalid);
          require(settings.snapshot().cameraWidth == 1280 && settings.snapshot().cameraHeight == 720, "validate resolution pair");
          settings.setValue("unknown", 1); require(!settings.values().contains("unknown"), "ignore unknown key"); }
        AppSettings restored;
        require(restored.snapshot().speed == 2.2 && restored.snapshot().smoothing == .85, "settings persist between launches");
        require(restored.snapshot().cameraFps == 60, "camera FPS persists");
        require(restored.snapshot().cameraWidth == 1280 && restored.snapshot().cameraHeight == 720, "resolution persists");
        restored.reset(); require(restored.snapshot().cameraWidth == 0 && restored.snapshot().cameraHeight == 0, "automatic resolution after reset"); require(restored.snapshot().cameraFps == 30, "reset camera FPS"); require(restored.snapshot().speed == 1 && restored.snapshot().previewOnly, "reset defaults");
        std::cout << "Settings validation and persistence passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
