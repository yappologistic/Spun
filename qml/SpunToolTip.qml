import QtQuick
import QtQuick.Controls

ToolTip {
    id: bubble
    objectName: "spunToolTip"
    popupType: Popup.Item
    focus: false
    modal: false
    dim: false
    enabled: false
    closePolicy: Popup.NoAutoClose
    delay: 650
    timeout: -1
    margins: 8
    horizontalPadding: SpunStyle.textGap
    verticalPadding: SpunStyle.gap
    implicitWidth: Math.min(260, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    x: parent ? (parent.width - width) / 2 : 0
    y: -height - 8
    font.family: SpunStyle.family
    font.pixelSize: SpunStyle.caption
    font.weight: Font.Normal
    readonly property bool motion: player.motion && theme.motionScale > 0
    contentItem: SpunText {
        text: bubble.text
        font: bubble.font
        textFormat: Text.PlainText
        color: theme.colors.text
        wrapMode: Text.WordWrap
        lineHeightMode: Text.FixedHeight; lineHeight: 16
    }
    background: Rectangle {
        color: SpunStyle.popup
        radius: SpunStyle.rowRadius
        border.width: 0
    }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
}
