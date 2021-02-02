#ifndef USBINTERFACE_H
#define USBINTERFACE_H
#include <QObject>
#include <QMutex>
//#include "artosynplayer.h"

#include <QTimer>
#include "../src/AVThread.h"
#include "stdint.h"

class LibUsb;
class HidThread;
struct usb_dev_handle;
typedef unsigned char       BYTE;

class UsbHidTask : public Task{
public :
    ~UsbHidTask(){}
    enum UsbHidTaskCommand{
        UsbHidTaskCommand_Read,
        UsbHidTaskCommand_ReadyRead,

    };
    UsbHidTask(HidThread *codec,UsbHidTaskCommand command,double param = 0,QString param2 = ""):
        mCodec(codec),command(command),param(param),param2(param2){}
protected :
    /** 现程实现 */
    virtual void run();
private :
    HidThread *mCodec;
    UsbHidTaskCommand command;
    double param;
    QString param2;
};


class HidThread : public QObject
{
	Q_OBJECT
    friend class UsbHidTask;
public: 
	HidThread();
    ~HidThread();

public:
	void ProcessVideoRead(BYTE* buf,int len);
    int write(char* data, int);
	int transfer_bulk_async(usb_dev_handle *dev,int ep,char *bytes,int size,int timeout);

    void addReadTask();
    void addReadyReadTask();
    void writeUart5Trans(QByteArray data);
    void setTempPairConfig(QString& number);

signals:
    void sendReadData(QByteArray);
    void hidLost();
	
protected:
    void readVideo(int timeout = 1000);
    void readData(int timeout = 10);

private:
	QString messageStr; 
	volatile  bool stopped;
    QTimer* _readConfigTimer;

//    QTimer* _refreshHidTimer;
//    bool    _hidLost;//usb设备丢失；
    AVThread mReadUsbThread;
    AVThread mReadyReadThread;

public slots:
    bool getConfig();
    bool setConfig();
    void setUsbUart5Trans();
    bool setOSD(bool state = true);

};
extern HidThread *hidThread;
#endif // USBINTERFACE_H
