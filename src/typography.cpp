#include "typography.h"
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <algorithm>
#include <cmath>

Typography::Typography(const QString &settingsPath, QObject *parent)
    : QObject(parent), m_settings(settingsPath, QSettings::IniFormat),
      m_selected(m_settings.value("fontFamily").toString()),
      m_system(QFontInfo(QFontDatabase::systemFont(QFontDatabase::GeneralFont)).family()) {
    const auto savedScale=m_settings.value("uiScale",1.).toDouble();
    m_scale=std::isfinite(savedScale)?qBound(.85,savedScale,1.5):1.;
    resolve();
    connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, [this] {
        const bool loaded = m_loaded;
        m_loaded = false;
        if (loaded) loadFamilies();
        resolve();
        emit changed();
    });
}
void Typography::resolve() {
    m_family = !m_selected.isEmpty() && QFontDatabase::hasFamily(m_selected) ? m_selected : m_system;
}
void Typography::setUiScale(qreal scale) {
    if(!std::isfinite(scale))return;
    scale=qBound(.85,scale,1.5);
    if(qFuzzyCompare(m_scale,scale))return;
    m_scale=scale;m_settings.setValue("uiScale",scale);m_settings.sync();emit scaleChanged();
}
void Typography::loadFamilies() {
    if (m_loaded) return;
    m_families = QFontDatabase::families();
    m_families.removeIf([](const QString &family) { return QFontDatabase::isPrivateFamily(family); });
    std::sort(m_families.begin(), m_families.end(), [](const QString &a, const QString &b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    m_loaded = true;
    emit familiesChanged();
}
bool Typography::select(const QString &family) {
    if (!family.isEmpty() && (!QFontDatabase::hasFamily(family) || QFontDatabase::isPrivateFamily(family))) return false;
    if (family == m_selected) return true;
    m_selected = family;
    resolve();
    m_settings.setValue("fontFamily", m_selected);
    m_settings.sync();
    emit changed();
    return true;
}
