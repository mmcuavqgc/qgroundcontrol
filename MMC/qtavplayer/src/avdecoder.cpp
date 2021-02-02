#include "avdecoder.h"
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>

#include "VideoBuffer.h"
#include "painter/GlPainter.h"

#define HW true

//输出的地址
AVFormatContext *outputContext = nullptr;
AVFormatContext *inputContext = nullptr;

static AVDecoder* avDecoder = NULL;
AVDecoder *getAVDecoder()
{
    if(!avDecoder)
        avDecoder = new AVDecoder();
    return avDecoder;
}

AVPixelFormat AVDecoder::get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{

    AVDecoder *opaque = (AVDecoder *) ctx->opaque;
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == opaque->mHWPixFormat)
        {
            qDebug() << (*p) << ";" << opaque->mHWPixFormat;
            qDebug() << "Successed to get HW surface format.";
            return *p;
        }
    }
    qDebug() << "Failed to get HW surface format.";
    return AV_PIX_FMT_NONE;
}

static int lockmgr(void **mtx, enum AVLockOp op)
{
    switch(op) {
    case AV_LOCK_CREATE:{
        QMutex *mutex = new QMutex;
        *mtx = mutex;
        return 0;
    }
    case AV_LOCK_OBTAIN:{
        QMutex *mutex = (QMutex*)*mtx;
        mutex->lock();
        return 0;
    }
    case AV_LOCK_RELEASE:{
        QMutex *mutex = (QMutex*)*mtx;
        mutex->unlock();
        return 0;
    }
    case AV_LOCK_DESTROY:{
        QMutex *mutex = (QMutex*)*mtx;
        delete mutex;
        return 0;
    }
    }
    return 1;
}

AVCodecTask::AVCodecTask(AVDecoder *codec, AVCodecTask::AVCodecTaskCommand command, double param, QString param2):
    mCodec(codec),command(command),param(param),param2(param2){}

void AVCodecTask::run(){
    switch(command){
    case AVCodecTaskCommand_Init:
        mCodec->init2();
        break;
    case AVCodecTaskCommand_SetPlayRate: //播放速率
        break;
    case AVCodecTaskCommand_Seek:        //跳转
        break;
    case AVCodecTaskCommand_GetPacket:     //获取数据
        mCodec->getPacket();
        break;
    case AVCodecTaskCommand_SetFileName: //设置播放对象 -- url
        mCodec->setFilenameImpl(param2);
        break;
    case AVCodecTaskCommand_DecodeToRender : //+ 渲染成帧
        mCodec->decodec();
        break;
    case AVCodecTaskCommand_SetDecodecMode :
        break;
    case AVCodecTaskCommand_ShowFrameByPosition :   //按位置显示帧
        break;
    case AVCodecTaskCommand_RePlay :   //重新加载
        mCodec->_rePlay();
        break;

    case AVCoTaskCommand_Render :   //播放
        mCodec->requestRender();
        break;

    default:
        break;
    }
}

AVDecoder::AVDecoder(QObject *parent) : QObject(parent)
  , videoq(new PacketQueue)
  , renderq(new FrameQueue)
  , _bisRelease(false)
{
#ifdef LIBAVUTIL_VERSION_MAJOR
#if (LIBAVUTIL_VERSION_MAJOR < 56)
    avcodec_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();
#endif
#endif

    avcodec_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();

    av_log_set_callback(NULL);//不打印日志
    av_lockmgr_register(lockmgr);
    outUrl =  "";
    qDebug() << "---rtmp output:" << outUrl;

    _usbfifo = new usbfifo();
    _hidUSB = getUsbExample();

    videoq->release();
    renderq->release();

    wakeupPlayer(); //启动显示线程
}

AVDecoder::~AVDecoder()
{
    QMutexLocker locker(&mDeleteMutex);
    _bisRelease = true;
    QThread::msleep(50);

    _fpsTimer->stop();

    if(mProcessThread.isRunning()) {
        mProcessThread.stop();
    }

    if(mDecodeThread.isRunning()) {
        mDecodeThread.stop();
    }

    if(mPlayeThread.isRunning()) {
        mPlayeThread.stop();
    }

    int i = 0;
    while(!mProcessThread.isRunning() && i++ < 200){
        QThread::msleep(1);
    }
    i = 0;
    while(!mDecodeThread.isRunning() && i++ < 200){
        QThread::msleep(1);
    }
    i = 0;
    while(!mPlayeThread.isRunning() && i++ < 200){
        QThread::msleep(1);
    }

    release(true);
    if(videoq){
        videoq->release();
        delete videoq;
        videoq =nullptr;
    }

    if(renderq){
        renderq->release();
        delete renderq;
        renderq =nullptr;
    }
}

void AVDecoder::deleteMediaCallback(AVMediaCallback *media)
{
    if (media != nullptr && mCallbackSet.contains(media)){
        mCallbackSet.remove(media);
    }
}

int AVDecoder::getCountMediaCallback()
{
    int num = mCallbackSet.count();
    return num;
}

void AVDecoder::addMediaCallback(AVMediaCallback *media)
{
    if(media != nullptr && !mCallbackSet.contains(media)){
        mCallbackSet.insert(media);
    }
}

/* ---------------------------线程任务操作-------------------------------------- */
void AVDecoder::load(){
    if(!_bisRelease) {
        qDebug() << "enter the load";
        mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_Init));
    }
}

void AVDecoder::setFilename(const QString &source){
    if(!_bisRelease) {
        mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_SetFileName,0,source));
    }
}

void AVDecoder::rePlay()
{
    if(!_bisRelease) {
        mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_RePlay));
    }
}

void AVDecoder::wakeupPlayer()
{
    if(!_bisRelease) {
       mPlayeThread.addTask(new AVCodecTask(this, AVCodecTask::AVCoTaskCommand_Render));
    }
}

void AVDecoder::saveTs(bool status)
{
    static bool cstatus = !status;
    if(cstatus == status){
        return;
    }else{
        cstatus = status;
    }
    if(status){
        if(tsSave == nullptr){
            QString dir = QString("%1").arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)) + "/MMC Station/Media/Video/" + QDateTime::currentDateTime().toString("yyyyMMdd") + "/";
            QDir tmpDir1;
            qDebug() << "---------------------dir" << dir;
            bool ok = false;
            if(tmpDir1.exists(dir)){
                ok = true;
            }else{
                ok = tmpDir1.mkpath(dir);
            }
            if(ok){
                QString fileName = QDateTime::currentDateTime().toString("hh_mm_ss");
                tsSave = fopen(QString("%1%2%3").arg(dir).arg(fileName).arg(".ts").toStdString().data(), "wb");
            }
        }
    }else{
        if(tsSave){
            fclose(tsSave);
            tsSave = nullptr;
        }
    }
}

