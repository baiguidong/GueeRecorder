#ifndef VIDEOSYNTHESIZER_H
#define VIDEOSYNTHESIZER_H
#include "InputSource/BaseLayer.h"

#include "MediaCodec/VideoEncoder.h"
#include "MediaCodec/MediaWriterFlv.h"
#include "MediaCodec/MediaWriterMp4.h"
#include "MediaCodec/MediaWriterTs.h"
#include "MediaCodec/SoundRecorder.h"

#include "ShaderProgramPool.h"
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObjectFormat>

class VideoSynthesizer : public QThread, public BaseLayer
{
    Q_OBJECT
public:
    ~VideoSynthesizer() override;
    static VideoSynthesizer& instance() { static VideoSynthesizer v; return v; }
    void init(QOpenGLContext* shardContext);
    void uninit();
    ShaderProgramPool& programPool() {return m_progPool;}
    const QString& layerType() const override { static QString tn = "synthesizer"; return tn; }
    BaseLayer *createLayer(const QString &type);
    void immediateUpdate();

    bool open(const QString &sourceName = QString());
    void close();

    bool play();
    bool pause();

    bool resetDefaultOption();
    bool setSize(int32_t width, int32_t height);
    int32_t width() const {return m_vidParams.width;}
    int32_t height() const {return m_vidParams.height;}
    bool setFrameRate(float fps);
    float frameRate() const override {return m_vidParams.frameRate;}
    float renderFps() const {return m_frameRate.fps();}
    float encodeFps() const {return m_vidEncoder.encodeFps();}
    int64_t timestamp() const {return m_timestamp.elapsed_milli();}

    bool setProfile(EVideoProfile profile);
    EVideoProfile profile() const {return m_vidParams.profile;}
    bool setPreset(EVideoPreset_x264 preset);
    EVideoPreset_x264 preset() const {return m_vidParams.presetX264;}
    bool setCSP(EVideoCSP csp);
    EVideoCSP csp() const {return m_vidParams.outputCSP;}
    bool setPsyTune(EPsyTuneType psy);
    EPsyTuneType psyTune() const {return m_vidParams.psyTune;}
    bool setBitrateMode(EVideoRateMode rateMode);
    EVideoRateMode bitrateMode() const {return m_vidParams.rateMode;}
    bool setBitrate(int32_t bitrate);
    int32_t bitrate() const {return m_vidParams.bitrate;}
    bool setBitrateMin(int32_t bitrateMin);
    int32_t bitrateMin() const {return m_vidParams.bitrateMin;}
    bool setBitrateMax(int32_t bitrateMax);
    int32_t bitrateMax() const {return m_vidParams.bitrateMax;}
    bool setVbvBuffer(int32_t vbvBufferSize);
    int32_t vbvBuffer() const {return m_vidParams.vbvBuffer;}
    bool setGopMin(int32_t gopMin);
    int32_t gopMin() const {return m_vidParams.gopMin;}
    bool setGopMax(int32_t gopMax);
    int32_t gopMax() const {return m_vidParams.gopMax;}
    bool setRefFrames(int32_t refFrames);
    int32_t refFrames() const {return m_vidParams.refFrames;}
    bool setBFrames(int32_t bFrames);
    int32_t bFrames() const {return m_vidParams.BFrames;}

    bool enableAudio(bool enable);
    bool audioIsEnabled() const { return m_audParams.enabled;}
    bool setSampleBits(ESampleBits bits);
    ESampleBits sampleBits() const { return m_audParams.sampleBits;}
    bool setSampleRate(int32_t rate);
    int32_t sampleRate() const { return m_audParams.sampleRate; }
    bool setChannels(int32_t channels);
    int32_t channels() const { return m_audParams.channels; }
    bool setAudioBitrate(int32_t bitrate );
    int32_t audioBitrate() const { return m_audParams.bitrate; }
    int32_t maxAudioBitrate() const;
    int32_t minAudioBitrate() const;

    SoundDevInfo& audCallbackDev() { return m_audRecorder.callbackDev(); }
    SoundDevInfo& audMicInputDev() { return m_audRecorder.micInputDev(); }
private:
    VideoSynthesizer();
    SVideoParams m_vidParams;
    SAudioParams m_audParams;
    FrameSynchronization    m_frameSync;
    FrameRateCalc           m_frameRate;
    FrameTimestamp          m_timestamp;

    struct FrameInfo
    {
        EVideoCSP csp;
        int64_t timestamp;
        int alignWidth;
        int alignHeight;
        int textureWidth;
        int textureHeight;
        GLenum internalFormat;
        GLenum dateType;
        int planeCount;
        int stride[3];
        int byteNum[3];

        QOpenGLBuffer*  buffer;
        QOpenGLFramebufferObject* fbo;
    };

    FrameInfo    m_frameData;

    GueeVideoEncoder m_vidEncoder;
    GueeMediaStream m_medStream;
    QVector<GueeMediaWriter*> m_writers;
    SoundRecorder m_audRecorder;

    bool initYuvFbo();
    void uninitYubFbo();

    bool m_immediateUpdate = false;
    bool m_threadWorking = false;
    bool m_videoSizeChanged = false;
    QOpenGLContext* m_context = nullptr;
    QOffscreenSurface* m_surface = nullptr;
    QVector4D m_backgroundColor;
    void renderThread();
    void run() override;
    BaseSource* onCreateSource(const QString &sourceName) override { Q_UNUSED(sourceName) return nullptr; }
    void onReleaseSource(BaseSource* source) override { Q_UNUSED(source) }
    ShaderProgramPool m_progPool;
    void loadShaderPrograms();
    void putFrameToEncoder(GLuint textureId);
signals:
    void frameReady(uint textureId);
    void initDone( bool success);
};

#endif // VIDEOSYNTHESIZER_H