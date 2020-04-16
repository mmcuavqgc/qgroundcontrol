#include "radiomember.h"

#include "QGCApplication.h"

#include <QNetworkInterface>
#include "stdio.h"

#include "QGCConfig.h"

#include <QProcess>
RadioMember::RadioMember(QObject *parent)
    : RadioMemberBase(parent)
{
    for(int i=0; i < 8; i++){
        _key[i] = new MMCKey(i+1, this);
        connect(_key[i], SIGNAL(press(MMCKey*)), this, SLOT(onPress(MMCKey*)));
        connect(_key[i], SIGNAL(upspring(MMCKey*)), this, SLOT(onUpspring(MMCKey*)));
        connect(_key[i], SIGNAL(longPress(MMCKey*)), this, SLOT(onLongPress(MMCKey*)));
        connect(_key[i], SIGNAL(click(MMCKey*)), this, SLOT(onClick(MMCKey*)));
    }
    keyBindFunction();

    QSettings settings;
    settings.beginGroup("USER_DEFINE_CONFIG");
    _lang = settings.value("Language").toString();
    settings.endGroup();

    QMap<uint, QString> enumToString;
    if(_lang == "Chinese")
    {
        enumToString.insert(KEY_UNDEFINED,                  QStringLiteral("未定义"));
        enumToString.insert(KEY_VOLUME_UP,                  QStringLiteral("音量加"));
        enumToString.insert(KEY_VOLUME_DOWN,                QStringLiteral("音量减"));
        enumToString.insert(KEY_BRIGHTNESS_UP,              QStringLiteral("亮度加"));
        enumToString.insert(KEY_BRIGHTNESS_DOWN,            QStringLiteral("亮度减"));
        enumToString.insert(KEY_VEHICLE_AUTO_MODE,          QStringLiteral("航线模式"));
        enumToString.insert(KEY_VEHICLE_LOITER_MODE,        QStringLiteral("悬停模式"));
    }else{
        enumToString.insert(KEY_UNDEFINED,                  QStringLiteral("Undefined"));
        enumToString.insert(KEY_VOLUME_UP,                  QStringLiteral("Volume Up"));
        enumToString.insert(KEY_VOLUME_DOWN,                QStringLiteral("Volume Down"));
        enumToString.insert(KEY_BRIGHTNESS_UP,              QStringLiteral("Gamma Up"));
        enumToString.insert(KEY_BRIGHTNESS_DOWN,            QStringLiteral("Gamma Down"));
        enumToString.insert(KEY_VEHICLE_AUTO_MODE,          QStringLiteral("Auto"));
        enumToString.insert(KEY_VEHICLE_LOITER_MODE,        QStringLiteral("Loiter"));
    }
    setEnumToStringMapping(enumToString);

//    _mmcStationID = getHostMacAddress(); //并不唯一  后续改为  -  硬盘序号+网卡序号+主板序号 加密算法
}


void RadioMember::setEnumToStringMapping(const QMap<uint, QString> &enumToString)
{
    _enumToString = enumToString;
}

QStringList RadioMember::keyModes()
{
    QStringList keyModes;

    QMap<uint, QString>::const_iterator i = _enumToString.constBegin();
    while (i != _enumToString.constEnd()) {
        //         cout << i.key() << ": " << i.value() << endl;
        keyModes << i.value();
        ++i;
    }
    return keyModes;
}


/* **********************************************************************
 * 接收数据 -- 数据转换
 * **********************************************************************/

void RadioMember::setKey(int i, uchar key)
{
    if(i >= 0 && i < 8)
        _key[7-i]->setKey((key & 0x02));
}

void RadioMember::setQuantity(uchar value)
{
    int energy = (uchar)value;
    if(energy > 100) energy = 100;
    set_energy(energy);
}

void RadioMember::setVoltage(uchar value) //130~168
{
    int voltage = (uchar)value;
    setVoltage((float)voltage/10);
}

void RadioMember::setRadioSourceState(int state)
{
    int statu = 0;
    if(state != 0) statu = 1;
    if(_radioSourceState == statu) return;
    qDebug() << QString("-------------------radioSourceState") << state;
    _radioSourceState = statu;
    emit radioSourceStateChanged(_radioSourceState);
}

void RadioMember::setLidState(int state)
{
    int statu = 0;
    if(state != 0) statu = 1;
    //    if(_radioSourceState == statu) return;
    //    qDebug() << QString("-------------------setLidState") << state;
}

void RadioMember::_say(const QString &text)
{
    qgcApp()->toolbox()->audioOutput()->say(text.toLower());
}

