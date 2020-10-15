#include "VideoSynthesizer.h"
#include "InputSource/ScreenLayer.h"
#include "InputSource/CameraLayer.h"
#include "InputSource/PictureLayer.h"

VideoSynthesizer::VideoSynthesizer()
{
    memset(&m_vidParams, 0, sizeof(m_vidParams));
    memset(&m_audParams, 0, sizeof(m_audParams));
    memset(&m_frameData, 0, sizeof(m_frameData));
    resetDefaultOption();

    m_vidEncoder.bindStream(&m_medStream);
    m_audRecorder.bindStream(&m_medStream);
}

VideoSynthesizer::~VideoSynthesizer()
{
    close(nullptr, nullptr);
    uninit();
}

void VideoSynthesizer::init(QOpenGLContext* shardContext)
{
    if (m_threadWorking == false)
    {
        if ( shardContext )
        {
            auto mainSur = shardContext->surface();
            //m_surface = new QOffscreenSurface(nullptr, dynamic_cast<QObject*>(this));
            m_surface = new QOffscreenSurface(nullptr);


//            QSurfaceFormat fmt = mainSur->format();
//            fprintf(stderr, "alphaBufferSize:%d\n"
//                            "redBufferSize:%d\n"
//                            "greenBufferSize:%d\n"
//                            "blueBufferSize:%d\n"
//                            "depthBufferSize:%d\n"
//                            "stencilBufferSize:%d\n"
//                            "colorSpace:%d\n"
//                            "hasAlpha:%d\n"
//                            "majorVersion:%d\n"
//                            "minorVersion:%d\n"
//                            "options:%d\n"
//                            "profile:%d\n"
//                            "renderableType:%d\n"
//                            "samples:%d\n"
//                            "stereo:%d\n"
//                            "swapBehavior:%d\n"
//                            "swapInterval:%d\n",
//                    fmt.alphaBufferSize(), fmt.redBufferSize(), fmt.greenBufferSize(), fmt.blueBufferSize(), fmt.depthBufferSize(), fmt.stencilBufferSize(),
//                    fmt.colorSpace(), fmt.hasAlpha(), fmt.majorVersion(), fmt.minorVersion(), fmt.options(), fmt.profile(),
//                    fmt.renderableType(), fmt.samples(), fmt.stereo(), fmt.swapBehavior(), fmt.swapInterval() );
            m_surface->setFormat(mainSur->format());
            m_surface->create();
            shardContext->doneCurrent();

            m_context = new QOpenGLContext();
            m_context->setFormat(shardContext->format());
            m_context->setShareContext(shardContext);
            m_context->create();

            if ( !m_context->makeCurrent(m_surface) )
            {
                delete m_context;
                m_context = nullptr;
                return;
            }
            loadShaderPrograms();
            m_context->doneCurrent();

            m_context->moveToThread(dynamic_cast<QThread*>(this));

            shardContext->makeCurrent(mainSur);

        }
        else
        {

        }

        qDebug() <<"GL_VENDOR:" << QString::fromUtf8((const char*)glGetString(GL_VENDOR));
        qDebug() <<"GL_RENDERER:" << QString::fromUtf8((const char*)glGetString(GL_RENDERER));
        qDebug() <<"GL_VERSION:" << QString::fromUtf8((const char*)glGetString(GL_VERSION));
        //qDebug() <<"GL_EXTENSIONS:" << QString::fromUtf8((const char*)glGetString(GL_EXTENSIONS));

        m_videoSizeChanged = false;
        m_threadWorking = true;
        m_frameSync.init(m_vidParams.frameRate);
        m_frameSync.start();
        m_frameRate.start();
        start();
    }

}

void VideoSynthesizer::uninit()
{
    if (m_threadWorking)
    {
        m_threadWorking = false;
        wait();
    }
    m_audRecorder.stopRec();
    while( childLayerCount() )
    {
        delete childLayer(0);
    }
    ScreenSource::static_uninit();
    if (m_program)
    {
        delete m_program;
        m_program = nullptr;
    }
    if (m_prog_x264mb)
    {
        delete m_prog_x264mb;
        m_prog_x264mb = nullptr;
    }
    if (m_surface)
    {
        delete m_surface;
        m_surface = nullptr;
    }
    if (m_context)
    {
        delete m_context;
        m_context = nullptr;
    }
}

BaseLayer *VideoSynthesizer::createLayer(const QString &type)
{
    if (!m_threadWorking) return nullptr;
    BaseLayer* layer = nullptr;
    if (type == "screen")
    {
        layer = new ScreenLayer();
    }
    else if (type == "camera")
    {
        layer = dynamic_cast<BaseLayer*>(new CameraLayer());
    }
    else if (type == "picture")
    {
        layer = new PictureLayer();
    }
    if (layer)
    {
        layer->setParent(this);
        layer->setViewportSize(m_glViewportSize, m_pixViewportSize);
        layer->setShaderProgram(m_progPool.createProgram("base"));
    }

    return layer;
}

