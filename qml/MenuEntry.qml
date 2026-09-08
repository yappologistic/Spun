import QtQuick
import QtQuick.Controls
MenuItem {
    id: entry
    required property var app
    property string glyphName: ""
    property string hint: ""
    implicitWidth: 288; implicitHeight: SpunStyle.target
    height: visible ? implicitHeight : 0
    padding: 0; indicator: null; arrow: null
    font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
    background: Rectangle {
        radius: SpunStyle.rowRadius
        color: "transparent"
        border.width: entry.visualFocus ? 2 : 0; border.color: entry.app.accent
        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: entry.app.ink; enabled: entry.enabled; pressed: entry.down; focused: entry.highlighted; hovered: entry.hovered }
    }
    contentItem: Item {
        opacity: entry.enabled ? 1 : SpunStyle.disabledOpacity
        Glyph { visible: entry.glyphName.length > 0; x: 12; anchors.verticalCenter: parent.verticalCenter; name: entry.glyphName; ink: entry.highlighted ? entry.app.accent : entry.app.mutedInk }
        SpunText { x: 44; anchors.verticalCenter: parent.verticalCenter; width: parent.width - x - 12 - trailing.width - 8; text: entry.text; font: entry.font; color: entry.app.ink; elide: Text.ElideRight }
        Item {
            id: trailing
            anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
            width: entry.checkable ? 22 : shortcutLabel.implicitWidth; height: 24
            SpunText { id: shortcutLabel; visible: !entry.checkable; anchors.centerIn: parent; text: entry.hint; color: entry.app.mutedInk; font.family: SpunStyle.family; font.pixelSize: SpunStyle.caption }
            Glyph { visible: entry.checkable && entry.checked; anchors.centerIn: parent; name: "check"; ink: entry.app.accent }
        }
    }
}