void AVDecoder::saveImage()
{
    _isSaveImage = true;
}

void AVDecoder::getPacketTask(int type)
{
    Q_UNUSED(type)
    if(!_bisRelease) {
        mProcessThread.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_GetPacket));
    }
}

void AVDecoder::decodecTask(int type)
{
    Q_UNUSED(type)
    if(!_bisRelease) {
        mDecodeThread /*mProcessThread*/.addTask(new AVCodecTask(this,AVCodecTask::AVCodecTaskCommand_DecodeToRender));
    }
}

#if defined(Q_OS_WIN32)
static int hw_decoder_init(AVCodecContext *ctx, AVBufferRef *hw_device_ctx, const AVCodecHWConfig* config)
{
    int err = 0;
    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, config->device_type,
                                      NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    av_buffer_unref(&hw_device_ctx);
    hw_device_ctx = nullptr;
    ctx->pix_fmt = config->pix_fmt;
    return err;
}
#endif



void AVDecoder::init(){
    return;

    QMutexLocker locker(&mDeleteMutex);
    if(mIsInit)
        return;
    mIsInit = true;
    statusChanged(AVDefine::AVMediaStatus_Loading);

    AVDictionary* options = NULL;

    av_dict_set(&options, "preset", "ultrafast ", 0); // av_opt_set(pCodecCtx->priv_data,"preset","fast",0);
    av_dict_set(&options, "tune", "zerolatency", 0);
    av_dict_set(&options, "rtsp_transport", "udp", 0);  //以udp方式打开，如果以tcp方式打开将udp替换为tcp

    //寻找视频
    mFormatCtx = avformat_alloc_context();
    mFormatCtx->interrupt_callback.opaque = this;
    _pushRtmpFlag = false;
    if(avformat_open_input(&mFormatCtx, mFilename.toStdString().c_str(), NULL, &options) != 0) //打开视频
    {
        qDebug() << "media open error : " << mFilename.toStdString().data();
        statusChanged(AVDefine::AVMediaStatus_NoMedia);
        mIsInit = false;
        QThread::msleep(10);
        rePlay();
        return;
    }
    mIsInit = false;

    //    mFormatCtx->probesize = 50000000;  //
    //    qDebug() << "------------mFormatCtx->probesize  mFormatCtx->max_analyze_duration2"<< mFormatCtx->probesize << mFormatCtx->max_analyze_duration;
    if(avformat_find_stream_info(mFormatCtx, NULL) < 0) //判断是否找到视频流信息
    {
        qDebug() << "media find stream error : " << mFilename.toStdString().data();
        statusChanged(AVDefine::AVMediaStatus_InvalidMedia);

        QThread::msleep(10);
        rePlay();
        return;
    }

    AVCodecParameters *pCodecPar = mFormatCtx->streams[AVMEDIA_TYPE_VIDEO]->codecpar; //查找解码器
    //获取一个合适的编码器pCodec find a decoder for the video stream
    // AVCodec *pCodec = avcodec_find_decoder(pCodecPar->codec_id);
    AVCodec *pCodec;
    switch (pCodecPar->codec_id){
    case AV_CODEC_ID_H264:
        pCodec = avcodec_find_decoder_by_name("h264_mediacodec");//硬解码264
        if (pCodec == NULL) {

            qDebug("Couldn't find Codec.\n");
            //            return -1;
        }
        break;
    case AV_CODEC_ID_MPEG4:
        pCodec = avcodec_find_decoder_by_name("mpeg4_mediacodec");//硬解码mpeg4
        if (pCodec == NULL) {

            qDebug("Couldn't find Codec.\n");
            //            return -1;
        } break;
    case AV_CODEC_ID_HEVC:
        pCodec = avcodec_find_decoder_by_name("hevc_mediacodec");//硬解码265
        if (pCodec == NULL) {
            qDebug("Couldn't find Codec.\n");
            //            return -1;
        }
        break;
    default:
        pCodec = avcodec_find_decoder(pCodecPar->codec_id);//软解
        if (pCodec == NULL) {
            qDebug("Couldn't find Codec.\n");
            //            return -1;
        }
        break;
    }

    /* 寻找视频流 */
    int ret = av_find_best_stream(mFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &mVideoCodec, 0); //查找视频流信息;
    if (ret < 0) {
        qDebug() << "Cannot find a video stream in the input file";
    }else{
        mVideoIndex = ret;
        //fps
        int fps_num = mFormatCtx->streams[mVideoIndex]->r_frame_rate.num;
        int fps_den = mFormatCtx->streams[mVideoIndex]->r_frame_rate.den;
        if (fps_den > 0) {
            _fps = fps_num / fps_den;

            foreach (AVMediaCallback* cCallback, mCallbackSet){
                QMutexLocker locker(&mCallbackMutex);
                if(cCallback != nullptr)
                   cCallback->mediaUpdateFps(_fps);
            }
        }

        if (!(mVideoCodecCtx = avcodec_alloc_context3(mVideoCodec))){
            QThread::msleep(10);
            rePlay();
            return;
        }
        mVideo = mFormatCtx->streams[mVideoIndex];
        if(mVideoCodecCtx) {
            mVideoCodecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
        }
        if (!mVideoCodecCtx){
            qDebug() << "create video context fail!" << mFormatCtx->filename;
            statusChanged(AVDefine::AVMediaStatus_InvalidMedia);
        }else{
            if (avcodec_parameters_to_context(mVideoCodecCtx, mVideo->codecpar) < 0){
                QThread::msleep(10);
                rePlay();
                return;
            }

            mVideoCodecCtxMutex.lockForWrite();
            if(HW && false){ //选择硬解

            }else{
                mVideoCodecCtx->thread_count = 0; //线程数
                mIsOpenVideoCodec = avcodec_open2(mVideoCodecCtx, mVideoCodec, NULL) >= 0;
                mUseHw = false;
            }
            mVideoCodecCtxMutex.unlock();

            if(mIsOpenVideoCodec){
                AVDictionaryEntry *tag = NULL;
                tag = av_dict_get(mFormatCtx->streams[mVideoIndex]->metadata, "rotate", tag, 0);
                av_free(tag);

                videoq->setTimeBase(mFormatCtx->streams[mVideoIndex]->time_base);
                videoq->init();
                renderq->init();
                //-- 启动抓包线程 和 解码线程
                getPacketTask();
                decodecTask();
            } else{
                statusChanged(AVDefine::AVMediaStatus_InvalidMedia);
            }

            if(mPixFormat != mVideoCodecCtx->pix_fmt)
                mPixFormat = mVideoCodecCtx->pix_fmt;
            if(mSize != QSize(mVideoCodecCtx->width, mVideoCodecCtx->height) )
                mSize = QSize(mVideoCodecCtx->width, mVideoCodecCtx->height);

            if(!mUseHw) //初始化转换器
            {
                switch (mVideoCodecCtx->pix_fmt) {
                case AV_PIX_FMT_YUV420P :
                case AV_PIX_FMT_NV12 :
                default: //AV_PIX_FMT_YUV420P  如果上面的格式不支持直接渲染，则转换成通用AV_PIX_FMT_YUV420P格式
                    //                        vFormat.format = AV_PIX_FMT_YUV420P;
                    if(mVideoSwsCtx != NULL){
                        sws_freeContext(mVideoSwsCtx);
                        mVideoSwsCtx = NULL;
                    }
                    mVideoSwsCtx = sws_getContext(
                                mVideoCodecCtx->width,
                                mVideoCodecCtx->height,
                                mVideoCodecCtx->pix_fmt,
                                mVideoCodecCtx->width,
                                mVideoCodecCtx->height,
                                (AVPixelFormat)AV_PIX_FMT_YUV420P,// (AVPixelFormat)vFormat.format,
                                SWS_BICUBIC,NULL,NULL,NULL);
                    break;
                }
            }

            if(_pushRtmpFlag){
                initEncodec();
            }

            statusChanged(AVDefine::AVMediaStatus_Buffering);
            mIsInit = true;

            qDebug()<<"jie ma qi de name======    "<<mVideoCodec->name;
            qDebug("init play_video is ok!!!!!!!");
        }
    }
}

