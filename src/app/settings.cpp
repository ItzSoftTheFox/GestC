#include "settings.hpp"
#include <algorithm>
#include <cmath>

QVariantMap AppSettings::defaults() {
    return {{"speed", 1.0}, {"smoothing", 0.55}, {"scrollSpeed", 12.0}, {"pinchThreshold", 0.30},
            {"confidence", 0.65}, {"camera", 0}, {"mirror", true}, {"skeleton", true}, {"previewOnly", true}, {"glassOpacity", 0.80}};
}
AppSettings::AppSettings(QObject* parent) : QObject(parent), values_(defaults()) {
    for (auto it = values_.cbegin(); it != values_.cend(); ++it) {
        const auto stored = storage_.value(it.key(), it.value());
        setValue(it.key(), stored);
    }
}
void AppSettings::setValue(const QString& key, const QVariant& input) {
    if (!values_.contains(key)) return;
    QVariant value;
    if (key == "mirror" || key == "skeleton" || key == "previewOnly") value = input.toBool();
    else {
        bool ok = false;
        const double number = input.toDouble(&ok);
        if (!ok || !std::isfinite(number)) return;
        double low = 0, high = 1;
        if (key == "speed") { low = 0.2; high = 3; }
        else if (key == "scrollSpeed") { low = 2; high = 30; }
        else if (key == "pinchThreshold") { low = 0.15; high = 0.6; }
        else if (key == "confidence") { low = 0.5; high = 0.9; }
        else if (key == "camera") { low = 0; high = 63; }
        else if (key == "glassOpacity") { low = 0.60; high = 1; }
        value = std::clamp(number, low, high);
        if (key == "camera") value = value.toInt();
    }
    if (values_.value(key) == value) return;
    values_[key] = value;
    storage_.setValue(key, value);
    emit changed();
}
void AppSettings::reset() {
    values_ = defaults();
    for (auto it = values_.cbegin(); it != values_.cend(); ++it) storage_.setValue(it.key(), it.value());
    emit changed();
}
handmouse::Settings AppSettings::snapshot() const {
    handmouse::Settings s;
    s.speed = values_["speed"].toDouble(); s.smoothing = values_["smoothing"].toDouble();
    s.scrollSpeed = values_["scrollSpeed"].toDouble(); s.pinchThreshold = values_["pinchThreshold"].toDouble();
    s.confidence = values_["confidence"].toDouble(); s.camera = values_["camera"].toInt();
    s.mirror = values_["mirror"].toBool(); s.skeleton = values_["skeleton"].toBool();
    s.previewOnly = values_["previewOnly"].toBool();
    return s;
}
