#include "radiomemberbase.h"
#include <QDebug>

#define  R_MODE  0X0A
#define  L_MODE  0X05

//------------------------------------------------[分割线]----------------------------------------------------
RadioMemberBase::RadioMemberBase(QObject *parent)
    : QObject(parent)
    , _checkStatus(No_Check)
    , _chargeState(-1)
    , _time(0)
    , _stateOfHealth(0)
    , _temperature(0)
    , _rockerState(-1)
    , _rcMode(-1)
    , _ver("")
    , _calirationState(0)
    , _channel1(1500)
    , _channel2(1500)
    , _channel3(1500)
    , _channel4(1500)
    , _channelBMax1(0)
    , _channelBMax2(0)
    , _channelBMax3(0)
    , _channelBMax4(0)
    , _channelBMin1(0)
    , _channelBMin2(0)
    , _channelBMin3(0)
    , _channelBMin4(0)
    , _channelBMid1(0)
    , _channelBMid2(0)
    , _channelBMid3(0)
    , _channelBMid4(0)
#if defined(Q_OS_WIN)
    , _teleControlType(0)
#endif
//    , _sbusCount(0)
{
    /* 同步电台需要主动获取数据的计时器 */
    _syncDataTimer = new QTimer(this);
    _syncDataTimer->setInterval(300);
    connect(_syncDataTimer, SIGNAL(timeout()), this, SLOT(onSyncData()));

}

void RadioMemberBase::setRadioID(QString id)
{
    qDebug() << QString("-------------------radioID") << id;
    if(_radioID == id)  return;
    _radioID = id;
    emit radioIDChanged();
}

void RadioMemberBase::setRadioLock(bool ck)
{
    int lock;
    if(ck)
        lock = 1;
    else
        lock = 0;

    if(_radioLock == lock) return;
    _radioLock = lock;
    emit radioLockChanged();
#ifndef Q_OS_ANDROID
    if(lock){ //串口上线时
        _syncDataTimer->start();
    }
#endif
}

void RadioMemberBase::setVer(uint32_t version)
{
    int a = version &0x1f; //日
    int b = (version >> 5) & (0x0f);//月
    int c = (version >> (5+4)) & (0x7f);//年
    int d = (version >> (5+4+7)) & (0xff);//优化
    int e = (version >> (5+4+7+8)) & (0x3f);//功能增删
    int f = (version >> (5+4+7+8+6)) & (0x03);//硬件平台
    QString year;
    if(c < 10)
        year = QString("0%1").arg(c);
    else
        year = QString("%1").arg(c);
    QString mouth;
    if(b < 10)
        mouth = QString("0%1").arg(b);
    else
        mouth = QString("%1").arg(b);
    QString day;
    if(a < 10)
        day = QString("0%1").arg(a);
    else
        day = QString("%1").arg(a);
    QString vers = QString("%1.%2.%3.%4%5%6").arg(f).arg(e).arg(d).arg(year).arg(mouth).arg(day);
    set_ver(vers);
}

//void RadioMemberBase::setSbusCount(int count)
//{
//    if(_sbusCount == count)
//        return;
//    _sbusCount = count;
//    emit sbusCountChanged();
//}


/* **********************************************************************
 * 下发指令
 * **********************************************************************/
void RadioMemberBase::onSyncData()
{
    int i = 0;
    if(_radioID.isEmpty()){
        queryRadioId(); i++;
    }

    if(i == 0) _syncDataTimer->stop(); //所有数据都已经存在，关闭定时器
}

void RadioMemberBase::queryRadioId()
{
    char type = 0x8f;
    char buff[1] = {0x01};
    emit _writeData(type, QByteArray(buff, 1));
}

void RadioMemberBase::setCalirationState(bool isLeft)
{
#if defined(Q_OS_WIN)
    char type = 0x2f;
#elif defined(Q_OS_ANDROID)
    char type = 0x02;
#endif
    char buff[1] = {L_MODE};
    if(!isLeft)
        buff[0] = R_MODE;
    emit _writeData(type, QByteArray(buff, 1));
}

void RadioMemberBase::sendCheckStatus()
{
    CheckStatus checkState = (CheckStatus)checkStatus();
    if(checkState < 6 && checkState % 2){ //单数 重复本次校准
    }else if(checkState == No_Check || checkState == ACK_CALIBRATION_COMPLETED || checkState == ERRORS){ //开始新的一轮校准
        set_checkStatus(IN_CALIBRATION);
    }else if(checkState < 6 && !(checkState % 2)){ //+1 下一步
        set_checkStatus((CheckStatus)(checkState+1));
    }else{//不在取值范围
        return;
    }
#if defined(Q_OS_WIN)
    char type = 0x4f;
#elif defined(Q_OS_ANDROID)
    char type = 0x04;
#endif
    char buff[1] = {checkStatus()};
    qDebug() << "-------------------sendCheckStatus" << type << QByteArray(buff, 1).toHex();
    emit _writeData(type, QByteArray(buff, 1));
}

void RadioMemberBase::rockerControl(int state)
{
    if(state != 0 && state != 1) return;
    if(state == _rockerState)
        qDebug() << "---------- RadioMemberBase::rockerControl is same" << _rockerState;
    char type = 0x7f;
    char buff[1] = {state};

    emit _writeData(type, QByteArray(buff, 1));
}
#if defined(Q_OS_WIN)
void RadioMemberBase::setRockerState(int state)
{
//    qDebug() << "-------RadioMemberBase::setRockerState" << state;
    if(_rockerState == state)
        return;
    _rockerState = state;
    emit rockerStateChanged(_rockerState);
}

void RadioMemberBase::setTeleControlType(int type)
{
//    qDebug() << "------RadioMemberBase::setTeleControlType" << type;
    if(_teleControlType == type)
        return;
    _teleControlType = type;
    emit teleControlTypeChanged();
}
#endif
//------------------------------------------------[分割线]----------------------------------------------------

MMCKey::MMCKey(int id, QObject *parent) : QObject(parent), _id(id)
{
    timer = new QTimer(this);
    timer->setInterval(100);
    connect(timer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
}

bool MMCKey::key()
{
    return _key;
}

void MMCKey::setKey(bool key)
{
    if(key == _key) return; 

    _key = key;
    emit keyChanged(_key);

    if(_key){
        emit press(this);
        _accumulatedTime = 1;
        timer->start();
    }else{
        emit upspring(this);
        timer->stop();
        if(_accumulatedTime  < 5) emit click(this);
    }
}

void MMCKey::onTimeOut()
{
    _accumulatedTime++;
    emit longPress(this);
}