bool VideoSynthesizer::open(const QString &sourceName)
{
    if (!m_threadWorking) return false;
    Q_UNUSED(sourceName);
    if(m_status >= Opened)
    {
        return true;
    }
    if (sourceName.isEmpty())
    {
        return false;
    }
    fprintf(stderr, "VideoSynthesizer open:%s\n", sourceName.toUtf8().data() );
    QStringList files = sourceName.split('|', QString::SkipEmptyParts);
    int64_t tim = QDateTime::currentMSecsSinceEpoch() - QDateTime(QDate(1904,1,1)).toMSecsSinceEpoch();

    for (auto fn : files)
    {
        if (fn.startsWith("rtmp://", Qt::CaseInsensitive))
        {

        }
        else if (fn.endsWith(".flv", Qt::CaseInsensitive))
        {
            GueeMediaWriterFlv* flv = new GueeMediaWriterFlv(m_medStream);
            m_writers.append(flv);
            flv->setTimeFlg(tim, tim);
            flv->setFilePath(fn.toStdString());
        }
        else if (fn.endsWith(".mp4", Qt::CaseInsensitive))
        {
            GueeMediaWriterMp4* mp4 = new GueeMediaWriterMp4(m_medStream);
            m_writers.append(mp4);
            mp4->setTimeFlg(tim, tim);
            mp4->setFilePath(fn.toStdString());
        }
        else if (fn.endsWith(".ts", Qt::CaseInsensitive))
        {
            GueeMediaWriterTs* ts = new GueeMediaWriterTs(m_medStream);
            m_writers.append(ts);
            ts->setTimeFlg(tim, tim);
            ts->setFilePath(fn.toStdString());
        }
    }
    m_vidParams.enabled = true;
    m_medStream.setVideoParams(m_vidParams);
    m_audParams.useADTS = false;
    m_audParams.encLevel = 2;
    m_medStream.setAudioParams(m_audParams);
    fprintf(stderr, "m_medStream.startParse()\n");
    if (!m_medStream.startParse())
    {
        return false;
    }
    if (m_audParams.enabled)
    {
        fprintf(stderr, "m_audRecorder.startEncode\n");
        if (!m_audRecorder.startEncode(&m_audParams))
        {
            return false;
        }
    }
            fprintf(stderr, "m_vidEncoder.startEncode\n");
    if (!m_vidEncoder.startEncode(&m_vidParams))
    {
        m_medStream.endParse();
        return false;
    }
    m_status = Opened;
            fprintf(stderr, "VideoSynthesizer.exit\n");
            return true;
}

void VideoSynthesizer::close(close_step_progress fun, void* param)
{
    FrameTimestamp  ts;
    ts.start();
    m_status = NoOpen;
    m_audRecorder.endEncode();
    if (fun) fun(param);
    qDebug() << "m_audRecorder.endEncode:" << ts.elapsed();
    m_vidEncoder.endEncode(fun, param);
    qDebug() << "m_vidEncoder.endEncode:" << ts.elapsed();
    m_medStream.endParse();
    if (fun) fun(param);
    qDebug() << "m_medStream.endParse:" << ts.elapsed();
    m_timestamp.stop();
    qDebug() << "m_timestamp.stop:" << ts.elapsed();
    for (auto w:m_writers)
    {
        delete w;
    }
    m_writers.clear();
    qDebug() << "close done:" << ts.elapsed();
}

bool VideoSynthesizer::play()
{
    if(m_status == Palying) return true;
    if(m_status == Paused)
    {
        m_audRecorder.pauseEncode(false);
        m_timestamp.start();
        m_status = Palying;
        return true;
    }
    else if (m_status == Opened)
    {
        m_audRecorder.pauseEncode(false);
        m_timestamp.start();
        m_status = Palying;
        return true;
    }
    return false;
}

bool VideoSynthesizer::pause()
{
    if(m_status == Paused) return true;
    if (m_status == Palying || m_status == Opened)
    {
        m_audRecorder.pauseEncode(true);
        m_status = Paused;
        m_timestamp.pause();
        return true;
    }
    return false;
}

bool VideoSynthesizer::resetDefaultOption()
{
    m_vidParams.encoder       = VE_X264;
    m_vidParams.profile       = VF_High;
    m_vidParams.presetX264    = VP_x264_SuperFast;
    m_vidParams.presetNvenc   = VP_Nvenc_LowLatencyDefault;
    m_vidParams.outputCSP     = Vid_CSP_I420;
    m_vidParams.psyTune       = eTuneAnimation;
    m_vidParams.width         = 0;
    m_vidParams.height        = 0;
    m_vidParams.frameRate     = 30.0f;
    m_vidParams.vfr           = false;
    m_vidParams.onlineMode    = false;
    m_vidParams.annexb        = true;
    m_vidParams.threadNum     = 0;
    m_vidParams.optimizeStill = false;
    m_vidParams.fastDecode    = false;
    m_vidParams.useMbInfo     = true;
    m_vidParams.useMbInfo     = false;
    m_vidParams.rateMode      = VR_ConstantBitrate;
    m_vidParams.constantQP    = 23;
    m_vidParams.bitrate       = 2000;
    m_vidParams.bitrateMax    = 0;
    m_vidParams.bitrateMin    = 0;
    m_vidParams.vbvBuffer     = 0;
    m_vidParams.gopMin        = 30;
    m_vidParams.gopMax        = 300;
    m_vidParams.refFrames     = 4;
    m_vidParams.BFrames       = 0;
    m_vidParams.BFramePyramid = 0;

//    m_vidParams.BFrames       = 0;
//    m_vidParams.BFramePyramid = 0;

    m_backgroundColor = QVector4D(0.05f, 0.05f, 0.05f, 1.0f);
    setSize( 1920, 1080);
    setFrameRate(25.0f);

    m_audParams.enabled = true;
    m_audParams.eCodec = AC_AAC;
    m_audParams.isOnlineMode = true;
    m_audParams.useADTS = false;
    m_audParams.encLevel = 2;
    m_audParams.bitrate = 64;

    m_audParams.sampleBits = eSampleBit16i;
    m_audParams.sampleRate = 22050;
    m_audParams.channels = 2;
    m_audParams.channelMask = 0;



    return true;
}