void AVDecoder::msleep(unsigned int msec)
{
    QTime dieTime = QTime::currentTime().addMSecs(msec);
    while (QTime::currentTime() < dieTime) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }
}

void AVDecoder::Delay(const AVPacket &packet, const int64_t &startTime) const
{
    if (packet.stream_index == 0)
    {
        AVRational timeBase = inputContext->streams[0]->time_base;
        AVRational timeBaseQ = {1, AV_TIME_BASE};
        int64_t ptsTime = av_rescale_q(packet.dts, timeBase, timeBaseQ);
        int64_t nowTime = av_gettime() - startTime;
        if (ptsTime > nowTime)
        {
            av_usleep(ptsTime - nowTime);
        }
    }
}

static int readRawDataCB(void *opaque, uint8_t *buf, int buf_size)
{
    (void)buf_size;
    AVDecoder* decoder = (AVDecoder*)opaque;
    int len = -1;
    while(len < 1)
    {
        len = g_fifo->read(buf);
        decoder->msleep(1);
    }
    return (-1 == len)?AVERROR(errno) :len;
}

void AVDecoder::init2(){
    QMutexLocker locker(&mDeleteMutex);

    g_fifo->clear();
    if(mIsInit)
        return;

    mIsInit = true;
    mIsInit = false;

    av_register_all();
    avformat_network_init();

    statusChanged(AVDefine::AVMediaStatus_Loading);

    if(mFilename.contains("rtsp"))
    {
        av_dict_set(&options, "preset", "ultrafast ", 0);   // av_opt_set(pCodecCtx->priv_data,"preset","fast",0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set(&options, "rtsp_transport", "tcp", 0);  // tcp打开

    } else {
        av_dict_set(&options, "preset", "ultrafast ", 0);   // av_opt_set(pCodecCtx->priv_data,"preset","fast",0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set(&options, "rtsp_transport", "udp", 0);  // 以udp方式打开，如果以tcp方式打开将udp替换为tcp
    }

    //寻找视频
    mFormatCtx = avformat_alloc_context();
    lastReadPacktTime = av_gettime();
    _pushRtmpFlag = false;
    mFormatCtx->interrupt_callback.opaque = this;
    uint8_t  *avio_ctx_buffer = NULL;
    size_t avio_ctx_buffer_size =/* 1024000 */327680;// 3072;// 32768;
    AVIOContext *avio_ctx = NULL;
    AVInputFormat *pAVInputFmt = NULL;
    int ret;

    avio_ctx_buffer = (uint8_t *)av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
    }

    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, NULL, &readRawDataCB, NULL, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
    }

    mFormatCtx->pb = avio_ctx;
    mFormatCtx->probesize = 50000000;

    ret = av_probe_input_buffer(mFormatCtx->pb, &pAVInputFmt, "", NULL, 0, 0);
    qDebug() << "enter the avcodec_alloc_context3 return";
    if ( ret< 0)
    {
        qDebug() << QString("Couldn't probe input buffer.\n");
        if (avio_ctx) {
            av_freep(&avio_ctx->buffer);
            av_freep(&avio_ctx);
        }
        QThread::msleep(10);
        rePlay();
        return;
    }

    mFormatCtx->interrupt_callback.opaque = this;
    if (avformat_open_input(&mFormatCtx, "", pAVInputFmt, NULL /*&options*/) != 0) {//打开视频
        qDebug() << QString("Couldn't open input stream.\n");
        if (avio_ctx) {
            av_freep(&avio_ctx->buffer);
            av_freep(&avio_ctx);
        }
        QThread::msleep(10);
        rePlay();
        return;
    }

    if(avformat_find_stream_info(mFormatCtx, NULL) < 0) //判断是否找到视频流信息
    {
        qDebug() << "media find stream error : " << mFilename.toStdString().data();
        statusChanged(AVDefine::AVMediaStatus_InvalidMedia);
        if (avio_ctx) {
            av_freep(&avio_ctx->buffer);
            av_freep(&avio_ctx);
        }
        QThread::msleep(10);
        rePlay();
        return;
    }

    mVideoCodec =  avcodec_find_decoder(AV_CODEC_ID_H264); //zhi jie fa xian
    if (!mVideoCodec) {
        qDebug() << "Cannot find a video stream in the input file";
    } else {
        mVideoIndex = AVMEDIA_TYPE_VIDEO; //ret;
        //fps
        int fps_num = mFormatCtx->streams[mVideoIndex]->r_frame_rate.num;
        int fps_den = mFormatCtx->streams[mVideoIndex]->r_frame_rate.den;

        if (fps_den > 0) {
            _fps = fps_num / fps_den;
            foreach (AVMediaCallback* cCallback, mCallbackSet){
                QMutexLocker locker(&mCallbackMutex);
                if(cCallback != nullptr)
                    cCallback->mediaUpdateFps(_fps);
            }
        }

#if defined(Q_OS_WIN32)
        //选择硬解
        mHWConfigList.clear();
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(mVideoCodec, i);
            if (!config) {
                break;
            }
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type != AV_HWDEVICE_TYPE_NONE) {
                mHWConfigList.push_back((AVCodecHWConfig *)config);
                qDebug() << "------------------------config->device_type" << config->device_type << config->pix_fmt;
                continue;
            }
        }
