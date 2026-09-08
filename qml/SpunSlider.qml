import QtQuick
import QtQuick.Controls

// Compact desktop slider: shared bar handle, separated tracks and keyboard focus.
Slider {
    id: control
    implicitHeight: SpunStyle.target
    padding: 4
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    property string valueText: Math.round(value * 100) + "%"
    background: Item {
        x: control.leftPadding; y: control.topPadding
        width: control.availableWidth; height: control.availableHeight
        readonly property real center: control.visualPosition * (width - 4) + 2
        opacity: control.enabled ? 1 : SpunStyle.disabledOpacity
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, parent.center - 5); height: 4; radius: 2
            color: control.mirrored ? theme.colors.outline : theme.colors.accent
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: Math.min(parent.width, parent.center + 5); width: parent.width - x; height: 4; radius: 2
            color: control.mirrored ? theme.colors.accent : theme.colors.outline
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: control.mirrored ? 0 : parent.width - width
            width: 3; height: 3; radius: 1.5; color: theme.colors.accent
            visible: control.position < .95
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - 4)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 4; height: control.pressed ? 20 : 16; radius: 2
        color: theme.colors.accent; opacity: control.enabled ? 1 : SpunStyle.disabledOpacity
        Behavior on height { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        Rectangle {
            anchors.centerIn: parent; width: 16; height: 28; radius: 8
            color: "transparent"; border.width: control.visualFocus ? 2 : 0; border.color: theme.colors.accent
        }
    }
    SpunToolTip { visible: control.enabled && (control.pressed || control.visualFocus); delay: 0; text: control.valueText }
}
