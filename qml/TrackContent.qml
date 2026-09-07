import QtQuick
Item {
    id: content
    required property var app
    property string title: ""
    property string subtitle: ""
    property url artwork: ""
    property string artworkName: ""
    property string fallback: "disc"
    Rectangle {
        x: SpunStyle.outerInset; anchors.verticalCenter: parent.verticalCenter
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
        x: SpunStyle.outerInset + SpunStyle.artwork + SpunStyle.textGap
        width: parent.width - x - SpunStyle.target - SpunStyle.gap
        anchors.verticalCenter: parent.verticalCenter; spacing: 2
        SpunText {
            width: parent.width; text: content.title; color: content.app.ink
            font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
            lineHeightMode: Text.FixedHeight; lineHeight: 20; elide: Text.ElideRight
        }
        SpunText {
            width: parent.width; text: content.subtitle; color: content.app.mutedInk
            font.family: SpunStyle.family; font.pixelSize: SpunStyle.caption
            lineHeightMode: Text.FixedHeight; lineHeight: 16; elide: Text.ElideRight
        }
    }
}