#endif

        if (!(mVideoCodecCtx = avcodec_alloc_context3(mVideoCodec))){
            QThread::msleep(10);
            rePlay();

            return;
        }


        mVideo = mFormatCtx->streams[mVideoIndex];
        if(mVideoCodecCtx) {
            mVideoCodecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
        }
        if (!mVideoCodecCtx){
            qDebug() << "create video context fail!" << mFormatCtx->filename;
            statusChanged(AVDefine::AVMediaStatus_InvalidMedia);
        }else{
            if (avcodec_parameters_to_context(mVideoCodecCtx, mVideo->codecpar) < 0){
                QThread::msleep(10);
                rePlay();
                return;
            }

#if defined(Q_OS_WIN32)
            bool hw = mHWConfigList.size() > 0;
#else
            bool hw = false;
#endif
            mVideoCodecCtxMutex.lockForWrite();
            if(HW && hw){ //选择硬解
#if defined(Q_OS_WIN32)
                for(int i = 0; i < mHWConfigList.size(); i++){
                    if(mVideoCodecCtx != NULL){
                        if(avcodec_is_open(mVideoCodecCtx))
                            avcodec_close(mVideoCodecCtx);
                    }
                    const AVCodecHWConfig *config = mHWConfigList.at(mHWConfigList.size() - 1- i);
                    mHWPixFormat = config->pix_fmt;
                    mVideoCodecCtx->get_format = get_hw_format; //回调 &mHWDeviceCtx
                    /** 硬解上下文 */
                    AVBufferRef *hWDeviceCtx = nullptr;
                    qDebug() << "-------AVDecoder::init2()6";
                    if ( hw_decoder_init(mVideoCodecCtx, hWDeviceCtx, config) < 0) {
                        mUseHw = false;
                    }else{
                        mUseHw = true;
                        mVideoCodecCtx->thread_count = 0;
                        mVideoCodecCtx->opaque = (void *) this;
                    }
                    if( mUseHw && avcodec_open2(mVideoCodecCtx, mVideoCodec, NULL) >= 0){
                        mIsOpenVideoCodec = true;
                        qDebug() << "-----------------------config->device_type OK" << config->device_type << config->pix_fmt;
                        break;
                    }else{
                        qDebug() << "-----------------------config->device_type NG" << config->device_type << config->pix_fmt << mVideoCodecCtx->hw_device_ctx;
                        if(mVideoCodecCtx->hw_device_ctx){
                            av_buffer_unref(&mVideoCodecCtx->hw_device_ctx);
                            mVideoCodecCtx->hw_device_ctx = nullptr;
                        }
                    }
                    qDebug() << "-------AVDecoder::init2()7";
                }
#endif
            }else{
                mVideoCodecCtx->thread_count = 0; //线程数
                //                mVideoCodecCtx->opaque = (void *) this;
                mIsOpenVideoCodec = avcodec_open2(mVideoCodecCtx, mVideoCodec, NULL) >= 0;
                mUseHw = false;
            }
            mVideoCodecCtxMutex.unlock();

            if(mIsOpenVideoCodec){
                AVDictionaryEntry *tag = NULL;
                tag = av_dict_get(mFormatCtx->streams[mVideoIndex]->metadata, "rotate", tag, 0);
                av_free(tag);
                videoq->setTimeBase(mFormatCtx->streams[mVideoIndex]->time_base);
                videoq->init();
                renderq->init();
            } else{
                statusChanged(AVDefine::AVMediaStatus_InvalidMedia);
            }

            if(mPixFormat != mVideoCodecCtx->pix_fmt)
                mPixFormat = mVideoCodecCtx->pix_fmt;
            if(mSize != QSize(mVideoCodecCtx->width, mVideoCodecCtx->height) )
                mSize = QSize(mVideoCodecCtx->width, mVideoCodecCtx->height);

            if(!mUseHw) //初始化转换器
            {
                switch (mVideoCodecCtx->pix_fmt) {
                case AV_PIX_FMT_YUV420P :
                case AV_PIX_FMT_NV12 :
                default: //AV_PIX_FMT_YUV420P  如果上面的格式不支持直接渲染，则转换成通用AV_PIX_FMT_YUV420P格式
                    //                        vFormat.format = AV_PIX_FMT_YUV420P;
                    if(mVideoSwsCtx != NULL){
                        sws_freeContext(mVideoSwsCtx);
                        mVideoSwsCtx = NULL;
                    }
                    mVideoSwsCtx = sws_getContext(
                                mVideoCodecCtx->width,
                                mVideoCodecCtx->height,
                                mVideoCodecCtx->pix_fmt,
                                mVideoCodecCtx->width,
                                mVideoCodecCtx->height,
                                (AVPixelFormat)AV_PIX_FMT_YUV420P,// (AVPixelFormat)vFormat.format,
                                SWS_BICUBIC,NULL,NULL,NULL);
                    break;
                }
            }
            qDebug() << "-------AVDecoder::init2()8";
            initEncodec();

            statusChanged(AVDefine::AVMediaStatus_Buffering);
            mIsInit = true;
            //-- 启动抓包线程 和 解码线程
            getPacketTask();
            decodecTask();

            qDebug()<<"jie ma qi de name======    "<<mVideoCodec->name;
            qDebug("init play_video is ok!!!!!!!");
        }
    }
}

