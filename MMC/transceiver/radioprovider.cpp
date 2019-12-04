#include "radioprovider.h"
#include "QGCApplication.h"

#include "QGCSerialPortInfo.h"

#include "MMC/mmcplugin.h"

RadioProvider::RadioProvider(const QString &device)
    : _device(device)
    , _serial(nullptr)
    , _findDevice(false)
    , _isconnect(false)
    , _findDevideName("")
{
    moveToThread(this);

    _openSerialTimer = new QTimer();
    _openSerialTimer->setInterval(1000);
//    _openSerialTimer->setSingleShot(true);
    connect(_openSerialTimer, SIGNAL(timeout()), this, SLOT(openSerialSlot()),Qt::QueuedConnection);

    _serial = new QSerialPort();
    connect(_serial, &QSerialPort::readyRead, this, &RadioProvider::readBytes, Qt::DirectConnection);

    start();
}

RadioProvider::~RadioProvider()
{
    quit();
    wait();

    if (_serial->isOpen()) {
        _serial->close();
        QObject::disconnect(_serial, &QSerialPort::readyRead, this, &RadioProvider::readBytes);
    }
    delete(_serial);
    _serial = nullptr;
}

void RadioProvider::findDevice()
{
//    qDebug() << "-------RadioProvider::findDevice()" << _isconnect;
    static QList<QString> tmpSerialList;
    static int findIndex = 0;
    if(!_isconnect && !_findDevice)
    {
        // 如果遍历完整个串口还没有数据或者第一次进来则进行重新查询
        if(tmpSerialList.count() == 0 || findIndex == tmpSerialList.count())
        {
            findIndex = 0;
            tmpSerialList.clear();
            QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();
            for(int j = 0; j < portList.count(); j++){
                if(portList.at(j).description().contains("CP210x"))
                    tmpSerialList.append(portList.at(j).portName());
            }
        }
        if(tmpSerialList.count() == 0)
            return;
        QSettings settings;
        QString serialName = settings.value("LOCAL_SERIAL_PORT_NAME", "").toString();
        QSerialPort *tmpSerial;
        if(tmpSerialList.contains(serialName) && findIndex == 0)
        {
            tmpSerial = new QSerialPort(serialName);
        }
        else
            tmpSerial = new QSerialPort(tmpSerialList.at(findIndex++));
        tmpSerial->setBaudRate(QSerialPort::Baud115200);
        if(!tmpSerial->open(QIODevice::ReadWrite))
            return;
        QTimer *tmpTimer = new QTimer;
        tmpTimer->setInterval(1000);
        tmpTimer->setSingleShot(true);
        connect(tmpTimer, &QTimer::timeout, this, [=]()
        {
            tmpTimer->stop();
            if(tmpSerial->bytesAvailable())
            {
                QByteArray b = tmpSerial->readAll();
                QString tmpPortName = tmpSerial->portName();
//                qDebug() << "------updateConnectLink" << tmpPortName << QString(b);
                uint8_t tmpData[256] = {0};
                for(int i = 0; i < b.length() && !_findDevice; i++){
                    if(0 < splicePacket(b.at(i), tmpData)){
                        _findDevideName = tmpPortName;
                        qDebug() << "------_findDevideName" << _findDevideName;
                        _findDevice = true;
                        QSettings settings;
                        settings.setValue("LOCAL_SERIAL_PORT_NAME", _findDevideName);
                        break;
                    }
                }
            }
            if(tmpSerial->isOpen())
                tmpSerial->close();
            tmpSerial->deleteLater();
        });
        tmpTimer->start();
    }
}

void RadioProvider::_start()
{
//    qDebug() << "--------------------------RadioProvider::_start()";
    _openSerialTimer->start();
}

void RadioProvider::lostDevice()
{
    _isconnect = false;
    _findDevice = false;
    _findDevideName = "";
    _openSerialTimer->start();
}

int RadioProvider::writeData(char type, QByteArray buff)
{
    return _writeData(type, buff);
}

