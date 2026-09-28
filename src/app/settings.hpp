#pragma once
#include "gestures/gesture_engine.hpp"
#include <QObject>
#include <QSettings>
#include <QVariantMap>

class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
public:
    explicit AppSettings(QObject* parent = nullptr);
    QVariantMap values() const { return values_; }
    handmouse::Settings snapshot() const;
    Q_INVOKABLE void setValue(const QString& key, const QVariant& value);
    Q_INVOKABLE void reset();
signals:
    void changed();
private:
    static QVariantMap defaults();
    QVariantMap values_;
    QSettings storage_;
};
