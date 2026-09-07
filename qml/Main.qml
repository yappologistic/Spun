import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Spun 1.0

ApplicationWindow {
    id: root
    objectName: "spunWindow"
    title: "Spun"
    visible: true
    width: miniMode ? 300 : sideOpen ? 860 : 530
    height: miniMode ? 354 : 730
    minimumWidth: 300; maximumWidth: 860
    minimumHeight: 300; maximumHeight: 730
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | (miniPinned && !native.supportsBlur ? Qt.WindowStaysOnTopHint : 0)
    font.family: SpunStyle.family
    property bool useCider: !testMode && cider.available
    property var actionService: musicActions
    property var deckPlayer: useCider ? cider : player
    onUseCiderChanged: { preferences.close(); crossfadeMenu.close(); queueMenu.close(); cancelQueueDrag(); songMenu.close(); musicBrowser.closeActions(); if (!useCider) libraryOpen = false; if (useCider) player.pause(); cider.queueVisible = queueOpen && useCider; discFlipped = false; closeQueueSearch(); Qt.callLater(presentDisc); syncLyrics() }
    property bool lyricsView: false
    property real swapOffset: 0
    property real outgoingOffset: 0
    property real outgoingOpacity: 0
    property real incomingOpacity: 1
    property real outgoingAngle: 0
    readonly property bool swapRunning: animate && discSwap.running
    function resetSwap() { swapOffset = 0; outgoingOffset = 0; outgoingOpacity = 0; incomingOpacity = 1 }
    function stopSwap() { discSwap.stop(); resetSwap(); presentation.releaseOutgoing() }
    function presentDisc() {
        const albumKey = deckPlayer.count ? (useCider ? "cider:" : "local:") + (deckPlayer.albumKey || deckPlayer.title) : ""
        presentation.present(deckPlayer.artwork, albumKey, animate && !discFlipped && visible && native.exposed, useCider || player.artworkLoading)
    }
    function syncLibrary() { if (!visible || visibility === Window.Minimized) { songMenu.close(); musicBrowser.closeActions() }; library.active = libraryOpen && useCider && visible && visibility !== Window.Minimized }
    function syncLyrics() { lyrics.remote = useCider; lyrics.active = discFlipped && lyricsView && visible && visibility !== Window.Minimized }
    onLyricsViewChanged: { syncLyrics(); if (!lyricsView && lyricList.activeFocus) contentItem.forceActiveFocus() }
    onVisibilityChanged: { syncLibrary(); syncLyrics(); if (root.visibility === Window.Minimized) stopSwap() }
    onVisibleChanged: { syncLibrary(); syncLyrics(); if (!root.visible) stopSwap() }
    onAnimateChanged: if (!animate) stopSwap()
    property bool discFlipped: false
    readonly property var discDetails: deckPlayer.discDetails
    readonly property var albumTracks: discDetails.tracks || []
    onDiscFlippedChanged: {
        scrubber.cancelScrub()
        stopSwap()
        cider.discVisible = discFlipped && useCider
        syncLyrics()
    }
    function flipDisc() { if (deckPlayer.count > 0) discFlipped = !discFlipped }
    readonly property bool miniPinned: miniMode && player.miniOnTop
    onMiniPinnedChanged: native.effects(root, backgroundBlur)
    property bool miniMode: player.miniMode
    onMiniModeChanged: Qt.callLater(function() {
        scrubber.cancelScrub(); stopSwap()
        if (miniMode) { preferences.close(); crossfadeMenu.close(); songMenu.close(); musicBrowser.closeActions(); libraryOpen = false; queueOpen = false; helpOpen = false; menu.close(); miniReveal.restart() }
        updateMask(); native.effects(root, backgroundBlur)
    })
    property bool editingText: activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit
    property bool queueSearchOpen: false
    property string queueQuery: ""
    property bool queuePrepared: false
    readonly property var queueRows: queuePrepared ? buildQueueRows(deckPlayer.queue) : []
    readonly property var filteredQueue: filterQueue(queueRows, queueQuery)
    function foldQueueText(value) { return (value || "").normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase() }
    function buildQueueRows(queue) {
        const rows = []
        for (let i = 0; i < queue.length; ++i) rows.push({ track: queue[i], sourceIndex: i })
        return rows
    }
    function filterQueue(rows, query) {
        const terms = foldQueueText(query).trim().split(/\s+/).filter(term => term.length > 0)
        if (!terms.length) return rows
        // Keep the search index for this queue revision. Typing neither rebuilds
        // the C++ queue nor normalizes every title and artist again.
        return rows.filter(row => {
            if (row.searchText === undefined) row.searchText = foldQueueText(row.track.title + " " + row.track.artist)
            return terms.every(term => row.searchText.includes(term))
        })
    }
    function openQueueSearch() {
        queueOpen = true
        queueSearchOpen = true
        Qt.callLater(function() { queueSearch.forceActiveFocus(); queueSearch.selectAll() })
    }
    function closeQueueSearch() {
        queueSearchOpen = false
        queueQuery = ""
        if (queueSearch && queueSearch.activeFocus) contentItem.forceActiveFocus()
    }
    property bool libraryOpen: false
    readonly property bool sideOpen: queueOpen || libraryOpen
    onLibraryOpenChanged: {
        if (libraryOpen) { player.miniMode = false; queueOpen = false; closeQueueSearch() }
        syncLibrary()
        Qt.callLater(updateMask)
    }
    function openMusicLink(text, clipboard) {
        const accepted = clipboard ? musicBrowser.browser.openClipboardLink() : musicBrowser.browser.openLink(text)
        if (!accepted) { notifyAction("Use an Apple Music song, album or playlist link.", true); return false }
        useCider = true; libraryOpen = true
        return true
    }
    function openLibrary() { libraryOpen = !libraryOpen }
    readonly property bool queueControlsReady: !useCider || (cider.queueReady && !cider.queueBusy && !cider.controlBusy && !cider.queueError.length)
    readonly property bool queueReorderAllowed: queueControlsReady && !queueQuery.trim().length
    property int queueDragSource: -1
    property int queueDropIndex: -1
    property int queueDragRevision: -1
    property real queueDragY: 0
    function cancelQueueDrag() { queueDragSource = -1; queueDropIndex = -1 }
    function updateQueueDrop() {
        const row = Math.max(0, Math.min(trackList.count - 1, Math.floor((queueDragY + trackList.contentY) / (SpunStyle.trackHeight + SpunStyle.smallGap))))
        queueDropIndex = row
    }
    function moveQueueRow(from, to, revision) {
        if (useCider) cider.moveQueue(from,to,revision)
        else player.move(from,to)
    }
    function showQueueActions(index, anchor) {
        queueMenu.rowIndex = index; queueMenu.revision = cider.queueRevision
        const point = anchor.mapToItem(root.contentItem,0,anchor.height)
        queueMenu.x = jewelCase.x + jewelCase.width - queueMenu.width - 16
        queueMenu.y = Math.max(jewelCase.y + 12,Math.min(root.height - queueMenu.height - 16,point.y))
        queueMenu.open()
    }
    Connections {
        target: root.deckPlayer
        function onQueueChanged() { queueMenu.close(); root.cancelQueueDrag() }
    }
    Timer {
        interval: 40; repeat: true; running: root.queueDragSource >= 0 && (root.queueDragY < 36 || root.queueDragY > trackList.height - 36)
        onTriggered: {
            trackList.contentY = Math.max(0,Math.min(Math.max(0,trackList.contentHeight - trackList.height),trackList.contentY + (root.queueDragY < 36 ? -10 : 10)))
            root.updateQueueDrop()
        }
    }
    property bool queueOpen: false
    property bool helpOpen: false
    readonly property bool menuOpen: fontPicker.shown || menu.visible || preferences.visible || crossfadeMenu.visible || queueMenu.visible || songMenu.visible || musicBrowser.actionsOpen
    property bool backgroundBlur: player.backgroundBlur && native.supportsBlur
    onBackgroundBlurChanged: { native.effects(root, backgroundBlur); Qt.callLater(updateMask) }
    property bool muted: false
    property real rememberedVolume: .65
    property real spinAngle: 0
    property real spinSpeed: 0
    property real wavePhase: 0
    property real progress: root.deckPlayer.duration > 0 ? root.deckPlayer.position / root.deckPlayer.duration : 0
    property color surface: theme.colors.card
    property color inset: theme.colors.surface
    property color ink: theme.colors.text
    property color mutedInk: theme.colors.muted
    property color accent: theme.colors.accent
    property color hairline: theme.colors.outline
    property color hoverFill: theme.colors.hover
    property bool light: inset.hslLightness > .5
    property bool animate: player.motion && theme.motionScale > 0
    readonly property int feedbackTime: SpunStyle.feedback
    readonly property int transitionTime: SpunStyle.navigate
    property int tick: 0
    function useLocal() {
        if (useCider && cider.playing) cider.pause()
        useCider = false
    }
    function time(ms) {
        let sec = Math.max(0, Math.floor(ms / 1000))
        return Math.floor(sec / 60).toString().padStart(2,"0") + ":" + (sec % 60).toString().padStart(2,"0")
    }
    function toggleMute() {
        if (root.deckPlayer.volume > 0) { rememberedVolume = root.deckPlayer.volume; root.deckPlayer.volume = 0 }
        else root.deckPlayer.volume = rememberedVolume > 0 ? rememberedVolume : .65
    }
    function updateMask() { native.shape(root, sideOpen) }
    function toggleQueue() {
        if (miniMode) { player.miniMode = false; queueOpen = true }
        else queueOpen = !queueOpen
    }
    function openSettings() {
        if (miniMode) { player.miniMode = false; Qt.callLater(openSettings); return }
        menu.x = Qt.binding(function() { return deck.x + deck.width - menu.width })
        menu.y = Qt.binding(function() { return Math.max(12,deck.y - menu.height - 10) })
        songMenu.close(); musicBrowser.closeActions(); menu.open()
    }
    onQueueOpenChanged: { if (queueOpen) queuePrepared = true; queueMenu.close(); cancelQueueDrag(); if (queueOpen) libraryOpen = false; if (!queueOpen) closeQueueSearch(); if (queueOpen && miniMode) player.miniMode = false; cider.queueVisible = queueOpen && useCider; Qt.callLater(updateMask) }
    onHelpOpenChanged: { if (helpOpen && miniMode) player.miniMode = false; Qt.callLater(updateMask) }
    onMenuOpenChanged: Qt.callLater(updateMask)
    onWidthChanged: Qt.callLater(updateMask)
    Component.onCompleted: { if (!testMode && player.ciderAutoStart) { useCider = true; cider.ensureRunning() }; updateMask(); native.place(root); Qt.callLater(presentDisc); syncLyrics() }
    onClosing: player.save()

    Rectangle {
        objectName: "blurBackdrop"
        width: root.width; height: root.height
        visible: root.backgroundBlur
        color: Qt.alpha(root.inset, .18)
        radius: 28 * theme.radius
        MouseArea { anchors.fill: parent; onPressed: root.startSystemMove() }
    }

    Shortcut { sequence: "Space"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: root.useCider ? cider.toggle() : player.count ? player.toggle() : files.open() }
    Shortcut { sequence: "Ctrl+V"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: root.openMusicLink("", true) }
    Shortcut { sequence: "Ctrl+O"; onActivated: files.open() }
    Shortcut { sequence: "Ctrl+Shift+O"; onActivated: folder.open() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
    Shortcut { sequence: "Ctrl+F"; onActivated: root.libraryOpen ? musicBrowser.focusSearch() : root.openQueueSearch() }
    Shortcut { sequence: "Ctrl+B"; enabled: root.useCider; onActivated: root.openLibrary() }
    Shortcut { sequence: "Ctrl+L"; onActivated: root.toggleQueue() }
    Shortcut { sequence: "Ctrl+M"; onActivated: player.miniMode = !player.miniMode }
    Shortcut { sequence: "Right"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: root.deckPlayer.seek(root.deckPlayer.position + 5000) }
    Shortcut { sequence: "Left"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: root.deckPlayer.seek(root.deckPlayer.position - 5000) }
    Shortcut { sequence: "Ctrl+Right"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: root.deckPlayer.next() }
    Shortcut { sequence: "Ctrl+Left"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: root.deckPlayer.previous() }
    Shortcut { sequence: "Up"; enabled: !root.libraryOpen && !trackList.activeFocus && !crossfadeMenu.visible && !preferences.visible && !root.editingText && !(root.discFlipped && (albumList.activeFocus || lyricList.activeFocus)); onActivated: root.deckPlayer.volume = Math.min(1, root.deckPlayer.volume + .05) }
    Shortcut { sequence: "Down"; enabled: !root.libraryOpen && !trackList.activeFocus && !crossfadeMenu.visible && !preferences.visible && !root.editingText && !(root.discFlipped && (albumList.activeFocus || lyricList.activeFocus)); onActivated: root.deckPlayer.volume = Math.max(0, root.deckPlayer.volume - .05) }
    Shortcut { sequence: "M"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: root.toggleMute() }
    Shortcut { sequence: "Escape"; onActivated: { if (fontPicker.shown || fontPicker.opening) { fontPicker.close(); return }; if (preferences.visible) { preferences.close(); return }; if (crossfadeMenu.visible) { crossfadeMenu.close(); return }; if (queueMenu.visible) { queueMenu.close(); return }; if (songMenu.visible) { songMenu.close(); return }; if (musicBrowser.actionsOpen) { musicBrowser.closeActions(); return }; if (root.queueSearchOpen) { root.closeQueueSearch(); return }; if (root.libraryOpen) { if (musicBrowser.detail && musicBrowser.browser.collectionQuery.length) musicBrowser.browser.collectionQuery = ""; else if (musicBrowser.detail) musicBrowser.browser.back(); else root.libraryOpen = false; return }; if (root.discFlipped) { root.discFlipped = false; return }; root.queueOpen = false; root.helpOpen = false; menu.close(); if (root.miniMode) player.miniMode = false } }
    Shortcut { sequence: "Y"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: { if (root.deckPlayer.count) { root.discFlipped = true; root.lyricsView = !root.lyricsView } } }
    Shortcut { sequence: "F"; enabled: !root.editingText && !crossfadeMenu.visible && !preferences.visible; onActivated: root.flipDisc() }
    Shortcut { sequence: "F1"; onActivated: root.helpOpen = !root.helpOpen }

    FileDialog {
        id: files
        objectName: "musicDialog"
        title: "Add music"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Music (*.mp3 *.flac *.wav *.ogg *.opus *.m4a *.aac *.aiff *.aif *.wma)", "All files (*)"]
        onAccepted: { root.useLocal(); player.addUrls(selectedFiles) }
    }
    FolderDialog { id: folder; title: "Add an album folder"; onAccepted: { root.useLocal(); player.addUrls([selectedFolder], player.count === 0) } }
    FileDialog { id: cover; title: "Choose the disc artwork"; nameFilters: ["Artwork (*.jpg *.jpeg *.png *.webp)"]; onAccepted: player.setCover(selectedFile) }

    Timer {
        interval: 16; repeat: true
        running: root.animate && root.visible && root.visibility !== Window.Minimized && native.exposed && (root.deckPlayer.playing || root.spinSpeed > .02)
        property double lastTime: 0
        onRunningChanged: lastTime = Date.now()
        onTriggered: {
            const now = Date.now()
            const dt = Math.min(.05, (now-lastTime)/1000)
            lastTime = now
            const target = root.deckPlayer.playing ? 9 : 0
            root.spinSpeed += (target-root.spinSpeed)*Math.min(1,dt*2.2)
            if (!root.discFlipped) root.spinAngle = (root.spinAngle+root.spinSpeed*dt)%360
            if (root.deckPlayer.playing) root.wavePhase=(root.wavePhase+dt*2.8)%(2*Math.PI)
        }
    }
    Connections {
        target: root.deckPlayer
        function onTrackChanged() { scrubber.cancelScrub(); if (!root.deckPlayer.count) root.discFlipped = false; Qt.callLater(root.presentDisc) }
        function onArtworkChanged() { Qt.callLater(root.presentDisc) }
        function onErrorChanged() { if (root.deckPlayer.error.length && root.miniMode) player.miniMode = false }
    }
    Connections {
        target: player
        function onImported() { if (player.count > 0) root.useLocal() }
        function onPlayingChanged() { if (player.playing) root.useLocal() }
    }

    // Centered source and window controls.
    Rectangle {
        id: badge
        objectName: "sourceBar"
        visible: !root.miniMode
        x: 265 - width / 2; y: 13; width: native.hyprland ? 256 : 344; height: 48; radius: 24
        color: root.surface
        border.width: 0
        MouseArea { anchors.fill: parent; onPressed: root.startSystemMove() }
        Rectangle {
            objectName: "sourceIndicator"
            SpunSpring { id: sourceMotion; targetValue: root.useCider ? 128 : 56 }
            x: sourceMotion.value; y: 6; width: 72; height: 36; radius: 18
            color: root.inset
        }
        Row {
            x: 56; y: 6; spacing: 0
            Repeater {
                model: ["Local", "Cider"]
                AbstractButton {
                    id: sourceTab
                    objectName: index === 0 ? "localSourceButton" : "ciderSourceButton"
                    required property string modelData
                    required property int index
                    width: 72; height: 36
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    Accessible.name: modelData
                    background: Rectangle {
                        radius: 18
                        color: "transparent"
                        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; pressed: sourceTab.down; focused: sourceTab.visualFocus; hovered: sourceTab.hovered }
                        border.width: sourceTab.visualFocus ? 2 : 0; border.color: root.accent
                    }
                    contentItem: SpunText { text: parent.modelData; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: (root.useCider === (parent.index === 1)) ? root.accent : root.mutedInk; font.pixelSize: SpunStyle.body; font.weight: Font.Medium }
                    onClicked: { if (index === 0 && root.useCider && cider.playing) cider.pause(); root.useCider = index === 1; if (index === 1 && player.ciderAutoStart) cider.ensureRunning() }
                }
            }
        }
        IconButton {
            objectName: "addMusicButton"
            x: 4; y: 4; glyphName: root.useCider ? "search" : "plus"; tip: root.useCider ? "Browse music · Ctrl+B" : "Add music · Ctrl+O"
            selected: root.libraryOpen; fill: root.libraryOpen ? root.inset : "transparent"
            ink: root.libraryOpen ? root.accent : root.ink; hoverFill: root.hoverFill; onClicked: root.useCider ? root.openLibrary() : files.open()
        }
        IconButton {
            objectName: "queueButton"
            x: 212; y: 4; glyphName: "queue"; tip: "Queue · Ctrl+L"
            selected: root.queueOpen; fill: root.queueOpen ? root.inset : "transparent"
            ink: root.queueOpen ? root.accent : root.ink; hoverFill: root.hoverFill
            onClicked: root.queueOpen = !root.queueOpen
        }
        IconButton { visible: !native.hyprland; x: 256; y: 4; glyphName: "minus"; tip: "Minimize"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: root.showMinimized() }
        IconButton { visible: !native.hyprland; x: 300; y: 4; glyphName: "close"; tip: "Close Spun"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: Qt.quit() }
    }

    Item {
        id: platter
        x: root.miniMode ? 9.2 : 45; y: root.miniMode ? 9.2 : 74
        width: 440; height: 440; clip: true
        scale: root.miniMode ? .64 : 1
        transformOrigin: Item.TopLeft
        HoverHandler { id: platterHover }
        // Shadow is circular, so the player keeps its silhouette on any wallpaper.
        Repeater {
            model: 7
            Rectangle {
                required property int index
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 9
                width: 410 + index * 3; height: width; radius: width / 2
                color: "transparent"; border.width: 8; border.color: "#05000000"
            }
        }
        Loader {
            objectName: "outgoingDiscLoader"
            anchors.centerIn: parent; width: 410; height: 410
            active: root.outgoingOpacity > 0
            visible: active
            opacity: root.outgoingOpacity
            transform: Translate { x: root.outgoingOffset }
            sourceComponent: Item {
                Disc { anchors.fill: parent; artwork: presentation.outgoing; rotation: root.outgoingAngle }
                Disc { anchors.fill: parent; overlay: true }
            }
        }
        Item {
            id: discAssembly
            anchors.centerIn: parent; width: 410; height: 410
            opacity: root.incomingOpacity
            transform: Translate { x: root.swapOffset }
            enabled: !root.swapRunning
            Flipable {
                id: discSides
                objectName: "discSides"
                anchors.fill: parent
                transform: Rotation {
                    id: flipRotation
                    origin.x: 205; origin.y: 205; axis.x: 0; axis.y: 1; axis.z: 0
                    angle: root.discFlipped ? 180 : 0
                    Behavior on angle { NumberAnimation { duration: SpunStyle.hero; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.standardCurve } }
                }
                front: Item {
                    anchors.fill: parent
                    Disc {
                        id: face
                        objectName: "discFace"
                        anchors.fill: parent
                        artwork: presentation.artwork
                        rotation: root.spinAngle
                    }
                    Disc { anchors.fill: parent; overlay: true }
                }
                back: Item {
                    objectName: "discBack"
                    anchors.fill: parent
                    Disc { anchors.fill: parent; labelColor: root.surface }
                    SpunText {
                        objectName: "discAlbumTitle"
                        id: albumHeading
                        x: 84; y: 52; width: 242; height: 50
                        text: root.lyricsView ? root.deckPlayer.title : root.discDetails.title || root.deckPlayer.album || "Unknown album"
                        font.pixelSize: root.miniMode ? 22 : SpunStyle.title; font.weight: Font.Normal
                        fontSizeMode: Text.Fit; minimumPixelSize: root.miniMode ? 17 : 15
                        color: root.ink; horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap
                        maximumLineCount: 2; elide: Text.ElideRight
                        HoverHandler { id: albumTitleHover }
                        SpunToolTip { visible: albumTitleHover.hovered && albumHeading.truncated; text: albumHeading.text }
                    }
                    SpunText {
                        id: albumArtist
                        objectName: "discAlbumArtist"
                        x: 72; y: 107; width: 266; height: 20
                        text: root.lyricsView ? root.deckPlayer.artist : root.discDetails.artist || root.deckPlayer.artist
                        font.pixelSize: root.miniMode ? 18 : SpunStyle.body; color: root.mutedInk
                        horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        HoverHandler { id: albumArtistHover }
                        SpunToolTip { visible: albumArtistHover.hovered && albumArtist.truncated; text: albumArtist.text }
                    }
                    SpunText {
                        objectName: "discAlbumYear"
                        x: 72; y: 131; width: 266; height: 18
                        text: root.lyricsView ? "" : root.discDetails.year || ""
                        font.pixelSize: root.miniMode ? 16 : SpunStyle.caption; color: root.accent
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ListView {
                        id: albumList
                        objectName: "albumTrackList"
                        x: 68; y: 262; width: 274; height: root.miniMode ? 76 : 82
                        readonly property int rowHeight: root.miniMode ? 36 : 32
                        property int rememberedRow: 0
                        property string rememberedAlbum: ""
                        activeFocusOnTab: root.discFlipped && !root.lyricsView
                        Accessible.name: "Album tracks"
                        function rememberPosition() { rememberedRow = Math.max(0, Math.round(contentY / (rowHeight + spacing))) }
                        function moveToRow(row) {
                            cancelFlick()
                            rememberedRow = Math.max(0, Math.min(count-1, row))
                            positionViewAtIndex(rememberedRow, ListView.Beginning)
                        }
                        function syncAlbum() {
                            if (!count) return
                            const key = root.useCider + "|" + root.discDetails.title + "|" + root.discDetails.artist
                            if (rememberedAlbum !== key) {
                                rememberedAlbum = key
                                rememberedRow = Math.max(0, root.albumTracks.findIndex(track => track.current === true || (!!track.id && track.id === root.discDetails.currentId)))
                            }
                            moveToRow(rememberedRow)
                        }
                        Keys.onPressed: event => {
                            switch (event.key) {
                            case Qt.Key_Up: moveToRow(rememberedRow-1); break
                            case Qt.Key_Down: moveToRow(rememberedRow+1); break
                            case Qt.Key_PageUp: moveToRow(rememberedRow-Math.floor(height/(rowHeight+spacing))); break
                            case Qt.Key_PageDown: moveToRow(rememberedRow+Math.floor(height/(rowHeight+spacing))); break
                            case Qt.Key_Home: moveToRow(0); break
                            case Qt.Key_End: moveToRow(count-1); break
                            default: event.accepted = false; return
                            }
                            event.accepted = true
                        }
                        TapHandler { onTapped: albumList.forceActiveFocus() }
                        onMovementEnded: rememberPosition()
                        visible: !root.lyricsView
                        model: root.albumTracks
                        clip: true; spacing: 2; boundsBehavior: Flickable.StopAtBounds
                        snapMode: ListView.SnapToItem
                        Connections { target: root; function onMiniModeChanged() { Qt.callLater(function() { albumList.moveToRow(albumList.rememberedRow) }) } }
                        onModelChanged: Qt.callLater(syncAlbum)
                        ScrollBar.vertical: ScrollBar {
                            width: 3; padding: 0; minimumSize: .12; policy: albumList.contentHeight > albumList.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                            contentItem: Rectangle { implicitWidth: 3; implicitHeight: 20; radius: 1.5; color: root.accent; opacity: parent.active || albumList.activeFocus ? .7 : .35 }
                            background: null
                        }
                        delegate: Item {
                            id: albumRow
                            required property var modelData
                            required property int index
                            readonly property bool current: modelData.current === true || (!!modelData.id && modelData.id === root.discDetails.currentId)
                            readonly property int numberWidth: root.miniMode ? 42 : 34
                            readonly property int durationWidth: root.miniMode ? 54 : 46
                            x: 6; width: albumList.width-12; height: albumList.rowHeight
                            SpunText {
                                objectName: "albumNumber" + albumRow.index
                                width: albumRow.numberWidth; anchors.baseline: albumTrackTitle.baseline
                                text: (albumRow.modelData.disc > 1 ? albumRow.modelData.disc + "." : "") + (albumRow.modelData.number || albumRow.index+1).toString().padStart(2,"0")
                                font.pixelSize: root.miniMode ? 16 : SpunStyle.caption; color: albumRow.current ? root.accent : root.mutedInk
                                elide: Text.ElideRight
                            }
                            SpunText {
                                id: albumTrackTitle
                                objectName: "albumTitle" + albumRow.index
                                x: albumRow.numberWidth+6; width: albumRow.width-x-albumRow.durationWidth-8
                                anchors.verticalCenter: parent.verticalCenter
                                text: albumRow.modelData.title; elide: Text.ElideRight
                                font.pixelSize: root.miniMode ? 18 : SpunStyle.body; color: albumRow.current ? root.accent : root.ink
                                HoverHandler { id: trackTitleHover }
                                SpunToolTip { visible: trackTitleHover.hovered && parent.truncated; text: parent.text }
                            }
                            SpunText {
                                objectName: "albumDuration" + albumRow.index
                                width: albumRow.durationWidth
                                anchors.right: parent.right; anchors.baseline: albumTrackTitle.baseline
                                text: albumRow.modelData.duration > 0 ? root.time(albumRow.modelData.duration) : "—"
                                horizontalAlignment: Text.AlignRight
                                font.pixelSize: root.miniMode ? 16 : SpunStyle.caption; color: albumRow.current ? root.accent : root.mutedInk
                            }
                        }
                    }
                    ListView {
                        id: lyricList
                        objectName: "lyricList"
                        x: 76; y: 260; width: 258; height: 84
                        visible: root.lyricsView && lyrics.lines.length > 0
                        model: lyrics.lines; clip: true; spacing: 8
                        boundsBehavior: Flickable.StopAtBounds
                        activeFocusOnTab: root.discFlipped && root.lyricsView
                        Accessible.name: "Lyrics"
                        property bool following: true
                        function followLine() {
                            if (following && lyrics.currentIndex >= 0) positionViewAtIndex(lyrics.currentIndex, ListView.Beginning)
                        }
                        onModelChanged: { following = true; Qt.callLater(function() { positionViewAtBeginning(); followLine() }) }
                        onMovementStarted: following = false
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                                following = false; contentY = Math.max(0, Math.min(Math.max(0, contentHeight-height), contentY + (event.key===Qt.Key_Down ? 30 : -30))); event.accepted = true
                            } else event.accepted = false
                        }
                        TapHandler { onTapped: lyricList.forceActiveFocus() }
                        Connections { target: lyrics; function onCurrentIndexChanged() { lyricList.followLine() } }
                        ScrollBar.vertical: ScrollBar {
                            width: 3; padding: 0; minimumSize: .12
                            contentItem: Rectangle { implicitWidth: 3; implicitHeight: 20; radius: 1.5; color: root.accent; opacity: .5 }
                            background: null
                        }
                        delegate: SpunText {
                            id: lyricLine
                            objectName: "lyricLine" + index
                            required property var modelData
                            required property int index
                            readonly property bool seekable: lyrics.timed && !lyrics.loading && modelData.start >= 0 && modelData.start < root.deckPlayer.duration && (!root.useCider || cider.canSeek)
                            function seekHere() {
                                if (seekable && lyrics.seekToLine(index)) {
                                    lyricList.following = true
                                    lyricList.forceActiveFocus()
                                    Qt.callLater(lyricList.followLine)
                                }
                            }
                            width: lyricList.width-10; x: 5
                            text: modelData.text; textFormat: Text.PlainText
                            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: root.miniMode ? 19 : 15
                            color: (seekable && lyricHit.containsMouse) || (lyrics.timed && index === lyrics.currentIndex) ? root.accent : root.ink
                            opacity: lyrics.timed && index !== lyrics.currentIndex && !(seekable && lyricHit.containsMouse) ? .62 : 1
                            Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.standardCurve } }
                            Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback } }
                            Accessible.role: seekable ? Accessible.Button : Accessible.StaticText
                            Accessible.name: text
                            Accessible.onPressAction: seekHere()
                            SpunToolTip { visible: lyricLine.seekable && lyricHit.containsMouse; text: "Seek to " + root.time(lyricLine.modelData.start) }
                            MouseArea {
                                id: lyricHit
                                objectName: "lyricHit" + lyricLine.index
                                anchors.fill: parent; hoverEnabled: true
                                enabled: lyricLine.seekable
                                cursorShape: Qt.PointingHandCursor
                                preventStealing: false
                                onClicked: lyricLine.seekHere()
                                onDoubleClicked: mouse => { mouse.accepted = true }
                            }
                        }
                    }
                    Rectangle {
                        x: lyricList.x; y: lyricList.y+lyricList.height-12
                        width: lyricList.width-4; height: 12
                        visible: lyricList.visible && !lyricList.atYEnd
                        gradient: Gradient {
                            GradientStop { position: 0; color: Qt.alpha(root.surface,0) }
                            GradientStop { position: 1; color: root.surface }
                        }
                    }
                    Column {
                        x: 80; y: 262; width: 250; spacing: 4
                        visible: root.lyricsView && !lyrics.lines.length
                        SpunText {
                            width: parent.width; text: lyrics.loading ? "Loading lyrics…" : lyrics.message
                            color: root.mutedInk; font.pixelSize: root.miniMode ? 18 : SpunStyle.body
                            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
                        }
                        IconButton {
                            objectName: "lyricsRetry"
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: !lyrics.loading && lyrics.message !== "No lyrics available"
                            glyphName: "repeat"; tip: "Retry lyrics"; ink: root.accent; hoverFill: root.hoverFill
                            onClicked: lyrics.refresh()
                        }
                    }
                    Row {
                        objectName: "discBackFooter"
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 348; height: 36; spacing: 10
                        SpunText {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !root.lyricsView && root.albumTracks.length > 0
                            text: root.albumTracks.length + (root.discDetails.scope === "loaded" ? (root.albumTracks.length === 1 ? " loaded track" : " loaded tracks") : (root.albumTracks.length === 1 ? " track" : " tracks"))
                            font.pixelSize: root.miniMode ? 16 : SpunStyle.caption; color: root.mutedInk
                        }
                        IconButton {
                            visible: root.lyricsView && lyrics.timed && !lyricList.following
                            objectName: "followLyricsButton"
                            width: 36; height: 36; glyphName: "follow"; tip: "Follow current line"
                            ink: root.accent; hoverFill: root.hoverFill
                            onClicked: { lyricList.following = true; lyricList.followLine() }
                        }
                        IconButton {
                            objectName: "lyricsViewButton"
                            width: 36; height: 36; glyphName: root.lyricsView ? "queue" : "lyrics"
                            tip: root.lyricsView ? "Album tracks · Y" : "Lyrics · Y"
                            ink: root.accent; hoverFill: root.hoverFill
                            onClicked: root.lyricsView = !root.lyricsView
                        }
                        IconButton {
                            objectName: "discReturnButton"
                            width: 36; height: 36
                            glyphName: "flip"; tip: "Show artwork · F"
                            ink: root.accent; hoverFill: root.hoverFill
                            onClicked: root.discFlipped = false
                        }
                    }

                    Column {
                        x: 80; y: 264; width: 250; spacing: 5
                        visible: !root.lyricsView && root.useCider && !root.albumTracks.length
                        SpunText {
                            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                            text: cider.discLoading ? "Loading album…" : cider.discError
                            font.pixelSize: root.miniMode ? 18 : SpunStyle.body; color: root.mutedInk
                        }
                        IconButton {
                            objectName: "retryDiscButton"
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: !cider.discLoading && !!cider.discError
                            glyphName: "repeat"; tip: "Retry album details"; ink: root.accent; hoverFill: root.hoverFill
                            onClicked: cider.refreshDisc()
                        }
                    }
                }
            }
            // Dragging the label moves the window; scrolling its reverse browses the tracks.
            DragHandler {
                target: null
                enabled: !root.discFlipped
                onActiveChanged: if (active) root.startSystemMove()
            }
            Item {
                x: 65; y: 42; width: 280; height: 108
                visible: root.discFlipped
                DragHandler { target: null; onActiveChanged: if (active) root.startSystemMove() }
            }
            TapHandler {
                id: discTap
                acceptedButtons: Qt.LeftButton
                onDoubleTapped: {
                    const p = lyricList.mapFromItem(discAssembly, discTap.point.position.x, discTap.point.position.y)
                    if (root.discFlipped && root.lyricsView && p.x >= 0 && p.x < lyricList.width && p.y >= 0 && p.y < lyricList.height) return
                    root.flipDisc()
                }
            }
            TapHandler { acceptedButtons: Qt.RightButton; onTapped: root.openSettings() }



        }
        Connections {
            target: presentation
            function onSwapRequested() {
                // The presenter has already installed the new outgoing image.
                // Stop the old sequence without releasing that new image.
                discSwap.stop(); root.resetSwap()
                if (!root.animate || root.discFlipped || !root.visible || !native.exposed) { presentation.releaseOutgoing(); return }
                scrubber.cancelScrub(); root.outgoingAngle = root.spinAngle
                root.swapOffset = 440; root.outgoingOffset = 0; root.outgoingOpacity = 1; root.incomingOpacity = 0
                discSwap.departureTime = SpunStyle.enter; discSwap.arrivalTime = SpunStyle.hero
                discSwap.start()
            }
        }
        SequentialAnimation {
            id: discSwap
            objectName: "discSwap"
            // Keep timing stable during a swap; reduced motion cancels the
            // sequence explicitly instead of rewriting its active durations.
            property int departureTime: 200
            property int arrivalTime: 350
            ParallelAnimation {
                NumberAnimation { target: root; property: "outgoingOffset"; to: -440; duration: discSwap.departureTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.exitCurve }
                NumberAnimation { target: root; property: "outgoingOpacity"; to: 0; duration: discSwap.departureTime }
            }
            ParallelAnimation {
                NumberAnimation { target: root; property: "swapOffset"; to: 0; duration: discSwap.arrivalTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
                NumberAnimation { target: root; property: "incomingOpacity"; to: 1; duration: discSwap.departureTime }
            }
            onFinished: { root.resetSwap(); presentation.releaseOutgoing() }
        }
        ProgressRing {
            objectName: "progressRing"
            anchors.fill: parent
            visible: root.deckPlayer.count > 0
            progress: scrubber.scrubbing ? scrubber.previewFraction : root.progress
            phase: root.wavePhase
            amplitude: root.deckPlayer.playing ? 2.8 : 0
            accent: root.accent
            Behavior on amplitude { NumberAnimation { duration: SpunStyle.enter } }
        }
        MouseArea {
            id: scrubber
            objectName: "scrubber"
            anchors.fill: parent
            hoverEnabled: true
            property bool scrubbing: false
            property real previewFraction: 0
            readonly property bool canSeek: !root.swapRunning && root.deckPlayer.duration > 0 && (!root.useCider || cider.canSeek)
            readonly property bool showPreview: canSeek && (scrubbing || (containsMouse && onRim(mouseX,mouseY)))
            cursorShape: canSeek && onRim(mouseX,mouseY) ? Qt.PointingHandCursor : Qt.ArrowCursor
            function onRim(x,y) { let r=Math.hypot(x-220,y-220); return r > 205 && r < 224 }
            function updatePreview(x,y) {
                let a=Math.atan2(y-220,x-220)+Math.PI/2
                if (a < 0) a += 2*Math.PI
                previewFraction = a/(2*Math.PI)
            }
            function cancelScrub() { scrubbing = false }
            onPressed: mouse => {
                if (onRim(mouse.x,mouse.y) && canSeek) { updatePreview(mouse.x,mouse.y); scrubbing=true }
                else mouse.accepted=false
            }
            onPositionChanged: mouse => { if (scrubbing || onRim(mouse.x,mouse.y)) updatePreview(mouse.x,mouse.y) }
            onReleased: {
                if (scrubbing && canSeek) root.deckPlayer.seek(root.deckPlayer.duration * previewFraction)
                scrubbing=false
            }
            onCanceled: cancelScrub()
            onWheel: wheel => { if (root.discFlipped && !onRim(wheel.x,wheel.y)) { wheel.accepted=false; return }; root.deckPlayer.volume = Math.max(0, Math.min(1, root.deckPlayer.volume + wheel.angleDelta.y/2400)); wheel.accepted=true }
        }
        DropArea {
            anchors.fill: parent
            onEntered: drag => { if (!drag.hasUrls && !drag.hasText) drag.accepted=false }
            onDropped: drop => {
                const urls = drop.hasUrls ? drop.urls : []
                if (urls.length && urls.every(url => url.toString().startsWith("file:"))) {
                    root.useLocal(); player.addUrls(urls); drop.acceptProposedAction()
                } else if (root.openMusicLink(urls.length ? urls[0].toString() : drop.text, false)) drop.acceptProposedAction()
            }
            Rectangle {
                anchors.fill: parent; radius: width / 2
                visible: parent.containsDrag
                color: root.inset; opacity: .94; border.width: 2; border.color: root.accent
                SpunText { anchors.centerIn: parent; text: "Open music"; color: root.ink; font.pixelSize: 13; font.letterSpacing: 1.3 }
            }
        }

    }

    Rectangle {
        id: rimPreview
        objectName: "rimPreview"
        visible: scrubber.showPreview
        z: 10
        readonly property real angle: scrubber.previewFraction * 2 * Math.PI
        readonly property point location: platter.mapToItem(root.contentItem, 220 + Math.sin(angle)*170, 220 - Math.cos(angle)*170)
        x: location.x - width/2; y: location.y - height/2
        width: 58; height: 26; radius: 13
        color: root.surface
        SpunText { anchors.centerIn: parent; text: root.time(root.deckPlayer.duration * scrubber.previewFraction); font.pixelSize: SpunStyle.caption; color: root.accent }
    }
    Timer { id: miniReveal; interval: 1400 }
    Item {
        visible: root.miniMode
        x: 50; y: 268; width: 200; height: 84
        HoverHandler { id: miniDockHover }
    }
    Rectangle {
        id: miniControls
        objectName: "miniControls"
        visible: root.miniMode
        x: 50; y: 302; width: 200; height: 48; radius: 24
        color: root.surface
        opacity: platterHover.hovered || miniDockHover.hovered || miniControlsHover.hovered || miniReveal.running || miniPrevious.visualFocus || miniPlay.visualFocus || miniNext.visualFocus || miniRestore.visualFocus ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: root.transitionTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.standardCurve } }
        HoverHandler { id: miniControlsHover }
        Row {
            x: 8; y: 4; spacing: 0
            IconButton { id: miniPrevious; objectName: "miniPrevious"; glyphName: "previous"; tip: "Previous track"; ink: root.ink; hoverFill: root.hoverFill; enabled: root.useCider ? cider.canPrevious : player.count > 0; onClicked: root.deckPlayer.previous() }
            IconButton { id: miniPlay; objectName: "miniPlay"; glyphSize: 26; width: 48; glyphName: root.deckPlayer.playing ? "pause" : "play"; tip: root.deckPlayer.playing ? "Pause" : "Play"; fill: root.accent; ink: theme.colors.onAccent; hoverFill: Qt.lighter(root.accent,1.08); onClicked: root.useCider ? cider.toggle() : player.count ? player.toggle() : files.open() }
            IconButton { id: miniNext; objectName: "miniNext"; glyphName: "next"; tip: "Next track"; ink: root.ink; hoverFill: root.hoverFill; enabled: root.useCider ? cider.canNext : player.count > 0; onClicked: root.deckPlayer.next() }
            IconButton { id: miniRestore; objectName: "miniRestore"; glyphName: "external"; tip: "Full player · Ctrl+M"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: player.miniMode = false }
        }
    }

    Rectangle {
        id: deck
        objectName: "playerDeck"
        visible: !root.miniMode
        x: 62; y: 533; width: 406; height: 144; radius: SpunStyle.panelRadius
        color: root.surface; border.width: 0
        MouseArea { anchors.fill: parent; onPressed: root.startSystemMove() }
        SpunText {
            objectName: "songTitle"
            x: 24; y: 16; width: root.useCider ? 238 : 272
            text: root.deckPlayer.title; color: root.ink; elide: Text.ElideRight
            font.pixelSize: SpunStyle.title; font.weight: Font.Normal
        }
        IconButton {
            objectName: "currentSongActions"
            visible: root.useCider; enabled: cider.count > 0
            x: 264; y: 12; width: 40; height: 40
            glyphName: "more"; tip: "Song actions"; ink: root.mutedInk; hoverFill: root.hoverFill
            onClicked: { menu.close(); songMenu.x = deck.x + deck.width - songMenu.width; songMenu.y = Qt.binding(function() { return deck.y - songMenu.height - 8 }); songMenu.open() }
        }
        SpunText {
            x: 24; y: 48; width: 284
            text: root.deckPlayer.artist; color: root.mutedInk; elide: Text.ElideRight
            font.pixelSize: SpunStyle.body
        }
        SpunText {
            x: 307; y: 18; width: 77; horizontalAlignment: Text.AlignRight
            text: root.time(root.deckPlayer.position); color: root.mutedInk
            font.pixelSize: 13
        }
        Row {
            x: 16; y: 80; spacing: 4
            IconButton { objectName: "shuffleButton"; y: 4; selected: root.deckPlayer.shuffle; glyphName: "shuffle"; tip: root.deckPlayer.shuffle ? "Shuffle on" : "Shuffle off"; enabled: !root.useCider || !cider.controlBusy; ink: root.deckPlayer.shuffle ? root.accent : root.mutedInk; hoverFill: root.hoverFill; onClicked: root.deckPlayer.shuffle = !root.deckPlayer.shuffle }
            IconButton { objectName: "previousButton"; y: 4; glyphName: "previous"; tip: "Previous track · Ctrl+←"; ink: root.ink; hoverFill: root.hoverFill; enabled: root.useCider ? cider.canPrevious : player.count > 0; onClicked: root.deckPlayer.previous() }
            IconButton {
                objectName: "playButton"
                glyphSize: 26
                width: 56; height: 48
                glyphName: root.deckPlayer.playing ? "pause" : "play"
                tip: root.deckPlayer.playing ? "Pause · Space" : "Play · Space"
                fill: root.accent; ink: theme.colors.onAccent; hoverFill: Qt.lighter(root.accent, 1.08)
                onClicked: root.useCider ? cider.toggle() : player.count ? player.toggle() : files.open()
            }
            IconButton { objectName: "nextButton"; y: 4; glyphName: "next"; tip: "Next track · Ctrl+→"; ink: root.ink; hoverFill: root.hoverFill; enabled: root.useCider ? cider.canNext : player.count > 0; onClicked: root.deckPlayer.next() }
            IconButton {
                objectName: "repeatButton"
                y: 4
                selected: root.deckPlayer.repeatMode > 0; glyphName: root.deckPlayer.repeatMode === 2 ? "repeat_one" : "repeat"; tip: ["Repeat off", "Repeat queue", "Repeat one"][root.deckPlayer.repeatMode]
                ink: root.deckPlayer.repeatMode ? root.accent : root.mutedInk; hoverFill: root.hoverFill
                onClicked: root.deckPlayer.repeatMode = (root.deckPlayer.repeatMode + 1) % 3
            }
        }

        IconButton { x: 256; y: 84; glyphName: root.deckPlayer.volume > 0 ? "volume" : "mute"; tip: "Mute · M"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: root.toggleMute() }
        Slider {
            id: volumeSlider
            objectName: "volumeSlider"
            x: 300; y: 86; width: 44; height: 36
            from: 0; to: 1; value: root.deckPlayer.volume
            onMoved: root.deckPlayer.volume = value
            Accessible.name: "Volume"
            background: Rectangle {
                x: volumeSlider.leftPadding; y: volumeSlider.topPadding + volumeSlider.availableHeight/2-2
                width: volumeSlider.availableWidth; height: 3; radius: 2; color: root.hairline
                Rectangle { width: parent.width * volumeSlider.visualPosition; height: 3; radius: 2; color: root.accent }
            }
            handle: Rectangle {
                x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth-width)
                y: volumeSlider.topPadding + volumeSlider.availableHeight/2-height/2
                width: 4; height: 16; radius: 2; color: root.accent
            }
        }
        IconButton { id: menuButton; objectName: "menuButton"; x: 350; y: 84; glyphName: "more"; tip: "More actions"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: root.openSettings() }

    }

    SpunText {
        x: 65; y: 705; width: 400; horizontalAlignment: Text.AlignHCenter
        visible: !root.miniMode
        text: root.useCider || player.count ? "" : "Play demo"
        color: root.mutedInk; font.pixelSize: SpunStyle.caption
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { root.useLocal(); player.demo() } }
    }

    Rectangle {
        id: jewelCase
        objectName: "queuePanel"
        x: 534; width: 310; anchors.top: badge.top; anchors.bottom: deck.bottom; radius: SpunStyle.panelRadius
        visible: root.queueOpen; color: root.surface; border.width: 0
        transform: Translate { id: queueEntrance; x: 0 }
        onVisibleChanged: {
            queueAppear.stop(); opacity = 1; queueEntrance.x = 0
            if (visible && root.animate) queueAppear.start()
        }
        Connections { target: root; function onAnimateChanged() { if (!root.animate) { queueAppear.stop(); jewelCase.opacity = 1; queueEntrance.x = 0 } } }
        ParallelAnimation {
            id: queueAppear
            NumberAnimation { target: jewelCase; property: "opacity"; from: 0; to: 1; duration: root.feedbackTime }
            NumberAnimation { target: queueEntrance; property: "x"; from: 12; to: 0; duration: root.transitionTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
        }
        Rectangle { x: 8; y: 18; width: 6; height: parent.height-36; radius: 3; color: root.inset }
        Repeater {
            model: Math.max(0, Math.floor((jewelCase.height - 60) / 12))
            Rectangle { required property int index; x: 9; y: 30+index*12; width: 4; height: 1; color: root.hairline }
        }
        SpunText { visible: !root.queueSearchOpen; x: SpunStyle.outerInset; y: 20; height: SpunStyle.target; verticalAlignment: Text.AlignVCenter; text: "Queue"; color: root.ink; font.pixelSize: SpunStyle.heading }
        SpunText { visible: !root.queueSearchOpen; x: 150; y: 33; width: 66; horizontalAlignment: Text.AlignRight; text: root.useCider ? (cider.queueReady ? cider.queue.length : "") : player.count; color: root.mutedInk; font.pixelSize: 12 }
        SpunSearchField {
            app: root
            id: queueSearch
            objectName: "queueSearchInput"
            x: 24; y: 20; width: 214; height: SpunStyle.target
            visible: root.queueSearchOpen
            text: root.queueQuery
            onTextChanged: root.queueQuery = text
            placeholderText: "Song or artist"
            Accessible.name: "Search queue"
            onAccepted: {
                if (root.filteredQueue.length && (!root.useCider || (cider.queueReady && !cider.queueError.length)))
                    root.deckPlayer.select(root.filteredQueue[0].sourceIndex)
            }
        }
        IconButton {
            objectName: "queueSearchButton"
            x: 246; y: 20; width: 40; height: 40
            glyphName: root.queueSearchOpen ? "close" : "search"
            tip: root.queueSearchOpen ? "Close search" : "Search queue · Ctrl+F"
            ink: root.queueSearchOpen ? root.accent : root.mutedInk; hoverFill: root.hoverFill
            onClicked: root.queueSearchOpen ? root.closeQueueSearch() : root.openQueueSearch()
        }
        Rectangle { x: 30; y: 65; width: 252; height: 1; color: root.hairline }
        ListView {
            id: trackList
            objectName: "trackList"
            activeFocusOnTab: visible
            property int keyboardIndex: -1
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                    keyboardIndex = Math.max(0, Math.min(count - 1, (keyboardIndex < 0 ? currentIndex : keyboardIndex) + (event.key === Qt.Key_Down ? 1 : -1)))
                    positionViewAtIndex(keyboardIndex, ListView.Contain); event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    const row = root.filteredQueue[keyboardIndex < 0 ? currentIndex : keyboardIndex]
                    if (row && root.queueControlsReady) root.deckPlayer.select(row.sourceIndex)
                    event.accepted = true
                } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                    const row = itemAtIndex(keyboardIndex < 0 ? currentIndex : keyboardIndex)
                    if (row) root.showQueueActions(row.sourceIndex, row)
                    event.accepted = true
                }
            }
            x: SpunStyle.inset; y: 80; width: parent.width - 2 * SpunStyle.inset; height: queueFooter.y - 2 * SpunStyle.gap - y - (root.useCider && cider.queueError.length ? 32 : 0)
            model: root.filteredQueue; clip: true; spacing: SpunStyle.smallGap
            visible: !root.useCider || cider.queueReady
            currentIndex: root.filteredQueue.findIndex(row => row.sourceIndex === root.deckPlayer.currentIndex)
            onCurrentIndexChanged: if (currentIndex >= 0 && !root.queueQuery.trim().length) positionViewAtIndex(currentIndex, ListView.Contain)
            onModelChanged: Qt.callLater(function() {
                if (root.queueQuery.trim().length) positionViewAtBeginning()
                else if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
            })
            // Keep the pointer grab on the view, so long drags survive delegate recycling.
            MouseArea {
                id: queueDragArea
                objectName: "queueDragArea"
                parent: trackList; x: 0; y: 0; width: 22; height: trackList.height; z: 10
                enabled: root.queueReorderAllowed; preventStealing: true
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property real startY: 0
                property int pressedSource: -1
                onPressed: mouse => {
                    startY = mouse.y
                    const index = trackList.indexAt(0,mouse.y + trackList.contentY)
                    pressedSource = index >= 0 ? root.filteredQueue[index].sourceIndex : -1
                    root.queueDragRevision = cider.queueRevision
                }
                onPositionChanged: mouse => {
                    if (!pressed || pressedSource < 0) return
                    if (root.queueDragSource < 0 && Math.abs(mouse.y - startY) < 6) return
                    root.queueDragSource = pressedSource; root.queueDragY = mouse.y; root.updateQueueDrop()
                }
                onReleased: mouse => {
                    const from = root.queueDragSource, to = root.queueDropIndex, revision = root.queueDragRevision
                    root.cancelQueueDrag(); pressedSource = -1
                    if (from >= 0 && to >= 0 && mouse.x >= -12 && mouse.x <= trackList.width + 12 && mouse.y >= 0 && mouse.y <= trackList.height) root.moveQueueRow(from,to,revision)
                }
                onCanceled: { pressedSource = -1; root.cancelQueueDrag() }
            }
            ScrollBar.vertical: ScrollBar { }
            delegate: Rectangle {
                id: trackRow
                required property int index
                required property var modelData
                readonly property int sourceIndex: modelData.sourceIndex
                objectName: "queueRow" + sourceIndex
                width: trackList.width; height: SpunStyle.trackHeight; radius: SpunStyle.rowRadius
                color: "transparent"
                Rectangle {
                    anchors.fill: parent; radius: parent.radius; color: SpunStyle.selected
                    opacity: root.deckPlayer.currentIndex === trackRow.sourceIndex ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback } }
                }
                SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; enabled: root.queueControlsReady; pressed: rowHit.pressed; focused: trackList.activeFocus && trackList.keyboardIndex === trackRow.index; hovered: rowHover.hovered }
                border.width: trackList.activeFocus && trackList.keyboardIndex === index ? 2 : 0
                border.color: root.accent
                Accessible.role: Accessible.ListItem
                Accessible.name: modelData.track.title + ", " + modelData.track.artist
                Accessible.onPressAction: if (root.queueControlsReady) root.deckPlayer.select(sourceIndex)
                HoverHandler { id: rowHover }
                MouseArea { id: rowHit; anchors.fill: parent; enabled: root.queueControlsReady; onClicked: root.deckPlayer.select(trackRow.sourceIndex) }
                TrackContent {
                    anchors.fill: parent; app: root
                    title: trackRow.modelData.track.title; subtitle: trackRow.modelData.track.artist
                    artwork: trackRow.modelData.track.artwork || ""; artworkName: "queueArtwork" + trackRow.sourceIndex
                }
                IconButton { id: queueActions; objectName: "queueActions" + trackRow.sourceIndex; x: parent.width - SpunStyle.target; anchors.verticalCenter: parent.verticalCenter; glyphName: "more"; tip: "Queue actions"; ink: root.mutedInk; hoverFill: root.hoverFill; enabled: root.queueControlsReady; onClicked: root.showQueueActions(trackRow.sourceIndex,this) }
                TapHandler { acceptedButtons: Qt.RightButton; onTapped: root.showQueueActions(trackRow.sourceIndex,queueActions) }
                Item {
                    x: 0; y: 0; width: 22; height: SpunStyle.trackHeight
                    opacity: root.queueReorderAllowed && (rowHover.hovered || root.queueDragSource === trackRow.sourceIndex) ? .7 : 0
                    Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback } }
                    Glyph { anchors.centerIn: parent; width: SpunStyle.smallIcon; height: width; name: "grip"; ink: root.mutedInk }

                }
                Rectangle {
                    visible: root.queueDragSource >= 0 && root.queueDropIndex === trackRow.sourceIndex && root.queueDragSource !== root.queueDropIndex
                    x: 24; y: root.queueDragSource < root.queueDropIndex ? parent.height : -3
                    width: parent.width - 30; height: 2; radius: 1; color: root.accent
                }
            }
            SpunText {
                anchors.centerIn: parent; width: 200; visible: trackList.count === 0
                text: root.queueQuery.trim().length ? "No matches" : "No tracks"
                horizontalAlignment: Text.AlignHCenter; color: root.mutedInk; font.pixelSize: 14; lineHeight: 1.4
            }
        }
        SpunText {
            visible: root.useCider && cider.queueReady && cider.queueError.length > 0
            x: SpunStyle.outerInset; y: queueFooter.y - 40; width: parent.width - 2 * SpunStyle.outerInset; elide: Text.ElideRight
            text: cider.queueError; color: root.mutedInk; font.pixelSize: SpunStyle.caption
        }
        Column {
            x: 30; y: 100; width: 250; spacing: 16
            visible: root.useCider && !cider.queueReady
            SpunText {
                width: parent.width
                text: cider.queueBusy && !cider.queueError.length ? "Loading queue…" : cider.queueError
                wrapMode: Text.WordWrap; color: root.ink; font.pixelSize: 13
            }
            SpunText {
                width: parent.width; visible: cider.needsToken
                text: "In Cider: Settings → Connectivity → Manage External Application Access. Create a token for Spun."
                wrapMode: Text.WordWrap; color: root.mutedInk; font.pixelSize: SpunStyle.caption; lineHeight: 1.25
            }
            TextField {
                id: ciderToken
                objectName: "ciderTokenInput"
                width: parent.width; height: 38; visible: cider.needsToken
                echoMode: TextInput.Password; placeholderText: "Cider app token"
                font.family: SpunStyle.family; font.pixelSize: 12
                color: root.ink; placeholderTextColor: root.mutedInk
                selectionColor: root.accent; selectedTextColor: theme.colors.onAccent
                background: Rectangle { color: root.inset; radius: 9 * theme.radius }
                onAccepted: { cider.connectQueue(text); clear() }
            }
            Button {
                objectName: "connectCiderButton"
                width: parent.width; height: 36; visible: cider.needsToken
                enabled: ciderToken.text.trim().length > 0 && !cider.queueBusy
                text: "Connect"
                contentItem: SpunText { text: parent.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: theme.colors.onAccent; font.family: SpunStyle.family; font.pixelSize: 12 }
                background: Rectangle { color: root.accent; radius: 9 * theme.radius; opacity: parent.enabled ? 1 : .35 }
                onClicked: { cider.connectQueue(ciderToken.text); ciderToken.clear() }
            }
        }
        Rectangle { x: SpunStyle.outerInset; y: queueFooter.y - SpunStyle.gap; width: parent.width - 2 * SpunStyle.outerInset; height: 1; color: root.hairline }
        Item {
            id: queueFooter
            objectName: "queueFooter"
            x: SpunStyle.outerInset; y: parent.height - height - SpunStyle.inset
            width: parent.width - 2 * SpunStyle.outerInset; height: 48
            Button {
                id: openCiderButton
                visible: root.useCider
                anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: SpunStyle.target
                text: "Open Cider"
                font.family: SpunStyle.family; font.pixelSize: SpunStyle.body; font.weight: Font.Medium
                contentItem: SpunText { text: parent.text; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: root.accent }
                background: Rectangle {
                    color: SpunStyle.selected
                    SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; pressed: openCiderButton.down; focused: openCiderButton.visualFocus; hovered: openCiderButton.hovered }
                    radius: height / 2 * theme.radius
                    border.width: openCiderButton.visualFocus ? 2 : 0; border.color: root.accent
                }
                onClicked: cider.raise()
            }
            IconButton { visible: !root.useCider; x: 0; anchors.verticalCenter: parent.verticalCenter; glyphName: "plus"; tip: "Add tracks"; ink: root.ink; hoverFill: root.hoverFill; onClicked: files.open() }
            IconButton { visible: !root.useCider; x: SpunStyle.target + SpunStyle.smallGap; anchors.verticalCenter: parent.verticalCenter; glyphName: "folder"; tip: "Add album folder"; ink: root.ink; hoverFill: root.hoverFill; onClicked: folder.open() }
            Button {
                id: clearQueueButton
                visible: !root.useCider; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 76; height: SpunStyle.target
                text: "Clear"; enabled: player.count > 0
                font.family: SpunStyle.family; font.pixelSize: SpunStyle.body; font.weight: Font.Medium
                background: Rectangle {
                    radius: height / 2 * theme.radius
                    color: "transparent"
                    SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; enabled: clearQueueButton.enabled; pressed: clearQueueButton.down; focused: clearQueueButton.visualFocus; hovered: clearQueueButton.hovered }
                    border.width: clearQueueButton.visualFocus ? 2 : 0; border.color: root.accent
                }
                contentItem: SpunText { text: parent.text; font: parent.font; color: root.accent; opacity: parent.enabled ? 1 : .38; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: root.deckPlayer.clear()
            }
        }

    }

    Loader {
        id: musicBrowser
        x: jewelCase.x; anchors.top: jewelCase.top; width: jewelCase.width; anchors.bottom: deck.bottom
        visible: root.libraryOpen
        active: false
        // Build the browser on its first opening, then retain its scroll, search,
        // focus and transition state for every subsequent visit.
        onVisibleChanged: if (visible) active = true
        onLoaded: item.reveal()
        readonly property var browser: item ? item.browser : library
        readonly property bool detail: Object.keys(browser.collection).length > 0
        readonly property bool actionsOpen: item ? item.actionsOpen : false
        function closeActions() { if (item) item.closeActions() }
        function focusSearch() { if (item) item.focusSearch() }
        sourceComponent: LibraryPanel { app: root }
    }

    Menu {
        id: queueMenu
        objectName: "queueEditMenu"
        property int rowIndex: -1
        property int revision: -1
        popupType: Popup.Item
        width: 272; padding: 8; spacing: 2; margins: 12
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit } }


        SettingsAction { objectName: "queueMoveUp"; text: "Move up"; glyphName: "up"; enabled: root.queueReorderAllowed && queueMenu.rowIndex > 0; onTriggered: root.moveQueueRow(queueMenu.rowIndex,queueMenu.rowIndex - 1,queueMenu.revision) }
        SettingsAction { objectName: "queueMoveDown"; text: "Move down"; glyphName: "down"; enabled: root.queueReorderAllowed && queueMenu.rowIndex < root.queueRows.length - 1; onTriggered: root.moveQueueRow(queueMenu.rowIndex,queueMenu.rowIndex + 1,queueMenu.revision) }
        SettingsAction { objectName: "queueRemove"; text: "Remove from queue"; glyphName: "close"; enabled: root.queueControlsReady; onTriggered: root.useCider ? cider.removeQueue(queueMenu.rowIndex,queueMenu.revision) : player.remove(queueMenu.rowIndex) }
    }
    TrackMenu {
        id: songMenu
        objectName: "currentSongMenu"
        app: root; currentSong: true
        onAboutToShow: root.actionService.observing = true
        onClosed: root.actionService.observing = false
    }
    function notifyAction(message, error) { actionNotice.text = message; actionNotice.failed = error; noticeTimer.interval = error ? 8000 : 2200; noticeTimer.restart() }
    Connections {
        target: cider
        function onApiFeedback(message, error) { root.notifyAction(message,error) }
        function onLaunchChanged() { if (cider.launching) root.notifyAction("Starting Cider…",false) }
    }
    Connections {
        target: root.actionService
        function onCurrentChanged() { songMenu.close() }
        function onFeedback(message, error) { root.notifyAction(message,error) }
    }
    Timer { id: noticeTimer }
    Rectangle {
        id: actionNotice
        objectName: "actionNotice"
        property string text: ""
        property bool failed: false
        visible: noticeTimer.running && !root.miniMode
        onVisibleChanged: Qt.callLater(root.updateMask)
        z: 30; x: 65; y: 688; width: 400; height: 36; radius: 14 * theme.radius
        color: root.surface
        SpunText { x: 12; y: 2; width: 342; height: 32; text: actionNotice.text; color: actionNotice.failed ? theme.colors.error : root.ink; font.pixelSize: SpunStyle.caption; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter }
        IconButton { x: 364; y: 0; width: 36; height: 36; glyphName: "close"; tip: "Dismiss"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: noticeTimer.stop() }
    }
    component SettingsAction: MenuEntry { app: root }
    component SettingsGap: MenuSeparator {
        padding: 0
        implicitHeight: 10
        contentItem: Item {}
        background: null
    }
    Menu {
        id: menu
        objectName: "settingsMenu"
        popupType: Popup.Item
        x: deck.x + deck.width - width; y: Math.max(12,deck.y - height - 10)
        width: 320; padding: 8; spacing: 2
        font.family: SpunStyle.family
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit } }
        onOpened: {  if (root.useCider) cider.refreshModes() }

        SettingsAction { text: "Add tracks"; glyphName: "plus"; hint: "Ctrl+O"; onTriggered: files.open() }
        SettingsAction { text: "Add album folder"; glyphName: "folder"; hint: "Ctrl+Shift+O"; onTriggered: folder.open() }
        SettingsAction { text: "Change artwork"; glyphName: "artwork"; enabled: !root.useCider && player.count > 0; onTriggered: cover.open() }
        SettingsGap {}
        SettingsAction { objectName: "flipDiscAction"; text: root.discFlipped ? "Show artwork" : "Flip disc"; glyphName: "flip"; hint: "F"; enabled: root.deckPlayer.count > 0; onTriggered: root.flipDisc() }
        SettingsAction { objectName: "miniToggle"; text: "Mini mode"; glyphName: "mini"; checkable: true; checked: player.miniMode; onTriggered: player.miniMode = !player.miniMode }
        SettingsAction { objectName: "preferencesAction"; text: "Preferences"; glyphName: "settings"; onTriggered: Qt.callLater(function() { preferences.open() }) }
        SettingsAction { text: "Play demo"; glyphName: "play"; onTriggered: { root.useLocal(); player.demo() } }
        SettingsGap {}
        SettingsAction { text: "Keyboard shortcuts"; glyphName: "keyboard"; hint: "F1"; onTriggered: root.helpOpen = true }
        SettingsAction { text: "Quit Spun"; glyphName: "power"; hint: "Ctrl+Q"; onTriggered: Qt.quit() }
    }

    Popup {
        id: preferences
        objectName: "preferencesPopup"
        popupType: Popup.Item; focus: true
        x: deck.x + deck.width - width
        y: Math.max(12, deck.y - height - 12)
        width: 352; height: Math.min(496, preferenceItems.implicitHeight + 72)
        padding: 12
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit } }
        onAboutToShow: fontPicker.prepare()
        onAboutToHide: fontPicker.close()
        onOpened: {  if (root.useCider) cider.refreshModes(); closePreferences.forceActiveFocus() }
        onClosed: { fontPicker.close(); menuButton.forceActiveFocus() }
        contentItem: Item {
            SpunText { x: 12; y: 10; text: "Preferences"; color: root.ink; font.pixelSize: SpunStyle.heading }
            IconButton { id: closePreferences; objectName: "closePreferences"; anchors.right: parent.right; glyphName: "close"; tip: "Close preferences"; ink: root.ink; onClicked: preferences.close() }
            Flickable {
                id: preferenceScroll
                x: 0; y: 48; width: parent.width; height: parent.height - y
                contentHeight: preferenceItems.implicitHeight; clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }
                function reveal(item) { const p = item.mapToItem(preferenceItems, 0, 0); if (p.y < contentY) contentY = p.y; else if (p.y + item.height > contentY + height) contentY = p.y + item.height - height }
                Column {
                    id: preferenceItems; width: parent.width; spacing: 4
                    AbstractButton {
                        id: fontChoice
                        objectName: "fontChoice"
                        width: parent.width; implicitHeight: 60
                        hoverEnabled: true; focusPolicy: Qt.StrongFocus
                        Accessible.name: "Font: " + typography.family
                        onClicked: fontPicker.open()
                        onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this)
                        background: Rectangle {
                            radius: SpunStyle.rowRadius; color: "transparent"
                            SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; hovered: fontChoice.hovered; pressed: fontChoice.down; focused: fontChoice.visualFocus }
                        }
                        contentItem: Item {
                            SpunText { x: 12; y: 7; text: "Font"; color: root.mutedInk; font.pixelSize: SpunStyle.caption }
                            SpunText { x: 12; y: 27; width: parent.width - 52; text: typography.selectedFamily.length && !typography.missing ? typography.family : "System default"; color: root.ink; font.pixelSize: SpunStyle.body; elide: Text.ElideRight }
                            Glyph { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; name: "down"; ink: root.mutedInk }
                        }
                    }
                    PreferenceSwitch { objectName: "miniPinToggle"; app: root; width: parent.width; text: "Keep Mini on top"; glyphName: "pin"; checked: player.miniOnTop; onToggled: player.miniOnTop = checked; onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                    PreferenceSwitch { objectName: "blurToggle"; app: root; width: parent.width; visible: native.supportsBlur; height: visible ? implicitHeight : 0; text: "Blur background"; glyphName: "blur"; checked: player.backgroundBlur; onToggled: player.backgroundBlur = checked; onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                    PreferenceSwitch { objectName: "motionToggle"; app: root; width: parent.width; text: "Animations"; glyphName: "motion"; checked: player.motion; onToggled: player.motion = checked; onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                    Rectangle { x: 12; width: parent.width - 24; height: 1; color: root.hairline }
                    PreferenceSwitch { objectName: "ciderAutoStartToggle"; app: root; width: parent.width; text: "Start Cider with Spun"; glyphName: "power"; checked: player.ciderAutoStart; onToggled: { player.ciderAutoStart = checked; if (checked && root.useCider) cider.ensureRunning() } onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                    PreferenceSwitch { objectName: "autoplayToggle"; app: root; width: parent.width; visible: root.useCider; height: visible ? implicitHeight : 0; text: "Autoplay"; glyphName: "autoplay"; checked: cider.autoplay; enabled: cider.modesReady && !cider.controlBusy; onToggled: cider.setAutoplay(checked); onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                    MenuEntry { objectName: "crossfadeAction"; app: root; width: parent.width; visible: root.useCider; text: "Crossfade"; glyphName: "crossfade"; onTriggered: { preferences.close(); Qt.callLater(function() { crossfadeMenu.open() }) } onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                }
            }
        }
    }

    Loader {
        id: fontPicker
        objectName: "fontPickerLoader"
        asynchronous: true
        readonly property bool shown: item ? item.visible : false
        property bool opening: false
        function prepare() {
            if (source.toString().length === 0)
                setSource("FontPicker.qml", { app: root, preferences: preferences })
        }
        function open() { opening = true; prepare(); if (item) { opening = false; item.open() } }
        function close() { opening = false; if (item) item.close() }
        onLoaded: if (opening && preferences.visible) { opening = false; item.open() }
        Connections {
            target: fontPicker.item
            function onClosed() { if (preferences.visible) fontChoice.forceActiveFocus() }
        }
    }

    Popup {
        id: crossfadeMenu
        objectName: "crossfadeMenu"
        focus: true
        property var service: cider
        popupType: Popup.Item
        x: deck.x + deck.width - width; y: Math.max(12, deck.y - height - 10)
        width: 320; height: crossfadeServiceError.visible ? 246 : 170; padding: 12
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit } }
        onOpened: {  service.refreshCrossfade() }

        contentItem: Item {
            PreferenceSwitch {
                app: root
                objectName: "crossfadeToggle"
                width: parent.width; text: "Crossfade"; glyphName: "crossfade"
                checked: crossfadeMenu.service.crossfade
                enabled: crossfadeMenu.service.crossfadeReady && !crossfadeMenu.service.crossfadeBusy
                onToggled: crossfadeMenu.service.setCrossfade(checked)
            }
            SpunText {
                x: 10; y: 54; text: "Duration"; font.pixelSize: 12; color: root.mutedInk
            }
            SpunText {
                anchors.right: parent.right; anchors.rightMargin: 10; y: 54
                text: crossfadeMenu.service.crossfadeBusy ? "…" : crossfadeMenu.service.crossfadeReady ? (crossfadeDuration.pressed ? crossfadeDuration.value : crossfadeMenu.service.crossfadeSeconds) + " s" : "—"
                font.pixelSize: 12; color: root.ink
            }
            Slider {
                id: crossfadeDuration; objectName: "crossfadeDuration"
                x: 10; y: 78; width: parent.width - 20; height: 36
                from: 1; to: 12; stepSize: 1; snapMode: Slider.SnapAlways
                value: crossfadeMenu.service.crossfadeSeconds
                enabled: crossfadeMenu.service.crossfadeReady && crossfadeMenu.service.crossfade && !crossfadeMenu.service.crossfadeBusy
                opacity: enabled ? 1 : .4
                Accessible.name: "Crossfade duration in seconds"
                onMoved: if (!pressed) crossfadeMenu.service.setCrossfadeSeconds(value)
                onPressedChanged: if (!pressed && enabled) crossfadeMenu.service.setCrossfadeSeconds(value)
                Connections { target: crossfadeMenu.service; function onCrossfadeChanged() { if (!crossfadeDuration.pressed) crossfadeDuration.value = crossfadeMenu.service.crossfadeSeconds } }
                background: Rectangle {
                    x: crossfadeDuration.leftPadding; y: crossfadeDuration.topPadding + crossfadeDuration.availableHeight / 2 - 2
                    width: crossfadeDuration.availableWidth; height: 3; radius: 2; color: root.hairline
                    Rectangle { width: parent.width * crossfadeDuration.visualPosition; height: 3; radius: 2; color: root.accent }
                }
                handle: Rectangle {
                    x: crossfadeDuration.leftPadding + crossfadeDuration.visualPosition * (crossfadeDuration.availableWidth - width)
                    y: crossfadeDuration.topPadding + crossfadeDuration.availableHeight / 2 - height / 2
                    width: 12; height: 12; radius: 6; color: root.accent
                    scale: crossfadeDuration.pressed || crossfadeDuration.visualFocus ? 1.2 : 1
                    Behavior on scale { NumberAnimation { duration: root.feedbackTime } }
                }
            }
            SpunText {
                id: crossfadeServiceError; x: 10; y: 116; width: parent.width - 20; height: 42
                visible: crossfadeMenu.service.crossfadeError.length > 0
                text: crossfadeMenu.service.crossfadeError; font.pixelSize: SpunStyle.caption; color: theme.colors.error; wrapMode: Text.WordWrap
            }
            SettingsAction {
                y: 165; width: parent.width; visible: crossfadeServiceError.visible
                text: "Try again"; glyphName: "refresh"; enabled: !crossfadeMenu.service.crossfadeBusy
                onTriggered: crossfadeMenu.service.refreshCrossfade()
            }
        }
    }

    Rectangle {
        visible: root.deckPlayer.error.length > 0
        x: 73; y: 419; width: 384; height: Math.max(78, errorText.implicitHeight+28); radius: 14
        color: root.surface; border.color: theme.colors.error
        SpunText { id: errorText; x: 14; y: 14; width: 310; text: root.deckPlayer.error; color: root.ink; font.pixelSize: SpunStyle.caption; wrapMode: Text.WordWrap }
        IconButton { x: 342; y: 7; glyphName: "close"; ink: root.mutedInk; tip: "Dismiss"; onClicked: root.deckPlayer.dismissError() }
    }
    Rectangle {
        x: 94; y: 146; width: 342; height: 450; radius: 22
        visible: root.helpOpen; color: root.surface; border.width: 0
        SpunText { x: 24; y: 23; text: "Shortcuts"; font.family: SpunStyle.family; font.pixelSize: 22; color: root.ink }
        SpunText {
            x: 24; y: 70; width: 294; color: root.mutedInk; font.pixelSize: 12; lineHeight: 1.55
            text: "Space                  Play / pause\n← / →                 Seek 5 seconds\nCtrl + ← / →       Previous / next\n↑ / ↓                    Volume\nM                         Mute\nCtrl + O               Add music\nCtrl + L                Show queue\nCtrl + F                Search queue\nCtrl + M              Mini / full player\nCtrl + B              Browse Cider music\nCtrl + V              Open music link\nF                          Flip disc\nY                          Lyrics / album tracks\n\nDouble-click to flip. Drag to move.\nScrub the outer rim to seek."
        }
        IconButton { x: 299; y: 8; glyphName: "close"; tip: "Close shortcuts"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: root.helpOpen=false }
    }
}
