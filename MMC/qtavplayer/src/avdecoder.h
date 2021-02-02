#ifndef AVDECODER_H
#define AVDECODER_H
#include <QObject>
#include <QReadWriteLock>
#include "AVThread.h"
#include <QVector>
#include <QImage>
#include "AVDefine.h"
#include "AVMediaCallback.h"
#include <QDebug>
#include <QQueue>

#if defined(Q_OS_WIN32)
    #include "../libusb0/libusb.h"
#else
    #include "usbexample.h"
#endif
#include "fifo.h"

extern "C"
{

    #ifndef INT64_C
    #define INT64_C
    #define UINT64_C
    #endif

    #include <libavutil/time.h>
    #include <libavcodec/avcodec.h>
    #include <libavcodec/avfft.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavfilter/avfilter.h>
    #include <libavutil/opt.h>
    #include <libswresample/swresample.h>
    #include <libavutil/hwcontext.h>

    #ifdef LIBAVUTIL_VERSION_MAJOR
    #if (LIBAVUTIL_VERSION_MAJOR >= 57)
    #define SUPPORT_HW
    #endif
    #endif
}
class AVDecoder;
class PacketQueue;
class FrameQueue;
AVDecoder *getAVDecoder();
class AVDecoder: public QObject
{
    Q_OBJECT
    friend class AVCodecTask;
public:
    explicit AVDecoder(QObject *parent = nullptr);
    ~AVDecoder();
    void addMediaCallback(AVMediaCallback *media);
    void deleteMediaCallback(AVMediaCallback *media);
    bool getmIsInitEC() {return mIsInitEC;}
    int getCountMediaCallback(void);

public :
    qint64  requestRenderNextFrame();
    void    load(); //初始化
    void    setFilename(const QString &source);
    void    rePlay(); //重新加载
    void    saveTs(bool status = false);
    void    saveImage();
    static void msleep(unsigned int msec);
    void    Delay(const AVPacket &packet, const int64_t &startTime) const;

protected:
    void init();
    void init2();
    void getPacket();
    void decodec();
    void setFilenameImpl(const QString &source);
    void _rePlay();
    void requestRender(); //显示数据
    void wakeupPlayer();

signals:
    void frameSizeChanged(int width, int height);
    void senderEncodecStatus(bool);

private:
    usbfifo                 *_usbfifo = nullptr;
#if defined(Q_OS_WIN32)
    LibUsb                  *_hidUSB = nullptr;
#else
    UsbExample              *_hidUSB = nullptr;
#endif
    /* 推流 */
    void initEncodec();
    int  OpenOutput(string outUrl);
    void getPacketTask(int type = -1);
    void decodecTask(int type = -1);
    void release(bool isDeleted = false);
    void statusChanged(AVDefine::AVMediaStatus);
    int decode_write(AVCodecContext *avctx, AVPacket *packet);
    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

private :
    /* fps统计 */
    void onFpsTimeout();
    /* 保存图片 */
    void saveFrame(AVFrame *frame = nullptr);

private:
    QString                     outUrl;
    bool                        mIsInitEC = false;
    bool                        mIsInit = false;
    bool                        mIsOpenVideoCodec = false;
    QString                     mFilename; // = "udp://@227.70.80.90:2000";
    int                         mVideoIndex;
    AVStream *                  mVideo = NULL;
    AVFormatContext *           mFormatCtx = nullptr;
    AVCodecContext *            mVideoCodecCtx = nullptr; //mCodecCtx
    AVCodec *                   mVideoCodec = nullptr;           //mCodec
    struct SwsContext *         mVideoSwsCtx = nullptr; //视频参数转换上下文
    PacketQueue*                videoq = nullptr;      //为解码的视频原始包
    FrameQueue*                 renderq = nullptr;      //为解码后视频包
    /* 资源锁 */
    QReadWriteLock              mVideoCodecCtxMutex;
    //  ----- HW
#if defined(Q_OS_WIN32)
      QList<AVCodecHWConfig *>  mHWConfigList;
#endif
    bool                        mUseHw = false;
    /** 硬解格式 */
    enum AVPixelFormat          mHWPixFormat;
    /** 保存TS流 */
    FILE *                      tsSave = nullptr;
    /** 保存Image */
    bool                        _isSaveImage = false;
    AVDictionary*               options = nullptr;
    AVFrame *                   _frameRGB = nullptr;
    uint8_t *                   _out_buffer = nullptr;
    struct SwsContext *         mRGBSwsCtx = nullptr; //RGB转码器 -- 保存图片用

private:
    QMutex                      mDeleteMutex;
    QTimer*                     _fpsTimer = nullptr; //帧率统计心跳
    uint                        _fpsFrameSum = 0;
    uchar                       _fps = 0;
    QSize                       mSize = QSize(0,0);
    enum AVPixelFormat          mPixFormat;  //原始格式格式
    AVThread                    mProcessThread;
    AVThread                    mDecodeThread;
    AVThread                    mPlayeThread;          //播放线程
    QSet<AVMediaCallback *>     mCallbackSet;
    QMutex                      mCallbackMutex;
    AVDefine::AVMediaStatus      mStatus = AVDefine::AVMediaStatus_UnknownStatus;
    bool                        _bisRelease;// false 不释放（默认）  true 释放
    bool                        _pushRtmpFlag = false;
    int                         videoWriteFrameCount = 0;
    int64_t                     start_time  = 0;              //统计推的开始时间
    qint64                      lastReadPacktTime = 0;

};

class AVCodecTask : public Task{
public :
    ~AVCodecTask(){}
    enum AVCodecTaskCommand{
        AVCodecTaskCommand_Init,
        AVCodecTaskCommand_SetPlayRate,
        AVCodecTaskCommand_Seek,
        AVCodecTaskCommand_GetPacket ,
        AVCodecTaskCommand_SetFileName ,
        AVCodecTaskCommand_DecodeToRender,
        AVCodecTaskCommand_SetDecodecMode,
        AVCodecTaskCommand_ShowFrameByPosition,
        AVCodecTaskCommand_RePlay,   //重新加载
        AVCodecTaskCommand_saveTS,   //保存TS流
        AVCodecTaskCommand_saveImage,//保存图片
        AVCoTaskCommand_Render,
    };
    AVCodecTask(AVDecoder *codec,AVCodecTaskCommand command,double param = 0,QString param2 = "");
protected :
    /** 现程实现 */
    virtual void run();
private :
    AVDecoder *mCodec;
    AVCodecTaskCommand command;
    double param;
    QString param2;
};

class FrameQueue{
public :
    FrameQueue(){init();}
    QQueue<AVFrame *> frames;
private :
    QReadWriteLock mutex;
public :
    void init();
    void put(AVFrame *frame);
    AVFrame *get();
    int size();
    void release();
};

class PacketQueue{
public :
    PacketQueue(){init();}
    QQueue<AVPacket *> packets;
    AVRational time_base;

private :
    QReadWriteLock mutex;
public :
    void setTimeBase(AVRational &timebase);
    void init();
    void put(AVPacket *pkt);
    AVPacket *get();
    void removeToTime(int time);
    int diffTime();
    int startTime();
    int size();
    void release();
};

#endif // AVDECODER_H
