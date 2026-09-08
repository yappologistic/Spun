import QtQuick
import QtQuick.Controls

Menu {
    id: menu
    required property var app
    property var selection: ({})
    property int savedTrackIndex: -1
    signal selectRequested()
    property var selectionBatch: []
    property bool confirmDelete: false
    readonly property var radioService: pinService || library
    readonly property bool radioCandidate: !batch && (currentSong || (!!selection.type && selection.type.endsWith("songs")))
    onOpened: { confirmDelete = false; if (radioCandidate) radioService.prepareRadio(selection, currentSong) }
    readonly property bool session: selection.type === "saved-queues"
    readonly property bool artist: selection.type === "artists"
    readonly property bool station: selection.type === "stations"
    readonly property bool batch: selectionBatch.length > 0
    property var pinService: null
    property bool currentSong: false
    readonly property var service: app.actionService
    popupType: Popup.Item
    width: 272; padding: 8; spacing: 2
    margins: 12
    font.family: SpunStyle.family
    background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
    enter: SpunPopupEnter {}
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
    component Entry: MenuEntry { app: menu.app }
    Entry {
        objectName: "playNextAction"
        visible: !menu.currentSong && !menu.station && !menu.artist; text: menu.batch ? "Play next · " + menu.selectionBatch.length : "Play next"; glyphName: "next"
        enabled: (menu.batch || !!menu.selection.playable) && !menu.service.busy
        onTriggered: menu.batch ? menu.service.enqueueMany(menu.selectionBatch, true) : menu.session ? menu.service.enqueueMany(menu.pinService.savedTracks(menu.selection.id), true) : menu.service.enqueue(menu.selection, true)
    }
    Entry {
        objectName: "addQueueAction"
        visible: !menu.currentSong && !menu.station && !menu.artist; text: menu.batch ? "Add to queue · " + menu.selectionBatch.length : "Add to queue"; glyphName: "queue"
        enabled: (menu.batch || !!menu.selection.playable) && !menu.service.busy
        onTriggered: menu.batch ? menu.service.enqueueMany(menu.selectionBatch, false) : menu.session ? menu.service.enqueueMany(menu.pinService.savedTracks(menu.selection.id), false) : menu.service.enqueue(menu.selection, false)
    }
    Entry {
        objectName: "queueFromHereAction"
        visible: !menu.currentSong && !menu.batch && !!menu.pinService && menu.pinService.tailCollection && menu.selection.collectionIndex !== undefined
        text: menu.pinService && menu.pinService.preparingTail ? "Cancel loading tracks" : "Queue from here"; glyphName: "queue"
        enabled: !!menu.selection.playable && !menu.service.busy && !menu.app.ciderService.controlBusy
        onTriggered: menu.pinService.preparingTail ? menu.pinService.cancelQueueFromHere() : menu.pinService.queueFromHere(menu.selection)
    }
    Entry {
        objectName: "appendSavedQueueAction"
        visible: !menu.currentSong && !!menu.pinService && (menu.batch || (!!menu.selection.type && menu.selection.type.endsWith("songs")))
        text: "Add to saved queue…"; glyphName: "plus"
        enabled: menu.batch || !!menu.selection.playable
        onTriggered: menu.app.openSavedQueuePicker(menu.batch ? menu.selectionBatch : [menu.selection], menu.pinService)
    }
    Entry {
        objectName: "playStationAction"; visible: menu.station
        text: "Play station"; glyphName: "play"; enabled: !!menu.selection.playable && !menu.pinService.starting
        onTriggered: { const index = menu.pinService.items.findIndex(row => row.id === menu.selection.id && row.type === "stations"); if (index >= 0) menu.pinService.play(index); else if (menu.selection.pinIndex !== undefined) menu.pinService.playPin(menu.selection.pinIndex) }
    }
    Entry {
        objectName: "shuffleCollectionAction"
        visible: !menu.batch && !menu.currentSong && !!menu.pinService && !!menu.selection.type && (menu.selection.type.endsWith("albums") || menu.selection.type.endsWith("playlists"))
        text: "Shuffle"; glyphName: "shuffle"
        enabled: !!menu.selection.playable && !menu.pinService.starting && !menu.app.ciderService.controlBusy
        onTriggered: menu.pinService.shuffleCollection(menu.selection)
    }
    Entry {
        objectName: "songRadioAction"
        visible: menu.radioCandidate
        text: menu.radioService.radioBusy ? "Finding station…" : menu.radioService.radioAvailable ? "Start radio" : menu.radioService.radioError.length ? "Radio unavailable" : "No station for this song"
        glyphName: "disc"
        enabled: menu.radioService.radioAvailable && !menu.radioService.radioBusy && !menu.app.ciderService.controlBusy && !menu.service.busy
        onTriggered: menu.radioService.playRadio()
        SpunToolTip { visible: parent.hovered && menu.radioService.radioError.length > 0; text: menu.radioService.radioError }
    }
    Entry {
        objectName: "bookmarkMomentAction"; visible: menu.currentSong
        text: "Bookmark this moment"; glyphName: "pin"
        enabled: !menu.app.listeningService.busy && !menu.service.busy && !menu.app.ciderService.controlBusy
        onTriggered: menu.app.listeningService.addBookmark()
    }
    Entry {
        objectName: "audioQualityAction"; visible: menu.currentSong
        text: "Audio quality"; glyphName: "volume"
        onTriggered: menu.app.showAudioQuality()
    }
    Entry {
        objectName: "copySongLinkAction"
        visible: !menu.batch && (menu.currentSong || (!!menu.selection.type && menu.selection.type.endsWith("songs")))
        text: "Copy song link"; glyphName: "external"
        enabled: menu.currentSong || !!menu.selection.url || !!menu.selection.catalogId || menu.selection.type === "songs"
        onTriggered: menu.currentSong ? menu.app.ciderService.copySongLink() : menu.pinService.copyLink(menu.selection)
    }
    Entry {
        objectName: "renameSavedQueueAction"; visible: menu.session; text: "Rename saved queue"; glyphName: "more"
        onTriggered: menu.app.renameQueue(menu.selection, menu.pinService)
    }
    Entry {
        objectName: "savedTrackUp"; visible: !menu.batch && menu.savedTrackIndex >= 0
        text: "Move up"; glyphName: "up"; enabled: menu.savedTrackIndex > 0 && !menu.pinService.collectionQuery.trim().length
        onTriggered: menu.pinService.editSavedTrack(menu.savedTrackIndex, menu.savedTrackIndex - 1, false)
    }
    Entry {
        objectName: "savedTrackDown"; visible: !menu.batch && menu.savedTrackIndex >= 0
        text: "Move down"; glyphName: "down"; enabled: menu.savedTrackIndex < (menu.pinService ? menu.pinService.collection.trackCount - 1 : -1) && !!menu.pinService && !menu.pinService.collectionQuery.trim().length
        onTriggered: menu.pinService.editSavedTrack(menu.savedTrackIndex, menu.savedTrackIndex + 1, false)
    }
    Entry {
        objectName: "savedTrackRemove"; visible: !menu.batch && menu.savedTrackIndex >= 0
        text: "Remove from saved queue"; glyphName: "close"
        onTriggered: menu.pinService.editSavedTrack(menu.savedTrackIndex, menu.savedTrackIndex, true)
    }
    Entry {
        objectName: "deleteSavedQueueAction"; visible: menu.session; text: "Delete saved queue"; glyphName: "close"
        onTriggered: menu.app.confirmDeleteQueue(menu.selection.id, menu.pinService)
    }
    Entry {
        objectName: "pinCollectionAction"
        visible: !menu.batch && !!menu.pinService && !menu.currentSong && !!menu.selection.type && (menu.selection.type.endsWith("albums") || menu.selection.type.endsWith("playlists") || menu.station || menu.artist)
        text: menu.pinService && menu.pinService.pins && menu.pinService.isPinned(menu.selection) ? (menu.artist ? "Unpin artist" : menu.station ? "Unpin station" : "Unpin collection") : (menu.artist ? "Pin artist" : menu.station ? "Pin station" : "Pin collection")
        glyphName: "pin"
        onTriggered: menu.pinService.togglePin(menu.selection)
    }
    Entry {
        objectName: "openPinnedCollectionAction"
        visible: !menu.batch && !menu.station && menu.selection.pinIndex !== undefined && !!menu.pinService
        text: menu.artist ? "Open artist" : "Open collection"; glyphName: "disc"
        onTriggered: menu.pinService.openPin(menu.selection.pinIndex)
    }
    Entry {
        objectName: "selectTrackAction"
        visible: !menu.currentSong && !menu.batch && !!menu.pinService && !!menu.selection.type && menu.selection.type.endsWith("songs")
        enabled: !!menu.selection.playable
        text: "Select track"; glyphName: "check"
        onTriggered: menu.selectRequested()
    }
    Entry {
        objectName: "viewArtistAction"
        visible: !menu.batch && (menu.currentSong ? !!menu.app.deckPlayer.artist : !!menu.selection.artist && !!menu.selection.type && (menu.selection.type.endsWith("songs") || menu.selection.type.endsWith("albums")))
        text: "View artist"; glyphName: "search"
        onTriggered: {
            if (menu.pinService && !menu.currentSong) menu.pinService.showArtist(menu.selection.artist)
            else menu.app.openArtistName(menu.app.deckPlayer.artist)
        }
    }
    Entry {
        objectName: "favoriteAction"
        visible: menu.currentSong; text: menu.service.favorite ? "Remove favorite" : "Favorite"; glyphName: "heart"
        enabled: menu.service.ready && !menu.service.busy
        onTriggered: menu.service.toggleFavorite()
    }
    Entry {
        objectName: "dislikeAction"; visible: menu.currentSong
        text: menu.service.disliked ? "Clear dislike" : "Suggest less like this"; glyphName: "minus"
        enabled: menu.service.ready && !menu.service.busy
        onTriggered: menu.service.toggleDislike()
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