bool VideoSynthesizer::setSize(int32_t width, int32_t height)
{
    if ( width < 16 || width > 5120 || height < 16 || height > 5120 )
        return false;
    if ( m_vidParams.width == width && m_vidParams.height == height)
    {
        return true;
    }
    qDebug() << "set size to:" << width << "x" << height;
    //if ()
    m_vidParams.width = width;
    m_vidParams.height = height;

    if ( width > height )
    {
        m_glViewportSize.setHeight(2.0);
        m_glViewportSize.setWidth(width * 2.0 / height);
    }
    else
    {
        m_glViewportSize.setHeight(height * 2.0 / width);
        m_glViewportSize.setWidth(2.0);
    }

    m_videoSizeChanged = true;
    return true;
}

bool VideoSynthesizer::setFrameRate(float fps)
{
    if ( fps <= 0.0f || fps > 200.0f )
        return false;
    m_vidParams.frameRate = fps;
    m_frameSync.init(fps);
    if (m_threadWorking)
        m_frameSync.start();
    setSourcesFramerate(fps);
    return true;
}

bool VideoSynthesizer::setProfile(EVideoProfile profile)
{
    m_vidParams.profile = profile;
    return true;
}

bool VideoSynthesizer::setPreset(EVideoPreset_x264 preset)
{
    m_vidParams.presetX264 = preset;
    return true;
}

bool VideoSynthesizer::setCSP(EVideoCSP csp)
{
    m_vidParams.outputCSP = csp;
    return true;
}

bool VideoSynthesizer::setPsyTune(EPsyTuneType psy)
{
    m_vidParams.psyTune = psy;
    return true;
}

bool VideoSynthesizer::setBitrateMode(EVideoRateMode rateMode)
{
    m_vidParams.rateMode = rateMode;
    return true;
}

bool VideoSynthesizer::setConstantQP(int32_t qp)
{
    m_vidParams.constantQP = qp;
    return true;
}

bool VideoSynthesizer::setBitrate(int32_t bitrate)
{
    m_vidParams.bitrate = bitrate;
    return true;
}

bool VideoSynthesizer::setBitrateMin(int32_t bitrateMin)
{
    m_vidParams.bitrateMin = bitrateMin;
    return true;
}

bool VideoSynthesizer::setBitrateMax(int32_t bitrateMax)
{
    m_vidParams.bitrateMax = bitrateMax;
    return true;
}

bool VideoSynthesizer::setVbvBuffer(int32_t vbvBufferSize)
{
    m_vidParams.vbvBuffer = vbvBufferSize;
    return true;
}

bool VideoSynthesizer::setGopMin(int32_t gopMin)
{
    m_vidParams.gopMin = gopMin;
    return true;
}

bool VideoSynthesizer::setGopMax(int32_t gopMax)
{
    m_vidParams.gopMax = gopMax;
    return true;
}

bool VideoSynthesizer::enableAudio(bool enable)
{
   // if (!m_threadWorking) return false;
    bool ret = false;
    m_audParams.enabled = enable;
    if (enable)
        ret = m_audRecorder.startRec(m_audParams.sampleRate, m_audParams.sampleBits, m_audParams.channels);
    else
        ret = m_audRecorder.stopRec();
    return ret;
}

bool VideoSynthesizer::setSampleBits(ESampleBits bits)
{
    bool ret = true;
    if (bits != eSampleBit16i && bits != eSampleBit32i && bits != eSampleBit32f)
    {
        return false;
    }
    if (bits != m_audParams.sampleBits)
    {
        m_audParams.sampleBits = bits;
        if (m_audParams.enabled && m_audRecorder.isOpened())
        {
            ret = enableAudio(true);
        }
    }
    return ret;
}

bool VideoSynthesizer::setSampleRate(int32_t rate)
{
    bool ret = true;
    if (rate != 11025 && rate != 22050 && rate != 44100)
    {
        return false;
    }
    if (rate != m_audParams.sampleRate)
    {
        m_audParams.sampleRate = rate;
        if (m_audParams.enabled && m_audRecorder.isOpened())
        {
            ret = enableAudio(true);
        }
    }
    return ret;
}

