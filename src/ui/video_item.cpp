#include "video_item.hpp"

VideoItem::VideoItem(QQuickItem *parent) : QQuickPaintedItem(parent), m_scanner(nullptr) {
}

void VideoItem::paint(QPainter *painter) {
    if (currentFrame.isNull()) return;
    
    // Vykreslíme obrázek. Funkce boundingRect() zajistí přizpůsobení velikosti podle QML.
    painter->drawImage(boundingRect().toRect(), currentFrame);
}

GestureScanner* VideoItem::scanner() const {
    return m_scanner;
}

void VideoItem::setScanner(GestureScanner* newScanner) {
    if (m_scanner == newScanner) return;
    
    m_scanner = newScanner;
    
    if (m_scanner) {
        // Propojíme signál frameReady ze skeneru s naší funkcí receiveFrame
        connect(m_scanner, &GestureScanner::frameReady, this, &VideoItem::receiveFrame);
    }
    
    emit scannerChanged();
}

void VideoItem::receiveFrame(const QImage &frame) {
    currentFrame = frame;
    // Funkce update() upozorní grafickou kartu na nutnost překreslení tohoto prvku
    update();
}