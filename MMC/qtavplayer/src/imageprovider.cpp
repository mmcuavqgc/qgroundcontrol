#include "imageprovider.h"
#include <QtQml/QtQml>

ImageProvider::ImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{

}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(id)
    Q_UNUSED(size)
    Q_UNUSED(requestedSize)
    return this->img;
}

QPixmap ImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(id)
    Q_UNUSED(size)
    Q_UNUSED(requestedSize)
    return QPixmap::fromImage(this->img);
}

void ImageProvider::setImage(QImage imge)
{
//    lock();
//    QMutexLocker locker(&_mutex);

//    if(imge.width() > 1600)
//        this->img = imge.scaled(img.width()/2, img.height()/2);
//    else
        this->img = imge.copy();

//    unlock();
}


void ImageProvider::lock()
{
    _mutex.lock();
}

bool ImageProvider::tryLock()
{
    return _mutex.tryLock();
}

void ImageProvider::unlock()
{
    _mutex.unlock();
}

ShowImage::ShowImage(QObject *parent) :
    QObject(parent)
{
    m_pImgProvider = new ImageProvider();
}

void ShowImage::registerPlugin()
{
    qmlRegisterType<ShowImage>("VLCQt", 1, 1, "showImage");
}

void ShowImage::sendimage(QImage sendimage)
{
    m_pImgProvider->setImage(sendimage);
//    emit callQmlRefeshImg();
}

void  ShowImage::sendQmlRefeshIm()
{
    emit callQmlRefeshImg();
}

