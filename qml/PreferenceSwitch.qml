import QtQuick
import QtQuick.Controls
Switch {
    id: control
    required property var app
    property string glyphName: ""
    implicitHeight: Math.max(48, preferenceLabel.implicitHeight + 16)
    hoverEnabled: true; focusPolicy: Qt.StrongFocus
    padding: 0; spacing: 0
    font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
    Accessible.name: text
    background: Rectangle {
        radius: SpunStyle.rowRadius
        color: "transparent"
        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: control.app.ink; enabled: control.enabled; pressed: control.down; focused: control.visualFocus; hovered: control.hovered }
        border.width: control.visualFocus ? 2 : 0; border.color: control.app.accent
    }
    indicator: Rectangle {
        x: control.width - width - 12; anchors.verticalCenter: parent.verticalCenter
        objectName: "switchTrack"
        width: 52; height: 32; radius: 16
        color: control.checked ? control.app.accent : control.app.inset
        border.width: control.checked ? 0 : 2
        border.color: control.app.mutedInk
        opacity: control.enabled ? 1 : SpunStyle.disabledOpacity
        Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        Rectangle {
            SpunSpring { id: thumbPosition; targetValue: control.checked ? 36 : 16; epsilon: .1 }
            SpunSpring { id: thumbSize; targetValue: control.down ? 28 : control.checked ? 24 : 16; epsilon: .1 }
            objectName: "switchThumb"
            x: thumbPosition.value - width / 2; anchors.verticalCenter: parent.verticalCenter
            width: thumbSize.value; height: width; radius: width / 2
            color: control.checked ? theme.colors.onAccent : control.app.mutedInk
            Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        }
    }
    contentItem: Item {
        opacity: control.enabled ? 1 : SpunStyle.disabledOpacity
        Glyph { x: 12; anchors.verticalCenter: parent.verticalCenter; name: control.glyphName; ink: control.app.mutedInk }
        SpunText { id: preferenceLabel; objectName: "preferenceLabel"; x: 44; width: control.width - x - 76; anchors.verticalCenter: parent.verticalCenter; text: control.text; font: control.font; color: control.app.ink; wrapMode: Text.WordWrap }
    }
}