void AVDecoder::initEncodec()
{
    if(outUrl.isEmpty())
        return;
    string output = outUrl.toStdString();
    qDebug() << "rtmp out is :" << outUrl;
    if(OpenOutput(output) == 0)
    {
        mIsInitEC = true;
    } else {
        mIsInitEC = false;
    }

    emit senderEncodecStatus(mIsInitEC);
}

int AVDecoder::OpenOutput(string outUrl)
{
    if(mVideoCodec->name == "hevc")
        return -1;

    inputContext = mFormatCtx;

    int ret  = avformat_alloc_output_context2(&outputContext, nullptr, "flv", outUrl.c_str());
    if(ret < 0)
    {
        qDebug() << " avformat_alloc_output_context2 error ";
        goto Error;
    }

    qDebug() << "len --------:" << inputContext->nb_streams;
    ret = avio_open2(&outputContext->pb, outUrl.c_str(), AVIO_FLAG_READ_WRITE,nullptr, &options);
    qDebug() << " avio_open ret " << ret;
    if(ret < 0)
    {
        qDebug() << "avio_open2 error";
        if(mIsInit ) {
            qDebug() << " OpenOutput mIsInit false";
        } else {
            goto Error;
        }
    }

    for(unsigned int i = 0; i < inputContext->nb_streams; i++)
    {
        AVStream *in_stream  = inputContext->streams[i];
        if(in_stream->codec->codec_type != AVMEDIA_TYPE_VIDEO) //只解视频数据
            continue;

        AVStream *out_stream = avformat_new_stream(outputContext, in_stream->codec->codec);
        //法一
        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        //法二
        //ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        //out_stream->codecpar->codec_tag = 0;
        if(ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "copy coddec context failed");
            qDebug() << "avcodec_copy_context error";
            goto Error;
        }
        out_stream->codec->codec_tag = 0;  //这个值一定要为0
        if (outputContext->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    //先关掉、然后再重启
    ret = avformat_write_header(outputContext, NULL);  //写入参数

    if(ret < 0)
    {
        qDebug() << "avformat_write_header error";
        goto Error;
    }
    return ret ;

Error:
    if(outputContext)
    {
        qDebug() << "error to output..";
        for(unsigned int i = 0; i < outputContext->nb_streams; i++)
        {
            avcodec_close(outputContext->streams[i]->codec);
        }
        avformat_close_input(&outputContext);
    }
    return ret ;
}

void AVDecoder::getPacket()
{
    if(!mIsInit || !mIsOpenVideoCodec){ //必须初始化
        statusChanged(AVDefine::AVMediaStatus_InvalidMedia);
        return;
    }

    AVPacket *pkt = av_packet_alloc();
    lastReadPacktTime = av_gettime();
    mVideoCodecCtxMutex.lockForRead();
    int ret = av_read_frame(mFormatCtx, pkt);
    mVideoCodecCtxMutex.unlock();

    if ((ret == AVERROR_EOF || avio_feof(mFormatCtx->pb))) { //读取完成
        av_packet_unref(pkt);
        av_freep(pkt);
        getPacketTask(1);
        return;
    }

    static uchar errorSum = 0;
    if (mFormatCtx->pb && mFormatCtx->pb->error)
    {
        av_packet_unref(pkt);
        av_freep(pkt);
        if(errorSum++ > 20){ //一直报错 重启
            rePlay();
        }
        getPacketTask(1);
        return;
    }else{
        errorSum = 0;
    }

    if(ret < 0)
    {
        av_packet_unref(pkt);
        av_freep(pkt);
        getPacketTask(1);
        return;
    }

    if (pkt->stream_index == mVideoIndex && mIsOpenVideoCodec)
    {
        //计算真实的渲染FPS
        //_fpsFrameSum++;

        if(tsSave)
            fwrite(pkt->data, 1, pkt->size, tsSave);//写数据到文件中
        pkt->pts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        videoq->put(pkt);   //显示

        if(mIsInitEC  && _pushRtmpFlag) {
#if 0
            AVPacket *videopacket_t = av_packet_alloc();
            av_copy_packet(videopacket_t, pkt);
            if (av_dup_packet(videopacket_t) < 0)
            {
                av_free_packet(videopacket_t);
            }
            videopacket_t->pos = 0;
            videopacket_t->flags = 1;
            videopacket_t->convergence_duration = 0;
            videopacket_t->side_data_elems = 0;
            videopacket_t->stream_index = 0;
            videopacket_t->duration = 0;

            auto inputStream = inputContext->streams[videopacket_t->stream_index];
            auto outputStream = outputContext->streams[0];  //只有视屏
            av_packet_rescale_ts(videopacket_t,inputStream->time_base,outputStream->time_base);
            av_interleaved_write_frame(outputContext, videopacket_t);  //推流
            av_freep(videopacket_t);
        }

        if(tsSave)
            fwrite(pkt->data, 1, pkt->size, tsSave);//写数据到文件中
        pkt->pts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        videoq->put(pkt);   //显示

#else
            AVStream *inStream, *outStream;
            AVPacket *videopacket_t = av_packet_alloc();
            av_copy_packet(videopacket_t, pkt);
            if (av_dup_packet(videopacket_t) < 0)
            {
                av_free_packet(videopacket_t);
            }

            videopacket_t->pos = 0;
            videopacket_t->flags = 1;
            videopacket_t->convergence_duration = 0;
            videopacket_t->side_data_elems = 0;
            videopacket_t->stream_index = 0;
            videopacket_t->duration = 0;

            //FIX：No PTS (Example: Raw H.264)
            //Simple Write PTS
            if (videopacket_t->pts == AV_NOPTS_VALUE)
            {
                //Write PTS
                AVRational time_base1 = inputContext->streams[0]->time_base;
                //Duration between 2 frames (us)
                int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(inputContext->streams[0]->r_frame_rate);
                //Parameters
                videopacket_t->pts = (double) (videoWriteFrameCount * calc_duration) / (double) (av_q2d(time_base1) * AV_TIME_BASE);
                videopacket_t->dts = videopacket_t->pts;
                videopacket_t->duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
            }

            /// 延迟发送，否则会出错
            Delay(*videopacket_t, lastReadPacktTime);
            inStream    = inputContext->streams[videopacket_t->stream_index];
            outStream   = outputContext->streams[0];

            /* copy packet */
            //转换PTS/DTS（Convert PTS/DTS）
            videopacket_t->pts = av_rescale_q_rnd(videopacket_t->pts, inStream->time_base, outStream->time_base,
                                                  (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            videopacket_t->dts = av_rescale_q_rnd(videopacket_t->dts, inStream->time_base, outStream->time_base,
                                                  (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            videopacket_t->duration = av_rescale_q(videopacket_t->duration, inStream->time_base, outStream->time_base);
            videopacket_t->pos = -1;

            //Print to Screen
            if (videopacket_t->stream_index == 0) {
                ++videoWriteFrameCount;
            }
            av_interleaved_write_frame(outputContext, videopacket_t);  //推流
        }
#endif

    } else {
        av_packet_unref(pkt);
        av_freep(pkt);
    }
    getPacketTask(1);
}

void AVDecoder::decodec()
{
    if(!mIsInit || !mIsOpenVideoCodec){ //必须初始化
        statusChanged(AVDefine::AVMediaStatus_InvalidMedia);
        return;
    }

    if(videoq->size() > 20) //清空
        videoq->release();

    if(videoq->size() <= 0) {
        decodecTask(1);
        return;
    }

    AVPacket *pkt = videoq->get();
    if (pkt->stream_index == mVideoIndex) {
        decode_write(mVideoCodecCtx, pkt);
    }
    av_packet_unref(pkt);
    av_freep(pkt);
    decodecTask(1);
}

int AVDecoder::decode_write(AVCodecContext *avctx, AVPacket *packet)
{
    AVFrame *frame = NULL, *sw_frame = NULL;

    int ret = 0;
    mVideoCodecCtxMutex.lockForRead();

    ret = avcodec_send_packet(avctx, packet);  // ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture,packet);
    if (ret < 0) {
        qWarning() << "Error during decoding";
        mVideoCodecCtxMutex.unlock();
        return ret;
    }

    AVFrame *tmp_frame = av_frame_alloc();
    sw_frame = av_frame_alloc();
    frame = sw_frame;

    while (avcodec_receive_frame(avctx, frame) == 0) {
        ret = 0;
        //        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_unref(sw_frame);
            av_frame_free(&sw_frame);
            mVideoCodecCtxMutex.unlock();
            return 0;
        } else if (ret < 0) {
            qWarning() << "Error while decoding";
            goto fail;
        }

        if(mSize != QSize(frame->width, frame->height)){
            /// 重要信息变更 需要转码时重启
            rePlay();
        }

        if (mUseHw) { // 硬解拷贝
            /* retrieve data from GPU to CPU */
            if(frame->format == mHWPixFormat ){
                if ((ret = av_hwframe_transfer_data(tmp_frame, frame, 0)) < 0) {
                    qWarning() << "Error transferring the data to system memory";

                    goto fail;
                }else
                    av_frame_copy_props(tmp_frame, frame);
            }
        } else{ //软解 转格式  --- [软解还没调好]  >> BUG 函数结束时 item->frame被清空 导致无数据
            if(mPixFormat != frame->format){
                /// 重要信息变更 需要转码时重启
                rePlay();
            }

            if(mVideoSwsCtx && frame->format != AV_PIX_FMT_YUV420P){
                ret = sws_scale(mVideoSwsCtx,
                                frame->data,
                                frame->linesize,
                                0,
                                frame->height,
                                tmp_frame->data,
                                tmp_frame->linesize);

                if(ret < 0){
                    qWarning() << "Error sws_scale";
                    goto fail;
                }
            }else{  //内存移动 -- 不然会被释放
                av_frame_move_ref(tmp_frame, frame);
            }
        }
        if(tmp_frame->pts != AV_NOPTS_VALUE){
            tmp_frame->pts = av_q2d(mFormatCtx->streams[mVideoIndex]->time_base ) * tmp_frame->pts * 1000;
        }

        //renderq->put(tmp_frame);
        if(renderq->size() > 20){
            //goto fail;
            //            renderq->release();
            renderq->put(tmp_frame);
        }else{
            renderq->put(tmp_frame);
        }

        av_frame_unref(sw_frame);
        av_frame_free(&sw_frame);
        mVideoCodecCtxMutex.unlock();
        return ret;
    }

fail:
    av_frame_unref(tmp_frame);
    av_frame_free(&tmp_frame);
    av_frame_unref(sw_frame);
    av_frame_free(&sw_frame);
    mVideoCodecCtxMutex.unlock();
    return ret;
}

void AVDecoder::setFilenameImpl(const QString &source){

    qDebug() << "------------------AVDecoder::setFilenameImpl" << source;
    if(mFilename == source) return;
    if(mFilename.size() > 0){
        release();//释放所有资源
    }
    mFilename = source;
    mIsOpenVideoCodec = false;
    mIsInit = false;
    if(mFilename.size() > 0)
        load();
}

void AVDecoder::_rePlay()
{
    if(mFilename.size() > 0){
        release();//释放所有资源
    }
    mIsOpenVideoCodec = false;
    mIsInit = false;
    if(mFilename.size() > 0){
        QThread::msleep(1);
        load();
    }
}

void AVDecoder::requestRender()
{
    /* 1-60  >2 45  >3 30  >4 20  >5 15 */
//    qDebug() << "enter requestRender----";
    int len = renderq->size();
    if(len >= 1){
        static qint64 lastTime2 = QDateTime::currentMSecsSinceEpoch();
        QDateTime::currentMSecsSinceEpoch() - lastTime2; //no data time
        qint64 lastTime = QDateTime::currentMSecsSinceEpoch();
        requestRenderNextFrame(); //x显示
        int space = QDateTime::currentMSecsSinceEpoch() - lastTime;

        //        qDebug() << "----------------------- len" << len << _fps << space;
        int frameStep = 1000 / (_fps +3)/* + 1*/;
        //        int space2  = frameStep;
        //        if(len > 5){
        //            space = frameStep * 2 / 3;
        //        }else if(len > 1){
        //            space = space2 - space - 2;
        //        }else{
        //            space = space2 - space + 1;
        //        }
        //        space -= space3;
        //        if(space <= 0){
        //            space = 20;
        //        }else if(space > frameStep){
        //            space = frameStep;
        //        }
        space = frameStep - space;
        //        qDebug() << "----------------------- len2  << space" << len  <<frameStep << space << space3;
        //        QThread::msleep(space);
        //        QThread::msleep(frameStep);
        lastTime2 = QDateTime::currentMSecsSinceEpoch();
    }
    wakeupPlayer();
}



/* ------------------------------------------------------私有函数-------------------------------------------------------------- */

void AVDecoder::statusChanged(AVDefine::AVMediaStatus status){
    if(mStatus == status)
        return;
    mStatus = status;
}

void AVDecoder::release(bool isDeleted){

    mDecodeThread.clearAllTask(); //清除所有任务
    mProcessThread.clearAllTask(); //清除所有任务
    if(videoq){
        videoq->release();
    }
    if(renderq){
        renderq->release();
    }

    mVideoCodecCtxMutex.lockForWrite();
    if(mVideoCodecCtx != NULL){
        if(avcodec_is_open(mVideoCodecCtx))
            avcodec_close(mVideoCodecCtx);
        //        if(mVideoCodecCtx->hw_device_ctx){
        //            av_buffer_unref(&mVideoCodecCtx->hw_device_ctx);
        //            mVideoCodecCtx->hw_device_ctx = nullptr;
        //        }
        avcodec_free_context(&mVideoCodecCtx);
        mVideoCodecCtx = NULL;
    }
    mVideoCodecCtxMutex.unlock();

#if defined(Q_OS_ANDROID)
#elif defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
    if(mVideoCodec != NULL){
        av_free(mVideoCodec);
        mVideoCodec = NULL;
    }
#endif
//    qDebug() << "_----------------------------RES3";
    if(mFormatCtx != NULL){
        if( mIsOpenVideoCodec ){
            av_read_pause(mFormatCtx);
            avformat_close_input(&mFormatCtx);
        }
        avformat_free_context(mFormatCtx);
        mFormatCtx = NULL;
    }
    if(mVideoSwsCtx != NULL){
        sws_freeContext(mVideoSwsCtx);
        mVideoSwsCtx = NULL;
    }
    if(mRGBSwsCtx != NULL){
        sws_freeContext(mRGBSwsCtx);
        mRGBSwsCtx = NULL;
    }

    qDebug() << "_----------------------------RES4";
    if(outputContext != NULL){
        avio_closep(&outputContext->pb);
        avformat_free_context(outputContext);
        outputContext = NULL;
    }

    if(_out_buffer != NULL){
        av_free(_out_buffer);
        _out_buffer = NULL;
    }
    if(_frameRGB != NULL){
        av_frame_unref(_frameRGB);
        av_free(_frameRGB);
        _frameRGB = NULL;
    }
    if(tsSave){
        fclose(tsSave);
        tsSave = nullptr;
    }
    statusChanged(AVDefine::AVMediaStatus_UnknownStatus);

    foreach (AVMediaCallback* cCallback, mCallbackSet){
        QMutexLocker locker(&mCallbackMutex);
        if(cCallback != nullptr)
           cCallback->formatCleanUpCallback();
    }
}

/* ------------------------显示函数---------------------------------- */

qint64 AVDecoder::requestRenderNextFrame(){
    qint64 time = 0;

    if(renderq->size()){
        AVFrame* frame = renderq->get();

        if(_isSaveImage){ //保存图片
            saveFrame(frame);
        }

        VlcVideoFrame* _frame = nullptr;

        foreach (AVMediaCallback* mCallback, mCallbackSet){
            QMutexLocker locker(&mCallbackMutex);

             if(mCallback){
                 mCallback->lockCallback((void**) &_frame);
                 if(!_frame->inited){
                     _frame->width  = frame->width;
                     _frame->height = frame->height;
                     _frame->planeCount = 3;

                     for (unsigned int i = 0; i < _frame->planeCount; ++i) {
                         if(i == 0){
                             _frame->pitch[i] = _frame->width;
                             _frame->lines[i] = _frame->height      ;
                             _frame->visiblePitch[i] = _frame->pitch[i];
                             _frame->visibleLines[i] = _frame->lines[i];
                         }else{
                             _frame->pitch[i] = _frame->width >> 1;
                             _frame->lines[i] =  _frame->height >> 1;
                             _frame->visiblePitch[i] = _frame->pitch[i];
                             _frame->visibleLines[i] = _frame->lines[i];
                         }
                         _frame->plane[i].resize(_frame->pitch[i] * _frame->lines[i]);
                     }
                 }

                 uint8_t* y = (uint8_t*)_frame->plane[0].data();
                 for(int i = 0; i < _frame->height; i++){ //将y分量拷贝
                     uint8_t* srcy = frame->data[0] + frame->linesize[0] * i;
                     ::memcpy(y, srcy, _frame->width);
                     srcy += frame->linesize[0] ;
                     y += _frame->width ;
                 }

                 int uvH = _frame->height >> 1;
                 uint8_t* u = (uint8_t*)_frame->plane[1].data();
                 uint8_t* v = (uint8_t*)_frame->plane[2].data();
                 if((AVPixelFormat)frame->format == AV_PIX_FMT_NV12){
                     //qDebug() << "-------------------->>HW" << frame->linesize[1] << _frame->height  << _frame->width;
                     if(frame->linesize[1] < _frame->width)
                     {
                         _frame->inited = true;
                         mCallback->unlockCallback();
                         av_frame_unref(frame);
                         av_free(frame);
                         qDebug() << "enter replay";
                         rePlay();
                         return time;
                     }
                     for(int i = 0; i < uvH; i++){ //将uv分量拷贝
                         uint8_t* uv = frame->data[1] + frame->linesize[1] * i;
                         for(int j = 0; j < (_frame->width >> 1); j++){
                             *v++ = *uv++;
                             *u++ = *uv++;
                         }
                     }
                 }else if((AVPixelFormat)frame->format == AV_PIX_FMT_YUV420P){
                     //                qDebug() << "-------------------->>" << frame->linesize[1] << frame->linesize[2] << frame->linesize[3] << frame->linesize[4] << uvH  << _frame->width;
                     for(int i = 0; i < uvH; i++){ //将uv分量拷贝
                         uint8_t* srcV = frame->data[1] + frame->linesize[1] * i;
                         uint8_t* srcU = frame->data[2] + frame->linesize[2] * i;  // + (_frame->width >> 1) + 16;
                         ::memcpy(v, srcV, (_frame->width >> 1));
                         ::memcpy(u, srcU, (_frame->width >> 1));
                         srcV += frame->linesize[1] ;
                         srcU += frame->linesize[2] ;
                         v += (_frame->width >> 1) ;
                         u += (_frame->width >> 1) ;
                     }
                 }
                 _frame->inited = true;
                 mCallback->unlockCallback();
             }
         }

        av_frame_unref(frame);
        av_free(frame);
    }

    return time;
}

/* ------------------------------------------------------队列操作-------------------------------------------------------------- */
void AVDecoder::onFpsTimeout()
{
    return;
    if(!mIsInit || !mIsOpenVideoCodec){ //必须初始化
        return;
    }

    if(renderq->size()<= 0) {
        return;
    }

    if(20 < _fpsFrameSum && _fpsFrameSum < 62){
        _fps = (_fpsFrameSum + _fps) / 2;

        foreach (AVMediaCallback* cCallback, mCallbackSet){
            QMutexLocker locker(&mCallbackMutex);
            if(cCallback != nullptr)
               cCallback->mediaUpdateFps(_fps);
        }
    }
    _fpsFrameSum = 0;
}
void AVDecoder::saveFrame(AVFrame *frame)
{
    if(!frame)
        return;
    if(mRGBSwsCtx == NULL){
        mRGBSwsCtx = sws_getContext(
                    frame->width,
                    frame->height,
                    (AVPixelFormat)frame->format,
                    frame->width,
                    frame->height,
                    (AVPixelFormat)AV_PIX_FMT_RGB24,// (AVPixelFormat)vFormat.format,
                    SWS_BICUBIC,NULL,NULL,NULL);
    }

    if(mRGBSwsCtx != nullptr){
        if(!_frameRGB){
            _frameRGB = av_frame_alloc();
            qint64  numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, frame->width, frame->height);
            _out_buffer  = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));//内存分配
            avpicture_fill((AVPicture *) _frameRGB, _out_buffer, AV_PIX_FMT_RGB24,
                           frame->width, frame->height);
        }

        int ret = sws_scale(mRGBSwsCtx,
                            frame->data,
                            frame->linesize,
                            0,
                            frame->height,
                            _frameRGB->data,
                            _frameRGB->linesize);

        if(ret < 0){
            qWarning() << "Error sws_scale";
        }else{
            QImage img =  QImage(_frameRGB->data[0], frame->width, frame->height, QImage::Format_RGB888);

            if(_isSaveImage){
                _isSaveImage = false;
                QString dir = QString("%1").arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)) + "/MMC Station/Media/Photo/" + QDateTime::currentDateTime().toString("yyyyMMdd") + "/";
                QDir tmpDir1;
                bool ok = false;
                if(tmpDir1.exists(dir)){
                    ok = true;
                }else{
                    ok = tmpDir1.mkpath(dir);
                }
                if(ok){
                    QString fileName = QDateTime::currentDateTime().toString("hhmmsszzz");

                    img.save(QString("%1%2%3").arg(dir).arg(fileName).arg(".jpg"));
                }

            }
        }
    }
}


