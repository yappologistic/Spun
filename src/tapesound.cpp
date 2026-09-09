#include "tapesound.h"
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <cmath>
#include <cstring>

TapeSound::TapeSound(bool outputEnabled,QObject *parent):QObject(parent),m_outputEnabled(outputEnabled) {}
TapeSound::~TapeSound() { stop(); }
void TapeSound::setEnabled(bool enabled) {
    if(m_enabled==enabled)return;
    m_enabled=enabled;if(!enabled)stop();emit changed();
}
void TapeSound::setVolume(qreal volume) {
    volume=qBound(0.,volume,1.);if(m_volume==volume)return;
    m_volume=volume;
    if(volume==0)stop();else if(m_sink)m_sink->setVolume(volume*.45);
    emit changed();
}
void TapeSound::observe(const QString &source,const QString &key) {
    const bool transition=!key.isEmpty()&&!m_key.isEmpty()&&source==m_source&&key!=m_key;
    m_source=source;m_key=key;
    if(transition)transport();
}
void TapeSound::transport() {
    if(!m_enabled || m_volume<=0 || (m_lastCue.isValid()&&m_lastCue.elapsed()<160))return;
    m_lastCue.restart();emit triggered();
    if(m_outputEnabled)play();
}
QByteArray TapeSound::synthesize(const QAudioFormat &format) {
    if(!format.isValid() || format.sampleRate()>192000 || format.channelCount()>8)return {};
    const int frames=qRound(format.sampleRate()*.28);
    QByteArray pcm(format.bytesForFrames(frames),Qt::Uninitialized);
    quint32 random=0x43a55e7u;double low=0;
    for(int i=0;i<frames;++i) {
        const double t=double(i)/format.sampleRate();
        random^=random<<13;random^=random>>17;random^=random<<5;
        const double noise=double(random)/2147483648.-1.;
        low+=.24*(noise-low);
        double sample=0;
        for(int hit=0;hit<3;++hit) {
            const double dt=t-(hit==0?.008:hit==1?.061:.132);
            if(dt<0)continue;
            const double strength=hit==0?.64:hit==1?.17:.43;
            // Latch impact, brief spring chatter, then the softer engaging key.
            const double attack=1-std::exp(-dt*4500);
            sample+=strength*attack*(low*2.2*std::exp(-dt*115)
                +(noise-low)*.45*std::exp(-dt*330)
                +std::sin(dt*(hit==2?180:125)*6.283185307)*.45*std::exp(-dt*92));
        }
        sample=qBound(-.85,sample,.85);
        // A short final fade guarantees a quiet endpoint with every sample rate.
        sample*=qBound(0.,(.28-t)/.025,1.);
        for(int channel=0;channel<format.channelCount();++channel) {
            char *out=pcm.data()+(i*format.channelCount()+channel)*format.bytesPerSample();
            switch(format.sampleFormat()) {
            case QAudioFormat::UInt8: *out=char(qRound((sample+1)*127.5));break;
            case QAudioFormat::Int16: {const qint16 value=qRound(sample*32767);std::memcpy(out,&value,2);break;}
            case QAudioFormat::Int32: {const qint32 value=qint32(sample*2147483647.);std::memcpy(out,&value,4);break;}
            case QAudioFormat::Float: {const float value=float(sample);std::memcpy(out,&value,4);break;}
            default:return {};
            }
        }
    }
    return pcm;
}
void TapeSound::stop() {
    if(m_sink) {auto *sink=m_sink.data();m_sink=nullptr;sink->disconnect(this);sink->stop();sink->deleteLater();}
    m_buffer.close();
}
void TapeSound::play() {
    stop();
    const auto device=QMediaDevices::defaultAudioOutput();if(device.isNull())return;
    QAudioFormat format;format.setSampleRate(24000);format.setChannelCount(1);format.setSampleFormat(QAudioFormat::Int16);
    if(!device.isFormatSupported(format))format=device.preferredFormat();
    if(m_format!=format || m_buffer.data().isEmpty()){m_format=format;m_buffer.setData(synthesize(format));}
    if(m_buffer.data().isEmpty() || !m_buffer.open(QIODevice::ReadOnly))return;
    auto *sink=new QAudioSink(device,format,this);m_sink=sink;sink->setVolume(m_volume*.45);
    connect(sink,&QAudioSink::stateChanged,this,[this,sink](QAudio::State state){
        if(m_sink==sink&&(state==QAudio::IdleState||(state==QAudio::StoppedState&&sink->error()!=QAudio::NoError)))
            QMetaObject::invokeMethod(this,[this,sink]{if(m_sink==sink)stop();},Qt::QueuedConnection);
    });
    sink->start(&m_buffer);
}
