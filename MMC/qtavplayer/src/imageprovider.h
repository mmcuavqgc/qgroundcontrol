#ifndef IMAGEPROVIDER_H
#define IMAGEPROVIDER_H
#include <QQuickImageProvider>
#include <QImage>
#include <QTcpSocket>
#include <QTcpServer>
#include <QBuffer>
#include <QMutex>

class ImageProvider : public QQuickImageProvider
{
public:
    ImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);

    void setImage(QImage);
    QImage img;
    QMutex _mutex;

    void lock();
    bool tryLock();
    void unlock();
};
class ShowImage : public QObject
{
    Q_OBJECT
public:
    explicit ShowImage(QObject *parent = 0);
    ImageProvider *m_pImgProvider;
    static void registerPlugin();

public slots:
    void sendimage(QImage);
    void sendQmlRefeshIm();

signals:
    void callQmlRefeshImg();
    void sendPic(QImage image);
};

#endif // IMAGEPROVIDER_H