bool VideoSynthesizer::setChannels(int32_t channels)
{
    bool ret = true;
    if (channels != 1 && channels != 2)
    {
        return false;
    }
    if (channels != m_audParams.channels)
    {
        m_audParams.channels = channels;
        if (m_audParams.enabled && m_audRecorder.isOpened())
        {
            ret = enableAudio(true);
        }
    }
    return ret;
}

bool VideoSynthesizer::setAudioBitrate(int32_t bitrate)
{
    bool ret = true;
    bitrate = qMin(maxAudioBitrate(), bitrate);
    bitrate = qMax(minAudioBitrate(), bitrate);
    if (bitrate != m_audParams.bitrate)
    {
        m_audParams.bitrate = bitrate;
    }
    return ret;
}

int32_t VideoSynthesizer::maxAudioBitrate() const
{
    return static_cast<int32_t>((6144.0 * double(m_audParams.sampleRate) / 1024.5) / 1024);
}

int32_t VideoSynthesizer::minAudioBitrate() const
{
    return 8 * m_audParams.channels;
}

void VideoSynthesizer::enablePreview(bool enabled)
{
    m_enablePreview = enabled;
}

bool VideoSynthesizer::setRefFrames(int refFrames)
{
    if (refFrames < 0 || refFrames > 8)
        return false;
    m_vidParams.refFrames = refFrames;
    return true;
}

bool VideoSynthesizer::setBFrames(int bFrames)
{
    if (bFrames < 0 || bFrames > 8)
        return false;
    m_vidParams.BFrames = bFrames;
    return true;
}

void VideoSynthesizer::renderThread()
{
    bool                    isUpdated = false;

    if ( !m_context->makeCurrent(m_surface) )
    {
        return;
    }
    QOpenGLFramebufferObjectFormat fboFmt;
    fboFmt.setMipmap(false);
    fboFmt.setInternalTextureFormat(GL_RGB);    //VMWare + UOS 中，GL_RGBA8 和 GL_RGBA 的FBO纹理共享是全黑的，可能是虚拟机的问题，这里使用 GL_RGB 就可以了。
    fboFmt.setTextureTarget(GL_TEXTURE_2D);
    m_pixViewportSize.setWidth(m_vidParams.width);
    m_pixViewportSize.setHeight(m_vidParams.height);

    auto create_fbo = [&fboFmt](int width, int height)->QOpenGLFramebufferObject* {
        QOpenGLFramebufferObject* fboRgba = new QOpenGLFramebufferObject(width, height, fboFmt);
        glBindTexture(GL_TEXTURE_2D, fboRgba->texture());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        return fboRgba;
    };

    QOpenGLFramebufferObject* fboRgba1 = create_fbo(m_vidParams.width, m_vidParams.height);
    QOpenGLFramebufferObject* fboRgba2 = m_vidParams.useMbInfo ? create_fbo(m_vidParams.width, m_vidParams.height) : nullptr;


    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    emit initDone(true);
    int64_t preTimer = -1000000;
    int64_t curTimer = 0;
    int64_t preTimeStamp = 0;

    while(m_threadWorking)
    {
        if ( m_videoSizeChanged &&
             (m_pixViewportSize.width() != m_vidParams.width || m_pixViewportSize.height() != m_vidParams.height ))
        {
            delete fboRgba1;
            if (fboRgba2 ) delete fboRgba2;
            fboRgba1 = create_fbo(m_vidParams.width, m_vidParams.height);
            fboRgba2 = m_vidParams.useMbInfo ? create_fbo(m_vidParams.width, m_vidParams.height) : nullptr;
            m_pixViewportSize.setWidth(m_vidParams.width);
            m_pixViewportSize.setHeight(m_vidParams.height);
            setViewportSize(m_glViewportSize, m_pixViewportSize, true);

            m_videoSizeChanged = false;
            emit frameReady(0);
        }
        curTimer = m_frameSync.isNextFrame();
        if (curTimer >= 0)
        {
            m_frameData.timestamp = qMax(int64_t(0), m_timestamp.elapsed());
            if ( updateSourceTextures(curTimer) )
            {
                isUpdated = true;
            }
            if ( curTimer - preTimer > 1000000 ) isUpdated = true;
        }

        if ( isUpdated || m_immediateUpdate )
        {
            if (isUpdated) preTimer = curTimer;

            m_immediateUpdate = false;
            QOpenGLFramebufferObject* fboRgba = fboRgba1;
            fboRgba->bind();
            glViewport(0, 0, m_vidParams.width, m_vidParams.height);
            glClearColor(m_backgroundColor.x(), m_backgroundColor.y(), m_backgroundColor.z(), m_backgroundColor.w());
            //glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            auto &cds = lockChilds();
            for ( auto it = cds.rbegin(); it!= cds.rend(); ++it)
            {
                (*it)->draw();
            }
            unlockChilds();
            glFlush();
            if (isUpdated)
            {
                if ( m_status  == Palying )
                {
                    if ( m_frameData.fbo == nullptr )
                    {
                        if (!initYuvFbo())
                            break;
                        preTimeStamp = 0;
                        if (fboRgba2)
                        {
                            fboRgba2->bind();
                            glViewport(0, 0, m_vidParams.width, m_vidParams.height);
                            glClearColor(0,0,0,0);
                            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                            glFlush();
                        }
                    }

                    if (m_vidParams.useMbInfo)
                    {
//                        QString fn1 = QString("/home/guee/Pictures/YUV/%1-1.jpg").arg(m_frameData.timestamp);
//                        QString fn2 = QString("/home/guee/Pictures/YUV/%1-2.jpg").arg(m_frameData.timestamp);
//                        fboRgba1->toImage().save(fn1);
//                        fboRgba2->toImage().save(fn2);

                        putFrameToEncoder(fboRgba1, fboRgba2);
                        fboRgba1 = fboRgba2;
                        fboRgba2 = fboRgba;
                    }
                    else
                    {
                        putFrameToEncoder(fboRgba1, nullptr);
                    }
                    //qDebug() <<"帧时间：" << m_frameData.timestamp << " 距离上帧：" << m_frameData.timestamp - preTimeStamp;
                    //fprintf(stderr, "帧时间：%d 距离上帧：%d\n", int(m_frameData.timestamp), int(m_frameData.timestamp - preTimeStamp) );
                    //preTimeStamp = m_frameData.timestamp;

                }
                else if ( m_status < Opened && m_frameData.fbo )
                {
                    uninitYubFbo();
                }
                m_frameRate.add();
                isUpdated = false;
            }
            if (m_enablePreview)
            {
                emit frameReady(fboRgba->texture());
            }
        }
        msleep(1);
    }

    delete fboRgba1;
    delete fboRgba2;
    fboRgba1 = nullptr;
    fboRgba2 = nullptr;
    uninitYubFbo();
}