void FrameQueue::init(){
    release();
}

void FrameQueue::put(AVFrame *frame){
    if(frame == NULL)
        return;
    mutex.lockForWrite();
    frames.push_back(frame);
    mutex.unlock();
}

AVFrame *FrameQueue::get(){
    AVFrame *frame = NULL;
    mutex.lockForWrite();
    if(frames.size() > 0){
        frame = frames.front();
        frames.pop_front();
    }
    mutex.unlock();
    return frame;
}

int FrameQueue::size(){
    mutex.lockForRead();
    int len = frames.size();
    mutex.unlock();
    return len;
}

void FrameQueue::release(){
    mutex.lockForWrite();
    QQueue<AVFrame *>::iterator begin = frames.begin();
    QQueue<AVFrame *>::iterator end = frames.end();
    while(begin != end){
        AVFrame *frame = *begin;
        if(frame != NULL){
            av_frame_unref(frame);
            av_free(frame);
        }
        frames.pop_front();
        begin = frames.begin();
    }
    mutex.unlock();
}

void PacketQueue::setTimeBase(AVRational &timebase){
    this->time_base.den = timebase.den;
    this->time_base.num = timebase.num;
}

void PacketQueue::init(){
    release();
}

void PacketQueue::put(AVPacket *pkt){
    if(pkt == NULL)
        return;
    mutex.lockForWrite();
    packets.push_back(pkt);
    mutex.unlock();
}

