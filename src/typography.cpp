#include "typography.h"
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <algorithm>

Typography::Typography(const QString &settingsPath, QObject *parent)
    : QObject(parent), m_settings(settingsPath, QSettings::IniFormat),
      m_selected(m_settings.value("fontFamily").toString()),
      m_system(QFontInfo(QFontDatabase::systemFont(QFontDatabase::GeneralFont)).family()) {
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