void VideoSynthesizer::run()
{
    renderThread();
}

void VideoSynthesizer::loadShaderPrograms()
{
    m_progPool.addShaderFromSourceFile("base", QOpenGLShader::Vertex, ":/Shaders/base.vert" );
    m_progPool.addShaderFromSourceFile("base", QOpenGLShader::Fragment, ":/Shaders/base.frag" );

    m_progPool.addShaderFromSourceFile("SelectScreen", QOpenGLShader::Vertex, ":/Shaders/SelectScreen.vert" );
    m_progPool.addShaderFromSourceFile("SelectScreen", QOpenGLShader::Fragment, ":/Shaders/SelectScreen.frag" );

    m_progPool.addShaderFromSourceFile("RgbToYuv", QOpenGLShader::Vertex, ":/Shaders/RgbToYuv.vert" );
    m_progPool.addShaderFromSourceFile("RgbToYuv", QOpenGLShader::Fragment, ":/Shaders/RgbToYuv.frag" );


    m_progPool.addShaderFromSourceFile("x264-mb", QOpenGLShader::Vertex, ":/Shaders/x264-mb.vert" );
    m_progPool.addShaderFromSourceFile("x264-mb", QOpenGLShader::Fragment, ":/Shaders/x264-mb.frag" );

    QVector<QPair<QString, QOpenGLShader::ShaderType>> s;
    s.push_back(QPair<QString, QOpenGLShader::ShaderType>("base", QOpenGLShader::Vertex));
    s.push_back(QPair<QString, QOpenGLShader::ShaderType>("base", QOpenGLShader::Fragment));
    m_progPool.setProgramShaders("base", s);
    s.clear();

    s.push_back(QPair<QString, QOpenGLShader::ShaderType>("SelectScreen", QOpenGLShader::Vertex));
    s.push_back(QPair<QString, QOpenGLShader::ShaderType>("SelectScreen", QOpenGLShader::Fragment));
    m_progPool.setProgramShaders("SelectScreen", s);
    s.clear();

    s.push_back(QPair<QString, QOpenGLShader::ShaderType>("RgbToYuv", QOpenGLShader::Vertex));
    s.push_back(QPair<QString, QOpenGLShader::ShaderType>("RgbToYuv", QOpenGLShader::Fragment));
    m_progPool.setProgramShaders("RgbToYuv", s);
    s.clear();

    s.push_back(QPair<QString, QOpenGLShader::ShaderType>("x264-mb", QOpenGLShader::Vertex));
    s.push_back(QPair<QString, QOpenGLShader::ShaderType>("x264-mb", QOpenGLShader::Fragment));
    m_progPool.setProgramShaders("x264-mb", s);
    s.clear();

    m_program = m_progPool.createProgram("RgbToYuv");
    m_prog_x264mb = m_progPool.createProgram("x264-mb");
}

