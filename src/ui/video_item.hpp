#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <QPainter>
#include "../core/gesture_scanner.hpp"

// Dědíme z třídy určené pro vlastní 2D kreslení v QML
class VideoItem : public QQuickPaintedItem {
    Q_OBJECT
    // Vytvoříme vlastnost, přes kterou QML předá odkaz na náš skener
    Q_PROPERTY(GestureScanner* scanner READ scanner WRITE setScanner NOTIFY scannerChanged)

public:
    explicit VideoItem(QQuickItem *parent = nullptr);
    
    // Hlavní funkce pro kreslení na obrazovku
    void paint(QPainter *painter) override;

    GestureScanner* scanner() const;
    void setScanner(GestureScanner* newScanner);

signals:
    void scannerChanged();

public slots:
    // Funkce, která přijme obrázek ze skeneru
    void receiveFrame(const QImage &frame);

private:
    QImage currentFrame;
    GestureScanner* m_scanner;
};