int RadioProvider::openSerial(QString name)
{
//    qDebug() << "---------------------openSerial" << name;
    static int result = -1;
    if(0 == result) {
        emit closeSerialSignals();
        msleep(10);
        //        _serial->close();
    }
    result = -1;

    _serial->setPortName(name);
    _serial->setBaudRate(QSerialPort::Baud115200);
    if(!_serial->isOpen()){
        if ( !_serial->open(QIODevice::ReadWrite)) {
            qWarning() << "Radio: Failed to open Serial Device" << name << _serial->errorString();
            return -2;
        }
    }else{
        qDebug() << "Radio: this is open Serial Device" << name;
        return -1;
    }

    if (_serial->isOpen()) {
        //查询一次电台 -- 通过接受判断是否为该电台 -- 相当于握手(查询ID)
        char type = 0x8f;
        char buff[1] = {0x01};
        writeData(type, QByteArray(buff, 1));
    }

    qDebug() << "=======================================openSerial" << name;
    result = 0;
    return 0;
}

void RadioProvider::openSerialSlot()
{
    //    qDebug() << "------------------------openSerialSlot" << GetCurrentThreadId();
//    if(!_findDevice)
    findDevice();
    if (!_isconnect && _findDevice){ //没上线
#if defined(Q_OS_ANDROID)
    openSerial("ttysWK1");
#elif defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
        openSerial(_findDevideName);
#endif
    }
    else if(_isconnect)
    {
        _openSerialTimer->stop();
    }
}

void RadioProvider::closeSerialSlot()
{
    if(_serial && _serial->isOpen())
        _serial->close();
}

void RadioProvider::run()
{
    exec();
}

void RadioProvider::readBytes()
{
    if(!_serial)
        return;
    uint8_t tmpData[256];
    int length = _serial->read((char *)tmpData, 256);
    if(length > 0){
        QByteArray buff = QByteArray((char*)tmpData, length);
        analysisPack(buff);
    }
}

void RadioProvider::analysisPack(QByteArray buff)
{
//    qDebug() << "-------------buff" << buff.toHex();
    uint8_t tmpData[256] = {0};
//    memset(tmpData, 0x00, 256 * sizeof(uint8_t));
    for(int i = 0; i < buff.length(); i++){
        if(0 < splicePacket((buff.at(i)), tmpData)){
            if(!_isconnect){
                emit isconnectChanged(true);
            }
            _isconnect = true; //重置标志位
            _openSerialTimer->stop();
            emit RTCMDataUpdate(tmpData[2], QByteArray((char*)&tmpData[4], tmpData[3]));
        }
    }
}

int RadioProvider::splicePacket(const uint8_t data, uint8_t *dst)
{
    static bool packeting = false;
    static QList<uint8_t> packList;

    // 05 5A type len check data
    if(packeting)
    {
        if(packList.count() == 1 && data != 0xA5){ //判断第二个字节
            packeting = false;
            return -1;
        }

        packList.append(data);
        if(packList.count() < 5) return -1;
        // 长度为数据段长度，去掉头，类型，长度，校验，5；
        if(packList.count() > 5 && packList.at(3)+5 <= packList.count())
        {
            packeting = false;
            uint8_t tmpData[255]={0};
            for(int i=0; i < packList.at(3) + 5; i++)
                tmpData[i] = packList.at(i);
#if defined(Q_OS_ANDROID)
            uint8_t check = MMC::_xor8(&tmpData[5], tmpData[3]);
#else
            uint8_t check = MMC::_crc8(&tmpData[2], tmpData[3] + 2);
#endif
//            qDebug() << "----MMC::_crc8" << check << packList.at(packList.count() - 1) << packList;
            if(check != packList.at(packList.count() - 1)) //crc
                return -1;
            memcpy(dst, &tmpData[0], tmpData[3] + 5);
//            qDebug() << "----------packList" << packList << tmpData[4];
//            qDebug() << "-----pack length" << tmpData[3];
            return tmpData[3];
        }
    }
    else if(!packeting && data == 0x5A)
    {
        packList.clear();
        packeting = true;
        packList.append(data);
    }
    return -1;
}

int RadioProvider::_writeData(char type, QByteArray buff)
{
    uint8_t len = buff.length();
    uint8_t fileData[256] = {0};
    uint8_t* buf = fileData;
    *buf++ = 0x5A;   //--头
    *buf++ = 0xA5;   //--id
    *buf++ = type;   //--type
    *buf++ = len;   //--len
    memcpy(buf, buff.data(), len); //data
    buf += len;
    *buf++ = MMC::_crc8(&fileData[2], len + 2);   //--check - CRC 校验长度为实际长度加上类型和长度两个字节
    qDebug() << "------------------RadioProvider::_writeData" << len  << QByteArray((char*)fileData, len + 5).toHex();
    return _serial->write((char*)fileData, len + 5);
}
