#pragma once
#include <QObject>
#include <QAudioFormat>
#include <QBuffer>
#include <QElapsedTimer>
#include <QPointer>
class QAudioSink;

// A short, synthesized transport cue; no samples or persistent audio stream.
class TapeSound : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY changed)
public:
    explicit TapeSound(bool outputEnabled=true, QObject *parent=nullptr);
    ~TapeSound() override;
    bool enabled() const { return m_enabled; }
    qreal volume() const { return m_volume; }
    void setEnabled(bool enabled);
    void setVolume(qreal volume);
    Q_INVOKABLE void observe(const QString &source, const QString &key);
    Q_INVOKABLE void transport();
    static QByteArray synthesize(const QAudioFormat &format);
signals:
    void changed();
    void triggered();
private:
    void play();
    void stop();
    bool m_enabled=false, m_outputEnabled=true;
    qreal m_volume=0;
    QString m_source, m_key;
    QElapsedTimer m_lastCue;
    QPointer<QAudioSink> m_sink;
    QBuffer m_buffer;
    QAudioFormat m_format;
};
