#pragma once
#include <QObject>
#include <QSettings>
#include <QStringList>

// A saved family name, never a bundled font or a change to desktop preferences.
class Typography : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString selectedFamily READ selectedFamily NOTIFY changed)
    Q_PROPERTY(QString family READ family NOTIFY changed)
    Q_PROPERTY(QString systemFamily READ systemFamily CONSTANT)
    Q_PROPERTY(bool missing READ missing NOTIFY changed)
    Q_PROPERTY(QStringList families READ families NOTIFY familiesChanged)
public:
    explicit Typography(const QString &settingsPath, QObject *parent = nullptr);
    QString selectedFamily() const { return m_selected; }
    QString family() const { return m_family; }
    QString systemFamily() const { return m_system; }
    bool missing() const { return !m_selected.isEmpty() && m_selected != m_family; }
    QStringList families() const { return m_families; }
    Q_INVOKABLE void loadFamilies();
    Q_INVOKABLE bool select(const QString &family);
signals:
    void changed();
    void familiesChanged();
private:
    void resolve();
    QSettings m_settings;
    QString m_selected, m_family, m_system;
    QStringList m_families;
    bool m_loaded = false;
};
