#include "vinylnoise.h"
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <cmath>
#include <cstring>
VinylNoise::VinylNoise(bool outputEnabled,QObject *parent):QIODevice(parent),m_outputEnabled(outputEnabled) {}
VinylNoise::~VinylNoise() { stop(); }
void VinylNoise::setActive(bool v){if(m_active==v)return;m_active=v;updateOutput();emit changed();}
void VinylNoise::setCrackle(bool v){if(m_crackle==v)return;m_crackle=v;updateOutput();emit changed();}
void VinylNoise::setHiss(bool v){if(m_hiss==v)return;m_hiss=v;updateOutput();emit changed();}
void VinylNoise::setVolume(qreal v){v=qBound(0.,v,1.);if(m_volume==v)return;m_volume=v;updateOutput();emit changed();}
void VinylNoise::stop(){
    if(m_sink){auto *sink=m_sink.data();m_sink=nullptr;sink->disconnect(this);sink->stop();sink->deleteLater();}
    close();
}
void VinylNoise::updateOutput(){
    if(!m_active||(!m_crackle&&!m_hiss)||m_volume<=0){stop();return;}
    if(isOpen()){if(m_sink)m_sink->setVolume(m_volume*.35);return;}
    m_format.setSampleRate(24000);m_format.setChannelCount(1);m_format.setSampleFormat(QAudioFormat::Int16);
    const auto device=m_outputEnabled?QMediaDevices::defaultAudioOutput():QAudioDevice{};
    if(m_outputEnabled){
        if(device.isNull())return;
        if(!device.isFormatSupported(m_format))m_format=device.preferredFormat();
    }
    if(!m_format.isValid())return;
    m_pop=m_low=m_fade=0;open(QIODevice::ReadOnly);
    if(!m_outputEnabled)return;
    auto *sink=new QAudioSink(device,m_format,this);m_sink=sink;sink->setVolume(m_volume*.35);
    connect(sink,&QAudioSink::stateChanged,this,[this,sink](QAudio::State state){
        if(m_sink==sink&&state==QAudio::StoppedState&&sink->error()!=QAudio::NoError)
            QMetaObject::invokeMethod(this,[this,sink]{if(m_sink==sink)stop();},Qt::QueuedConnection);
    });
    sink->start(this);
}
qint64 VinylNoise::readData(char *data,qint64 length){
    const int frameBytes=m_format.bytesPerFrame();if(frameBytes<=0)return 0;
    const qint64 frames=length/frameBytes;const int rate=m_format.sampleRate();
    const bool crackle=m_crackle.load(),hiss=m_hiss.load();
    const double decay=std::exp(-1./(rate*.0008));
    for(qint64 i=0;i<frames;++i){
        m_random^=m_random<<13;m_random^=m_random>>17;m_random^=m_random<<5;
        const double white=double(m_random)/2147483648.-1.;
        m_low+=.025*(white-m_low);
        if(crackle&&m_random%quint32(qMax(1,rate/5))==0)m_pop=.25+.45*std::abs(white);
        m_pop*=decay;m_fade=qMin(1.,m_fade+1./(rate*.04));
        const double value=qBound(-.8,((crackle?m_pop*white:0)+(hiss?white*.012+m_low*.035:0))*m_fade,.8);
        for(int channel=0;channel<m_format.channelCount();++channel){
            char *out=data+i*frameBytes+channel*m_format.bytesPerSample();
            switch(m_format.sampleFormat()){
            case QAudioFormat::UInt8:*out=char(qRound((value+1)*127.5));break;
            case QAudioFormat::Int16:{const qint16 v=qRound(value*32767);std::memcpy(out,&v,2);break;}
            case QAudioFormat::Int32:{const qint32 v=qint32(value*2147483647.);std::memcpy(out,&v,4);break;}
            case QAudioFormat::Float:{const float v=float(value);std::memcpy(out,&v,4);break;}
            default:return 0;
            }
        }
    }
    return frames*frameBytes;
}
