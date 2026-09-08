import QtQuick
import QtQuick.Controls
Item {
    id: content
    required property var app
    property real leadingInset: SpunStyle.outerInset
    property real trailingInset: SpunStyle.target + SpunStyle.gap
    property bool artistLink: false
    signal artistClicked()
    signal selectionRequested(int modifiers)
    property string title: ""
    property string subtitle: ""
    property url artwork: ""
    property string artworkName: ""
    property string fallback: "disc"
    Rectangle {
        x: content.leadingInset; anchors.verticalCenter: parent.verticalCenter
        width: SpunStyle.artwork; height: width; radius: 8 * theme.radius; color: content.app.inset
        Glyph { anchors.centerIn: parent; name: content.fallback; ink: content.app.mutedInk }
        Image {
            objectName: content.artworkName
            anchors.fill: parent; source: content.artwork
            sourceSize.width: 96; sourceSize.height: 96
            asynchronous: true; cache: true; fillMode: Image.PreserveAspectCrop
        }
    }
    Column {
        x: content.leadingInset + SpunStyle.artwork + SpunStyle.textGap
        width: parent.width - x - content.trailingInset
        anchors.verticalCenter: parent.verticalCenter; spacing: 2
        // Grow the artist target downward, preserving text baselines and the
        // primary row's center hit area above it.
        anchors.verticalCenterOffset: 4
        SpunText {
            objectName: content.artworkName + "Title"
            height: 20; maximumLineCount: 1
            width: parent.width; text: content.title; color: content.app.ink
            font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
            lineHeightMode: Text.FixedHeight; lineHeight: 20; elide: Text.ElideRight
        }
        AbstractButton {
            id: artistButton
            objectName: content.artworkName + "Artist"
            width: parent.width; height: 24; enabled: content.artistLink; hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            Accessible.name: content.artistLink ? "View artist " + content.subtitle : content.subtitle
            background: null
            onClicked: content.artistClicked()
            MouseArea { anchors.fill: parent; onClicked: mouse => {
                if (mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) content.selectionRequested(mouse.modifiers)
                else content.artistClicked()
            } }
            contentItem: SpunText {
                maximumLineCount: 1
                text: content.subtitle; color: content.artistLink && (artistButton.hovered || artistButton.visualFocus) ? content.app.accent : content.app.mutedInk
                font.family: SpunStyle.family; font.pixelSize: SpunStyle.caption; font.underline: content.artistLink && (artistButton.hovered || artistButton.visualFocus)
                lineHeightMode: Text.FixedHeight; lineHeight: 16; elide: Text.ElideRight
                verticalAlignment: Text.AlignTop
                Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
            }
        }
    }
}
