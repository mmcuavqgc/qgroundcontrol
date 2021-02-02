import QtQuick 2.5
import QtQuick.Window 2.2
import qtavplayer 1.0
import QtMultimedia 5.5

Rectangle {
    anchors.fill: parent
    color:        Qt.rgba(0,0,0,0.75)
    property bool record: false  //保存视频流用接口

    function saveImage(){ //保存图片
        avplayer.saveImage()
    }

    onRecordChanged: {
        avplayer.saveTs(record)
    }

    VideoOutput {
        source: avplayer.rtspPlayer
        anchors.fill: parent
    }

    AVPlayer{
        id : avplayer
    }

    Component.onCompleted: {
        avplayer.setUrl(videoSource)
    }
}
