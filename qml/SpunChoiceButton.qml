import QtQuick
import QtQuick.Controls

AbstractButton {
    id: control
    property color ink: theme.colors.text
    property color mutedInk: theme.colors.muted
    property color accent: theme.colors.accent
    property bool selected: false
    property bool pill: true
    implicitHeight: 36
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: text
    background: Rectangle {
        radius: 18 * theme.radius
        color: "transparent"
        Rectangle {
            anchors.fill: parent; radius: parent.radius; color: SpunStyle.selected; opacity: control.selected && control.pill ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        }
        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: control.ink; enabled: control.enabled; pressed: control.down; focused: control.visualFocus; hovered: control.hovered }
        border.width: control.visualFocus ? 2 : 0
        border.color: control.accent
    }
    contentItem: SpunText {
        text: control.text; color: control.selected ? control.accent : control.mutedInk
        font.family: SpunStyle.family; font.pixelSize: SpunStyle.body; font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        opacity: control.enabled ? 1 : SpunStyle.disabledOpacity
        Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
    }
}