void RadioMember::setVoltage(float value)
{
    float voltag = voltage();
    if(voltag == value) return;
    set_voltage(value);
    voltag = voltage();

    static float minVoltage = 13.6;
    if(voltag < minVoltage){
        minVoltage = voltag;
        if(_lang == "Chinese"){
            _say(QString("低电压，%1伏").arg(voltag));
        }else{
            _say(QString("Low Voltage,%1V").arg(voltag));
        }

    }else if(voltag > 14 &&  minVoltage < 13.4){ //表示充了电
        minVoltage = 13.6;
    }
}

void RadioMember::switchControl(int state)
{
    char type = 0x6f;
    char buff[1] = {state};
    emit _writeData(type, QByteArray(buff, 1));
}



/* **********************************************************************
 * 下发指令
 * **********************************************************************/

void RadioMember::radioControl(int state)
{
    qDebug() << "---------RadioMember::radioControl" << state;
    if(state != 0 && state != 1 /*&& state != 2*/) return;
    char type = 0x7f;
    char buff[1] = {state};
    emit _writeData(type, QByteArray(buff, 1));

}

void RadioMember::analysisPack(int type, QByteArray msg)
{
//    qDebug() << "----analysisPack" << msg << type;
    uchar* buff = (uchar*)msg.data();
//    int length = msg.length();
    ushort tep = 0;
//    float tmp;

    switch (type) {
    case 0x1F: { //遥控器各通道 -- 16字节
        channels_data channelsData;
        memcpy(&channelsData, msg.data(), msg.length());
        //          qDebug() << "------channels_data" << channelsData.channels[0] << channelsData.keys[0];
        set_channel1(channelsData.channels[0]);
        set_channel2(channelsData.channels[1]);
        set_channel3(channelsData.channels[2]);
        set_channel4(channelsData.channels[3]);

        qDebug() << "--------keys" << channelsData.keys[0] << channelsData.keys[1] << channelsData.keys[2]
                 << channelsData.keys[3] << channelsData.keys[4] << channelsData.keys[5] << channelsData.keys[6]
                 << channelsData.keys[7];
        setKey(0,channelsData.keys[0]);
        setKey(1,channelsData.keys[1]);
        setKey(2,channelsData.keys[2]);
        setKey(3,channelsData.keys[3]);
        setKey(4,channelsData.keys[4]);
        setKey(5,channelsData.keys[5]);
        setKey(6,channelsData.keys[6]);
        setKey(7,channelsData.keys[7]);
        break;
    }
    case 0x3F:{  //心跳
        heart_data heartdata;
        memcpy(&heartdata, (uchar*)msg.data(), msg.length());
        //        qDebug() << "--------heart beat" << msg.length() << sizeof(heart_data) << heartdata.charge_state << heartdata.bat_vol << heartdata.bat_per
        //                 << heartdata.time << heartdata.state_of_health << heartdata.temperature << heartdata.mcu_mode
        //                 << heartdata.rc_mode << heartdata.caliration_state << heartdata.ver << heartdata.radio_state
        //                 << heartdata.remote_source;
        //        qDebug() << "---------count index:" << heartdata.sbus_cnt << heartdata.pc_message_cnt;
        this->set_chargeState(heartdata.charge_state);
        this->setVoltage(heartdata.bat_vol/10.0f);
        this->setQuantity(heartdata.bat_per);
        this->set_time(heartdata.time);
        this->set_stateOfHealth(heartdata.state_of_health);
        this->set_temperature(heartdata.temperature);
        this->setTeleControlType(heartdata.remote_source);
        this->set_rcMode(heartdata.rc_mode);
        this->set_calirationState(heartdata.caliration_state);
        this->setVer(heartdata.ver);
        this->setRockerState(heartdata.radio_state);
        this->set_sbusCount(heartdata.sbus_cnt);
        this->set_micCount(heartdata.mic_cnt);
        this->set_pcCount(heartdata.pc_message_cnt);
        this->set_vehicleCount(heartdata.fly_message_cnt);
        break;
    }
    case 0x4F:{  //遥控器校准时各通道值
        this->set_checkStatus(*buff++);
        //        qDebug() << "---------------------- RadioMember::analysisPack" << type << msg.toHex();
        qDebug() << "-----set_checkStatus" << checkStatus();
        break;
    }
    case 0x5F:{  //遥控器校准时各通道值
        //        qDebug() << "---------------------- RadioMember::analysisPack" << type << msg.toHex();
        calirate_data calirData;
        memcpy(&calirData, (uchar*)msg.data(), msg.length());
        //        qDebug() << "------------calirate data:"
        //                 << calirData.maxChannel1 << calirData.maxChannel2 << calirData.maxChannel3 << calirData.maxChannel4
        //                 << calirData.minChannel1 << calirData.minChannel2 << calirData.minChannel3 << calirData.minChannel4
        //                 << calirData.midChannel1 << calirData.midChannel2 << calirData.midChannel3 << calirData.midChannel4;
        this->set_channelBMax1(calirData.maxChannel1);
        this->set_channelBMax2(calirData.maxChannel2);
        this->set_channelBMax3(calirData.maxChannel3);
        this->set_channelBMax4(calirData.maxChannel4);

        this->set_channelBMid1(calirData.midChannel1);
        this->set_channelBMid2(calirData.midChannel2);
        this->set_channelBMid3(calirData.midChannel3);
        this->set_channelBMid4(calirData.midChannel4);

        this->set_channelBMin1(calirData.minChannel1);
        this->set_channelBMin2(calirData.minChannel2);
        this->set_channelBMin3(calirData.minChannel3);
        this->set_channelBMin4(calirData.minChannel4);
        break;
    }
    case 0x8F:{  //单片机唯一ID
        this->setRadioID(QString(msg)/*QByteArray((char*)buff, 12)*/);
        break;
    }
    default:
        break;
    }
}