bool VideoSynthesizer::initYuvFbo()
{

    switch(m_vidParams.outputCSP)
    {
    case Vid_CSP_I420:
    case Vid_CSP_YV12:
        m_frameData.alignWidth = ( m_vidParams.width + 1 ) / 2 * 2;
        m_frameData.alignHeight = ( m_vidParams.height + 3 ) / 4 * 4;
        m_frameData.textureWidth = ( m_vidParams.width + 3 ) / 4 * 4;
        m_frameData.textureHeight = m_frameData.alignHeight + m_frameData.alignHeight / 2;
        m_frameData.internalFormat = GL_LUMINANCE;
        m_frameData.dateType = GL_UNSIGNED_BYTE;
        m_frameData.planeCount = 3;
        m_frameData.stride[0] = m_frameData.textureWidth;
        m_frameData.stride[1] = m_frameData.textureWidth / 2;
        m_frameData.stride[2] = m_frameData.textureWidth / 2;
        m_frameData.byteNum[0] = m_frameData.stride[0] * m_frameData.alignHeight;
        m_frameData.byteNum[1] = m_frameData.stride[1] * m_frameData.alignHeight / 2;
        m_frameData.byteNum[2] = m_frameData.stride[2] * m_frameData.alignHeight / 2;
        break;
    case Vid_CSP_NV12:
    case Vid_CSP_NV21:
        m_frameData.alignWidth = ( m_vidParams.width + 1 ) / 2 * 2;
        m_frameData.alignHeight = ( m_vidParams.height + 1 ) / 2 * 2;
        m_frameData.textureWidth = ( m_vidParams.width + 3 ) / 4 * 4;
        m_frameData.textureHeight = m_frameData.alignHeight + m_frameData.alignHeight / 2;
        m_frameData.internalFormat = GL_LUMINANCE;
        m_frameData.dateType = GL_UNSIGNED_BYTE;
        m_frameData.planeCount = 2;
        m_frameData.stride[0] = m_frameData.textureWidth;
        m_frameData.stride[1] = m_frameData.textureWidth;
        m_frameData.stride[2] = 0;
        m_frameData.byteNum[0] = m_frameData.stride[0] * m_frameData.alignHeight;
        m_frameData.byteNum[1] = m_frameData.stride[1] * m_frameData.alignHeight / 2;
        m_frameData.byteNum[2] = 0;
        break;
    case Vid_CSP_I422:
    case Vid_CSP_YV16:
        m_frameData.alignWidth = ( m_vidParams.width + 1 ) / 2 * 2;
        m_frameData.alignHeight = ( m_vidParams.height + 1 ) / 2 * 2;
        m_frameData.textureWidth = ( m_vidParams.width + 3 ) / 4 * 4;
        m_frameData.textureHeight = m_frameData.alignHeight * 2;
        m_frameData.internalFormat = GL_LUMINANCE;
        m_frameData.dateType = GL_UNSIGNED_BYTE;
        m_frameData.planeCount = 3;
        m_frameData.stride[0] = m_frameData.textureWidth;
        m_frameData.stride[1] = m_frameData.textureWidth / 2;
        m_frameData.stride[2] = m_frameData.textureWidth / 2;
        m_frameData.byteNum[0] = m_frameData.stride[0] * m_frameData.alignHeight;
        m_frameData.byteNum[1] = m_frameData.stride[1] * m_frameData.alignHeight;
        m_frameData.byteNum[2] = m_frameData.stride[2] * m_frameData.alignHeight;
        break;
    case Vid_CSP_NV16:
        m_frameData.alignWidth = ( m_vidParams.width + 1 ) / 2 * 2;
        m_frameData.alignHeight = m_vidParams.height;
        m_frameData.textureWidth = ( m_vidParams.width + 3 ) / 4 * 4;
        m_frameData.textureHeight = m_frameData.alignHeight * 2;
        m_frameData.internalFormat = GL_LUMINANCE;
        m_frameData.dateType = GL_UNSIGNED_BYTE;
        m_frameData.planeCount = 2;
        m_frameData.stride[0] = m_frameData.textureWidth;
        m_frameData.stride[1] = m_frameData.textureWidth;
        m_frameData.stride[2] = 0;
        m_frameData.byteNum[0] = m_frameData.stride[0] * m_frameData.alignHeight;
        m_frameData.byteNum[1] = m_frameData.stride[1] * m_frameData.alignHeight;
        m_frameData.byteNum[2] = 0;
        break;
    case Vid_CSP_YUY2:
    case Vid_CSP_UYVY:
        m_frameData.alignWidth = ( m_vidParams.width + 1 ) / 2 * 2;
        m_frameData.alignHeight = m_vidParams.height;
        m_frameData.textureWidth = m_frameData.alignWidth;
        m_frameData.textureHeight = m_frameData.alignHeight;
        m_frameData.internalFormat = GL_LUMINANCE_ALPHA;
        m_frameData.dateType = GL_UNSIGNED_BYTE;
        m_frameData.planeCount = 1;
        m_frameData.stride[0] = m_frameData.textureWidth * 2;
        m_frameData.stride[1] = 0;
        m_frameData.stride[2] = 0;
        m_frameData.byteNum[0] = m_frameData.stride[0] * m_frameData.alignHeight;
        m_frameData.byteNum[1] = 0;
        m_frameData.byteNum[2] = 0;
        break;
    case Vid_CSP_I444:
        m_frameData.alignWidth = m_vidParams.width;
        m_frameData.alignHeight = m_vidParams.height;
        m_frameData.textureWidth = ( m_vidParams.width + 3 ) / 4 * 4;
        m_frameData.textureHeight = m_frameData.alignHeight * 3;
        m_frameData.internalFormat = GL_LUMINANCE;
        m_frameData.dateType = GL_UNSIGNED_BYTE;
        m_frameData.planeCount = 3;
        m_frameData.stride[0] = m_frameData.textureWidth;
        m_frameData.stride[1] = m_frameData.textureWidth;
        m_frameData.stride[2] = m_frameData.textureWidth;
        m_frameData.byteNum[0] = m_frameData.stride[0] * m_frameData.alignHeight;
        m_frameData.byteNum[1] = m_frameData.stride[1] * m_frameData.alignHeight;
        m_frameData.byteNum[2] = m_frameData.stride[2] * m_frameData.alignHeight;
        break;
    default:
        return false;
    }
    uninitYubFbo();
    m_frameData.csp = m_vidParams.outputCSP;
    QOpenGLFramebufferObjectFormat fboFmt;
    fboFmt.setMipmap(false);
    fboFmt.setInternalTextureFormat(m_frameData.internalFormat);
    fboFmt.setTextureTarget(GL_TEXTURE_2D);
    m_frameData.fbo = new QOpenGLFramebufferObject(m_frameData.textureWidth, m_frameData.textureHeight, fboFmt);
    m_frameData.buffer = new QOpenGLBuffer(QOpenGLBuffer::PixelPackBuffer);
    if ( !m_frameData.buffer->create() )
    {
        uninitYubFbo();
        return false;
    }

    m_frameData.buffer->bind();
    m_frameData.buffer->setUsagePattern(QOpenGLBuffer::StreamRead);
    m_frameData.buffer->allocate(m_frameData.byteNum[0] + m_frameData.byteNum[1] + m_frameData.byteNum[2]);
    m_frameData.buffer->release();
    m_vbo = new QOpenGLBuffer();
    if ( !m_vbo->create() )
    {
        uninitYubFbo();
        return false;
    }

    m_vbo->bind();
    m_vbo->allocate(&m_vertex, 5 * 4 * sizeof(GLfloat));

    m_program->bind();
    QMatrix4x4 m;
    m.ortho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    m_program->setUniformValue("qt_ModelViewProjectionMatrix",m);
    m_program->setUniformValue("qt_Texture0", 0);
    m_program->setUniformValue("PlaneType", m_frameData.csp);
    m_program->setUniformValue("surfaceSize", m_frameData.textureWidth, m_frameData.textureHeight);
    m_program->setUniformValue("alignSize", m_frameData.alignWidth, m_frameData.alignHeight);
    m_program->setUniformValue("imageSize", m_vidParams.width, m_vidParams.height);
    m_program->release();
    m_vbo->release();

    if (m_vidParams.useMbInfo)
    {
        m_frameData.mbWidth = (m_vidParams.width + 15) / 16;
        m_frameData.mbHeight = (m_vidParams.height + 15) / 16;
        fboFmt.setInternalTextureFormat(GL_LUMINANCE);
        qDebug() << "MB_INFO Width:" << m_frameData.mbWidth << ", Height:" << m_frameData.mbHeight;
        m_frameData.fbo_mb = new QOpenGLFramebufferObject(m_frameData.mbWidth, m_frameData.mbHeight, fboFmt);
        m_frameData.buf_mb = new uint8_t[m_frameData.mbWidth * m_frameData.mbHeight];

        m_prog_x264mb->bind();
        m_prog_x264mb->setUniformValue("qt_ModelViewProjectionMatrix",m);
        m_prog_x264mb->setUniformValue("qt_Texture0", 0);
        m_prog_x264mb->setUniformValue("qt_Texture1", 1);
        m_prog_x264mb->setUniformValue("imageSize", m_vidParams.width, m_vidParams.height);
        m_prog_x264mb->setUniformValue("mbSize", m_frameData.mbWidth, m_frameData.mbHeight);
        m_prog_x264mb->release();

        //m_imgTemp = QImage(m_vidParams.width, m_vidParams.height, QImage::Format_ARGB32);
    }
    return true;
}

