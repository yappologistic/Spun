#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <QTimer>

class Disc : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool vinyl READ vinyl WRITE setVinyl NOTIFY vinylChanged)
    Q_PROPERTY(bool cassette READ cassette WRITE setCassette NOTIFY cassetteChanged)
    Q_PROPERTY(QColor shellColor READ shellColor WRITE setShellColor NOTIFY cassetteChanged)
    Q_PROPERTY(QImage artwork READ artwork WRITE setArtwork NOTIFY artworkChanged)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged)
    Q_PROPERTY(bool overlay READ overlay WRITE setOverlay NOTIFY overlayChanged)
public:
    explicit Disc(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    bool cassette() const { return m_cassette; }
    void setCassette(bool v) { if(m_cassette==v)return;m_cassette=v;update();emit cassetteChanged(); }
    QColor shellColor() const { return m_shellColor; }
    void setShellColor(QColor v) { if(m_shellColor==v)return;m_shellColor=v;update();emit cassetteChanged(); }
    bool vinyl() const { return m_vinyl; }
    void setVinyl(bool value) { if(m_vinyl==value)return; m_vinyl=value; update(); emit vinylChanged(); }
    QImage artwork() const { return m_art; }
    void setArtwork(const QImage &image);
    bool overlay() const { return m_overlay; }
    void setOverlay(bool value);
    QColor labelColor() const { return m_labelColor; }
    void setLabelColor(const QColor &value) { if (m_labelColor==value) return; m_labelColor=value; update(); emit labelColorChanged(); }
    static QImage fallbackArt(int size = 1000);
signals:
    void vinylChanged();
    void cassetteChanged();
    void artworkChanged();
    void overlayChanged();
    void labelColorChanged();
private:
    QImage m_art, m_fallback;
    QColor m_labelColor;
    bool m_overlay = false, m_vinyl = false;
    void paintVinyl(QPainter *painter);
    void paintCassette(QPainter *painter);
    bool m_cassette=false;
    QColor m_shellColor=QColor("#262326");
};

class ProgressRing : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(bool linear READ linear WRITE setLinear NOTIFY changed)
    Q_PROPERTY(qreal progress READ progress WRITE setProgress NOTIFY changed)
    Q_PROPERTY(qreal phase READ phase WRITE setPhase NOTIFY changed)
    Q_PROPERTY(qreal amplitude READ amplitude WRITE setAmplitude NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY changed)
public:
    explicit ProgressRing(QQuickItem *parent = nullptr) : QQuickItem(parent) { setFlag(ItemHasContents); }
    bool linear() const { return m_linear; }
    void setLinear(bool value) { if (m_linear==value) return; m_linear=value; update(); emit changed(); }
    qreal progress() const { return m_progress; }
    qreal phase() const { return m_phase; }
    qreal amplitude() const { return m_amplitude; }
    QColor accent() const { return m_accent; }
    void setProgress(qreal v) { v=qBound(0.,v,1.); if (m_progress==v) return; m_progress=v; update(); emit changed(); }
    void setPhase(qreal v) { if (m_phase==v) return; m_phase=v; update(); emit changed(); }
    void setAmplitude(qreal v) { if (m_amplitude==v) return; m_amplitude=v; update(); emit changed(); }
    void setAccent(QColor v) { if (m_accent==v) return; m_accent=v; update(); emit changed(); }
signals:
    void changed();
protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
private:
    bool m_linear=false;
    qreal m_progress=0, m_phase=0, m_amplitude=0;
    QColor m_accent;
};

// Holds the previous artwork while the scene graph exchanges two discs.
class DiscPresentation : public QObject {
    Q_OBJECT
    Q_PROPERTY(QImage artwork READ artwork NOTIFY changed)
    Q_PROPERTY(QImage outgoing READ outgoing NOTIFY changed)
public:
    explicit DiscPresentation(QObject *parent=nullptr);
    QImage artwork() const { return m_art; }
    QImage outgoing() const { return m_outgoing; }
    Q_INVOKABLE void present(const QImage &art, const QString &key, bool animate, bool waitForArt=false);
    Q_INVOKABLE void releaseOutgoing();
signals:
    void changed();
    void swapRequested();
private:
    QImage m_art, m_outgoing;
    QString m_key, m_pendingKey;
    bool m_pendingAnimate=false;
    QTimer m_wait;
};
