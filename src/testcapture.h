#pragma once

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QPainter>
#include <QElapsedTimer>
#include <QTest>

// The offscreen GLX platform's window readback can return a stale drawable
// after resizing. Capture the actual scene graph into a Qt-owned render target
// instead. Native checks still read the presented window, including its alpha.
inline QImage captureTestWindow(QQuickWindow *window) {
    if (QGuiApplication::platformName() != "offscreen"
        || qEnvironmentVariable("QT_QUICK_BACKEND") == "software")
        return window->grabWindow();

    // Explicit pixel dimensions avoid an old offscreen drawable size surviving
    // a resize while a transient notice is leaving the scene.
    const QSize pixels(qRound(window->width() * window->devicePixelRatio()),
                       qRound(window->height() * window->devicePixelRatio()));
    const auto result = window->contentItem()->grabToImage(pixels);
    if (!result) return {};
    QElapsedTimer timer;
    timer.start();
    while (result->image().isNull() && timer.elapsed() < 4000)
        QTest::qWait(20);
    const auto scene = result->image();
    if (scene.isNull()) return {};

    QImage frame(scene.size(), QImage::Format_ARGB32_Premultiplied);
    frame.fill(window->color());
    QPainter painter(&frame);
    painter.drawImage(0, 0, scene);
    return frame;
}