void VideoSynthesizer::uninitYubFbo()
{
    if ( m_frameData.fbo_mb )
    {
        delete m_frameData.fbo_mb;
        m_frameData.fbo_mb = nullptr;
    }
    if ( m_frameData.buf_mb )
    {
        delete []m_frameData.buf_mb;
        m_frameData.buf_mb = nullptr;
    }
    if ( m_frameData.buffer )
    {
        delete m_frameData.buffer;
        m_frameData.buffer = nullptr;
    }
    if (m_frameData.fbo)
    {
        delete m_frameData.fbo;
        m_frameData.fbo = nullptr;
    }
    if (m_vbo)
    {
        delete m_vbo;
        m_vbo = nullptr;
    }
}

void VideoSynthesizer::putFrameToEncoder(QOpenGLFramebufferObject* fboCur, QOpenGLFramebufferObject* fboPrev)
{
    if ( m_status < Palying || m_frameData.buffer == nullptr )
    {
        return;
    }
//    static int64_t t0 = 0;
//    static int64_t c0 = 0;
//    FrameTimestamp ttt;
    if (fboPrev)
    {
//        m_frameData.fbo_mb->bind();
//        m_prog_x264mb->bind();
//        glViewport(0, 0, m_frameData.mbWidth, m_frameData.mbHeight);
//        glActiveTexture(GL_TEXTURE0);
//        glBindTexture(GL_TEXTURE_2D, fboCur->texture());
//        glActiveTexture(GL_TEXTURE1);
//        glBindTexture(GL_TEXTURE_2D, fboPrev->texture());
//        m_vbo->bind();
//        m_prog_x264mb->enableAttributeArray(0);
//        m_prog_x264mb->enableAttributeArray(1);
//        m_prog_x264mb->setAttributeBuffer(0, GL_FLOAT, 0, 3, 5 * sizeof(GLfloat));
//        m_prog_x264mb->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));

//        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
//        glReadBuffer(GL_FRONT);
//        glFlush();
//        glReadPixels(0, 0, m_frameData.mbWidth, m_frameData.mbHeight,
//                     GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frameData.buf_mb);
//        m_vbo->release();
//        m_prog_x264mb->release();
//        m_frameData.fbo_mb->release();
//        glActiveTexture(GL_TEXTURE0);



//        QImage img1 = fboPrev->toImage().convertToFormat(QImage::Format_ARGB32);

//        QImage img2 = fboCur->toImage().convertToFormat(QImage::Format_ARGB32);

//        for (int y = 0; y < m_frameData.mbHeight; ++y )
//        {
//            int h = qMin(m_vidParams.height - y * 16, 16);
//            for (int x = 0; x < m_frameData.mbWidth; ++x)
//            {
//                bool isSame = true;
//                int w = qMin(m_vidParams.width - x * 16, 16);
//                for ( int dy = 0; dy < h; ++dy)
//                {
//                    if ( memcmp( img1.scanLine(y * 16 + dy) + x * 16 * 4,  img2.scanLine(y * 16 + dy) + x * 16 * 4, w * 4) != 0 )
//                    {
//                        isSame = false;
//                        break;
//                    }
//                }

//                if (!isSame && m_frameData.buf_mb[y*m_frameData.mbWidth + x])
//                {
//                    qDebug() << "Frame:" << m_frameData.timestamp << " Block x:" << x << ", y:" << y << "  Fail!";
//                    m_frameData.buf_mb[y*m_frameData.mbWidth + x]  = 192;
//                }
//                else if (isSame && !m_frameData.buf_mb[y*m_frameData.mbWidth + x])
//                {
//                    m_frameData.buf_mb[y*m_frameData.mbWidth + x]  = 64;
//                }
//            }
//        }
//        QString fnp = QString("/home/guee/Pictures/YUV/%1-Pic.jpg").arg(m_frameData.timestamp);
//        img1.save(fnp);

//        QImage img((const uchar*)m_frameData.buf_mb, m_frameData.mbWidth, m_frameData.mbHeight, m_frameData.mbWidth, QImage::Format_Grayscale8);
//        QString fn = QString("/home/guee/Pictures/YUV/%1.png").arg(m_frameData.timestamp);
//        img.save(fn, "png");
    }
    uint8_t* offset = nullptr;
    m_frameData.fbo->bind();

    glViewport(0, 0, m_frameData.fbo->width(), m_frameData.fbo->height());
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_program->bind();
    m_vbo->bind();
    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, 5 * sizeof(GLfloat));
    m_program->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));
    glBindTexture(GL_TEXTURE_2D, fboCur->texture());
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
//ttt.start();
    m_frameData.buffer->bind();
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, m_frameData.fbo->width(), m_frameData.fbo->height(),
                 m_frameData.internalFormat, m_frameData.dateType, offset);
    uint8_t* dat = static_cast<uint8_t*>(m_frameData.buffer->map(QOpenGLBuffer::ReadOnly));
    if (dat)
    {
//        QImage img((const uchar*)dat, m_frameData.textureWidth, m_frameData.textureHeight, m_frameData.stride[0], QImage::Format_Grayscale8);
//        QString fn = QString("/home/guee/Pictures/YUV/%1.png").arg(m_frameData.timestamp);
//        img.save(fn, "png");
        uint8_t* plane[3] = {dat, dat + m_frameData.byteNum[0], dat + m_frameData.byteNum[0] + m_frameData.byteNum[1]};
        m_vidEncoder.putFrame(m_frameData.timestamp, plane, m_frameData.stride, m_frameData.buf_mb);
        //m_vidEncoder.putFrame(m_frameData.timestamp, reinterpret_cast<uint8_t*>(dat), m_frameData.stride[0]);
        m_frameData.buffer->unmap();
    }
//t0 += ttt.elapsed(); ++c0; ttt.stop();
//qDebug() << "T0:" << t0 / c0;
    m_frameData.buffer->release();
    m_vbo->release();
    m_program->release();
    m_frameData.fbo->release();
}

void VideoSynthesizer::onLayerOpened(BaseLayer *layer)
{
    m_immediateUpdate = true;
    emit layerAdded(layer);
}

void VideoSynthesizer::onLayerRemoved(BaseLayer *layer)
{
    BaseLayer::onLayerRemoved(layer);
    m_immediateUpdate = true;
    emit layerRemoved(layer);
}

void VideoSynthesizer::onSizeChanged(BaseLayer *layer)
{
    BaseLayer::onSizeChanged(layer);
    m_immediateUpdate = true;
    emit layerMoved(layer);
}


