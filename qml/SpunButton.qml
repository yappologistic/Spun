import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool tonal: false
    property bool filled: false
    implicitWidth: Math.max(64, contentItem.implicitWidth + 32)
    implicitHeight: SpunStyle.target
    horizontalPadding: 16
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    contentItem: SpunText {
        text: control.text
        font.pixelSize: SpunStyle.body; font.weight: Font.Medium
        color: !control.enabled ? theme.colors.text : control.filled ? theme.colors.onAccent : theme.colors.accent
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        opacity: control.enabled ? 1 : SpunStyle.disabledOpacity
    }
    background: Rectangle {
        SpunSpring { id: corner; targetValue: control.down ? 12 * theme.radius : control.height / 2 * theme.radius }
        radius: corner.value
        color: !control.enabled && (control.tonal || control.filled) ? Qt.alpha(theme.colors.text, SpunStyle.disabledContainerOpacity) : control.filled ? theme.colors.accent : control.tonal ? SpunStyle.selected : "transparent"
        border.width: control.visualFocus ? 2 : 0; border.color: theme.colors.accent
        SpunStateLayer {
            anchors.fill: parent; radius: parent.radius; color: control.filled ? theme.colors.onAccent : theme.colors.accent
            enabled: control.enabled; hovered: control.hovered; pressed: control.down; focused: control.visualFocus
        }
    }
}
