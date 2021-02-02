#include "usbinterface.h"
#include <QDateTime>
#include <QDebug>
//#include "artosynplayer.h"
#include "hidapi.h"
#include "libusb.h"
#include "../src/fifo.h"

HDCOMD iHDCOMD;

HidThread *hidThread;
extern usbfifo *g_fifo;
extern QMutex usb_byte_fifo_mutex;


static uint16_t ArtoSyn_CheckSum(uint8_t *pBuffer, uint16_t len)
{
    uint16_t u16Index = 0;
    uint16_t u16Sum = 0;
    for(u16Index = 0; u16Index < len; u16Index++)
    {
        u16Sum += (uint16_t)(pBuffer[u16Index]);
    }

    return u16Sum;
}

void UsbHidTask::run()
{
    switch(command){
    case UsbHidTaskCommand_Read:
        mCodec->readVideo();
        break;
    case UsbHidTaskCommand_ReadyRead:
        mCodec->readData();
        break;
    }
}

int HidThread::write(char* data,int len)
{
    QMutexLocker locker(&_mutexCOMMONDEV);
    if(COMMONDEV)
        return transfer_bulk_async(COMMONDEV, EP_OUT2, data, len, 100);
    else
        return -1;
}

void HidThread::addReadTask()
{
    if(mReadUsbThread.size() < 2)
        mReadUsbThread.addTask(new UsbHidTask(this,UsbHidTask::UsbHidTaskCommand_Read));
}

void HidThread::addReadyReadTask()
{
    if(mReadyReadThread.size() < 2)
        mReadyReadThread.addTask(new UsbHidTask(this,UsbHidTask::UsbHidTaskCommand_ReadyRead));
}

void HidThread::readVideo(int timeout)
{
    addReadTask();
    QMutexLocker locker(&_mutexVIDEODEV);
    if(VIDEODEV == NULL)
        return;
    static unsigned char buf[BUF_SIZE_HID*12];
    int ret = transfer_bulk_async(VIDEODEV, EP_IN2, (char*)buf, sizeof(buf), timeout);

    if (ret > 0 && g_fifo!=NULL)
    {
        ProcessVideoRead(buf,ret);
    }
    else if(ret == -5)
    {
        VIDEODEV = NULL;
        emit hidLost();
    }
}

void HidThread::readData(int timeout)
{
    addReadyReadTask();
    if(COMMONDEV == NULL)
        return;

    /*static */unsigned char buff[BUF_SIZE_HID*2];
    _mutexCOMMONDEV.lock();
    int ret  = transfer_bulk_async(COMMONDEV, EP_IN3, (char*)buff, sizeof(buff), timeout);
    _mutexCOMMONDEV.unlock();
    if (ret > 0)
    {
        QByteArray array((char*)buff, ret);
        emit sendReadData(array); //直接传会访问异常  非深拷贝
    }
    else if(ret == -5)
    {
        COMMONDEV = NULL;
        emit hidLost();
    }
}

