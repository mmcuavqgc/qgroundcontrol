/*
* VLC-Qt QML Player
* Copyright (C) 2015 Tadej Novak <tadej@tano.si>
*/

import QtQuick              2.5
import VLCQt                1.1
import QtMultimedia         5.5
import QGroundControl           1.0
import QtQuick.Window           2.2
import QtGraphicalEffects       1.0
import "qrc:/qml/"

Rectangle {
    anchors.fill:       parent
    color:              Qt.rgba(0,0,0,0.75)

    property string     videoSource:    "rtmp://live.hkstv.hk.lxdns.com/live/hks1"
    property alias      record:         vidEncoder.record  //保存视频流用接口

    function saveImage(){ //保存图片
        vidEncoder.saveImage()
    }

    MouseArea {
        anchors.fill: parent
        onDoubleClicked: {
            QGroundControl.videoManager.fullScreen = !QGroundControl.videoManager.fullScreen
        }
    }


    VlcVideoPlayer {
        id:             vidEncoder
        visible:        true
        anchors.fill:   parent
        onStateChanged: {
            console.log("--------------onStateChanged", state)
        }
    }

    Text {
        z:1
        anchors.top: parent.top
        anchors.left: parent.left
        color: vidEncoder && vidEncoder.encodecStatus ? "#0f0" : "#900"
        text:  vidEncoder && vidEncoder.encodecStatus ? "RTMP push OK ": "RTMP push NG"
    }


    Connections {
        target: mainWindowInner.item
        onReallyClose: {
            console.log("onReallyClose-----");
            vidEncoder.destroy();
        }
    }

    Component.onCompleted: {
        vidEncoder.url = videoSource
    }
}








