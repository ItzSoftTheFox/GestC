#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <QPointer>
#include "app/controller.hpp"

class VideoItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Controller* controller READ controller WRITE setController NOTIFY controllerChanged)
public:
    explicit VideoItem(QQuickItem* parent = nullptr);
    void paint(QPainter* painter) override;
    Controller* controller() const { return controller_; }
    void setController(Controller* controller);
signals:
    void controllerChanged();
    void rendered(double ms);
private:
    QImage image_;
    QPointer<Controller> controller_;
};
