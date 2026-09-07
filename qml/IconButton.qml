import QtQuick
import QtQuick.Controls

AbstractButton {
    id: control
    property string glyphName: "play"
    property string tip: ""
    property color ink: "#eee8de"
    property color fill: "transparent"
    property color hoverFill: "#30ffffff"
    property bool selected: false
    property int glyphSize: SpunStyle.icon
    readonly property bool motionEnabled: SpunStyle.motion
    readonly property int motionTime: SpunStyle.feedback
    implicitWidth: SpunStyle.target; implicitHeight: SpunStyle.target
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: tip
    SpunToolTip { visible: control.hovered && control.enabled && control.visible && !control.down && control.tip.length > 0; text: control.tip }
    background: Rectangle {
        SpunSpring { id: cornerMotion; targetValue: Math.min(control.width, control.height) * (control.down ? .35 : .5) }
        radius: cornerMotion.value
        color: control.fill
        Rectangle {
            anchors.fill: parent; radius: parent.radius; color: SpunStyle.selected
            opacity: control.selected && control.fill.a === 0 ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback } }
        }
        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: control.ink; enabled: control.enabled; pressed: control.down; focused: control.visualFocus; hovered: control.hovered }
        border.width: control.visualFocus ? 2 : 0
        border.color: theme.colors.accent
    }
    contentItem: Item {
        SpunSpring { id: pressMotion; targetValue: control.down && control.motionEnabled ? .95 : 1; epsilon: .002 }
        scale: pressMotion.value
        Glyph { anchors.centerIn: parent; width: control.glyphSize; height: width; name: control.glyphName; ink: control.ink; opacity: control.enabled ? 1 : 0.3 }
    }

}
