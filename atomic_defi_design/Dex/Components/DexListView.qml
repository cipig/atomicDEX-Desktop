import QtQuick 2.15
import QtQuick.Controls 2.15

import "../Constants"
import App 1.0

ListView
{
    id: root

    property alias position: scrollVert.position
    property alias scrollVert: scrollVert
    property bool scrollbar_visible: contentItem.childrenRect.height > height
    readonly property double scrollbar_margin: scrollbar_visible ? 8 : 0
    property bool visibleBackground: false

    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: DexScrollBar
    {
        id: scrollVert
        visibleBackground: root.visibleBackground
    }

    implicitWidth: contentItem.childrenRect.width
    implicitHeight: contentItem.childrenRect.height

    clip: true

    // Opacity animation
    opacity: root.count === 0 ? 0 : enabled ? 1 : 0.2
    Behavior on opacity
    {
        SmoothedAnimation
        {
            duration: Style.animationDuration * 0.5;velocity: -1
        }
    }
}