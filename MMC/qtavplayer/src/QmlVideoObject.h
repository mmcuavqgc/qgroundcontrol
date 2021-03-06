﻿/****************************************************************************
* VLC-Qt - Qt and libvlc connector library
* Copyright (C) 2013 Tadej Novak <tadej@tano.si>
*
* Based on Phonon multimedia library
* Copyright (C) 2011 Harald Sitter <sitter@kde.org>
*
* This library is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this library. If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#ifndef VLCQT_QMLVIDEOOBJECT_H_
#define VLCQT_QMLVIDEOOBJECT_H_

#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtQuick/QQuickPaintedItem>
#include <QVideoFrame>

#include "Enums.h"
#include "AVMediaCallback.h"
#include "painter/GlslPainter.h"


//class QVideoFrame;
class ShowImage;
class VlcMediaPlayer;
class AVDecoder;

/*!
    \class VlcQmlVideoObject QmlVideoObject.h VLCQtQml/QmlVideoObject.h
    \ingroup VLCQtQml
    \brief QML video object (deprecated)

    A basic QML video object for painting video. It acts as a replacement for video widget.

    \deprecated Deprecated since VLC-Qt 1.1, will be removed in 2.0
 */
class /*Q_DECL_DEPRECATED */ VlcQmlVideoObject : public QQuickPaintedItem,
                                             public AVMediaCallback
//                                           public VlcVideoMemoryStream
{
Q_OBJECT
public:
    Q_PROPERTY(ShowImage* qFrame READ qFrame NOTIFY qFrameChanged /*CONSTANT*/)

    ShowImage* qFrame(){ return _qFrame; }

    /*!
        \brief VlcQmlVideoObject constructor.
        \param parent parent item
     */
    explicit VlcQmlVideoObject(QQuickItem *parent = 0);

    /*!
        VlcMediaPlayer destructor
     */
    virtual ~VlcQmlVideoObject();

    /*!
        \brief Get current aspect ratio
        \return aspect ratio
     */
    Vlc::Ratio aspectRatio() const;

    /*!
        \brief Set aspect ratio
        \param aspectRatio new aspect ratio
     */
    void setAspectRatio(const Vlc::Ratio &aspectRatio);

    /*!
        \brief Get current crop ratio
        \return crop ratio
     */
    Vlc::Ratio cropRatio() const;

    /*!
        \brief Set crop ratio
        \param cropRatio new crop ratio
     */
    void setCropRatio(const Vlc::Ratio &cropRatio);


    Q_INVOKABLE void frameReady();
    Q_INVOKABLE void reset();

signals:
    void newVideoImage(QImage);
    void frameSizeChanged(int, int);
    void qFrameChanged();

protected:
    AVDecoder *_decoder;

private:
    virtual void *lockCallback(void **planes);
    virtual void unlockCallback();
    virtual void formatCleanUpCallback();
    virtual void sendimage(QImage img);

    virtual QRectF boundingRect() const;
    void geometryChanged(const QRectF &newGeometry,
                         const QRectF &oldGeometry);

    void paint(QPainter *painter);

    void lock();
    bool tryLock();
    void unlock();

    void updateBoundingRect();
    void updateAspectRatio();
    void updateCropRatio();

private:
    QMutex          _mutex;
    QMutex          _flagMutex;
    VlcVideoFrame   _frame;
    QRectF          _geometry;
    QRectF          _boundingRect;
    QSize           _frameSize;
    GlslPainter *   _graphicsPainter;
    bool            _paintedOnce;
    bool            _gotSize;
    Vlc::Ratio      _aspectRatio;
    Vlc::Ratio      _cropRatio;
    ShowImage*      _qFrame;
    bool            _updateFlag;
};

#endif // VLCQT_QMLVIDEOOBJECT_H_
