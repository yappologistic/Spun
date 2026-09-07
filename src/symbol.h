#pragma once
#include <QQuickPaintedItem>
#include <QSvgRenderer>
#include <QSharedPointer>

class Symbol : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QColor ink READ ink WRITE setInk NOTIFY inkChanged)
public:
    explicit Symbol(QQuickItem *parent = nullptr);
    QString name() const { return m_name; }
    QColor ink() const { return m_ink; }
    void setName(const QString &name);
    void setInk(const QColor &ink);
    void paint(QPainter *painter) override;
signals:
    void nameChanged();
    void inkChanged();
private:
    QString m_name;
    QColor m_ink = QColor("#f4ede5");
    QSharedPointer<QSvgRenderer> m_svg;
    QRectF m_bounds;
};