bool HidThread::getConfig()
{
    static bool flag = false;
    if(!flag){
        flag = setOSD(true);
    }

    char buff[10] = {0xFF, 0x5A, 0x19, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    int ret = write((char*)buff, 10);
    return ret > 0;
}

bool HidThread::setConfig()
{
    char buff[11] = {0xFF, 0x5A, 0x5B, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01};
    int ret = write((char*)buff, 11);
    return ret > 0;
}

bool HidThread::setOSD(bool state)
{
    char buff[11] = {0xFF, 0x5A, 0x11, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01};
    if(!state){
        buff[10] = 0x00;
        buff[8]  = 0x00;
    }
    int ret = write((char*)buff, 11);
    return ret > 0;
}

void HidThread::setUsbUart5Trans()
{
    char buff[10] = {0xFF, 0x5A, 0x87, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    write((char*)buff, 10);
}

void HidThread::writeUart5Trans(QByteArray data)
{
    char packet[512] = {0};
    char buff[10] = {0xFF, 0x5A, 0x86, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint16_t len = data.length();
    buff[6] = (char)len & 0xFF;
    buff[7] = (char)(len >> 8) & 0xFF;
    uint16_t chk_sum = ArtoSyn_CheckSum((uint8_t *)data.data(), data.length());
    buff[8] = (char)chk_sum & 0xFF;
    buff[9] = (char)(chk_sum >> 8) & 0xFF;
    qDebug() << "----HidThread::writeUart5Trans" << QByteArray(buff, 10).toHex() << data.length();

    memcpy(packet, buff, 10);
    memcpy(packet + 10, data.data(), data.length());
    write(packet, 10+data.length());
}

void HidThread::setTempPairConfig(QString& number)
{
    QString num = number;
    num = num.replace(" ","");
    int numbers = num.length();

    char payload[7] = {0};
    char buff[17] = {0xFF, 0x5A, 0x67, 0x00, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00};

    bool ok;
    if(numbers%2 == 0)
    {
        int k=0;
        for (int i=0;i<numbers;i+=2)
        {
            payload[k] = num.mid(i,2).toInt(&ok,16);
            if(!ok)
            {
                qDebug("Error data format!");
                return;
            }
            k++;
        }
    }

    memcpy(&buff[10],payload,7);
    uint16_t chk_sum = ArtoSyn_CheckSum((uint8_t *)payload, 7);
    buff[8] = (char)chk_sum & 0xFF;
    buff[9] = (char)(chk_sum >> 8) & 0xFF;

    write(buff, 17);
}

void HidThread::ProcessVideoRead(BYTE* buf,int len)
{
    if(g_fifo)
        g_fifo->write(buf,len);
}

HidThread::HidThread()
{
    _readConfigTimer = new QTimer(this);
    connect(_readConfigTimer, &QTimer::timeout, this, &HidThread::getConfig);
    _readConfigTimer->start(1000);

//    _refreshHidTimer = new QTimer(this);
//    _refreshHidTimer->setSingleShot(false);
//    _refreshHidTimer->setInterval(1000);

    addReadTask();
    addReadyReadTask();
}

HidThread::~HidThread()
{
    qDebug() << "HidThread release";
    mReadUsbThread.stop();
    mReadyReadThread.stop();
}

int HidThread::transfer_bulk_async(usb_dev_handle *dev,int ep,char *bytes,int size,int timeout)
{
	if (dev==NULL)
	{
		return -1;
	}

	void* async_context = NULL;
	int ret;
	// Setup the async transfer.  This only needs to be done once
	// for multiple submit/reaps. (more below)
	//
	if (ep==EP_OUT2)
	{
		ret=usb_interrupt_setup_async(dev, &async_context, ep);
	}
	/*else if(ep==EP_IN2)
	{
		ret=usb_isochronous_setup_async(dev, &async_context, ep,size);
	}*/
	else
	{
		ret = usb_bulk_setup_async(dev, &async_context, ep);
	}
	
	if (ret < 0)
	{
		//qDebug()<<"1111111111"<<usb_strerror();

		//printf("error usb_bulk_setup_async:\n%s\n", usb_strerror());
		goto Done;
	}

	// Submit this transfer.  This function returns immediately and the
	// transfer is on it's way to the device.
	//
	ret = usb_submit_async(async_context, bytes, size);
	if (ret < 0)
	{
		//qDebug()<<"222222222222222"<<usb_strerror();
		//printf("error usb_submit_async:\n%s\n", usb_strerror());
		usb_free_async(&async_context);
		goto Done;
	}

	// Wait for the transfer to complete.  If it doesn't complete in the
	// specified time it is cancelled.  see also usb_reap_async_nocancel().
	//
	
	ret = usb_reap_async(async_context, timeout);
	//ret = usb_reap_async_nocancel(async_context, timeout);

	// Free the context.
	usb_free_async(&async_context);
//    _hidLost = false;

Done:
	return ret;
}

