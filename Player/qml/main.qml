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
        color: "red"
    }

    VideoRender{
        anchors.fill: parent
    }
}
