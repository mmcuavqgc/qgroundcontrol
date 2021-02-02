#include "libusb.h"
//#include <atlstr.h> // CString
#include <QDebug>

//MonitorThread *monitorThread;

usb_dev_handle *COMMONDEV = NULL;
usb_dev_handle *VIDEODEV = NULL;
usb_dev_handle *AUDIODEV = NULL;
usb_dev_handle *CUSTOMERDEV = NULL;

QMutex _mutexCOMMONDEV;
QMutex _mutexVIDEODEV;
QMutex _mutexAUDIODEV;
QMutex _mutexCUSTOMERDEV;

static LibUsb* usbExample = NULL;
LibUsb *getUsbExample()
{
    if(!usbExample)
        usbExample = new LibUsb();
    return usbExample;
}

LibUsb::LibUsb(QObject *parent) : QObject(parent)
{
    devListModel = new QStringListModel(devList);

    _updateHidTimer = new QTimer(this);
    _updateHidTimer->setSingleShot(false);
    _updateHidTimer->setInterval(3000);
    connect(_updateHidTimer, SIGNAL(timeout()), this, SLOT(UpdateDeviceList()));

    hidThread = new HidThread();
    connect(hidThread, SIGNAL(sendReadData(QByteArray)), this, SLOT(upConfigData(QByteArray)));
    connect(hidThread, SIGNAL(hidLost()), this, SLOT(hidLost()));

    usb_init(); /* initialize the library */
    GetHid("AAAA","AA97");
}

LibUsb::~LibUsb()
{
    if(COMMONDEV){
        usb_close(COMMONDEV);
        COMMONDEV=NULL;
    }

    if(VIDEODEV){
        usb_close(VIDEODEV);
        VIDEODEV=NULL;
    }
    if(AUDIODEV){
        usb_close(AUDIODEV);
        AUDIODEV=NULL;
    }
    if(CUSTOMERDEV){
        usb_close(CUSTOMERDEV);
        CUSTOMERDEV=NULL;
    }

    if(hidThread) {
        delete hidThread;
        hidThread = NULL;
    }
}

bool LibUsb::setConfig()
{
    if(!hidThread)
        return false;
    return hidThread->setConfig();
}

void LibUsb::setTempConfig(QString& value)
{
    if(!hidThread) return ;
    hidThread->setTempPairConfig(value);
}

void LibUsb::setUsbUart5Trans()
{
    if(hidThread)
        hidThread->setUsbUart5Trans();
}

void LibUsb::writeUart5Trans(QByteArray data)
{
    if(hidThread)
        hidThread->writeUart5Trans(data);
}

void LibUsb::upConfigData(QByteArray array)
{
   emit  sendConfigData(array);
}

void LibUsb::hidLost()
{
    qDebug() << "--LibUsb::hidLost()";
    if(!VIDEODEV)
    {
        devList.clear();
        devListModel->removeRows(0,devListModel->rowCount());
        devListModel->setStringList(devList);
        UpdateDeviceList();
        _updateHidTimer->start();
    }
}

bool LibUsb::checkHidList()
{
    struct usb_bus *bus=NULL;
    struct usb_device *dev=NULL;
    int ret;
    ret=usb_find_busses(); /* find all busses */
    //qDebug()<<ret;
    ret=usb_find_devices(); /* find all connected devices */
    //qDebug()<<ret;
    for (bus = usb_busses; bus; bus = bus->next)
    {
        for (dev = bus->devices; dev; dev = dev->next)
        {
            if ((dev->descriptor.idVendor == MY_VID)
                    && (dev->descriptor.idProduct == MY_PID))
            {
                return true;
            }
        }
    }
    return false;
}

