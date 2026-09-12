import QtQuick
import QtQuick.Controls

TextField {
    id: field
    required property var app
    property bool searchIcon: true
    property string labelText: ""
    readonly property bool formField: labelText.length > 0
    readonly property bool raisedLabel: activeFocus || text.length > 0
    Accessible.name: formField ? labelText : placeholderText
    implicitHeight: formField ? 56 : SpunStyle.target
    font.family: SpunStyle.family
    font.pixelSize: SpunStyle.body
    color: app.ink
    placeholderTextColor: app.mutedInk
    selectionColor: app.accent
    selectedTextColor: theme.colors.onAccent
    leftPadding: searchIcon ? 40 : 16
    rightPadding: formField ? 16 : 40
    topPadding: formField ? 24 : 0
    bottomPadding: formField ? 8 : 0
    background: Rectangle {
        color: field.app.inset
        SpunStateLayer {
            anchors.fill: parent
            radius: parent.radius
            color: field.app.ink
            focused: field.activeFocus
            hovered: field.hovered
        }
        radius: field.formField ? 4 * theme.radius : height / 2 * theme.radius
        bottomLeftRadius: field.formField ? 0 : radius
        bottomRightRadius: field.formField ? 0 : radius
        border.width: !field.formField && field.activeFocus ? 2 : 0
        border.color: field.app.accent
    }
    Rectangle {
        visible: field.formField
        anchors.bottom: parent.bottom
        width: parent.width
        height: field.activeFocus ? 2 : 1
        color: field.activeFocus ? field.app.accent : field.app.mutedInk
    }
    SpunText {
        visible: field.formField
        x: field.leftPadding
        y: field.raisedLabel ? 7 : (field.height - implicitHeight) / 2
        text: field.labelText
        font.pixelSize: field.raisedLabel ? SpunStyle.caption : SpunStyle.body
        color: field.activeFocus ? field.app.accent : field.app.mutedInk
        Behavior on y {
            NumberAnimation {
                duration: SpunStyle.feedback
                easing.type: Easing.BezierSpline
                easing.bezierCurve: SpunStyle.effectsCurve
            }
        }
        Behavior on font.pixelSize {
            NumberAnimation {
                duration: SpunStyle.feedback
                easing.type: Easing.BezierSpline
                easing.bezierCurve: SpunStyle.effectsCurve
            }
        }
    }
    Glyph {
        visible: field.searchIcon
        x: SpunStyle.textGap
        anchors.verticalCenter: parent.verticalCenter
        width: SpunStyle.smallIcon
        height: width
        name: "search"
        ink: field.app.mutedInk
    }
}
