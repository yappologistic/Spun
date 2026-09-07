import QtQuick
import QtQuick.Controls

Menu {
    id: menu
    required property var app
    property var selection: ({})
    property bool currentSong: false
    readonly property var service: app.actionService
    popupType: Popup.Item
    width: 272; padding: 8; spacing: 2
    margins: 12
    font.family: SpunStyle.family
    background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
    enter: SpunPopupEnter {}
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit } }
    component Entry: MenuEntry { app: menu.app }
    Entry {
        objectName: "playNextAction"
        visible: !menu.currentSong; text: "Play next"; glyphName: "next"
        enabled: !!menu.selection.playable && !menu.service.busy
        onTriggered: menu.service.enqueue(menu.selection, true)
    }
    Entry {
        objectName: "addQueueAction"
        visible: !menu.currentSong; text: "Add to queue"; glyphName: "queue"
        enabled: !!menu.selection.playable && !menu.service.busy
        onTriggered: menu.service.enqueue(menu.selection, false)
    }
    Entry {
        objectName: "favoriteAction"
        visible: menu.currentSong; text: menu.service.favorite ? "Remove favorite" : "Favorite"; glyphName: "heart"
        enabled: menu.service.ready && !menu.service.busy
        onTriggered: menu.service.toggleFavorite()
    }
    Entry {
        objectName: "saveLibraryAction"
        visible: menu.currentSong; text: menu.service.saved ? "Saved to library" : "Save to library"; glyphName: menu.service.saved ? "check" : "plus"
        enabled: menu.service.ready && !menu.service.busy && !menu.service.saved
        onTriggered: menu.service.save()
    }
    Entry {
        objectName: "songStatusRetry"
        visible: menu.currentSong && !menu.service.ready
        text: menu.service.error.length ? "Retry song status" : "Checking song…"; glyphName: "refresh"
        enabled: menu.service.error.length > 0 && !menu.service.busy
        onTriggered: menu.service.refresh()
    }
    MenuItem {
        visible: menu.currentSong && menu.service.error.length > 0
        height: visible ? implicitHeight : 0
        implicitHeight: errorLabel.implicitHeight + 16
        enabled: false; background: null
        contentItem: SpunText { id: errorLabel; width: 206; text: menu.service.error; color: theme.colors.error; font.family: SpunStyle.family; font.pixelSize: SpunStyle.caption; wrapMode: Text.WordWrap }
    }
}
