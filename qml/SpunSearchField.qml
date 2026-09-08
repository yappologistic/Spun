import QtQuick
import QtQuick.Controls
TextField {
    id: field
    required property var app
    property bool searchIcon: true
    Accessible.name: placeholderText
    implicitHeight: SpunStyle.target
    font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
    color: app.ink; placeholderTextColor: app.mutedInk
    selectionColor: app.accent; selectedTextColor: theme.colors.onAccent
    leftPadding: searchIcon ? 40 : 16; rightPadding: 40
    background: Rectangle {
        color: field.app.inset
        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: field.app.ink; focused: field.activeFocus; hovered: field.hovered }
        radius: height / 2 * theme.radius
        border.width: field.activeFocus ? 2 : 0; border.color: field.app.accent
    }
    Glyph { visible: field.searchIcon; x: SpunStyle.textGap; anchors.verticalCenter: parent.verticalCenter; width: SpunStyle.smallIcon; height: width; name: "search"; ink: field.app.mutedInk }
}
