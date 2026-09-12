import QtQuick
import QtQuick.Controls

Rectangle {
    id: panel
    required property var app
    property var browser: library
    readonly property bool detail: Object.keys(browser.collection).length > 0
    readonly property bool sessionPage: detail && browser.collection.type === "saved-queues"
    readonly property bool sessions: browser.section === "sessions"
    readonly property bool forYou: browser.section === "for-you"
    readonly property bool releases: browser.section === "releases"
    readonly property bool artistSimilar: artistPage && browser.artistView === "similar"
    readonly property bool artistPage: detail && browser.collection.type === "artists"
    function songKey(row) {
        if (!row || (row.type !== "songs" && row.type !== "library-songs") || !row.id) return ""
        return row.catalogId ? "songs:" + row.catalogId : row.type + ":" + row.id
    }
    readonly property bool queuedStatusReady: app.ciderService.queueReady && !app.ciderService.queueError.length && !app.ciderService.recovering
    readonly property var queuedSongKeys: {
        const cider = app.ciderService
        const keys = new Set()
        if (!visible || !app.useCider) return keys
        const rows = cider.queue
        for (let i = Math.max(0, cider.currentIndex); i < rows.length; ++i) {
            const key = songKey(rows[i]); if (key.length) keys.add(key)
        }
        return keys
    }
    readonly property bool artistSongs: artistPage && browser.artistView === "songs"
    property var savedPositions: []
    property int navigationDirection: 0
    property var selectionKeys: []
    property int selectionAnchor: -1
    readonly property int selectionCount: selectionKeys.length
    readonly property bool trackListFocused: list.activeFocus
    readonly property var selectedTracks: {
        const rows = browser.items
        const keys = new Set(selectionKeys)
        return rows.filter((item, index) => keys.has(rowKey(index, item)))
    }
    function rowKey(index, item) { return index + ":" + item.type + ":" + item.id }
    function clearSelection() { selectionKeys = []; selectionAnchor = -1 }
    function chooseRow(index, modifiers) {
        const item = browser.items[index]
        if (!item) return
        const selectable = item.type.endsWith("songs") && item.playable
        const control = modifiers & Qt.ControlModifier
        const shift = modifiers & Qt.ShiftModifier
        if (selectable && (control || shift || selectionCount > 0)) {
            let next = selectionKeys.slice()
            if (shift && selectionAnchor >= 0) {
                next = control ? next : []
                for (let i = Math.min(index, selectionAnchor); i <= Math.max(index, selectionAnchor); ++i) {
                    const row = browser.items[i], key = rowKey(i, row)
                    if (row.type.endsWith("songs") && row.playable && next.indexOf(key) < 0) next.push(key)
                }
            } else {
                const key = rowKey(index, item), at = next.indexOf(key)
                if (at >= 0) next.splice(at, 1); else next.push(key)
                selectionAnchor = index
            }
            selectionKeys = next; list.currentIndex = index; return
        }
        if (control || shift) return
        clearSelection(); list.currentIndex = index; activate(index)
    }
    function selectAllTracks() {
        selectionKeys = browser.items.map((item, index) => item.playable && item.type.endsWith("songs") ? rowKey(index, item) : "").filter(key => key.length)
    }
    readonly property bool linkPreview: browser.section === "search" && browser.query.trim().startsWith("https://music.apple.com/")
    Connections {
        target: panel.browser
        function onTailReady(tracks) {
            if (panel.app.actionService.busy || panel.app.ciderService.controlBusy) panel.app.notifyAction("Cider is busy. Nothing was queued; try again when it finishes.", true)
            else if (!panel.app.ciderService.queueReady || panel.app.ciderService.queueBusy || panel.app.ciderService.queueError.length) panel.app.notifyAction("Connect Cider and refresh Queue before adding this range.", true)
            else panel.app.ciderService.insertQueue(tracks, panel.app.ciderService.queue.length, panel.app.ciderService.queueRevision)
        }
        function onFeedback(message, error) { panel.app.notifyAction(message, error) }
    }
    readonly property bool actionsOpen: trackMenu.visible || discographyMenu.visible
    function closeActions() { trackMenu.close(); discographyMenu.close() }
    function showActions(item, anchor, batch = false) {
        trackMenu.selectionBatch = batch || selectedTracks.some(row => row.id === item.id && row.type === item.type) ? selectedTracks.slice() : []
        trackMenu.selection = Object.assign({}, item)
        trackMenu.savedTrackIndex = panel.sessionPage && !batch && item.savedIndex !== undefined ? item.savedIndex : -1
        const point = anchor.mapToItem(panel, anchor.width, anchor.height)
        trackMenu.x = Math.max(8, Math.min(panel.width - trackMenu.width - 8, point.x - trackMenu.width))
        trackMenu.y = Math.max(8, Math.min(panel.height - trackMenu.height - 8, point.y))
        trackMenu.open()
    }
    TrackMenu { id: trackMenu; objectName: "browserTrackMenu"; app: panel.app; pinService: panel.browser; onSelectRequested: { const index = panel.browser.items.findIndex(row => row === selection || (row.id === selection.id && row.type === selection.type)); if (index >= 0) { list.forceActiveFocus(); panel.chooseRow(index, Qt.ControlModifier) } } }
    DropArea {
        anchors.fill: parent
        onEntered: drag => { if (!drag.hasUrls && !drag.hasText) drag.accepted = false }
        onDropped: drop => { if (panel.app.openMusicLink(drop.hasUrls ? drop.urls[0].toString() : drop.text, false)) drop.acceptProposedAction() }
    }
    property real savedY: 0
    property real pageY: 0
    property int pageIndex: -1
    property bool appending: false
    readonly property int feedbackTime: app.feedbackTime
    readonly property int transitionTime: app.transitionTime
    function settle() { entrance.stop(); listEntrance.stop(); opacity = 1; entranceOffset.x = 0; list.opacity = 1; listOffset.x = 0 }
    function reveal() { settle(); if (visible && app.animate) entrance.start() }
    transform: Translate { id: entranceOffset }
    onVisibleChanged: { if (!visible) closeActions(); reveal() }
    Connections { target: panel.app; function onAnimateChanged() { if (!panel.app.animate) panel.settle() } }
    ParallelAnimation {
        id: entrance
        NumberAnimation { target: panel; property: "opacity"; from: 0; to: 1; duration: panel.feedbackTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve }
        NumberAnimation { target: entranceOffset; property: "x"; from: 12; to: 0; duration: panel.transitionTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
    }
    ParallelAnimation {
        id: listEntrance; objectName: "libraryListEntrance"
        property real startOffset: 0
        NumberAnimation { target: list; property: "opacity"; from: 0; to: 1; duration: panel.feedbackTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve }
        NumberAnimation { target: listOffset; property: "x"; from: listEntrance.startOffset; to: 0; duration: panel.transitionTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
    }
    color: app.surface; radius: SpunStyle.panelRadius
    width: 310
    objectName: "libraryPanel"
    function focusSearch() { Qt.callLater(function() { const field = panel.detail ? collectionSearch : search; field.forceActiveFocus(); field.selectAll() }) }
    function activate(index) {
        if (index < 0) return
        if (!detail) savedY = list.contentY
        list.currentIndex = index
        browser.open(index)
        if (detail) list.forceActiveFocus()
    }
    Connections {
        target: panel.browser
        function onNavigating() { panel.navigationDirection = 1; panel.savedPositions = panel.savedPositions.slice(-15).concat([{ y: list.contentY, index: list.currentIndex }]) }
        function onNavigationReset() { panel.savedPositions = []; panel.navigationDirection = 0; panel.clearSelection() }
        function onItemsChanging(append) { if (!append) panel.clearSelection(); panel.closeActions(); panel.appending = append; panel.pageY = append ? list.contentY : 0; panel.pageIndex = append ? list.currentIndex : -1 }
        function onItemsChanged() { const keys = new Set(panel.browser.items.map((item, index) => panel.rowKey(index, item))); panel.selectionKeys = panel.selectionKeys.filter(key => keys.has(key)); Qt.callLater(function() { list.contentY = panel.pageY; list.currentIndex = panel.pageIndex; if (!panel.appending && panel.visible && list.count) { if (panel.app.animate) { listEntrance.startOffset = panel.navigationDirection * 12; listEntrance.restart() }; panel.navigationDirection = 0 } }) }
        function onReturned() { const positions = panel.savedPositions.slice(); const previous = positions.pop() || { y: 0, index: -1 }; panel.savedPositions = positions; panel.navigationDirection = -1; Qt.callLater(function() { list.currentIndex = Math.min(list.count - 1, previous.index); list.contentY = previous.y; list.forceActiveFocus(Qt.BacktabFocusReason) }) }
    }
    component SmallButton: SpunChoiceButton {
        ink: panel.app.ink; mutedInk: panel.app.mutedInk; accent: panel.app.accent
    }
    Rectangle {
        visible: !panel.detail && !panel.sessions && !panel.releases && !panel.forYou && panel.selectionCount === 0
        SpunSpring { id: sectionMotion; targetValue: 20 + ["search", "songs", "albums", "playlists"].indexOf(panel.browser.section === "recent" ? "songs" : panel.browser.section) * 68 }
        x: sectionMotion.value
        y: 16; width: 66; height: 36; radius: 18 * theme.radius; color: SpunStyle.selected
    }
    Row {
        id: libraryTabs
        function focusTab(index) {
            const button = libraryTabItems.itemAt(Math.max(0, Math.min(libraryTabItems.count - 1, index)))
            if (button) button.forceActiveFocus(Qt.TabFocusReason)
        }
        visible: !panel.detail && !panel.sessions && !panel.releases && !panel.forYou && panel.selectionCount === 0
        x: 20; y: 16; spacing: 2
        Repeater {
            id: libraryTabItems
            model: ["search", "songs", "albums", "playlists"]
            SmallButton {
                required property string modelData
                required property int index
                objectName: "libraryTab_" + modelData
                Accessible.role: Accessible.PageTab
                Accessible.selectable: true; Accessible.selected: selected
                Keys.onShortcutOverride: event => {
                        if (event.modifiers === Qt.NoModifier && [Qt.Key_Left, Qt.Key_Right, Qt.Key_Home, Qt.Key_End].includes(event.key)) event.accepted = true
                    }
                    Keys.onLeftPressed: libraryTabs.focusTab(index - 1)
                Keys.onRightPressed: libraryTabs.focusTab(index + 1)
                Keys.onPressed: event => {
                        if (event.key === Qt.Key_Home) { libraryTabs.focusTab(0); event.accepted = true }
                        else if (event.key === Qt.Key_End) { libraryTabs.focusTab(libraryTabItems.count - 1); event.accepted = true }
                        else event.accepted = false
                    }

                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                pill: false; width: 66; text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                selected: panel.browser.section === modelData || (modelData === "songs" && panel.browser.section === "recent")
                onClicked: { panel.browser.section = modelData; search.forceActiveFocus() }
            }
        }
    }
    IconButton { visible: (panel.sessions || panel.releases || panel.forYou) && !panel.detail && !panel.selectionCount; x: 16; y: 14; glyphName: "back"; tip: "Back"; ink: panel.app.ink; onClicked: panel.browser.section = "search" }
    SpunText { visible: (panel.sessions || panel.releases || panel.forYou) && !panel.detail && !panel.selectionCount; x: 64; y: 20; height: 28; text: panel.forYou ? "For You" : panel.releases ? "Latest releases" : "Saved queues"; color: panel.app.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
    SpunSearchField {
        app: panel.app
        id: search
        objectName: "librarySearchInput"
        visible: !panel.detail
        x: SpunStyle.outerInset; y: 64; width: parent.width - 2 * SpunStyle.outerInset; height: SpunStyle.target
        text: panel.browser.query
        onTextEdited: panel.browser.query = text
        maximumLength: 2048
        placeholderText: panel.forYou ? "Find a recommendation" : panel.releases ? "Find a release" : panel.sessions ? "Find a saved queue" : panel.browser.section === "search" ? "Search or paste a link" : panel.browser.section === "recent" ? "Search recently played" : "Search your " + panel.browser.section
        Accessible.name: placeholderText
        IconButton {
            visible: search.text.length > 0 || panel.browser.section !== "search"
            x: parent.width - SpunStyle.target; y: 0
            glyphName: search.text.length ? "close" : "refresh"; tip: search.text.length ? "Clear search" : "Refresh library"; ink: panel.app.mutedInk; hoverFill: panel.app.hoverFill
            onClicked: { if (search.text.length) panel.browser.query = ""; else panel.browser.reload(); search.forceActiveFocus() }
        }
        onAccepted: { panel.browser.rememberSearch(); panel.browser.reload(); list.forceActiveFocus() }
        Keys.onDownPressed: { list.forceActiveFocus(); if (list.count) list.currentIndex = 0 }
    }
    Flow {
        width: parent.width - 2 * SpunStyle.outerInset; height: 68
        visible: !panel.detail && !panel.linkPreview && panel.browser.section === "search"
        x: SpunStyle.outerInset; y: 112; spacing: SpunStyle.smallGap
        Repeater {
            model: ["songs", "albums", "playlists", "artists", "stations"]
            SmallButton {
                required property string modelData
                objectName: "libraryKind_" + modelData
                width: 84; height: 32; text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                selected: panel.browser.kind === modelData
                onClicked: panel.browser.kind = modelData
            }
        }
    }
    SmallButton {
        objectName: "forYouButton"; x: SpunStyle.outerInset + 2 * (84 + SpunStyle.smallGap); y: 112 + 32 + SpunStyle.smallGap
        width: 84; height: 32; text: "For You"
        visible: !panel.detail && !panel.linkPreview && panel.browser.section === "search" && !panel.browser.query.trim().length
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        onClicked: panel.browser.section = "for-you"
    }
    Row {
        visible: !panel.detail && (panel.browser.section === "songs" || panel.browser.section === "recent" || panel.browser.section === "albums")
        x: SpunStyle.outerInset; y: 112; spacing: SpunStyle.smallGap
        SmallButton { objectName: "allSongsTab"; width: 46; height: 32; text: "A–Z"; selected: !panel.browser.newestFirst && panel.browser.section !== "recent"; onClicked: { if (panel.browser.section === "recent") panel.browser.section = "songs"; panel.browser.newestFirst = false } }
        SmallButton { objectName: "recentlyAddedTab"; width: 132; height: 32; text: "Recently added"; selected: panel.browser.newestFirst && panel.browser.section !== "recent"; onClicked: { if(panel.browser.section === "recent") panel.browser.section = "songs"; panel.browser.newestFirst = true } }
        SmallButton { visible: panel.browser.section !== "albums"; objectName: "recentSongsTab"; width: 76; height: 32; text: "History"; selected: panel.browser.section === "recent"; onClicked: panel.browser.section = "recent" }
    }
    Column {
        id: pinnedCollections
        objectName: "pinnedCollections"
        visible: !panel.detail && panel.browser.section === "search" && !panel.browser.query.trim().length && panel.browser.pins.length > 0
        x: SpunStyle.outerInset; y: 192; width: parent.width - 2 * SpunStyle.outerInset; spacing: 12
        Item {
            width: parent.width; height: 36
            SpunText { anchors.verticalCenter: parent.verticalCenter; text: "Pinned"; color: panel.app.mutedInk; font.pixelSize: SpunStyle.caption }
            SmallButton { objectName: "pinnedReleasesButton"; visible: panel.browser.hasArtistPins; anchors.right: parent.right; width: 100; height: 36; text: "Releases"; onClicked: panel.browser.section = "releases"; Accessible.name: "Latest releases from pinned artists" }
        }
        ListView {
            width: parent.width; height: 148; orientation: ListView.Horizontal; spacing: 12
            model: panel.browser.pins; clip: true; boundsBehavior: Flickable.StopAtBounds
            delegate: AbstractButton {
                id: pinButton
                required property int index
                required property var modelData
                objectName: "pinnedCollection" + index
                width: 112; height: 144; hoverEnabled: true
                enabled: !panel.browser.starting
                Accessible.name: (modelData.type === "artists" ? "Open artist " : "Play ") + modelData.title
                onClicked: panel.browser.playPin(index)
                background: Rectangle {
                    radius: SpunStyle.rowRadius; color: "transparent"
                    SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: panel.app.ink; enabled: pinButton.enabled; hovered: pinButton.hovered; pressed: pinButton.down; focused: pinButton.visualFocus }
                }
                contentItem: Item {
                    SpunSpring { id: pinPress; targetValue: pinButton.down && SpunStyle.motion ? .96 : 1; epsilon: .001 }
                    Rectangle { x: 4; y: 4; width: 104; height: 104; radius: 8; color: panel.app.inset; scale: pinPress.value
                        Glyph { anchors.centerIn: parent; name: "disc"; ink: panel.app.mutedInk }
                        Image { anchors.fill: parent; source: pinButton.modelData.artwork || ""; sourceSize.width: 144; sourceSize.height: 144; asynchronous: true; fillMode: Image.PreserveAspectCrop }
                    }
                    SpunText { x: 4; y: 112; width: 76; height: 26; text: pinButton.modelData.title; color: panel.app.ink; font.pixelSize: SpunStyle.caption; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                    IconButton { x: 80; y: 108; width: 32; height: 36; glyphName: "more"; tip: pinButton.modelData.title; ink: panel.app.mutedInk; hoverFill: panel.app.hoverFill
                        onClicked: panel.showActions(Object.assign({}, pinButton.modelData, {pinIndex: pinButton.index}), this)
                    }
                }
                TapHandler { acceptedButtons: Qt.RightButton; onTapped: panel.showActions(Object.assign({}, pinButton.modelData, {pinIndex: pinButton.index}), pinButton) }
                Keys.onMenuPressed: panel.showActions(Object.assign({}, modelData, {pinIndex: index}), pinButton)
                SpunToolTip { visible: pinButton.hovered && !trackMenu.visible; text: pinButton.modelData.title }
            }
        }
    }
    SpunSearchField {
        app: panel.app
        id: collectionSearch
        objectName: "collectionSearchInput"
        visible: panel.detail
        x: SpunStyle.outerInset; y: collectionHeader.height + SpunStyle.gap; width: parent.width - 2 * SpunStyle.outerInset; height: SpunStyle.target
        text: panel.browser.collectionQuery
        onTextEdited: panel.browser.collectionQuery = text
        maximumLength: 200
        placeholderText: panel.artistSimilar ? "Search similar artists" : panel.artistPage && !panel.artistSongs ? "Search albums" : "Search tracks"
        Accessible.name: panel.artistSimilar ? "Search similar artists" : panel.artistPage ? (panel.artistSongs ? "Search artist top songs" : "Search artist albums") : "Search this collection"
        IconButton {
            visible: collectionSearch.text.length > 0
            x: parent.width - SpunStyle.target; y: 0
            glyphName: "close"; tip: "Clear search"; ink: panel.app.mutedInk; hoverFill: panel.app.hoverFill
            onClicked: { panel.browser.collectionQuery = ""; collectionSearch.forceActiveFocus() }
        }
        onAccepted: { list.forceActiveFocus(); if (list.count) list.currentIndex = 0 }
        Keys.onDownPressed: { list.forceActiveFocus(); if (list.count) list.currentIndex = 0 }
    }
    Item {
        id: collectionHeader
        IconButton {
            objectName: "pinArtistButton"; visible: panel.artistPage
            x: parent.width - 56; y: 14; glyphName: "pin"
            selected: panel.browser.pins && panel.browser.isPinned(panel.browser.collection)
            Accessible.checkable: true; Accessible.checked: selected
            tip: selected ? "Unpin artist" : "Pin artist"
            ink: selected ? panel.app.accent : panel.app.mutedInk
            onClicked: panel.browser.togglePin(panel.browser.collection)
        }
        visible: panel.detail; width: parent.width; height: panel.artistPage ? 184 : 144
        IconButton {
            objectName: "libraryBack"
            x: 16; y: 14; glyphName: "back"; tip: "Back · Esc"
            ink: panel.app.ink; hoverFill: panel.app.hoverFill
            onClicked: panel.browser.back()
        }
        SpunText {
            x: 64; y: 20; width: parent.width - x - (panel.artistPage ? 60 : SpunStyle.outerInset); height: 28
            text: panel.browser.collection.title || ""; color: panel.app.ink
            font.family: SpunStyle.family; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium; elide: Text.ElideRight
            HoverHandler { id: titleHover }
            SpunToolTip { visible: titleHover.hovered && parent.truncated; text: parent.text }
        }
        Rectangle {
            x: 24; y: 64; width: 64; height: 64; radius: 8; color: panel.app.inset
            Glyph { anchors.centerIn: parent; name: "disc"; ink: panel.app.mutedInk }
            Image { anchors.fill: parent; source: panel.browser.collection.artwork || ""; sourceSize.width: 160; sourceSize.height: 160; asynchronous: true; fillMode: Image.PreserveAspectCrop }
        }
        SpunText {
            x: 101; y: 65; width: 182; height: 18
            text: [panel.browser.collection.artist, panel.browser.collection.year].filter(v => v && v.length).join(" · ")
            color: panel.app.mutedInk; font.pixelSize: SpunStyle.caption; elide: Text.ElideRight
        }
        SmallButton {
            objectName: "libraryPlayCollection"
            x: 101; y: 94; width: 66; height: 34
            visible: !panel.artistPage
            text: panel.sessionPage ? "Play next" : panel.browser.starting ? "Starting…" : "Play"
            enabled: !!panel.browser.collection.playable && !panel.browser.starting && !panel.app.actionService.busy
            selected: true
            onClicked: panel.sessionPage ? panel.app.actionService.enqueueMany(panel.browser.savedTracks(panel.browser.collection.id), true) : panel.browser.playCollection()
        }
        IconButton {
            objectName: "libraryShuffleCollection"; x: 173; y: 90
            visible: !panel.artistPage && !panel.sessionPage
            glyphName: "shuffle"; tip: "Shuffle"; ink: panel.app.accent
            enabled: !!panel.browser.collection.playable && !panel.browser.starting && !panel.app.ciderService.controlBusy
            onClicked: panel.browser.shuffleCollection(panel.browser.collection)
        }
        SmallButton {
            id: discographyChoice; objectName: "discographyFilter"; visible: panel.artistPage && panel.browser.artistView === "albums"
            x: 101; y: 92; width: 182; height: 36
            text: ({"all":"All releases", "full-albums":"Full albums", "singles":"Singles & EPs", "live-albums":"Live albums"})[panel.browser.discography]
            Accessible.name: "Filter discography: " + text
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            contentItem: Item {
                SpunText { x: 12; width: parent.width - 44; height: parent.height; text: discographyChoice.text; font.pixelSize: SpunStyle.body; font.weight: Font.Medium; color: panel.app.mutedInk; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                Glyph { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; name: "down"; ink: panel.app.mutedInk }
            }
            onClicked: discographyMenu.open()
            Menu {
                id: discographyMenu; onOpened: panel.app.focusFirstMenuItem(discographyMenu); objectName: "discographyMenu"; y: parent.height + 4; width: 204; padding: 8; spacing: 2; popupType: Popup.Item
                background: Rectangle { radius: SpunStyle.popupRadius; color: SpunStyle.popup }
                enter: SpunPopupEnter {}
                exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                Repeater { model: [{key:"all",label:"All releases"},{key:"full-albums",label:"Full albums"},{key:"singles",label:"Singles & EPs"},{key:"live-albums",label:"Live albums"}]
                    MenuEntry { required property var modelData; app: panel.app; objectName: "discography_" + modelData.key; text: modelData.label; Accessible.checkable: true; Accessible.checked: panel.browser.discography === modelData.key; glyphName: panel.browser.discography === modelData.key ? "check" : ""; onTriggered: panel.browser.discography = modelData.key }
                }
            }
        }
        Row {
            visible: panel.artistPage; x: SpunStyle.outerInset; y: 140; spacing: 4
            SmallButton { objectName: "artistTopSongsTab"; width: 90; height: 34; text: "Top songs"; selected: panel.artistSongs; onClicked: panel.browser.artistView = "songs" }
            SmallButton { objectName: "artistAlbumsTab"; width: 76; height: 34; text: "Albums"; selected: panel.browser.artistView === "albums"; onClicked: panel.browser.artistView = "albums" }
            SmallButton { objectName: "artistSimilarTab"; width: 88; height: 34; text: "Similar"; selected: panel.artistSimilar; onClicked: panel.browser.artistView = "similar"; Accessible.name: "Similar artists" }
        }
        SpunText {
            visible: panel.sessionPage
            x: panel.artistPage ? 101 : 195; y: 104; width: panel.artistPage ? 182 : 52; horizontalAlignment: panel.artistPage ? Text.AlignLeft : Text.AlignRight
            text: panel.browser.collection.trackCount >= 0 ? panel.browser.collection.trackCount + (panel.browser.collection.trackCount === 1 ? " track" : " tracks") : ""
            color: panel.app.mutedInk; font.pixelSize: SpunStyle.caption; elide: Text.ElideRight
        }
    }
    IconButton {
        objectName: "collectionActions"
        visible: panel.detail && !panel.artistPage; enabled: panel.sessionPage || !!panel.browser.collection.playable
        x: parent.width - SpunStyle.outerInset - SpunStyle.target; y: 92
        glyphName: "more"; tip: "Collection actions"; ink: panel.app.mutedInk; hoverFill: panel.app.hoverFill
        onClicked: panel.showActions(panel.browser.collection, this)
    }
    Rectangle {
        objectName: "trackSelectionBar"
        visible: panel.selectionCount > 0; x: 16; y: 12; width: parent.width - 32; height: 44; radius: 22
        color: panel.app.surface; z: 3
        SpunText { x: 12; anchors.verticalCenter: parent.verticalCenter; text: panel.selectionCount + " selected"; color: panel.app.ink; font.pixelSize: SpunStyle.body }
        IconButton { objectName: "selectedTrackActions"; x: parent.width - 84; anchors.verticalCenter: parent.verticalCenter; glyphName: "more"; tip: "Selected track actions"; ink: panel.app.accent; enabled: !panel.app.actionService.busy; onClicked: panel.showActions({}, this, true) }
        IconButton { objectName: "clearTrackSelection"; x: parent.width - 42; anchors.verticalCenter: parent.verticalCenter; glyphName: "close"; tip: "Clear selection · Esc"; ink: panel.app.mutedInk; onClicked: panel.clearSelection() }
    }
    MouseArea {
        id: libraryDragArea; objectName: "libraryDragArea"
        parent: panel.app.contentItem
        readonly property point origin: { const layout = panel.parent.x + panel.parent.y + panel.width + panel.height + entranceOffset.x + listOffset.x; return panel.mapToItem(parent, list.x + SpunStyle.outerInset, list.y) }
        x: origin.x; y: origin.y; width: SpunStyle.artwork; height: list.height; z: 24
        visible: (panel.visible && list.count > 0 && panel.browser.items[0].type.endsWith("songs")) || panel.app.libraryDragging
        enabled: (panel.visible && !panel.app.actionService.busy && !panel.app.ciderService.controlBusy) || panel.app.libraryDragging
        preventStealing: panel.app.libraryDragging
        hoverEnabled: true
        SpunToolTip { visible: libraryDragArea.containsMouse && !libraryDragArea.pressed && panel.visible && !trackMenu.visible; text: "Drag artwork to queue" }
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        property point startPoint
        property int pressedIndex: -1
        property var tracks: []
        onPressed: mouse => {
            startPoint = Qt.point(mouse.x, mouse.y)
            pressedIndex = list.indexAt(0, mouse.y + list.contentY)
            const row = panel.browser.items[pressedIndex]
            if (!row || !row.playable || !row.type.endsWith("songs")) { mouse.accepted = false; return }
            tracks = panel.selectionKeys.indexOf(panel.rowKey(pressedIndex, row)) >= 0 ? panel.selectedTracks.slice() : [Object.assign({}, row)]
        }
        onPositionChanged: mouse => {
            if (!pressed || pressedIndex < 0) return
            if (!panel.app.libraryDragging && Math.abs(mouse.x - startPoint.x) < 10) return
            if (!panel.app.libraryDragging) panel.app.beginLibraryDrag(tracks)
            const point = mapToItem(panel.app.contentItem, mouse.x, mouse.y)
            panel.app.updateLibraryDrag(point.x, point.y)
        }
        onReleased: mouse => {
            if (panel.app.libraryDragging) panel.app.endLibraryDrag(true)
            else if (pressedIndex >= 0) { list.forceActiveFocus();panel.chooseRow(pressedIndex,mouse.modifiers) }
            pressedIndex = -1;tracks = []
        }
        onCanceled: { if(panel.app.libraryDragging)panel.app.endLibraryDrag(false);pressedIndex=-1;tracks=[] }
    }
    ListView {
        id: list
        objectName: "libraryList"
        x: SpunStyle.inset; y: panel.detail ? collectionSearch.y + collectionSearch.height + SpunStyle.gap : (!panel.linkPreview && panel.browser.section === "search") ? 188 : panel.browser.section === "songs" || panel.browser.section === "recent" || panel.browser.section === "albums" ? 152 : search.y + search.height + SpunStyle.gap
        width: parent.width - 2 * SpunStyle.inset; height: footer.y - SpunStyle.gap - y
        visible: !pinnedCollections.visible
        model: panel.browser.items; clip: true; spacing: 4
        transform: Translate { id: listOffset }
        currentIndex: -1; boundsBehavior: Flickable.StopAtBounds
        onCountChanged: if (currentIndex >= count) currentIndex = -1
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) {
                panel.selectAllTracks(); event.accepted = true
            } else if (event.key === Qt.Key_Space && list.currentIndex >= 0) {
                panel.chooseRow(list.currentIndex, event.modifiers | Qt.ControlModifier); event.accepted = true
            } else if ((event.key === Qt.Key_Down || event.key === Qt.Key_Up) && (event.modifiers & Qt.ShiftModifier)) {
                const destination = Math.max(0, Math.min(list.count - 1, list.currentIndex + (event.key === Qt.Key_Down ? 1 : -1)))
                if (panel.selectionAnchor < 0) panel.selectionAnchor = Math.max(0, list.currentIndex)
                panel.chooseRow(destination, event.modifiers); list.positionViewAtIndex(destination, ListView.Contain); event.accepted = true
            } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                const row = itemAtIndex(currentIndex)
                if (row) panel.showActions(row.modelData, row)
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (panel.selectionCount > 0) panel.showActions({}, list, true); else panel.activate(currentIndex)
                event.accepted = true
            } else if (event.key === Qt.Key_Home) { currentIndex = 0; positionViewAtBeginning(); event.accepted = true }
            else if (event.key === Qt.Key_End) { currentIndex = count - 1; positionViewAtEnd(); event.accepted = true }
        }
        ScrollBar.vertical: ScrollBar {
            visible: list.contentHeight > list.height
            width: 3; padding: 0; minimumSize: .08
            contentItem: Rectangle { radius: 2; color: panel.app.mutedInk; opacity: parent.active ? .65 : .2 }
        }
        delegate: AbstractButton {
            id: row
            required property int index
            required property var modelData
            readonly property bool song: modelData.type.endsWith("songs")
            readonly property bool alreadyQueued: song && panel.queuedStatusReady && panel.queuedSongKeys.has(panel.songKey(modelData))
            objectName: "libraryRow" + index
            width: list.width; height: SpunStyle.trackHeight
            enabled: !song || (modelData.playable && !panel.browser.starting)
            HoverHandler { id: rowHover }
            Accessible.name: (song ? "Play " : "Open ") + modelData.title + ", " + modelData.artist + (alreadyQueued ? ", already queued" : "")
            onClicked: panel.chooseRow(index, Qt.NoModifier)
            MouseArea {
                id: rowPointer
                anchors.fill: parent
                onClicked: mouse => { list.forceActiveFocus(); panel.chooseRow(row.index, mouse.modifiers) }
            }
            background: Rectangle {
                radius: SpunStyle.rowRadius
                color: "transparent"
                Rectangle { anchors.fill: parent; radius: parent.radius; color: SpunStyle.selected; opacity: panel.selectionKeys.indexOf(panel.rowKey(row.index, row.modelData)) >= 0 ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                }
                SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: panel.app.ink; enabled: row.enabled; pressed: row.down || rowPointer.pressed; focused: list.activeFocus && list.currentIndex === row.index; hovered: rowHover.hovered }
            }
            contentItem: TrackContent {
                z: 1
                app: panel.app; opacity: row.enabled ? 1 : .4
                trailingInset: SpunStyle.target + SpunStyle.gap + (row.alreadyQueued ? 22 : 0)
                artistLink: !(panel.forYou && !panel.detail) && !panel.artistPage && !panel.selectionCount && !!row.modelData.artist && row.modelData.type !== "artists" && (row.song || row.modelData.type.endsWith("albums"))
                onArtistClicked: panel.browser.showArtist(row.modelData.artist)
                onSelectionRequested: modifiers => { list.forceActiveFocus(); panel.chooseRow(row.index, modifiers) }
                title: row.modelData.title
                subtitle: (panel.forYou && !panel.detail ? [row.modelData.recommendation, row.modelData.artist].filter(v => v).join(" · ") : panel.artistPage && !panel.artistSongs && !panel.artistSimilar ? row.modelData.year : panel.releases && !panel.detail ? [row.modelData.artist, row.modelData.year].filter(v => v).join(" · ") : row.modelData.artist) || (row.modelData.trackCount >= 0 ? row.modelData.trackCount + " tracks" : row.modelData.type === "artists" ? "Artist" : row.modelData.type === "stations" ? "Radio station" : "Playlist")
                artwork: row.modelData.artwork || ""; artworkName: "libraryArtwork" + row.index
                fallback: row.song || row.modelData.type.endsWith("albums") ? "disc" : "queue"
            }
            Item {
                objectName: "alreadyQueued" + row.index; visible: row.alreadyQueued; z: 2
                x: parent.width - SpunStyle.target - 22; width: 20; height: 32; anchors.verticalCenter: parent.verticalCenter
                Glyph { anchors.centerIn: parent; width: 16; height: 16; name: "check"; ink: panel.app.mutedInk }
                HoverHandler { id: queuedHover }
                SpunToolTip { visible: queuedHover.hovered; text: "Already queued" }
            }
            IconButton {
                id: rowActions
                z: 2
                objectName: "libraryActions" + row.index
                x: parent.width - SpunStyle.target; anchors.verticalCenter: parent.verticalCenter
                enabled: (row.modelData.type === "artists" || row.modelData.type === "saved-queues" || !!row.modelData.playable) && !panel.app.actionService.busy
                glyphName: "more"; tip: row.modelData.type === "artists" ? "Artist actions" : "Track actions"; ink: panel.app.mutedInk; hoverFill: panel.app.hoverFill
                onClicked: { list.currentIndex = row.index; panel.showActions(row.modelData, this) }
            }
            TapHandler { acceptedButtons: Qt.RightButton; onTapped: panel.showActions(row.modelData, rowActions) }
            Keys.onPressed: event => { if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) { panel.showActions(row.modelData, rowActions); event.accepted = true } }
            SpunToolTip { visible: row.hovered && !rowActions.hovered && !trackMenu.visible && row.visible && !row.down; text: row.modelData.title + (row.modelData.artist ? "\n" + row.modelData.artist : "") + (row.song && !row.modelData.playable ? "\nUnavailable" : "") }
        }
        SpunLoading {
            anchors.horizontalCenter: parent.horizontalCenter; y: parent.height / 2 - 38; width: 180
            visible: panel.browser.busy && list.count === 0
            label: "Loading library"
        }
        SpunText {
            anchors.centerIn: parent; width: 222
            visible: list.count === 0
            text: panel.browser.busy ? "Loading…" : panel.browser.error || (panel.browser.section === "search" && !panel.browser.query.trim().length && !panel.detail ? "" : (panel.detail ? panel.browser.collectionQuery.trim().length : panel.browser.query.trim().length) ? "No matches" : panel.releases && !panel.detail ? (panel.browser.hasArtistPins ? "No recent releases available" : "Pin an artist to see their latest releases") : panel.detail ? panel.artistSimilar ? "No similar artists available" : panel.artistPage && !panel.artistSongs ? ({"all":"No releases available", "full-albums":"No full albums available", "singles":"No singles or EPs available", "live-albums":"No live albums available"})[panel.browser.discography] : "No tracks available" : panel.forYou ? "No recommendations available yet" : panel.sessions ? "No saved queues yet" : panel.browser.section === "recent" ? "No recently played songs" : "No " + panel.browser.section + " in your library")
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: panel.app.mutedInk; font.pixelSize: 12; lineHeight: 1.3
        }
    }
    ListView {
        id: recentSearchList; objectName: "recentSearchList"
        visible: !panel.detail && panel.browser.section === "search" && !panel.browser.query.trim().length && !panel.browser.busy && !panel.browser.error.length
        x: SpunStyle.outerInset; y: pinnedCollections.visible ? pinnedCollections.y + pinnedCollections.height + 16 : 192
        width: parent.width - 2 * SpunStyle.outerInset; height: Math.max(0, footer.y - y - 12)
        clip: true; spacing: 2; boundsBehavior: Flickable.StopAtBounds
        model: visible ? panel.browser.recentSearches : []
        header: SpunText { height: 28; text: recentSearchList.count ? "Recent searches" : ""; color: panel.app.mutedInk; font.pixelSize: SpunStyle.caption }
        ScrollBar.vertical: ScrollBar {}
        delegate: Item {
            required property string modelData
            required property int index
            width: recentSearchList.width; height: SpunStyle.target
            SmallButton {
                objectName: "recentSearch" + index; width: parent.width - SpunStyle.target; height: parent.height
                text: modelData
                onClicked: { const browser = panel.browser, field = search, term = modelData; browser.query = term; browser.rememberSearch(); field.forceActiveFocus() }
                contentItem: SpunText { text: modelData; color: panel.app.ink; font.pixelSize: SpunStyle.body; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
            }
            IconButton { objectName: "removeRecentSearch" + index; x: parent.width - width; glyphName: "close"; tip: "Remove search"; ink: panel.app.mutedInk; onClicked: panel.browser.removeRecentSearch(modelData) }
        }
    }
    SpunText {
        visible: (panel.releases || panel.forYou) && !panel.detail && panel.browser.releaseNotice.length > 0
        x: SpunStyle.outerInset; y: footer.y - 20; width: parent.width - 2 * SpunStyle.outerInset; height: 18
        text: panel.browser.releaseNotice; color: panel.app.mutedInk; font.pixelSize: SpunStyle.caption
    }
    Item {
        id: footer
        objectName: "libraryFooter"
        x: SpunStyle.outerInset; y: parent.height - height - SpunStyle.inset; width: parent.width - 2 * SpunStyle.outerInset; height: 48
        SpunText {
            anchors.fill: parent
            visible: panel.browser.playError.length > 0 && !panel.browser.needsConnection
            text: panel.browser.playError; color: theme.colors.error
            font.pixelSize: SpunStyle.caption; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter
        }
        SmallButton {
            objectName: "libraryMore"
            anchors.centerIn: parent; width: parent.width; height: SpunStyle.target
            visible: panel.browser.needsConnection || (!panel.browser.playError.length && (panel.browser.hasMore || panel.browser.error.length > 0))
            enabled: !panel.browser.busy
            text: panel.browser.busy ? ((panel.detail && panel.browser.collectionQuery.length) || (panel.browser.section === "recent" && panel.browser.query.length) ? panel.artistSimilar ? "Searching artists…" : panel.artistPage && !panel.artistSongs ? "Searching albums…" : "Searching tracks…" : "Loading…") : panel.browser.needsConnection ? "Connection settings" : panel.browser.error.length ? ((panel.detail && panel.browser.collectionQuery.length) || (panel.browser.section === "recent" && panel.browser.query.length) ? "Search incomplete · Retry" : "Try again") : "Load more"
            selected: true
            onClicked: {
                if (panel.browser.needsConnection) panel.app.toggleQueue()
                else if (panel.app.ciderService.recovering) panel.app.ciderService.reconnect()
                else if (panel.browser.hasMore) panel.browser.more()
                else panel.browser.reload()
            }
        }
        SpunText {
            visible: panel.browser.busy && list.count > 0 && !panel.browser.hasMore
            anchors.centerIn: parent; text: "Loading…"; color: panel.app.mutedInk; font.pixelSize: SpunStyle.caption
        }
    }
}