void LibUsb::GetHid(QString vid, QString pid)
{
    struct usb_bus *bus=NULL;
    struct usb_device *dev=NULL;
    usb_dev_handle *tmpdev[5]={0,0,0,0,0};
    memset(tmpdev,0,5);
    int index=0;
    int ret;

    ret=usb_find_busses(); /* find all busses */
    //qDebug()<<ret;
    ret=usb_find_devices(); /* find all connected devices */
    //qDebug()<<ret;

    if (vid=="AAAA" &&pid=="AA97" )
    {
        for (bus = usb_busses; bus; bus = bus->next)
        {
            for (dev = bus->devices; dev; dev = dev->next)
            {
                if ((dev->descriptor.idVendor == MY_VID)
                        && (dev->descriptor.idProduct == MY_PID))
                {
                    //COMMONDEV=usb_open(dev);
                    //qDebug()<<"bNumConfigurations:"+dev->descriptor.bNumConfigurations;
                    for (int i=0;i<dev->descriptor.bNumConfigurations;i++)
                    {
                        //qDebug()<<"bNumInterfaces:"+dev->config[i].bNumInterfaces;
                        for (int j=0;j<dev->config[i].bNumInterfaces;j++)
                        {
                            tmpdev[index]=usb_open(dev);
                            index++;
                        }
                    }
                }
            }
        }
        COMMONDEV = tmpdev[0]; //interface 0
        VIDEODEV = tmpdev[1]; //interface 1
        AUDIODEV = tmpdev[2]; //interface 2
        CUSTOMERDEV= tmpdev[3]; //interface 3
        qDebug() << "------ libusb interface 0 1 2 3" <<COMMONDEV<<VIDEODEV<<AUDIODEV<<CUSTOMERDEV;

        if (COMMONDEV!=NULL)
        {
            ret=usb_set_configuration(COMMONDEV, MY_CONFIG);

            if (ret < 0)
            {
                printf("error setting config #%d: %s\n", MY_CONFIG, usb_strerror());
                if (COMMONDEV!=NULL)
                {
                    usb_close(COMMONDEV);
                    COMMONDEV=NULL;
                }
            }
            else
            {
                printf("success: set configuration #%d\n", MY_CONFIG);
            }

            if (usb_claim_interface(COMMONDEV, 0) <0)
            {
                printf("error claiming interface #%d:\n%s\n", MY_INTF, usb_strerror());
                if (COMMONDEV!=NULL)
                {
                    usb_close(COMMONDEV);
                    COMMONDEV=NULL;
                }
            }
            else
            {
                printf("success: claim_interface #%d\n", MY_INTF);
            }
            if (COMMONDEV!=NULL)
            {
                //AddTreeRoot("Ar8020_interface_0","");
                devList.append("Ar8020_interface_0");
            }

        }


        if (VIDEODEV!=NULL)
        {
            ret=usb_set_configuration(VIDEODEV, MY_CONFIG);

            if (ret < 0)
            {
                printf("error setting config #%d: %s\n", MY_CONFIG, usb_strerror());
                if (VIDEODEV!=NULL)
                {
                    usb_close(VIDEODEV);
                    VIDEODEV=NULL;
                }
            }
            else
            {
                printf("success: set configuration #%d\n", MY_CONFIG);
            }

            if (usb_claim_interface(VIDEODEV, 1) <0)
            {
                printf("error claiming interface #%d:\n%s\n", MY_INTF, usb_strerror());
                if (VIDEODEV!=NULL)
                {
                    usb_close(VIDEODEV);
                    VIDEODEV=NULL;
                }
            }
            else
            {
                printf("success: claim_interface #%d\n", MY_INTF);
            }
            if (VIDEODEV!=NULL)
            {
                //AddTreeRoot("Ar8020_interface_1","");
                devList.append("Ar8020_interface_1");
            }
        }

        if (AUDIODEV!=NULL)
        {
            ret=usb_set_configuration(AUDIODEV, MY_CONFIG);

            if (ret < 0)
            {
                printf("error setting config #%d: %s\n", MY_CONFIG, usb_strerror());
                if (AUDIODEV!=NULL)
                {
                    usb_close(AUDIODEV);
                    AUDIODEV=NULL;
                }
            }
            else
            {
                printf("success: set configuration #%d\n", MY_CONFIG);
            }

            if (usb_claim_interface(AUDIODEV, 2) <0)
            {
                printf("error claiming interface #%d:\n%s\n", MY_INTF, usb_strerror());
                if (AUDIODEV!=NULL)
                {
                    usb_close(AUDIODEV);
                    AUDIODEV=NULL;
                }
            }
            else
            {
                printf("success: claim_interface #%d\n", MY_INTF);
            }
            if (AUDIODEV!=NULL)
            {
                //AddTreeRoot("Ar8020_interface_2","");
                devList.append("Ar8020_interface_2");
            }
        }

        if (CUSTOMERDEV!=NULL)
        {
            ret=usb_set_configuration(CUSTOMERDEV, MY_CONFIG);

            if (ret < 0)
            {
                printf("error setting config #%d: %s\n", MY_CONFIG, usb_strerror());
                if (CUSTOMERDEV!=NULL)
                {
                    usb_close(CUSTOMERDEV);
                    CUSTOMERDEV=NULL;
                }
            }
            else
            {
                printf("success: set configuration #%d\n", MY_CONFIG);
            }

            if (usb_claim_interface(CUSTOMERDEV, 3) <0)
            {
                printf("error claiming interface #%d:\n%s\n", MY_INTF, usb_strerror());
                if (CUSTOMERDEV!=NULL)
                {
                    usb_close(CUSTOMERDEV);
                    CUSTOMERDEV=NULL;
                }
            }
            else
            {
                printf("success: claim_interface #%d\n", MY_INTF);
            }
            if (CUSTOMERDEV!=NULL)
            {
                //AddTreeRoot("Ar8020_interface_3","");
                devList.append("Ar8020_interface_3");
            }
        }
    }
    if(VIDEODEV)
        _updateHidTimer->stop();
    devListModel->setStringList(devList); //设备名称
}

void LibUsb::UpdateDeviceList()
{
//    qDebug() << "------------LibUsb::UpdateDeviceList()";
    if(checkHidList())
        GetHid("AAAA","AA97");
}
