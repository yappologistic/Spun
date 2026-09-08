#pragma once
#include <QIODevice>
#include <QAudioFormat>
#include <QPointer>
#include <atomic>
class QAudioSink;

class VinylNoise : public QIODevice {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(bool crackle READ crackle WRITE setCrackle NOTIFY changed)
    Q_PROPERTY(bool hiss READ hiss WRITE setHiss NOTIFY changed)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY changed)
public:
    explicit VinylNoise(bool outputEnabled=true,QObject *parent=nullptr);
    ~VinylNoise() override;
    bool active() const { return m_active; }
    bool crackle() const { return m_crackle; }
    bool hiss() const { return m_hiss; }
    qreal volume() const { return m_volume; }
    void setActive(bool value);
    void setCrackle(bool value);
    void setHiss(bool value);
    void setVolume(qreal value);
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return isOpen()?4096+QIODevice::bytesAvailable():0; }
signals:
    void changed();
protected:
    qint64 readData(char *data,qint64 length) override;
    qint64 writeData(const char *,qint64) override { return -1; }
private:
    void updateOutput();
    void stop();
    bool m_outputEnabled, m_active=false;
    std::atomic<bool> m_crackle{false}, m_hiss{false};
    qreal m_volume=0;
    QAudioFormat m_format;
    QPointer<QAudioSink> m_sink;
    quint32 m_random=0x41a399u;
    double m_pop=0,m_low=0,m_fade=0;
};
