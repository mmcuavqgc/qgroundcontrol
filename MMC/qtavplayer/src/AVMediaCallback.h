#ifndef AVMEDIACALLBACK_H
#define AVMEDIACALLBACK_H

#include "AVDefine.h"
#include <QByteArray>
//#include <QAudioFormat>

class AVMediaCallback {

public :
    AVMediaCallback(){}
    virtual ~AVMediaCallback(){}
    /** 更新视频帧回调 */
    virtual void mediaUpdateVideoFrame(void*){}

    virtual void *lockCallback(void **planes){ Q_UNUSED(planes) return 0;}
    virtual void unlockCallback(){}
    virtual void formatCleanUpCallback(){}
    virtual void sendimage(QImage img){}

    /** 更新音频数据格式 */
    virtual void mediaUpdateAudioFrame(const QByteArray &){}
    /** 视频总时间变化信息 */
    virtual void mediaDurationChanged(int duration){Q_UNUSED(duration)}
    /** 可以渲染第一帧调 */
    virtual void mediaCanRenderFirstFrame(){}
    /** 状态回调 */
    virtual void mediaStatusChanged(AVDefine::AVMediaStatus){}
    /** 存在音频回调 */
    virtual void mediaHasAudioChanged(){}
    /** 存在视频回调 */
    virtual void mediaHasVideoChanged(){}
    /** 缓冲到的毫秒数 */
    virtual void mediaUpdateBufferSize(int time){Q_UNUSED(time)}
    /** 设置FPS */
    virtual void mediaUpdateFps(uchar fps){Q_UNUSED(fps)}
    /** 获取FPS */
    virtual uchar mediaGetFps(){ return 0; }
};


#endif // AVMEDIACALLBACK_H