/* **********************************************************************
 * 按键方法控制
 * **********************************************************************/

int RadioMember::keyBinding(int key, QString type)
{
    /* 1-缩小  2-放大  3-自定义1  4-自定义2  5-FN  6-云台模式   7-录像   8-拍照  */
    if(key < 1 || key > 8) return -1;

    QMap<uint, QString>::const_iterator i = _enumToString.constBegin();
    int value = -1;
    while (i != _enumToString.constEnd()) {
        if(i.value() == type){
            value = i.key();
            break;
        }
        ++i;
    }

    if(value != -1){
        _keyId[key] = (KEY_TYPE)value; //此处还要判断一次value是否在枚举范围内

        QSettings settings;
        settings.beginGroup("MMC_KEYS_TYPE");
        settings.setValue(QString("MMC_KEY%1").arg(key),    (KEY_TYPE)value);
        settings.endGroup();
    }else
        return -2;
    return 1;
}

QString RadioMember::getKeyValue(int key)
{
    if(key < 1 || key > 8) return "";
    if(_enumToString.contains(_keyId[key])){ //包含此键
        return _enumToString[_keyId[key]];
    }
    return "";
}

void RadioMember::keyBindFunction()
{
    /* 按键与功能绑定 -- 后面可用配置对应*/
    _keyId[1] =KEY_ZOOM_UP;      //放大
    _keyId[2] =KEY_ZOOM_DOWN;        //缩小

    _keyId[5] =KEY_FN;             //FN
    _keyId[6] =KEY_MODE;           //云台模式
    _keyId[7] =KEY_REC;            //录像
    _keyId[8] =KEY_PHOTO;          //拍照

    QSettings settings; //读取ini
    settings.beginGroup("MMC_KEYS_TYPE");
    _keyId[3] = settings.value(QString("MMC_KEY%1").arg(3), KEY_UNDEFINED).toInt();
    _keyId[4] = settings.value(QString("MMC_KEY%1").arg(4), KEY_UNDEFINED).toInt();
    settings.endGroup();
}

void RadioMember::onPress(MMCKey *key)
{
    return;
//    qDebug() << "-------RadioMember::onPress(MMCKey *key)" << key->id();
    switch (_keyId[key->id()]) {
    case KEY_ZOOM_DOWN: //缩小
        zoomDown();
        break;
    case KEY_ZOOM_UP:        //放大
        zoomUp();
        break;
    case KEY_MODE:           //云台模式
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        controlPayload(KEY_MODE, true);
#endif
        break;
    case KEY_PHOTO:          //拍照
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        controlPayload(KEY_PHOTO, true);
#endif
        break;
    case KEY_REC:            //录像
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        controlPayload(KEY_REC, true);
#endif
        break;
    case KEY_FN:             //FN -- 复合功能
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        controlPayload(KEY_FN, true);
#endif
        break;
    default:
        break;
    }
}