AVPacket *PacketQueue::get(){
    AVPacket *pkt = NULL;
    mutex.lockForWrite();
    if(packets.size() > 0){
        pkt = packets.front();
        packets.pop_front();
    }
    mutex.unlock();
    return pkt;
}

void PacketQueue::removeToTime(int time){
    mutex.lockForRead();
    QQueue<AVPacket *>::iterator begin = packets.begin();
    QQueue<AVPacket *>::iterator end = packets.end();
    while(begin != end){
        AVPacket *pkt = *begin;
        if(pkt != NULL){
            if(av_q2d(time_base) * pkt->pts * 1000 >= time || packets.size() == 1){
                break;
            }
            av_packet_unref(pkt);
            av_freep(pkt);
        }
        packets.pop_front();
        begin = packets.begin();
    }
    mutex.unlock();
}

int PacketQueue::diffTime(){
    int time = 0;
    mutex.lockForRead();
    if(packets.size() > 1){
        int start = av_q2d(time_base) * packets.front()->pts * 1000;
        int end = av_q2d(time_base) * packets.back()->pts * 1000;
        time = end - start;
    }
    mutex.unlock();
    return time;
}

int PacketQueue::startTime(){
    int time = -1;
    mutex.lockForRead();
    if(packets.size() > 0){
        time = av_q2d(time_base) * packets.front()->pts * 1000;
    }
    mutex.unlock();
    return time;
}

int PacketQueue::size(){
    mutex.lockForRead();
    int len = packets.size();
    mutex.unlock();
    return len;
}

void PacketQueue::release(){
    mutex.lockForWrite();
    QQueue<AVPacket *>::iterator begin = packets.begin();
    QQueue<AVPacket *>::iterator end = packets.end();
    while(begin != end){
        AVPacket *pkt = *begin;
        if(pkt != NULL){
            av_packet_unref(pkt);
            av_freep(pkt);
        }
        packets.pop_front();
        begin = packets.begin();
    }
    mutex.unlock();
}
