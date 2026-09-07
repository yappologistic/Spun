#pragma once
#include <QGuiApplication>
#include <QQuickWindow>
#include <QProcess>
#include <QTest>

// Synthetic QTest mouse events do not activate a Wayland surface through the
// compositor. Restore real focus after resize/hide/show before testing shortcuts.
inline void focusTestWindow(QQuickWindow *window) {
    if (window->isActive()) return;
    // Offscreen diagnostics never dispatch to the user's compositor. A caller
    // can also opt out of compositor activation for a background native preview.
    if (qEnvironmentVariableIsSet("SPUN_TEST_NO_COMPOSITOR_FOCUS") && QGuiApplication::platformName()=="wayland") return;
    if (QGuiApplication::platformName()=="wayland" && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
        QProcess focus;
        const auto script=QString("local w=hl.get_window(\"pid:%1\"); if w then hl.dispatch(hl.dsp.focus({window=w})) end")
            .arg(QCoreApplication::applicationPid());
        focus.start("hyprctl", {"eval",script});
        if (!focus.waitForFinished(1500)) { focus.kill(); focus.waitForFinished(500); }
    }
    window->requestActivate();
    for (int i=0;i<25 && !window->isActive();++i) QTest::qWait(20);
}
template<typename Key>
inline void testKeyClick(QQuickWindow *window, Key key, Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
    focusTestWindow(window);
    QTest::keyClick(window,key,modifiers);
}