void RadioMember::onUpspring(MMCKey *key)
{
    return;
    switch (_keyId[key->id()]) {
    case KEY_ZOOM_DOWN: //缩小
        zoomStop();
        break;
    case KEY_ZOOM_UP:        //放大
        zoomStop();
        break;
    case KEY_MODE:           //云台模式  -- 长按压
        if(key->accumulatedTime()  > 2) locking();
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        controlPayload(KEY_MODE, false);
#endif
        break;
    case KEY_PHOTO:          //拍照
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        controlPayload(KEY_PHOTO, false);
#endif
        break;
    case KEY_REC:            //录像
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        controlPayload(KEY_REC, false);
#endif
        break;
    case KEY_FN:             //FN -- 复合功能
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        controlPayload(KEY_FN, false);
#endif
        break;
    default:
        break;
    }
}

void RadioMember::onLongPress(MMCKey *key)
{
    return;
    switch (_keyId[key->id()]) {
    case KEY_ZOOM_DOWN: //缩小
        zoomDown();
        break;
    case KEY_ZOOM_UP:        //放大
        zoomUp();
        break;
    case KEY_MODE:           //云台模式
        break;
    case KEY_PHOTO:          //拍照
        break;
    case KEY_REC:            //录像
        break;
    default:
        break;
    }
}

void RadioMember::onClick(MMCKey *key)
{
    return;
    switch (_keyId[key->id()]) {
    case KEY_ZOOM_DOWN: //缩小
        break;
    case KEY_ZOOM_UP:        //放大
        break;
    case KEY_FN:             //FN -- 复合功能
        fnClick();
        break;
    case KEY_MODE:           //云台模式
        Mode();
        break;
    case KEY_PHOTO:          //拍照
        photo();
        break;
    case KEY_REC:            //录像
        REC();
        break;

        /* 其它扩展功能 -- 服务自定义功能键 */
    case KEY_UNDEFINED:                     //未定义
        break;
    default:
        break;
    }
}

/* **********************************************************************
 * 按键可用方法
 * **********************************************************************/
void RadioMember::photo()
{
//#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
//    Vehicle* activeVehicle = qgcApp()->toolbox()->multiVehicleManager()->activeVehicle();
//    if(activeVehicle){
//        MountInfo* mount = activeVehicle->mmcMountManager()->currentMount();
//        if(mount)
//            mount->doCameraTrigger();
//    }
//#endif
}

void RadioMember::REC()
{
//#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
//    Vehicle* activeVehicle = qgcApp()->toolbox()->multiVehicleManager()->activeVehicle();
//    if(activeVehicle){
//        MountInfo* mount = activeVehicle->mmcMountManager()->currentMount();
//        if(mount && mount->mountType() == MountInfo::MOUNT_CAM_INFO){
//            CameraMount* camMount = dynamic_cast<CameraMount*>(mount);
//            camMount->videoTape();
//        }
//    }
//#endif
}

void RadioMember::Mode()
{
//#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
//    Vehicle* activeVehicle = qgcApp()->toolbox()->multiVehicleManager()->activeVehicle();
//    if(activeVehicle){
//        MountInfo* mount = activeVehicle->mmcMountManager()->currentMount();
//        if(mount && mount->mountType() == MountInfo::MOUNT_CAM_INFO){
//            CameraMount* camMount = dynamic_cast<CameraMount*>(mount);
//            int mode = camMount->mode();
//            if(mode == 0) mode = 2;
//            else mode = 3 - mode;
//            camMount->controlMode(mode);
//        }
//        else if(mount && mount->mountType() == MountInfo::MOUNT_BASE)
//        {
//            PayloadMount* payloadMount = dynamic_cast<PayloadMount*>(mount);
//            static int i=0;
//            payloadMount->controlGimbalMode(i);
//            if(i == 0)
//                i=1;
//            else
//                i=0;
//        }
//    }
//#endif
}

void RadioMember::locking()
{
//#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
//    Vehicle* activeVehicle = qgcApp()->toolbox()->multiVehicleManager()->activeVehicle();
//    if(activeVehicle){
//        MountInfo* mount = activeVehicle->mmcMountManager()->currentMount();
//        if(mount && mount->mountType() == MountInfo::MOUNT_CAM_INFO){
//            CameraMount* camMount = dynamic_cast<CameraMount*>(mount);
//            int mode = camMount->mode();
//            if(mode != 0){
//                mode = 0;
//                camMount->controlMode(mode);
//            }
//        }
//        else if(mount && mount->mountType() == MountInfo::MOUNT_BASE) //长按回中
//        {
//            PayloadMount* payloadMount = dynamic_cast<PayloadMount*>(mount);
//            payloadMount->controlGimbalMode(2);
//        }
//    }
//#endif
}

void RadioMember::zoomUp()
{
    if(_zoom == 0) return;
    _zoom = 2;
    controlZoom(_zoom);
}

void RadioMember::zoomDown()
{
    if(_zoom == 2) return;
    _zoom = 0;
    controlZoom(_zoom);
}

