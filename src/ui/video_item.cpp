#include "video_item.hpp"
#include <QPainter>
VideoItem::VideoItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &VideoItem::rendered, this, [this](double ms) {
        if (controller_) controller_->reportRender(ms);
    }, Qt::QueuedConnection);
}
void VideoItem::paint(QPainter* painter) {
    if (image_.isNull()) return;
    const double start = CameraWorker::now();
    QSizeF size = image_.size(); size.scale(boundingRect().size(), Qt::KeepAspectRatio);
    QRectF target(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(target, image_);
    emit rendered((CameraWorker::now() - start) * 1000);
}
void VideoItem::setController(Controller* controller) {
    if (controller_ == controller) return;
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = controller;
    image_ = {};
    if (controller_) connect(controller_, &Controller::frameReady, this, [this](const QImage& image) { image_ = image; update(); });
    update(); emit controllerChanged();
}
