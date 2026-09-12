#include "tx6.h"
#include "player.h"
#include <QAudioDevice>
#include <QMediaDevices>
#include <QFileInfo>
#include <cmath>
#include <cstring>
#include <algorithm>

QAudioFormat Tx6::format() { QAudioFormat f;f.setSampleRate(48000);f.setChannelCount(2);f.setSampleFormat(QAudioFormat::Float);return f; }
Tx6::Tx6(Player *player,const QString &settings,bool outputEnabled):QObject(player),m_player(player),m_settings(settings,QSettings::IniFormat),m_outputEnabled(outputEnabled) {
    m_visible=m_settings.value("tx6Visible",false).toBool();
    m_channels[0].name="TP-7 · USB 1/2";
    for(int c=0;c<6;++c)for(int b=0;b<3;++b)coefficients(c,b);
    m_player=nullptr; setPlayer(player);
    m_pump.setInterval(8);
    connect(&m_pump,&QTimer::timeout,this,[this]{
        if(!m_output || m_pending.isEmpty())return;
        const auto written=m_output->write(m_pending.constData(),std::min<qint64>(m_pending.size(),m_sink->bytesFree()));
        if(written>0){m_pending.remove(0,written);m_writtenFrames+=written/8;}
    });
    m_meterTimer.setInterval(40);
    connect(&m_meterTimer,&QTimer::timeout,this,[this]{bool alive=false;for(auto &p:m_outputPeak)p*=.77f;for(auto &c:m_channels){c.peak*=.77f;alive|=c.peak>.001f;}emit metersChanged();if(!alive&&!m_player->playing())m_meterTimer.stop();});
    updateRoute();
}
Tx6::~Tx6() { m_player->attachMixer(nullptr);flush(); }
bool Tx6::active() const { return m_visible&&m_powered&&m_available&&m_player->medium()=="tp7"; }
QString Tx6::status() const { return !m_powered?"OFF":!m_available?"LOCAL ONLY":active()?"USB 1–12":"BYPASS"; }
QVariantList Tx6::channels() const {
    QVariantList rows;
    for(const auto &c:m_channels)rows.append(QVariantMap{{"name",c.name},{"loaded",!c.name.isEmpty()},{"loading",c.loading},{"level",c.level},{"high",c.eq[0]},{"mid",c.eq[1]},{"low",c.eq[2]},{"mute",c.mute},{"solo",c.solo}});
    return rows;
}
QVariantList Tx6::meters() const { QVariantList result;for(const auto &c:m_channels)result.append(double(c.peak));return result; }
void Tx6::updateRoute(){flush();m_player->refreshMixerRoute();emit changed();}
void Tx6::setVisible(bool v){if(v==m_visible)return;m_visible=v;m_settings.setValue("tx6Visible",v);updateRoute();}
void Tx6::setPowered(bool v){if(v==m_powered)return;m_powered=v;updateRoute();}
void Tx6::setAvailable(bool v){if(v==m_available)return;m_available=v;updateRoute();}
void Tx6::setDelay(bool v){if(v==m_delay)return;m_delay=v;if(!v)m_echo.fill(0);emit changed();}
void Tx6::setCompressor(bool v){if(v==m_compressor)return;m_compressor=v;emit changed();}
void Tx6::setLevel(int c,double v){if(c<0||c>=6||!std::isfinite(v))return;m_channels[c].level=std::clamp(v,0.,1.);emit changed();}
void Tx6::setEq(int c,int b,double v){if(c<0||c>=6||b<0||b>=3||!std::isfinite(v))return;m_channels[c].eq[b]=std::clamp(v,-12.,12.);coefficients(c,b);emit changed();}
void Tx6::toggleMute(int c){if(c<0||c>=6)return;m_channels[c].mute=!m_channels[c].mute;emit changed();}
void Tx6::toggleSolo(int c){if(c<0||c>=6)return;m_channels[c].solo=!m_channels[c].solo;emit changed();}
void Tx6::reset(){for(int c=0;c<6;++c){m_channels[c].level=1;m_channels[c].mute=false;m_channels[c].solo=false;for(int b=0;b<3;++b)setEq(c,b,0);}m_delay=false;m_compressor=false;flush();emit changed();}
void Tx6::fail(const QString &s){emit feedback(s,true);}
void Tx6::load(int c,const QUrl &url) {
    if(c<1||c>=6||!url.isLocalFile()||!QFileInfo(url.toLocalFile()).isFile()){fail("Choose a local audio file for channels 2–6.");return;}
    auto &ch=m_channels[c];ch.decoder.reset();ch.pending.reset();ch.loading=false;
    ch.pending=std::make_unique<QTemporaryFile>();
    if(!ch.pending->open()){ch.pending.reset();fail("Could not prepare a temporary mixer track.");return;}
    ch.loading=true;ch.decoder=std::make_unique<QAudioDecoder>();
    auto *decoder=ch.decoder.get();decoder->setAudioFormat(format());
    auto cancel=[this,c,decoder](const QString &error){auto &ch=m_channels[c];if(ch.decoder.get()!=decoder)return;decoder->stop();ch.decoder.release()->deleteLater();ch.pending.reset();ch.loading=false;emit changed();fail(error);};
    connect(decoder,&QAudioDecoder::bufferReady,this,[this,c,decoder,cancel]{
        if(m_channels[c].decoder.get()!=decoder)return;
        auto buffer=decoder->read();auto &ch=m_channels[c];
        if(buffer.format()!=format()){cancel("This decoder cannot supply stereo 48 kHz audio to the mixer.");return;}
        // Keep RAM bounded. Stems are streamed from private, automatically removed PCM files.
        if(ch.pending->size()+buffer.byteCount()>512LL*1024*1024){cancel("This stem exceeds the mixer’s 23-minute stereo limit. Choose a shorter file.");return;}
        if(ch.pending->write(buffer.constData<char>(),buffer.byteCount())!=buffer.byteCount())cancel("Could not write the mixer’s temporary audio cache.");
    });
    connect(decoder,&QAudioDecoder::finished,this,[this,c,decoder,url]{auto &ch=m_channels[c];if(ch.decoder.get()!=decoder)return;ch.pending->flush();ch.pcm=std::move(ch.pending);ch.name=QFileInfo(url.toLocalFile()).completeBaseName();ch.loading=false;ch.decoder.release()->deleteLater();emit changed();emit feedback("Channel "+QString::number(c+1)+" ready · follows TP-7 transport",false);});
    connect(decoder,qOverload<QAudioDecoder::Error>(&QAudioDecoder::error),this,[decoder,cancel](QAudioDecoder::Error){cancel("Could not decode this mixer track: "+decoder->errorString());});
    decoder->setSource(url);decoder->start();emit changed();
}
void Tx6::unload(int c){if(c<1||c>=6)return;auto &ch=m_channels[c];ch.decoder.reset();ch.pending.reset();ch.pcm.reset();ch.name.clear();ch.loading=false;ch.peak=0;emit changed();}
float Tx6::Filter::run(float x,int c){const double y=b0*x+z1[c];z1[c]=b1*x-a1*y+z2[c];z2[c]=b2*x-a2*y;return float(y);}
void Tx6::coefficients(int c,int b){
    auto &f=m_channels[c].filters[b];const double db=m_channels[c].eq[b];
    const double A=std::pow(10.,db/40.),w=2*M_PI*(b==0?8000:b==1?1000:160)/48000.,co=std::cos(w),si=std::sin(w);
    double b0,b1,b2,a0,a1,a2;
    if(b==1){const double alpha=si/1.4;b0=1+alpha*A;b1=-2*co;b2=1-alpha*A;a0=1+alpha/A;a1=-2*co;a2=1-alpha/A;}
    else {const double beta=2*std::sqrt(A)*si/std::sqrt(2.);
        if(b==2){b0=A*((A+1)-(A-1)*co+beta);b1=2*A*((A-1)-(A+1)*co);b2=A*((A+1)-(A-1)*co-beta);a0=(A+1)+(A-1)*co+beta;a1=-2*((A-1)+(A+1)*co);a2=(A+1)+(A-1)*co-beta;}
        else {b0=A*((A+1)+(A-1)*co+beta);b1=-2*A*((A-1)+(A+1)*co);b2=A*((A+1)+(A-1)*co-beta);a0=(A+1)-(A-1)*co+beta;a1=2*((A-1)-(A+1)*co);a2=(A+1)-(A-1)*co-beta;}
    }
    f.b0=b0/a0;f.b1=b1/a0;f.b2=b2/a0;f.a1=a1/a0;f.a2=a2/a0;
}
QByteArray Tx6::process(const QAudioBuffer &buffer){
    if(!buffer.isValid()||buffer.format()!=format())return {};
    const int frames=buffer.frameCount();const auto *in=buffer.constData<float>();
    QByteArray result(buffer.byteCount(),0);auto *out=reinterpret_cast<float*>(result.data());
    const bool solo=std::any_of(m_channels.begin(),m_channels.end(),[](const auto &c){return c.solo;});
    std::array<QByteArray,6> stems;
    for(int c=1;c<6;++c)if(m_channels[c].pcm){auto &f=*m_channels[c].pcm;const auto offset=std::max<qint64>(0,qRound64(buffer.startTime()*48000./1000000.))*8;if(f.seek(offset))stems[c]=f.read(buffer.byteCount());}
    for(int i=0;i<frames;++i){float mix[2]{};
        for(int c=0;c<6;++c){auto &ch=m_channels[c];const float target=ch.mute||(solo&&!ch.solo)?0:ch.level;ch.smoothed+=(target-ch.smoothed)*.004;
            for(int side=0;side<2;++side){float sample=c==0?in[i*2+side]:stems[c].size()>=(i+1)*8?reinterpret_cast<const float*>(stems[c].constData())[i*2+side]:0;
                if(!std::isfinite(sample))sample=0;
                for(auto &filter:ch.filters)sample=filter.run(sample,side);
                sample*=ch.smoothed;ch.peak=std::max(ch.peak,std::abs(sample));mix[side]+=sample;
            }
        }
        const float peak=std::max(std::abs(mix[0]),std::abs(mix[1]));m_envelope+=(peak-m_envelope)*(peak>m_envelope?.02f:.0003f);
        const float gain=m_compressor&&m_envelope>.35f?std::pow(.35f/m_envelope,.75f):1;
        m_master+=(m_player->volume()-m_master)*.004;
        for(int side=0;side<2;++side){float sample=mix[side]*gain;
            if(m_delay){const float echo=m_echo[m_echoCursor];m_echo[m_echoCursor]=sample+echo*.32f;sample+=echo*.23f;m_echoCursor=(m_echoCursor+1)%m_echo.size();}
            // The master has a peak safety ceiling, not a makeup-gain boost.
            out[i*2+side]=std::clamp(float(sample*m_master),-1.f,1.f);
            m_outputPeak[side]=std::max(m_outputPeak[side],std::abs(out[i*2+side]));
        }
    }
    if(!m_meterTimer.isActive())m_meterTimer.start();
    return result;
}
void Tx6::consume(const QAudioBuffer &buffer){
    if(!active()||!buffer.isValid()||!m_player->playing())return;
    if(buffer.format()!=format()){setPowered(false);fail("Mixer bypassed: this audio backend does not provide stereo PCM.");return;}
    auto pcm=process(buffer);if(!m_outputEnabled)return;
    if(!m_sink){const auto device=QMediaDevices::defaultAudioOutput();if(device.isNull()||!device.isFormatSupported(format())){setPowered(false);fail("Mixer bypassed: no stereo 48 kHz output is available.");return;}
        m_sink=std::make_unique<QAudioSink>(device,format());m_sink->setBufferSize(16384);m_output=m_sink->start();
        if(!m_output){setPowered(false);fail("Could not start mixer output. Normal playback restored.");return;}m_pump.start();
    }
    if(m_pending.size()+pcm.size()>128000){setPowered(false);fail("Mixer bypassed after an audio output interruption.");return;}
    m_pending.append(pcm);
}
void Tx6::flush(){m_pump.stop();m_output=nullptr;if(m_sink)m_sink->stop();m_sink.reset();m_pending.clear();m_echo.fill(0);m_echoCursor=0;m_envelope=0;m_outputPeak[0]=m_outputPeak[1]=0;m_master=m_player->volume();for(auto &c:m_channels){c.peak=0;for(auto &f:c.filters){std::fill_n(f.z1,2,0);std::fill_n(f.z2,2,0);}}emit metersChanged();}

void Tx6::setPlayer(Player *player) {
    if (!player || player==m_player) return;
    if (m_player) { flush(); disconnect(m_player,nullptr,this,nullptr); m_player->attachMixer(nullptr); }
    m_player=player;player->attachMixer(this);
    connect(player,&Player::mediumChanged,this,&Tx6::updateRoute);
    connect(player,&Player::trackChanged,this,&Tx6::flush);
    connect(player,&Player::seeked,this,&Tx6::flush);
    connect(player,&Player::playingChanged,this,[this]{if(!m_player->playing())flush();});
    updateRoute();
}
