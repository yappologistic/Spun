#pragma once
#include <QObject>
#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioDecoder>
#include <QAudioSink>
#include <QTemporaryFile>
#include <QSettings>
#include <QTimer>
#include <array>
#include <memory>

class Player;
// The USB link is represented in the UI. Audio stays inside Spun: the decoded
// TP-7 stream supplies the clock for all six stereo channels.
class Tx6 : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY changed)
    Q_PROPERTY(bool powered READ powered WRITE setPowered NOTIFY changed)
    Q_PROPERTY(bool available READ available WRITE setAvailable NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
    Q_PROPERTY(QVariantList meters READ meters NOTIFY metersChanged)
    Q_PROPERTY(QVariantList stereoMeters READ stereoMeters NOTIFY metersChanged)
    Q_PROPERTY(bool delay READ delay WRITE setDelay NOTIFY changed)
    Q_PROPERTY(bool compressor READ compressor WRITE setCompressor NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit Tx6(Player *player, const QString &settings, bool outputEnabled = true);
    ~Tx6() override;
    bool visible() const { return m_visible; }
    bool powered() const { return m_powered; }
    bool available() const { return m_available; }
    bool active() const;
    qint64 writtenFrames() const { return m_writtenFrames; }
    bool delay() const { return m_delay; }
    bool compressor() const { return m_compressor; }
    QString status() const;
    QVariantList channels() const;
    QVariantList meters() const;
    QVariantList stereoMeters() const { return {double(m_outputPeak[0]),double(m_outputPeak[1])}; }
    void setVisible(bool value);
    void setPowered(bool value);
    void setAvailable(bool value);
    void setDelay(bool value);
    void setCompressor(bool value);
    Q_INVOKABLE void setLevel(int channel, double value);
    Q_INVOKABLE void setEq(int channel, int band, double db);
    Q_INVOKABLE void toggleMute(int channel);
    Q_INVOKABLE void toggleSolo(int channel);
    Q_INVOKABLE void load(int channel, const QUrl &file);
    Q_INVOKABLE void unload(int channel);
    Q_INVOKABLE void reset();
    void consume(const QAudioBuffer &buffer);
    void flush();
    static QAudioFormat format();
    QByteArray process(const QAudioBuffer &buffer); // Deterministic PCM path, also exercised by diagnostics.
signals:
    void changed();
    void metersChanged();
    void feedback(const QString &message, bool error);
private:
    struct Filter { double b0=1,b1=0,b2=0,a1=0,a2=0,z1[2]{},z2[2]{}; float run(float x,int c); };
    struct Channel {
        double level=1, smoothed=1;
        std::array<double,3> eq{};
        std::array<Filter,3> filters;
        bool mute=false,solo=false,loading=false;
        float peak=0;
        QString name;
        std::unique_ptr<QTemporaryFile> pcm, pending;
        std::unique_ptr<QAudioDecoder> decoder;
    };
    void updateRoute();
    void coefficients(int channel, int band);
    void fail(const QString &message);
    Player *m_player;
    QSettings m_settings;
    std::array<Channel,6> m_channels;
    bool m_visible=false,m_powered=true,m_available=true,m_delay=false,m_compressor=false,m_outputEnabled=true;
    std::unique_ptr<QAudioSink> m_sink;
    QIODevice *m_output=nullptr;
    qint64 m_writtenFrames=0;
    QByteArray m_pending;
    QTimer m_pump,m_meterTimer;
    std::array<float,48000> m_echo{};
    size_t m_echoCursor=0;
    float m_envelope=0;
    float m_outputPeak[2]{};
    double m_master=0.65;
};