void RadioMember::zoomStop()
{
    _zoom = 1;
    controlZoom(_zoom);
}

void RadioMember::controlZoom(int zoom)
{
//#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
//    Vehicle* activeVehicle = qgcApp()->toolbox()->multiVehicleManager()->activeVehicle();
//    if(activeVehicle){
//        MountInfo* mount = activeVehicle->mmcMountManager()->currentMount();
//        if(mount && mount->mountType() == MountInfo::MOUNT_CAM_INFO){
//            CameraMount* camMount = dynamic_cast<CameraMount*>(mount);
//            camMount->controlZoom(zoom);
//        }
//        else if(mount && mount->mountType() == MountInfo::MOUNT_BASE)
//        {
//            PayloadMount* payloadMount = dynamic_cast<PayloadMount*>(mount);
//            payloadMount->controlZoom(zoom);
//        }
//    }
//#endif
}

void RadioMember::fnClick()
{
//#if defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
//    Vehicle* activeVehicle = qgcApp()->toolbox()->multiVehicleManager()->activeVehicle();
//    if(activeVehicle){
//        MountInfo* mount_ = activeVehicle->mmcMountManager()->currentMount();
//        if(mount_){
//            switch (mount_->mountReadId()){
//            case MountInfo::MOUNT_SPEAKE: //喊话器   --  [禁音与恢复] -- 该挂载不需要此功能
//                if(1){

//                }
//                break;
//            case MountInfo::MOUNT_4GSPEAKE: //4G喊话器   --  [禁音与恢复]
//                if(1){
//                    Speake4GMount* mount = dynamic_cast<Speake4GMount*>(mount_);
//                    static int volume = 0;
//                    if(mount->volume() != 0) {
//                        volume = mount->volume();
//                        mount->volumeControl(0);
//                    }else{
//                        mount->volumeControl(volume);
//                        volume = 0;
//                    }
//                }
//                break;
//            case MountInfo::MOUNT_LIGHT: //探照灯  --  [开关灯]
//                if(1){
//                    LightMount* mount = dynamic_cast<LightMount*>(mount_);
//                    mount->lightControl(1 - mount->state() != 0);
//                }
//                break;

//            case MountInfo::MOUNT_DROP: //抛投  -- [投掷]
//                if(1){
//                    DropMount* mount = dynamic_cast<DropMount*>(mount_);
//                    mount->dropCmmd();
//                }
//                break;
//            case CameraMount::CAM_SONGXIA20: //20倍松下云台  -- [相机模式]
//                if(1){
//                    CameraMount* mount = dynamic_cast<CameraMount*>(mount_);
//                    mount->switchCameraMode();
//                }
//                break;

//            case CameraMount::CAM_PG2IF1_LS1Z20: //高清双光 20b
//                if(1){
//                    PG2IF1CameraMount* mount = dynamic_cast<PG2IF1CameraMount*>(mount_);
//                    mount->controlFrame(1 - mount->frame());
//                }
//                break;

//            case CameraMount::CAM_Filr: //Filr红外
//            case CameraMount::CAM_PGIY1: //海视英科红外
//                if(1){
//                    CameraMount* mount = dynamic_cast<CameraMount*>(mount_);
//                    mount->switchColor();
//                }
//                break;
//            }
//        }
//    }
//#endif
}


void RadioMember::controlPayload(KEY_TYPE keyType, bool isPress)
{
//    Vehicle* activeVehicle = qgcApp()->toolbox()->multiVehicleManager()->activeVehicle();
//    if(activeVehicle){
//        MountInfo* mount = activeVehicle->mmcMountManager()->currentMount();
//        if(mount && mount->mountType() == MountInfo::MOUNT_BASE)
//        {
//            PayloadMount* payloadMount;
//            payloadMount = dynamic_cast<PayloadMount*>(mount);
//            switch (keyType)
//            {
//            case KEY_ZOOM_DOWN:   //缩小
//                break;
//            case KEY_ZOOM_UP:        //放大
//                break;
//            case KEY_FN:             //FN
//                payloadMount->controlCameraMode(isPress? 1 : 0);
//                break;
//            case KEY_MODE:           //云台模式
//                payloadMount->controlGimbalMode(isPress? 1: 0);
//                break;
//            case KEY_REC:            //录像
//                payloadMount->controlRecodec(isPress? 1: 0);
//                break;
//            case KEY_PHOTO:          //拍照
//                payloadMount->controlPhoto(isPress? 1: 0);
//                break;
//            default:
//                break;
//            }
//        }
//    }
}


