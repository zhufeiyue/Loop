import QtQuick 2.12
import QtQuick.Controls 2.5
import QuickVideoRendering 1.0

ApplicationWindow {
    id: window
    visible: true
    width: 640
    height: 480
    title: "测试"

    Rectangle{
        anchors.fill: parent
//        color: "red"
    }

    VideoRender{
        //anchors.left: parent.left
        //anchors.top: parent.top
        //width: parent.width/2
        //height: parent.height/2
       anchors.fill: parent
       z: 2

        objectName: "render1"

        Component.onCompleted: {
            console.log("here")
        }
    }

    VideoRender{
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: parent.width/2
        height: parent.height/2
    }
}
