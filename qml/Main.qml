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
    readonly property bool vinyl: player.vinyl
    onVinylChanged: Qt.callLater(updateMask)
    property bool useCider: !testMode && root.ciderService.available
    property var ciderService: cider
    property var listeningService: listening
    property var actionService: musicActions
    property var savedService: library
    Binding { target: root.ciderService; property: "liveVisible"; value: root.useCider && root.visible && root.visibility !== Window.Minimized }
    function refreshOpenCiderDetails() {
        if (songMenu.visible) root.actionService.refresh()
        if (crossfadeMenu.visible) { crossfadeMenu.service.refreshCrossfade(); crossfadeMenu.service.refreshAudioOptions() }
        if (qualityPopup.visible) root.ciderService.refreshAudioQuality()
    }
    Connections { target: root.listeningService; function onFeedback(message, error) { root.notifyAction(message, error) } }
    Connections { target: root.ciderService; function onRemoteSettingsChanged() { root.refreshOpenCiderDetails() } }
    Timer {
        interval: 30000; repeat: true
        running: root.useCider && root.visible && root.visibility !== Window.Minimized && (songMenu.visible || crossfadeMenu.visible || qualityPopup.visible)
        onTriggered: root.refreshOpenCiderDetails()
    }
    property var deckPlayer: useCider ? root.ciderService : player
    onUseCiderChanged: { recoveryPopup.close(); quickJump.close(); savedQueuePicker.close(); clearQueueSelection(); cleanupPopup.close(); qualityPopup.close(); if(libraryDragging)endLibraryDrag(false); preferences.close(); crossfadeMenu.close(); queueMenu.close(); cancelQueueDrag(); songMenu.close(); musicBrowser.closeActions(); if (!useCider) libraryOpen = false; if (useCider) player.pause(); root.ciderService.queueVisible = queueOpen && useCider; discFlipped = false; closeQueueSearch(); Qt.callLater(presentDisc); syncLyrics() }
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
        root.ciderService.discVisible = discFlipped && useCider
        syncLyrics()
    }
    function flipDisc() { if (deckPlayer.count > 0) discFlipped = !discFlipped }
    readonly property bool miniPinned: miniMode && player.miniOnTop
    onMiniPinnedChanged: native.effects(root, backgroundBlur)
    property bool miniMode: player.miniMode
    onMiniModeChanged: Qt.callLater(function() {
        scrubber.cancelScrub(); stopSwap()
        if (miniMode) { savedQueuePicker.close(); qualityPopup.close(); if(libraryDragging)endLibraryDrag(false); preferences.close(); crossfadeMenu.close(); songMenu.close(); musicBrowser.closeActions(); libraryOpen = false; queueOpen = false; helpOpen = false; menu.close(); miniReveal.restart() }
        updateMask(); native.effects(root, backgroundBlur)
    })
    property bool editingText: activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit
    property bool queueSearchOpen: false
    property string queueQuery: ""
    property bool queuePrepared: false
    readonly property var queueRows: queuePrepared ? buildQueueRows(deckPlayer.queue) : []
    readonly property var filteredQueue: filterQueue(queueRows, queueQuery)
    // Recompute the queue sum only when tracks or the current index change.
    // Playback ticks update a single minute bucket, without rescanning the list.
    function queueTime(rows, current) {
        const start = current >= 0 && current < rows.length ? current : 0
        let total = 0, unknown = 0, currentDuration = 0
        for (let i = start; i < rows.length; ++i) {
            const duration = Number(rows[i].track.duration)
            if (Number.isFinite(duration) && duration > 0) {
                total += duration
                if (i === current) currentDuration = duration
            } else ++unknown
        }
        return { milliseconds: total, unknown: unknown, currentDuration: currentDuration, count: rows.length - start }
    }
    readonly property var queueTiming: queueTime(queueRows, deckPlayer.currentIndex)
    readonly property bool timedCurrent: deckPlayer.currentIndex >= 0 && deckPlayer.currentIndex < queueRows.length
    readonly property real queueCurrentDuration: timedCurrent ? Math.max(0, Number(deckPlayer.duration) || queueTiming.currentDuration) : 0
    readonly property int queueUnknownDurations: queueTiming.unknown - (timedCurrent && queueTiming.currentDuration === 0 && queueCurrentDuration > 0 ? 1 : 0)
    readonly property real queueRemainingMs: !queueOpen ? 0 : Math.max(0, queueTiming.milliseconds - queueTiming.currentDuration + queueCurrentDuration - (timedCurrent ? Math.min(queueCurrentDuration, Math.max(0, deckPlayer.position)) : 0))
    readonly property int queueRemainingMinutes: queueUnknownDurations ? Math.floor(queueRemainingMs / 60000) : Math.ceil(queueRemainingMs / 60000)
    readonly property string queueTimeSummary: {
        const count = queueTiming.count
        if (useCider && !root.ciderService.queueReady) return ""
        const songs = count + (count === 1 ? " song" : " songs")
        if (!count) return songs
        const minutes = queueRemainingMinutes
        if (queueUnknownDurations && minutes === 0) return songs + " · Duration incomplete"
        const time = minutes >= 60 ? Math.floor(minutes / 60) + " h" + (minutes % 60 ? " " + minutes % 60 + " min" : "") : minutes + " min"
        return songs + " · " + (queueUnknownDurations ? "≥ " : "") + time + " left"
    }
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
    function openArtistName(name) { useCider = true; libraryOpen = true; library.showArtist(name) }
    function openLibrary() { libraryOpen = !libraryOpen }
    readonly property bool queueControlsReady: !useCider || (!root.actionService.busy && root.ciderService.queueReady && !root.ciderService.queueBusy && !root.ciderService.controlBusy && !root.ciderService.queueError.length)
    readonly property bool queueReorderAllowed: queueControlsReady && !queueQuery.trim().length
    property bool libraryDragging: false
    property var libraryDragTracks: []
    property int libraryDropIndex: -1
    property int libraryDropRevision: -1
    property real libraryDragY: 0
    property real libraryDragX: 0
    function beginLibraryDrag(tracks) {
        noticeTimer.stop(); libraryDragTracks = tracks; libraryDragging = true; queueOpen = true
        contentItem.forceActiveFocus()
    }
    function updateLibraryDrag(x, y) {
        const point = trackList.mapFromItem(root.contentItem, x, y)
        libraryDragY = point.y;libraryDragX = point.x
        if (!root.queueControlsReady || point.x < 0 || point.x > trackList.width || point.y < 0 || point.y > trackList.height) { libraryDropIndex = -1;return }
        libraryDropIndex = Math.max(root.ciderService.currentIndex + 1, Math.min(root.ciderService.queue.length, Math.floor((point.y + trackList.contentY + SpunStyle.trackHeight / 2) / (SpunStyle.trackHeight + SpunStyle.smallGap))))
        libraryDropRevision = root.ciderService.queueRevision
    }
    function endLibraryDrag(drop) {
        const index = libraryDropIndex, revision = libraryDropRevision, tracks = libraryDragTracks
        libraryDragging = false;libraryDropIndex = -1;libraryDragTracks = []
        if (drop && index >= 0 && root.queueControlsReady) root.ciderService.insertQueue(tracks,index,revision)
        else libraryOpen = true
    }
    Timer {
        interval: 40; repeat: true
        running: root.libraryDragging && root.libraryDropIndex >= 0 && (root.libraryDragY < 36 || root.libraryDragY > trackList.height - 36)
        onTriggered: {
            trackList.contentY = Math.max(0, Math.min(Math.max(0,trackList.contentHeight - trackList.height),trackList.contentY + (root.libraryDragY < 36 ? -10 : 10)))
            const point = trackList.mapToItem(root.contentItem,root.libraryDragX,root.libraryDragY)
            root.updateLibraryDrag(point.x,point.y)
        }
    }
    property var queueSelection: []
    property int queueSelectionAnchor: -1
    property int queueSelectionRevision: -1
    readonly property int queueSelectionCount: queueSelection.length
    function clearQueueSelection() { queueSelection = []; queueSelectionAnchor = -1; queueSelectionRevision = -1 }
    function chooseQueueRow(index, modifiers) {
        if (!queueControlsReady) return
        const selecting = useCider && ((modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) || queueSelectionCount > 0)
        if (!selecting) { deckPlayer.select(index); return }
        if (index <= ciderService.currentIndex || index < 0) return
        let next = queueSelection.slice()
        if ((modifiers & Qt.ShiftModifier) && queueSelectionAnchor >= 0) {
            if (!(modifiers & Qt.ControlModifier)) next = []
            const first = Math.min(index, queueSelectionAnchor), last = Math.max(index, queueSelectionAnchor)
            for (const row of filteredQueue) if (row.sourceIndex >= first && row.sourceIndex <= last && row.sourceIndex > ciderService.currentIndex && next.indexOf(row.sourceIndex) < 0) next.push(row.sourceIndex)
        } else {
            const at = next.indexOf(index); if (at >= 0) next.splice(at, 1); else next.push(index)
            queueSelectionAnchor = index
        }
        queueSelection = next.sort((a,b) => a-b); queueSelectionRevision = ciderService.queueRevision
    }
    function selectUpcomingQueue() {
        if (!useCider || !queueControlsReady) return
        queueSelection = filteredQueue.filter(row => row.sourceIndex > ciderService.currentIndex).map(row => row.sourceIndex)
        queueSelectionAnchor = queueSelection.length ? queueSelection[0] : -1; queueSelectionRevision = ciderService.queueRevision
    }
    function editQueueSelection(operation) {
        if (!queueSelectionCount || !queueControlsReady) return
        if (operation === "remove") {
            cleanupPopup.preview = {mode: "selection", indices: queueSelection.slice(), revision: queueSelectionRevision, rows: queueSelection.map(i => ciderService.queue[i])}
            cleanupPopup.open()
        } else ciderService.editQueueSelection(queueSelection.slice(), operation, queueSelectionRevision)
    }
    function openQueueSelectionActions(anchor) { showQueueActions(queueSelection[0], anchor) }
    QuickJump { id: quickJump; app: root }
    function openQuickJump() { menu.close(); musicBrowser.closeActions(); queueMenu.close(); quickJump.show() }
    function revealQueueTrack(index) {
        player.miniMode = false; libraryOpen = false; clearQueueSelection(); queueOpen = true; queueQuery = ""
        Qt.callLater(function() { trackList.keyboardIndex = index; trackList.positionViewAtIndex(index, ListView.Contain); trackList.forceActiveFocus() })
    }
    SavedQueuePicker { id: savedQueuePicker; app: root }
    function openSavedQueuePicker(tracks, service) { savedQueuePicker.show(tracks, service) }
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
        if (useCider) root.ciderService.moveQueue(from,to,revision)
        else player.move(from,to)
    }
    function showQueueActions(index, anchor) {
        if (queueSelectionCount && queueSelection.indexOf(index) < 0) clearQueueSelection()
        queueMenu.rowIndex = index; queueMenu.revision = root.ciderService.queueRevision
        const point = anchor.mapToItem(root.contentItem,0,anchor.height)
        queueMenu.x = jewelCase.x + jewelCase.width - queueMenu.width - 16
        queueMenu.y = Math.max(jewelCase.y + 12,Math.min(root.height - queueMenu.height - 16,point.y))
        queueMenu.open()
    }
    Connections {
        target: root.deckPlayer
        function onQueueChanged() { queueMenu.close(); root.cancelQueueDrag(); root.clearQueueSelection() }
    }
    Connections {
        target: root.ciderService
        function onCurrentIndexChanged() { root.clearQueueSelection(); queueMenu.close() }
    }
    onQueueQueryChanged: clearQueueSelection()
    Timer {
        interval: 40; repeat: true; running: root.queueDragSource >= 0 && (root.queueDragY < 36 || root.queueDragY > trackList.height - 36)
        onTriggered: {
            trackList.contentY = Math.max(0,Math.min(Math.max(0,trackList.contentHeight - trackList.height),trackList.contentY + (root.queueDragY < 36 ? -10 : 10)))
            root.updateQueueDrop()
        }
    }
    property bool queueOpen: false
    property bool helpOpen: false
    readonly property bool menuOpen: recoveryPopup.visible || quickJump.visible || savedQueuePicker.visible || cleanupPopup.visible || qualityPopup.visible || savedQueueMenu.visible || saveQueuePopup.visible || deleteQueuePopup.visible || fontPicker.shown || fontPicker.opening || menu.visible || preferences.visible || crossfadeMenu.visible || queueMenu.visible || songMenu.visible || musicBrowser.actionsOpen
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
        if (useCider && root.ciderService.playing) root.ciderService.pause()
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
    onQueueOpenChanged: { clearQueueSelection(); if (queueOpen) queuePrepared = true; queueMenu.close(); cancelQueueDrag(); if (queueOpen) libraryOpen = false; if (!queueOpen) closeQueueSearch(); if (queueOpen && miniMode) player.miniMode = false; root.ciderService.queueVisible = queueOpen && useCider; Qt.callLater(updateMask) }
    onHelpOpenChanged: { if (helpOpen && miniMode) player.miniMode = false; Qt.callLater(updateMask) }
    onMenuOpenChanged: Qt.callLater(updateMask)
    onWidthChanged: Qt.callLater(updateMask)
    Component.onCompleted: { if (!testMode && player.ciderAutoStart) { useCider = true; root.ciderService.ensureRunning() }; updateMask(); native.place(root); Qt.callLater(presentDisc); syncLyrics() }
    onClosing: player.save()

    Rectangle {
        objectName: "blurBackdrop"
        width: root.width; height: root.height
        visible: root.backgroundBlur
        color: Qt.alpha(root.inset, .18)
        radius: 28 * theme.radius
        MouseArea { anchors.fill: parent; onPressed: root.startSystemMove() }
    }

    Shortcut { sequence: "Ctrl+K"; enabled: !root.menuOpen || quickJump.visible; onActivated: quickJump.visible ? quickJump.close() : root.openQuickJump() }
    Shortcut { sequence: "Space"; enabled: !(root.activeFocusItem instanceof AbstractButton) && !(root.useCider && root.queueOpen && trackList.activeFocus) && !(root.libraryOpen && musicBrowser.item && musicBrowser.item.trackListFocused) && !root.editingText && !root.menuOpen; onActivated: root.useCider ? root.ciderService.toggle() : player.count ? player.toggle() : files.open() }
    Shortcut { sequence: "Ctrl+V"; enabled: !root.editingText && !root.menuOpen; onActivated: root.openMusicLink("", true) }
    Shortcut { sequence: "Ctrl+O"; enabled: !root.menuOpen; onActivated: files.open() }
    Shortcut { sequence: "Ctrl+Shift+O"; enabled: !root.menuOpen; onActivated: folder.open() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
    Shortcut { sequence: "Ctrl+F"; enabled: !root.menuOpen; onActivated: root.libraryOpen ? musicBrowser.focusSearch() : root.openQueueSearch() }
    Shortcut { sequence: "Ctrl+B"; enabled: root.useCider && !root.menuOpen; onActivated: root.openLibrary() }
    Shortcut { sequence: "Ctrl+L"; enabled: !root.menuOpen; onActivated: root.toggleQueue() }
    Shortcut { sequence: "Ctrl+M"; enabled: !root.menuOpen; onActivated: player.miniMode = !player.miniMode }
    Shortcut { sequence: "Right"; enabled: !(root.activeFocusItem instanceof Slider) && !root.editingText && !root.menuOpen; onActivated: root.deckPlayer.seek(root.deckPlayer.position + 5000) }
    Shortcut { sequence: "Left"; enabled: !(root.activeFocusItem instanceof Slider) && !root.editingText && !root.menuOpen; onActivated: root.deckPlayer.seek(root.deckPlayer.position - 5000) }
    Shortcut { sequence: "Ctrl+Right"; enabled: !root.editingText && !root.menuOpen; onActivated: root.deckPlayer.next() }
    Shortcut { sequence: "Ctrl+Left"; enabled: !root.editingText && !root.menuOpen; onActivated: root.deckPlayer.previous() }
    Shortcut { sequence: "Up"; enabled: !root.libraryOpen && !trackList.activeFocus && !root.menuOpen && !root.editingText && !(root.discFlipped && (albumList.activeFocus || lyricList.activeFocus)); onActivated: root.deckPlayer.volume = Math.min(1, root.deckPlayer.volume + .05) }
    Shortcut { sequence: "Down"; enabled: !root.libraryOpen && !trackList.activeFocus && !root.menuOpen && !root.editingText && !(root.discFlipped && (albumList.activeFocus || lyricList.activeFocus)); onActivated: root.deckPlayer.volume = Math.max(0, root.deckPlayer.volume - .05) }
    Shortcut { sequence: "M"; enabled: !root.editingText && !root.menuOpen; onActivated: root.toggleMute() }
    Shortcut { sequence: "Escape"; onActivated: { if(root.libraryDragging) { root.endLibraryDrag(false);return }; if(quickJump.visible) { quickJump.close();return }; if(savedQueuePicker.visible) { savedQueuePicker.close();return }; if(cleanupPopup.visible) { cleanupPopup.close();return }; if(qualityPopup.visible) { qualityPopup.close();return }; if (saveQueuePopup.visible) { saveQueuePopup.close(); return }; if (deleteQueuePopup.visible) { deleteQueuePopup.close(); return }; if (savedQueueMenu.visible) { savedQueueMenu.close(); return }; if (fontPicker.shown || fontPicker.opening) { fontPicker.close(); return }; if (preferences.visible) { preferences.close(); return }; if (crossfadeMenu.visible) { crossfadeMenu.close(); return }; if (queueMenu.visible) { queueMenu.close(); return }; if (songMenu.visible) { songMenu.close(); return }; if (musicBrowser.actionsOpen) { musicBrowser.closeActions(); return }; if (menu.visible) { menu.close(); return }; if (root.queueSelectionCount > 0) { root.clearQueueSelection(); return }; if (root.queueSearchOpen) { root.closeQueueSearch(); return }; if (root.libraryOpen) { if (musicBrowser.item && musicBrowser.item.selectionCount > 0) musicBrowser.item.clearSelection(); else if (musicBrowser.detail && musicBrowser.browser.collectionQuery.length) musicBrowser.browser.collectionQuery = ""; else if (musicBrowser.detail) musicBrowser.browser.back(); else root.libraryOpen = false; return }; if (root.discFlipped) { root.discFlipped = false; return }; root.queueOpen = false; root.helpOpen = false; menu.close(); if (root.miniMode) player.miniMode = false } }
    Shortcut { sequence: "Y"; enabled: !root.editingText && !root.menuOpen; onActivated: { if (root.deckPlayer.count) { root.discFlipped = true; root.lyricsView = !root.lyricsView } } }
    Shortcut { sequence: "F"; enabled: !root.editingText && !root.menuOpen; onActivated: root.flipDisc() }
    Shortcut { sequence: "F1"; enabled: !root.menuOpen; onActivated: root.helpOpen = !root.helpOpen }

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
            id: sourceTabs
            function focusTab(index) {
                const button = sourceTabItems.itemAt(Math.max(0, Math.min(1, index)))
                if (button) button.forceActiveFocus(Qt.TabFocusReason)
            }
            x: 56; y: 6; spacing: 0
            Repeater {
                id: sourceTabItems
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
                    Accessible.role: Accessible.PageTab
                    Accessible.selectable: true; Accessible.selected: root.useCider === (index === 1)
                    Keys.onShortcutOverride: event => {
                        if (event.modifiers === Qt.NoModifier && [Qt.Key_Left, Qt.Key_Right, Qt.Key_Home, Qt.Key_End].includes(event.key)) event.accepted = true
                    }
                    Keys.onLeftPressed: sourceTabs.focusTab(index - 1)
                    Keys.onRightPressed: sourceTabs.focusTab(index + 1)
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Home) { sourceTabs.focusTab(0); event.accepted = true }
                        else if (event.key === Qt.Key_End) { sourceTabs.focusTab(1); event.accepted = true }
                        else event.accepted = false
                    }

                    Keys.onReturnPressed: clicked()
                    Keys.onEnterPressed: clicked()
                    background: Rectangle {
                        radius: 18
                        color: "transparent"
                        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; pressed: sourceTab.down; focused: sourceTab.visualFocus; hovered: sourceTab.hovered }
                        border.width: sourceTab.visualFocus ? 2 : 0; border.color: root.accent
                    }
                    contentItem: SpunText { text: parent.modelData; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: (root.useCider === (parent.index === 1)) ? root.accent : root.mutedInk; font.pixelSize: SpunStyle.body; font.weight: Font.Medium }
                    onClicked: { if (index === 0 && root.useCider && root.ciderService.playing) root.ciderService.pause(); root.useCider = index === 1; if (index === 1 && player.ciderAutoStart) root.ciderService.ensureRunning() }
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
                Disc { vinyl: root.vinyl; anchors.fill: parent; artwork: presentation.outgoing; rotation: root.outgoingAngle }
                Disc { vinyl: root.vinyl; anchors.fill: parent; overlay: true }
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
                    Disc { vinyl: root.vinyl;
                        id: face
                        objectName: "discFace"
                        anchors.fill: parent
                        artwork: presentation.artwork
                        rotation: root.spinAngle
                    }
                    Disc { vinyl: root.vinyl; anchors.fill: parent; overlay: true }
                }
                back: Item {
                    objectName: "discBack"
                    anchors.fill: parent
                    Disc { vinyl: root.vinyl; anchors.fill: parent; labelColor: root.surface }
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
                            readonly property bool seekable: lyrics.timed && !lyrics.loading && modelData.start >= 0 && modelData.start < root.deckPlayer.duration && (!root.useCider || root.ciderService.canSeek)
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
                            Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                            Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
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
                            text: root.ciderService.discLoading ? "Loading album…" : root.ciderService.discError
                            font.pixelSize: root.miniMode ? 18 : SpunStyle.body; color: root.mutedInk
                        }
                        IconButton {
                            objectName: "retryDiscButton"
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: !root.ciderService.discLoading && !!root.ciderService.discError
                            glyphName: "repeat"; tip: "Retry album details"; ink: root.accent; hoverFill: root.hoverFill
                            onClicked: root.ciderService.refreshDisc()
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
        Loader {
            objectName: "tonearmLoader"
            anchors.fill: parent
            active: root.vinyl
            visible: active && !root.discFlipped && !root.swapRunning
            sourceComponent: Tonearm { app: root }
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
            readonly property bool canSeek: !root.swapRunning && root.deckPlayer.duration > 0 && (!root.useCider || root.ciderService.canSeek)
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
        Behavior on opacity { NumberAnimation { duration: root.transitionTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        HoverHandler { id: miniControlsHover }
        Row {
            x: 8; y: 4; spacing: 0
            IconButton { id: miniPrevious; objectName: "miniPrevious"; glyphName: "previous"; tip: "Previous track"; ink: root.ink; hoverFill: root.hoverFill; enabled: root.useCider ? root.ciderService.canPrevious : player.count > 0; onClicked: root.deckPlayer.previous() }
            IconButton { id: miniPlay; objectName: "miniPlay"; glyphSize: 26; width: 48; glyphName: root.deckPlayer.playing ? "pause" : "play"; tip: root.deckPlayer.playing ? "Pause" : "Play"; fill: root.accent; ink: theme.colors.onAccent; hoverFill: Qt.lighter(root.accent,1.08); onClicked: root.useCider ? root.ciderService.toggle() : player.count ? player.toggle() : files.open() }
            IconButton { id: miniNext; objectName: "miniNext"; Accessible.description: miniPeek.visible ? miniPeek.summary : ""; showTip: false; glyphName: "next"; tip: "Next track"; ink: root.ink; hoverFill: root.hoverFill; enabled: root.useCider ? root.ciderService.canNext : player.count > 0; onClicked: root.deckPlayer.next()
                NextTrackTip { id: miniPeek; app: root; visible: root.miniMode && miniNext.enabled && (miniNext.hovered || miniNext.visualFocus) && !miniNext.down && !quickJump.visible }
            }
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
            visible: root.useCider; enabled: root.ciderService.count > 0
            x: 264; y: 12; width: 40; height: 40
            glyphName: "more"; tip: "Song actions"; ink: root.mutedInk; hoverFill: root.hoverFill
            onClicked: { menu.close(); songMenu.x = deck.x + deck.width - songMenu.width; songMenu.y = Qt.binding(function() { return deck.y - songMenu.height - 8 }); songMenu.open() }
        }
        AbstractButton {
            id: playerArtist
            objectName: "playerArtistLink"
            x: 24; y: 44; width: 284; height: 28
            enabled: root.useCider && root.deckPlayer.artist.length > 0; hoverEnabled: true
            Accessible.name: "View artist " + root.deckPlayer.artist
            background: null
            onClicked: root.openArtistName(root.deckPlayer.artist)
            contentItem: SpunText { text: root.deckPlayer.artist; color: playerArtist.hovered || playerArtist.visualFocus ? root.accent : root.mutedInk; elide: Text.ElideRight; font.pixelSize: SpunStyle.body; verticalAlignment: Text.AlignVCenter; font.underline: playerArtist.hovered || playerArtist.visualFocus }
        }
        SpunText {
            x: 307; y: 18; width: 77; horizontalAlignment: Text.AlignRight
            text: root.time(root.deckPlayer.position); color: root.mutedInk
            font.pixelSize: 13
        }
        Row {
            x: 16; y: 80; spacing: 4
            IconButton { objectName: "shuffleButton"; y: 4; selected: root.deckPlayer.shuffle; glyphName: "shuffle"; tip: root.deckPlayer.shuffle ? "Shuffle on" : "Shuffle off"; enabled: !root.useCider || !root.ciderService.controlBusy; ink: root.deckPlayer.shuffle ? root.accent : root.mutedInk; hoverFill: root.hoverFill; onClicked: root.deckPlayer.shuffle = !root.deckPlayer.shuffle }
            IconButton { objectName: "previousButton"; y: 4; glyphName: "previous"; tip: "Previous track · Ctrl+←"; ink: root.ink; hoverFill: root.hoverFill; enabled: root.useCider ? root.ciderService.canPrevious : player.count > 0; onClicked: root.deckPlayer.previous() }
            IconButton {
                objectName: "playButton"
                glyphSize: 26
                width: 56; height: 48
                glyphName: root.deckPlayer.playing ? "pause" : "play"
                tip: root.deckPlayer.playing ? "Pause · Space" : "Play · Space"
                fill: root.accent; ink: theme.colors.onAccent; hoverFill: Qt.lighter(root.accent, 1.08)
                onClicked: root.useCider ? root.ciderService.toggle() : player.count ? player.toggle() : files.open()
            }
            IconButton { objectName: "nextButton"; y: 4; glyphName: "next"; tip: "Next track · Ctrl+→"; ink: root.ink; hoverFill: root.hoverFill; enabled: root.useCider ? root.ciderService.canNext : player.count > 0; onClicked: root.deckPlayer.next() }
            IconButton {
                objectName: "repeatButton"
                y: 4
                selected: root.deckPlayer.repeatMode > 0; glyphName: root.deckPlayer.repeatMode === 2 ? "repeat_one" : "repeat"; tip: ["Repeat off", "Repeat queue", "Repeat one"][root.deckPlayer.repeatMode]
                ink: root.deckPlayer.repeatMode ? root.accent : root.mutedInk; hoverFill: root.hoverFill
                onClicked: root.deckPlayer.repeatMode = (root.deckPlayer.repeatMode + 1) % 3
            }
        }

        IconButton { x: 256; y: 84; glyphName: root.deckPlayer.volume > 0 ? "volume" : "mute"; tip: "Mute · M"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: root.toggleMute() }
        SpunSlider {
            id: volumeSlider
            objectName: "volumeSlider"
            x: 300; y: 86; width: 44; height: 36
            from: 0; to: 1; stepSize: .05; value: root.deckPlayer.volume
            onMoved: root.deckPlayer.volume = value
            Accessible.name: "Volume"
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
            NumberAnimation { target: jewelCase; property: "opacity"; from: 0; to: 1; duration: root.feedbackTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve }
            NumberAnimation { target: queueEntrance; property: "x"; from: 12; to: 0; duration: root.transitionTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
        }
        Rectangle { x: 8; y: 18; width: 6; height: parent.height-36; radius: 3; color: root.inset }
        Repeater {
            model: Math.max(0, Math.floor((jewelCase.height - 60) / 12))
            Rectangle { required property int index; x: 9; y: 30+index*12; width: 4; height: 1; color: root.hairline }
        }
        SpunText { visible: !root.queueSearchOpen; x: SpunStyle.outerInset; y: root.libraryDragging ? 20 : 16; height: root.libraryDragging ? SpunStyle.target : 26; verticalAlignment: Text.AlignVCenter; text: root.libraryDragging ? "Insert " + root.libraryDragTracks.length + (root.libraryDragTracks.length === 1 ? " song" : " songs") : "Queue"; color: root.libraryDragging ? root.accent : root.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
        SpunText {
            objectName: "queueTimeSummary"
            visible: !root.queueSearchOpen && !root.libraryDragging
            x: SpunStyle.outerInset; y: 43; width: 174; height: 18
            text: root.queueTimeSummary; color: root.mutedInk; font.pixelSize: SpunStyle.caption; elide: Text.ElideRight
            HoverHandler { id: queueTimeHover }
            SpunToolTip { visible: queueTimeHover.hovered; text: root.queueTimeSummary + "\nCurrent and upcoming songs. Excludes repeats and future autoplay." }
        }
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
                if (root.filteredQueue.length && (!root.useCider || (root.ciderService.queueReady && !root.ciderService.queueError.length)))
                    root.deckPlayer.select(root.filteredQueue[0].sourceIndex)
            }
        }
        IconButton { objectName: "savedQueueMenuButton"; visible: root.useCider && !root.queueSearchOpen; x: 204; y: 20; glyphName: "more"; tip: "Queue actions"; ink: root.mutedInk; onClicked: savedQueueMenu.open() }
        IconButton {
            objectName: "queueSearchButton"
            x: 246; y: 20; width: 40; height: 40
            glyphName: root.queueSearchOpen ? "close" : "search"
            tip: root.queueSearchOpen ? "Close search" : "Search queue · Ctrl+F"
            ink: root.queueSearchOpen ? root.accent : root.mutedInk; hoverFill: root.hoverFill
            onClicked: root.queueSearchOpen ? root.closeQueueSearch() : root.openQueueSearch()
        }
        Rectangle {
            objectName: "queueSelectionBar"; visible: root.queueSelectionCount > 0; z: 3
            x: 20; y: 12; width: parent.width - 40; height: 52; radius: 20; color: root.surface
            SpunText { x: 8; anchors.verticalCenter: parent.verticalCenter; text: root.queueSelectionCount + " selected"; color: root.ink; font.pixelSize: SpunStyle.body }
            IconButton { objectName: "queueSelectionActions"; x: parent.width - 84; anchors.verticalCenter: parent.verticalCenter; glyphName: "more"; tip: "Selected queue actions"; ink: root.accent; enabled: root.queueControlsReady; onClicked: root.openQueueSelectionActions(this) }
            IconButton { objectName: "clearQueueSelection"; x: parent.width - 42; anchors.verticalCenter: parent.verticalCenter; glyphName: "close"; tip: "Clear selection · Esc"; ink: root.mutedInk; onClicked: root.clearQueueSelection() }
        }
        Rectangle { x: 30; y: 65; width: 252; height: 1; color: root.hairline }
        ListView {
            id: trackList
            objectName: "trackList"
            activeFocusOnTab: visible
            property int keyboardIndex: -1
            Keys.onPressed: event => {
                if (root.useCider && event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) { root.selectUpcomingQueue(); event.accepted = true
                } else if (root.useCider && event.key === Qt.Key_Space) {
                    const row = root.filteredQueue[keyboardIndex < 0 ? currentIndex : keyboardIndex]
                    if (row) root.chooseQueueRow(row.sourceIndex, event.modifiers | Qt.ControlModifier)
                    event.accepted = true
                } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                    if ((event.modifiers & Qt.ShiftModifier) && root.queueSelectionAnchor < 0) root.queueSelectionAnchor = root.filteredQueue[Math.max(0, keyboardIndex < 0 ? currentIndex : keyboardIndex)]?.sourceIndex ?? -1
                    keyboardIndex = Math.max(0, Math.min(count - 1, (keyboardIndex < 0 ? currentIndex : keyboardIndex) + (event.key === Qt.Key_Down ? 1 : -1)))
                    if ((event.modifiers & Qt.ShiftModifier) && root.filteredQueue[keyboardIndex]) root.chooseQueueRow(root.filteredQueue[keyboardIndex].sourceIndex, event.modifiers)
                    positionViewAtIndex(keyboardIndex, ListView.Contain); event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    const row = root.filteredQueue[keyboardIndex < 0 ? currentIndex : keyboardIndex]
                    if (row && root.queueControlsReady) { if (root.queueSelectionCount) root.openQueueSelectionActions(trackList); else root.deckPlayer.select(row.sourceIndex) }
                    event.accepted = true
                } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                    const row = itemAtIndex(keyboardIndex < 0 ? currentIndex : keyboardIndex)
                    if (row) root.showQueueActions(row.sourceIndex, row)
                    event.accepted = true
                }
            }
            x: SpunStyle.inset; y: 80; width: parent.width - 2 * SpunStyle.inset; height: queueFooter.y - 2 * SpunStyle.gap - y - (root.useCider && root.ciderService.queueError.length ? 32 : 0)
            model: root.filteredQueue; clip: true; spacing: SpunStyle.smallGap
            visible: !root.useCider || root.ciderService.queueReady
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
                enabled: root.queueReorderAllowed && !root.queueSelectionCount; preventStealing: true
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property real startY: 0
                property int pressedSource: -1
                onPressed: mouse => {
                    startY = mouse.y
                    const index = trackList.indexAt(0,mouse.y + trackList.contentY)
                    pressedSource = index >= 0 ? root.filteredQueue[index].sourceIndex : -1
                    root.queueDragRevision = root.ciderService.queueRevision
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
                    opacity: root.deckPlayer.currentIndex === trackRow.sourceIndex || root.queueSelection.indexOf(trackRow.sourceIndex) >= 0 ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                }
                SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; enabled: root.queueControlsReady; pressed: rowHit.pressed; focused: trackList.activeFocus && trackList.keyboardIndex === trackRow.index; hovered: rowHover.hovered }
                border.width: trackList.activeFocus && trackList.keyboardIndex === index ? 2 : 0
                border.color: root.accent
                Accessible.role: Accessible.ListItem
                Accessible.name: modelData.track.title + ", " + modelData.track.artist
                Accessible.onPressAction: root.chooseQueueRow(sourceIndex, Qt.NoModifier)
                Accessible.checkable: root.useCider && sourceIndex > root.ciderService.currentIndex
                Accessible.checked: root.queueSelection.indexOf(sourceIndex) >= 0
                HoverHandler { id: rowHover }
                MouseArea { id: rowHit; anchors.fill: parent; enabled: root.queueControlsReady; onClicked: mouse => { trackList.forceActiveFocus(); trackList.keyboardIndex = trackRow.index; root.chooseQueueRow(trackRow.sourceIndex, mouse.modifiers) } }
                TrackContent {
                    anchors.fill: parent; app: root
                    title: trackRow.modelData.track.title; subtitle: trackRow.modelData.track.artist
                    artwork: trackRow.modelData.track.artwork || ""; artworkName: "queueArtwork" + trackRow.sourceIndex
                }
                IconButton { id: queueActions; objectName: "queueActions" + trackRow.sourceIndex; x: parent.width - SpunStyle.target; anchors.verticalCenter: parent.verticalCenter; glyphName: "more"; tip: "Queue actions"; ink: root.mutedInk; hoverFill: root.hoverFill; enabled: root.queueControlsReady; onClicked: root.showQueueActions(trackRow.sourceIndex,this) }
                TapHandler { acceptedButtons: Qt.RightButton; onTapped: root.showQueueActions(trackRow.sourceIndex,queueActions) }
                Glyph { visible: root.queueSelection.indexOf(trackRow.sourceIndex) >= 0; x: 1; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; name: "check"; ink: root.accent }
                Item {
                    x: 0; y: 0; width: 22; height: SpunStyle.trackHeight
                    opacity: root.queueReorderAllowed && !root.queueSelectionCount && (rowHover.hovered || root.queueDragSource === trackRow.sourceIndex) ? .7 : 0
                    Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                    Glyph { anchors.centerIn: parent; width: SpunStyle.smallIcon; height: width; name: "grip"; ink: root.mutedInk }

                }
                Rectangle {
                    visible: root.queueDragSource >= 0 && root.queueDropIndex === trackRow.sourceIndex && root.queueDragSource !== root.queueDropIndex
                    x: 24; y: root.queueDragSource < root.queueDropIndex ? parent.height : -3
                    width: parent.width - 30; height: 2; radius: 1; color: root.accent
                }
            }
            Rectangle {
                parent: trackList; z: 15; x: 24; width: trackList.width - 30; height: 3; radius: 1; color: root.accent
                y: Math.max(0, Math.min(trackList.height - height, root.libraryDropIndex * (SpunStyle.trackHeight + SpunStyle.smallGap) - trackList.contentY - 2))
                visible: root.libraryDragging && root.libraryDropIndex >= 0
            }
            SpunText {
                anchors.centerIn: parent; width: 200; visible: trackList.count === 0
                text: root.queueQuery.trim().length ? "No matches" : "No tracks"
                horizontalAlignment: Text.AlignHCenter; color: root.mutedInk; font.pixelSize: 14; lineHeight: 1.4
            }
        }
        SpunText {
            visible: root.useCider && root.ciderService.queueReady && root.ciderService.queueError.length > 0
            x: SpunStyle.outerInset; y: queueFooter.y - 40; width: parent.width - 2 * SpunStyle.outerInset; elide: Text.ElideRight
            text: root.ciderService.queueError; color: root.mutedInk; font.pixelSize: SpunStyle.caption
        }
        Column {
            id: connectionForm
            objectName: "ciderConnectionForm"
            property var service: root.ciderService
            property bool manual: false
            x: SpunStyle.outerInset; y: 100; width: parent.width - 2 * SpunStyle.outerInset; spacing: 12
            visible: root.useCider && (!connectionForm.service.queueReady || connectionForm.service.needsToken)
            component ConnectionButton: SpunButton {
                width: connectionForm.width; tonal: true
            }
            SpunText {
                width: parent.width
                text: connectionForm.service.connectionMessage || (connectionForm.service.queueBusy && !connectionForm.service.queueError.length ? "Loading queue…" : connectionForm.service.queueError)
                wrapMode: Text.WordWrap; color: root.ink; font.pixelSize: SpunStyle.body
            }
            ConnectionButton {
                objectName: "authorizeCiderButton"
                text: connectionForm.service.authorizing ? "Cancel" : "Connect to Cider"
                onClicked: { if(connectionForm.service.authorizing) connectionForm.service.cancelAuthorization(); else connectionForm.service.authorize() }
            }
            ConnectionButton {
                visible: connectionForm.service.recovering && !connectionForm.service.authorizing
                text: connectionForm.service.launching ? "Starting Cider…" : connectionForm.service.available ? "Retry connection" : "Open Cider"
                enabled: !connectionForm.service.launching
                onClicked: { if (!connectionForm.service.available) connectionForm.service.ensureRunning(); connectionForm.service.reconnect() }
            }
            ConnectionButton {
                objectName: "manualCiderTokenButton"
                visible: !connectionForm.service.authorizing
                text: connectionForm.manual ? "Hide manual token" : "Use an app token"
                onClicked: connectionForm.manual = !connectionForm.manual
            }
            SpunText {
                width: parent.width; visible: connectionForm.manual && !connectionForm.service.authorizing
                text: "In Cider: Settings → Connectivity → Manage External Application Access."
                wrapMode: Text.WordWrap; color: root.mutedInk; font.pixelSize: SpunStyle.caption; lineHeight: 1.25
            }
            TextField {
                id: ciderToken
                objectName: "ciderTokenInput"
                width: parent.width; height: SpunStyle.target; visible: connectionForm.manual && !connectionForm.service.authorizing
                echoMode: TextInput.Password; placeholderText: "Cider app token"; maximumLength: 8192
                leftPadding: 16; rightPadding: 16
                font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
                color: root.ink; placeholderTextColor: root.mutedInk
                selectionColor: root.accent; selectedTextColor: theme.colors.onAccent
                background: Rectangle { color: root.inset; radius: 12 * theme.radius; border.width: ciderToken.activeFocus ? 2 : 0; border.color: root.accent }
                onAccepted: { connectionForm.service.connectQueue(text); clear() }
            }
            ConnectionButton {
                objectName: "connectCiderButton"
                visible: connectionForm.manual && !connectionForm.service.authorizing
                enabled: ciderToken.text.trim().length > 0 && !connectionForm.service.queueBusy
                text: "Connect with token"
                onClicked: { connectionForm.service.connectQueue(ciderToken.text); ciderToken.clear() }
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
                onClicked: root.ciderService.raise()
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

    function previewQueueCleanup(mode) {
        const preview = root.ciderService.previewCleanup(mode)
        if (!preview.rows) return
        cleanupPopup.preview = preview
        cleanupPopup.open()
    }
    Popup {
        id: cleanupPopup; objectName: "cleanupPopup"; property var preview: ({})
        onClosed: preview = ({})
        readonly property bool stale: root.ciderService.queueRevision !== preview.revision || !root.ciderService.queueReady || root.ciderService.queueError.length > 0
        focus: true; modal: true; popupType: Popup.Item; padding: 24
        x: (root.width - width) / 2; y: (root.height - height) / 2
        width: 360; height: Math.min(440, root.height - 48, 196 + (preview.rows || []).length * 64)
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.dialogRadius }
        Overlay.modal: Rectangle { color: Qt.alpha("black", .32) }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        contentItem: Loader { active: cleanupPopup.visible; sourceComponent: Item {
            SpunText { text: cleanupPopup.preview.mode === "selection" ? "Remove selected songs?" : cleanupPopup.preview.mode === "duplicates" ? "Remove duplicates?" : "Clear upcoming?"; color: root.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
            SpunText {
                y: 34; width: parent.width; height: 48; wrapMode: Text.WordWrap
                text: cleanupPopup.stale ? "The queue changed. Close and preview again." : !(cleanupPopup.preview.rows || []).length ? (cleanupPopup.preview.mode === "duplicates" ? "No upcoming duplicates." : "No upcoming songs.") : (cleanupPopup.preview.rows || []).length + " to remove. Keeps the current song and history."
                color: root.mutedInk; font.pixelSize: SpunStyle.caption
            }
            ListView {
                objectName: "cleanupPreviewList"; y: 88; width: parent.width; height: parent.height - y - 60
                clip: true; model: cleanupPopup.preview.rows || []; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { width: 3; contentItem: Rectangle { radius: 2; color: root.mutedInk; opacity: .4 } }
                delegate: Item {
                    required property var modelData; width: ListView.view.width; height: 64
                    TrackContent { anchors.fill: parent; app: root; leadingInset: 0; trailingInset: 4; title: modelData.title; subtitle: modelData.artist; artwork: modelData.artwork || ""; fallback: "disc" }
                }
            }
            SpunButton { objectName: "cancelCleanup"; x: parent.width - 184; y: parent.height - 40; width: 80; text: "Cancel"; onClicked: cleanupPopup.close() }
            SpunButton { objectName: "confirmCleanup"; x: parent.width - width; y: parent.height - 40; width: 96; text: "Remove"; tonal: true; enabled: !cleanupPopup.stale && root.queueControlsReady && (cleanupPopup.preview.rows || []).length > 0; onClicked: { if (cleanupPopup.preview.mode === "selection") root.ciderService.editQueueSelection(cleanupPopup.preview.indices, "remove", cleanupPopup.preview.revision); else root.ciderService.cleanQueue(cleanupPopup.preview.mode, cleanupPopup.preview.revision); cleanupPopup.close() } }
        } }
    }
    function openSavedQueues() { root.useCider = true; root.libraryOpen = true; musicBrowser.browser.section = "sessions" }
    function renameQueue(queue, service) { saveQueuePopup.service = service; saveQueuePopup.renameTarget = queue; saveQueuePopup.open() }
    function confirmDeleteQueue(id, service) { deleteQueuePopup.queueId = id; deleteQueuePopup.service = service; deleteQueuePopup.open() }
    Menu {
        id: savedQueueMenu; parent: jewelCase; x: 30; y: 64; width: 264; padding: 8; popupType: Popup.Item
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
        enter: SpunPopupEnter {}
        SettingsAction { objectName: "saveQueueAction"; text: "Save queue"; glyphName: "plus"; enabled: root.ciderService.queueReady && !root.ciderService.queueBusy && root.ciderService.queue.length > 0; onTriggered: saveQueuePopup.open() }
        SettingsAction { objectName: "recoverSessionAction"; visible: root.useCider && !!root.listeningService.session.trackCount; text: "Recover session…"; glyphName: "refresh"; enabled: !root.listeningService.busy; onTriggered: root.showRecovery() }
        SettingsAction { objectName: "savedQueuesAction"; text: "Saved queues"; glyphName: "queue"; onTriggered: root.openSavedQueues() }
        SettingsAction { objectName: "deduplicateQueueAction"; text: "Remove duplicates…"; glyphName: "minus"; enabled: root.queueControlsReady; onTriggered: root.previewQueueCleanup("duplicates") }
        SettingsAction { objectName: "clearUpcomingAction"; text: "Clear upcoming…"; glyphName: "close"; enabled: root.queueControlsReady; onTriggered: root.previewQueueCleanup("upcoming") }
    }
    Popup {
        id: saveQueuePopup; objectName: "saveQueuePopup"; property var service: library
        property var renameTarget: ({})
        onClosed: renameTarget = ({})
        function submit(name) { if (renameTarget.id ? service.renameSavedQueue(renameTarget, name) : service.saveQueue(name)) close() }
        focus: true; modal: true; popupType: Popup.Item
        x: (root.width - width) / 2; y: 220; width: 330; height: 200; padding: 24
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.dialogRadius }
        Overlay.modal: Rectangle { color: Qt.alpha("black", .32) }
        enter: SpunPopupEnter {}
        contentItem: Loader { active: saveQueuePopup.visible; sourceComponent: Item {
            Component.onCompleted: Qt.callLater(function() { savedQueueName.forceActiveFocus(); if(saveQueuePopup.renameTarget.id)savedQueueName.selectAll() })
            SpunText { text: saveQueuePopup.renameTarget.id ? "Rename saved queue" : "Save queue"; color: root.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
            SpunText { y: 30; text: saveQueuePopup.renameTarget.id ? "Saved locally in Spun" : "Current and upcoming tracks"; color: root.mutedInk; font.pixelSize: SpunStyle.caption }
            SpunSearchField { id: savedQueueName; objectName: "savedQueueName"; app: root; searchIcon: false; rightPadding: 16; Accessible.name: "Queue name"; y: 58; width: parent.width; height: 40; placeholderText: "Queue name"; text: saveQueuePopup.renameTarget.title || ""; maximumLength: 80; onAccepted: if(text.trim().length)saveQueuePopup.submit(text) }
            SpunButton { objectName: "cancelSaveQueue"; x: parent.width - 168; y: 110; width: 80; text: "Cancel"; onClicked: saveQueuePopup.close() }
            SpunButton { objectName: "confirmSaveQueue"; x: parent.width - width; y: 110; width: 80; text: "Save"; tonal: true; enabled: savedQueueName.text.trim().length > 0; onClicked: saveQueuePopup.submit(savedQueueName.text) }
        } }
    }
    Popup {
        id: deleteQueuePopup; focus: true; modal: true; popupType: Popup.Item; property string queueId: ""; property var service: library
        x: (root.width - width) / 2; y: 240; width: 330; height: 132; padding: 24
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.dialogRadius }
        Overlay.modal: Rectangle { color: Qt.alpha("black", .32) }
        enter: SpunPopupEnter {}
        contentItem: Loader { active: deleteQueuePopup.visible; sourceComponent: Item {
            SpunText { text: "Delete saved queue?"; color: root.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
            SpunButton { x: parent.width - 176; y: 42; width: 80; text: "Cancel"; onClicked: deleteQueuePopup.close() }
            SpunButton { objectName: "confirmDeleteQueue"; x: parent.width - width; y: 42; width: 88; text: "Delete"; tonal: true; onClicked: { deleteQueuePopup.service.deleteSavedQueue(deleteQueuePopup.queueId);deleteQueuePopup.close() } }
        } }
    }
    Menu {
        id: queueMenu
        objectName: "queueEditMenu"
        readonly property bool batch: root.queueSelectionCount > 0
        onOpened: { if (!batch && root.useCider && rowIndex >= 0 && rowIndex < root.ciderService.queue.length) library.prepareRadio(root.ciderService.queue[rowIndex]) }
        property int rowIndex: -1
        property int revision: -1
        popupType: Popup.Item
        width: 272; padding: 8; spacing: 2; margins: 12
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }


        SettingsAction { objectName: "queueSelectTrack"; visible: root.useCider && !queueMenu.batch && queueMenu.rowIndex > root.ciderService.currentIndex; text: "Select track"; glyphName: "check"; onTriggered: root.chooseQueueRow(queueMenu.rowIndex, Qt.ControlModifier) }
        SettingsAction { objectName: "queueSelectAll"; visible: root.useCider && queueMenu.batch; text: "Select all upcoming"; glyphName: "check"; onTriggered: root.selectUpcomingQueue() }
        SettingsAction { objectName: "queueBatchNext"; visible: queueMenu.batch; text: "Play next"; glyphName: "next"; enabled: root.queueControlsReady; onTriggered: root.editQueueSelection("next") }
        SettingsAction { objectName: "queueBatchUp"; visible: queueMenu.batch; text: "Move up"; glyphName: "up"; enabled: root.queueReorderAllowed; onTriggered: root.editQueueSelection("up") }
        SettingsAction { objectName: "queueBatchDown"; visible: queueMenu.batch; text: "Move down"; glyphName: "down"; enabled: root.queueReorderAllowed; onTriggered: root.editQueueSelection("down") }
        SettingsAction { objectName: "queueBatchEnd"; visible: queueMenu.batch; text: "Move to end"; glyphName: "queue"; enabled: root.queueControlsReady; onTriggered: root.editQueueSelection("end") }
        SettingsAction { objectName: "queueBatchSave"; visible: queueMenu.batch; text: "Add to saved queue…"; glyphName: "plus"; enabled: root.queueControlsReady; onTriggered: root.openSavedQueuePicker(root.queueSelection.map(i => root.ciderService.queue[i]), library) }
        SettingsAction { objectName: "queueBatchRemove"; visible: queueMenu.batch; text: "Remove selected…"; glyphName: "close"; enabled: root.queueControlsReady; onTriggered: root.editQueueSelection("remove") }
        SettingsAction { objectName: "queueMoveUp"; visible: !queueMenu.batch; text: "Move up"; glyphName: "up"; enabled: root.queueReorderAllowed && queueMenu.rowIndex > 0; onTriggered: root.moveQueueRow(queueMenu.rowIndex,queueMenu.rowIndex - 1,queueMenu.revision) }
        SettingsAction { objectName: "queueMoveDown"; visible: !queueMenu.batch; text: "Move down"; glyphName: "down"; enabled: root.queueReorderAllowed && queueMenu.rowIndex < root.queueRows.length - 1; onTriggered: root.moveQueueRow(queueMenu.rowIndex,queueMenu.rowIndex + 1,queueMenu.revision) }
        SettingsAction { objectName: "queueRemove"; visible: !queueMenu.batch; text: "Remove from queue"; glyphName: "close"; enabled: root.queueControlsReady; onTriggered: root.useCider ? root.ciderService.removeQueue(queueMenu.rowIndex,queueMenu.revision) : player.remove(queueMenu.rowIndex) }
        SettingsAction {
            objectName: "queueRadioAction"; visible: root.useCider && !queueMenu.batch
            text: library.radioBusy ? "Finding station…" : library.radioAvailable ? "Start radio" : "Radio unavailable"
            glyphName: "disc"; enabled: library.radioAvailable && !library.radioBusy && !root.ciderService.controlBusy
            onTriggered: library.playRadio()
        }
        SettingsAction { objectName: "queueCopyLink"; visible: root.useCider && !queueMenu.batch; text: "Copy song link"; glyphName: "external"; enabled: queueMenu.rowIndex >= 0 && queueMenu.rowIndex < root.ciderService.queue.length; onTriggered: library.copyLink(root.ciderService.queue[queueMenu.rowIndex]) }

    }
    TrackMenu {
        id: songMenu
        objectName: "currentSongMenu"
        app: root; currentSong: true
        onAboutToShow: root.actionService.observing = true
        onClosed: root.actionService.observing = false
    }
    function showAudioQuality() { qualityPopup.open();root.ciderService.refreshAudioQuality() }
    Popup {
        id: qualityPopup; objectName: "audioQualityPopup"
        popupType: Popup.Item; focus: true; width: 340; height: 176; padding: 20
        x: (root.width - width) / 2; y: Math.min(root.height - height - 24, 330)
        onVisibleChanged: Qt.callLater(root.updateMask)
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        contentItem: Loader { active: qualityPopup.visible; sourceComponent: Item {
            SpunText { text: "Audio quality"; color: root.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
            IconButton { x: parent.width - width; y: -8; glyphName: "close"; tip: "Close"; ink: root.mutedInk; onClicked: qualityPopup.close() }
            SpunText { objectName: "audioQualityText"; y: 42; width: parent.width; text: root.ciderService.qualityBusy ? "Checking…" : root.ciderService.audioQuality; color: root.ink; font.pixelSize: SpunStyle.body; wrapMode: Text.WordWrap }
            IconButton { x: parent.width - width; y: 96; glyphName: "refresh"; tip: "Refresh quality"; ink: root.accent; enabled: !root.ciderService.qualityBusy; onClicked: root.ciderService.refreshAudioQuality() }
        } }
    }
    function showRecovery() { recoveryPopup.open(); root.listeningService.prepareRecovery() }
    Popup {
        id: recoveryPopup; objectName: "recoveryPopup"
        parent: Overlay.overlay; x: (parent.width - width) / 2; y: (parent.height - height) / 2
        width: Math.min(420, root.width - 24); height: Math.min(260, root.height - 24)
        padding: 24; modal: true; dim: false; focus: true; popupType: Popup.Item
        background: Rectangle { radius: SpunStyle.popupRadius; color: SpunStyle.popup }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: SpunStyle.exit } }
        onOpened: recoveryCancel.forceActiveFocus()
        onClosed: root.listeningService.cancelRecovery()
        contentItem: Item {
            SpunText { text: "Recover session"; font.pixelSize: SpunStyle.heading; color: root.ink }
            SpunText {
                y: 38; width: parent.width; height: 112; wrapMode: Text.WordWrap; maximumLineCount: 5; elide: Text.ElideRight
                font.pixelSize: SpunStyle.body; color: root.mutedInk
                text: root.listeningService.error || (root.listeningService.busy ? "Restoring your session…" : root.ciderService.queueBusy ? "Checking Cider’s queue…" : !root.listeningService.session.trackCount ? "No saved session yet. Enable Remember Cider session in Preferences." : root.ciderService.queue.length ? "Cider already has a queue. Recovery keeps your existing queue safe. You can try again when it is empty." : "Resume “" + root.listeningService.session.title + "” at " + root.time(root.listeningService.session.position) + ", with " + root.listeningService.session.trackCount + " tracks? This starts playback.")
            }
            SpunButton { id: recoveryCancel; objectName: "recoveryCancel"; anchors.left: parent.left; anchors.bottom: parent.bottom; text: root.listeningService.busy ? "Close" : "Cancel"; onClicked: recoveryPopup.close() }
            SpunButton { objectName: "recoveryConfirm"; anchors.right: parent.right; anchors.bottom: parent.bottom; text: "Restore"; tonal: true
                enabled: !root.listeningService.busy && !!root.listeningService.session.trackCount && root.ciderService.queueReady && !root.ciderService.queueBusy && !root.ciderService.controlBusy && !root.ciderService.queueError.length && root.ciderService.queue.length === 0 && !root.actionService.busy
                onClicked: root.listeningService.restoreSession()
            }
        }
        Connections { target: root.listeningService; function onFeedback(message, error) { if (recoveryPopup.visible && !error && !root.listeningService.busy) recoveryPopup.close() } }
    }
    function notifyAction(message, error) { actionNotice.text = message; actionNotice.failed = error; actionNotice.savedUndo = false; actionNotice.undo = !error && message === "Removed from queue" && root.ciderService.canUndoQueue; noticeTimer.interval = error || actionNotice.undo ? 8000 : 2200; noticeTimer.restart() }
    Connections {
        target: root.ciderService
        function onApiFeedback(message, error) { root.notifyAction(message,error) }
        function onLaunchChanged() { if (root.ciderService.launching) root.notifyAction("Starting Cider…",false) }
    }
    Connections {
        target: root.actionService
        function onCurrentChanged() { songMenu.close(); qualityPopup.close() }
        function onFeedback(message, error) { root.notifyAction(message,error) }
    }
    Connections {
        target: root.savedService
        function onSavedEditCommitted() { actionNotice.savedUndo = true; noticeTimer.interval = 8000; noticeTimer.restart() }
    }
    Timer { id: noticeTimer }
    Rectangle {
        id: actionNotice
        objectName: "actionNotice"
        property string text: ""
        property bool failed: false
        property bool undo: false
        property bool savedUndo: false
        readonly property bool canUndo: savedUndo ? root.savedService.canUndoSavedQueue : undo && root.ciderService.canUndoQueue
        visible: noticeTimer.running && (!root.miniMode || failed)
        onVisibleChanged: Qt.callLater(root.updateMask)
        z: 30; x: root.miniMode ? 12 : deck.x; y: root.miniMode ? 234 : deck.y + deck.height + 8; width: root.miniMode ? 276 : deck.width; height: root.miniMode ? 56 : 40; radius: SpunStyle.rowRadius
        color: root.surface
        SpunText { x: 12; y: 2; width: noticeDismiss.x - x - 8 - (actionNotice.canUndo ? 80 : 0); height: parent.height - 4; text: actionNotice.text; color: actionNotice.failed ? theme.colors.error : root.ink; font.pixelSize: SpunStyle.body; wrapMode: Text.WordWrap; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        SpunButton {
            objectName: "undoQueueButton"; visible: actionNotice.canUndo
            x: noticeDismiss.x - width - 8; anchors.verticalCenter: parent.verticalCenter; width: 72; height: 40; enabled: actionNotice.savedUndo || root.queueControlsReady
            text: "Undo"
            onClicked: { noticeTimer.stop(); if(actionNotice.savedUndo)root.savedService.undoSavedQueue();else root.ciderService.undoQueueRemoval() }
        }
        IconButton { id: noticeDismiss; objectName: "dismissActionNotice"; anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; glyphName: "close"; tip: "Dismiss"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: noticeTimer.stop() }
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
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        onOpened: {  if (root.useCider) root.ciderService.refreshModes() }

        SettingsAction { text: "Add tracks"; glyphName: "plus"; hint: "Ctrl+O"; onTriggered: files.open() }
        SettingsAction { text: "Add album folder"; glyphName: "folder"; hint: "Ctrl+Shift+O"; onTriggered: folder.open() }
        SettingsAction { text: "Change artwork"; glyphName: "artwork"; enabled: !root.useCider && player.count > 0; onTriggered: cover.open() }
        SettingsGap {}
        SettingsAction { objectName: "quickJumpAction"; text: "Quick jump"; hint: "Ctrl+K"; glyphName: "search"; onTriggered: root.openQuickJump() }
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
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        onAboutToShow: fontPicker.prepare()
        onAboutToHide: fontPicker.close()
        onOpened: {  if (root.useCider) root.ciderService.refreshModes(); closePreferences.forceActiveFocus() }
        onClosed: { fontPicker.close(); menuButton.forceActiveFocus() }
        contentItem: Item {
            SpunText { x: 12; y: 10; text: "Preferences"; color: root.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
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
                    Row {
                        width: parent.width; height: 48; spacing: 4
                        Repeater {
                            id: recordChoices
                            model: ["CD", "Vinyl"]
                            delegate: AbstractButton {
                                id: recordChoice
                                required property string modelData
                                objectName: modelData === "CD" ? "cdStyleButton" : "vinylStyleButton"
                                width: (preferenceItems.width - 4) / 2; height: SpunStyle.target
                                text: modelData; checkable: true; autoExclusive: true; checked: player.vinyl === (modelData === "Vinyl"); hoverEnabled: true
                                Accessible.name: modelData + " appearance"; Accessible.checked: checked
                                Keys.onLeftPressed: recordChoices.itemAt(0).forceActiveFocus(Qt.TabFocusReason)
                                Keys.onRightPressed: recordChoices.itemAt(1).forceActiveFocus(Qt.TabFocusReason)
                                Keys.onReturnPressed: clicked()
                                Keys.onEnterPressed: clicked()
                                onClicked: player.vinyl = modelData === "Vinyl"
                                onActiveFocusChanged: if(activeFocus) preferenceScroll.reveal(this)
                                background: Rectangle {
                                    radius: 20 * theme.radius; color: "transparent"
                                    border.width: recordChoice.visualFocus ? 2 : 0; border.color: root.accent
                                    Rectangle { anchors.fill: parent; radius: parent.radius; color: SpunStyle.selected; opacity: recordChoice.checked ? 1 : 0
                                        Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                                    }
                                    SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; hovered: recordChoice.hovered; pressed: recordChoice.down; focused: recordChoice.visualFocus }
                                }
                                contentItem: SpunText { text: recordChoice.text; color: recordChoice.checked ? root.accent : root.mutedInk; font.pixelSize: SpunStyle.body; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                    }

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
                            border.width: fontChoice.visualFocus ? 2 : 0; border.color: root.accent
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
                    PreferenceSwitch { objectName: "rememberSessionToggle"; app: root; width: parent.width; text: "Remember Cider session"; glyphName: "queue"; checked: root.listeningService.rememberSession; enabled: !root.listeningService.busy; onToggled: root.listeningService.rememberSession = checked; onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                    PreferenceSwitch { objectName: "ciderAutoStartToggle"; app: root; width: parent.width; text: "Start Cider with Spun"; glyphName: "power"; checked: player.ciderAutoStart; onToggled: { player.ciderAutoStart = checked; if (checked && root.useCider) root.ciderService.ensureRunning() } onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                    PreferenceSwitch { objectName: "autoplayToggle"; app: root; width: parent.width; visible: root.useCider; height: visible ? implicitHeight : 0; text: "Autoplay"; glyphName: "autoplay"; checked: root.ciderService.autoplay; enabled: root.ciderService.modesReady && !root.ciderService.controlBusy; onToggled: root.ciderService.setAutoplay(checked); onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
                    MenuEntry { objectName: "crossfadeAction"; app: root; width: parent.width; visible: root.useCider; text: "Audio settings"; glyphName: "crossfade"; onTriggered: { preferences.close(); Qt.callLater(function() { crossfadeMenu.open() }) } onActiveFocusChanged: if (activeFocus) preferenceScroll.reveal(this) }
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
        property var service: root.ciderService
        popupType: Popup.Item
        x: deck.x + deck.width - width; y: Math.max(12, deck.y - height - 10)
        width: 320; height: Math.max(170, audioExtras.y + audioExtras.implicitHeight + 24); padding: 12
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
        onOpened: { service.refreshCrossfade(); service.refreshAudioOptions() }

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
            SpunSlider {
                id: crossfadeDuration; objectName: "crossfadeDuration"
                x: 10; y: 78; width: parent.width - 20; height: 36
                from: 1; to: 12; stepSize: 1; snapMode: Slider.SnapAlways
                value: crossfadeMenu.service.crossfadeSeconds
                enabled: crossfadeMenu.service.crossfadeReady && crossfadeMenu.service.crossfade && !crossfadeMenu.service.crossfadeBusy
                Accessible.name: "Crossfade duration in seconds"
                onMoved: if (!pressed) crossfadeMenu.service.setCrossfadeSeconds(value)
                onPressedChanged: if (!pressed && enabled) crossfadeMenu.service.setCrossfadeSeconds(value)
                Connections { target: crossfadeMenu.service; function onCrossfadeChanged() { if (!crossfadeDuration.pressed) crossfadeDuration.value = crossfadeMenu.service.crossfadeSeconds } }
                valueText: Math.round(value) + " s"
            }
            SpunText {
                id: crossfadeServiceError; x: 10; y: 116; width: parent.width - 20; height: 42
                visible: crossfadeMenu.service.crossfadeError.length > 0
                text: crossfadeMenu.service.crossfadeError; font.pixelSize: SpunStyle.caption; color: theme.colors.error; wrapMode: Text.WordWrap
            }
            Column {
                id: audioExtras; x: 0; y: crossfadeServiceError.visible ? 216 : 118; width: parent.width; spacing: 4
                PreferenceSwitch {
                    objectName: "automixToggle"; app: root; width: parent.width
                    visible: crossfadeMenu.service.audioOptions.automix !== undefined
                    text: "Automix"; glyphName: "autoplay"; checked: !!crossfadeMenu.service.audioOptions.automix
                    enabled: !crossfadeMenu.service.audioBusy; onToggled: crossfadeMenu.service.setAudioOption("automix", checked)
                }
                SpunText { visible: crossfadeMenu.service.audioOptions.listeningMode !== undefined; x: 10; text: "Listening mode"; color: root.mutedInk; font.pixelSize: SpunStyle.caption; height: 24; verticalAlignment: Text.AlignVCenter }
                Row {
                    visible: crossfadeMenu.service.audioOptions.listeningMode !== undefined; width: parent.width; spacing: 4
                    Repeater { model: crossfadeMenu.service.audioOptions.listeningMode !== undefined ? ["off", "gaming", "unwind"] : []
                        AbstractButton {
                            id: modeChoice; required property string modelData; objectName: "listeningMode_" + modelData
                            width: (audioExtras.width - 8) / 3; height: 40; hoverEnabled: true
                            enabled: !crossfadeMenu.service.audioBusy
                            readonly property bool selected: crossfadeMenu.service.audioOptions.listeningMode === modelData
                            text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                            onClicked: crossfadeMenu.service.setAudioOption("listeningMode", modelData)
                            background: Rectangle { radius: 20 * theme.radius; color: "transparent"
                                Rectangle { anchors.fill: parent; radius: parent.radius; color: SpunStyle.selected; opacity: modeChoice.selected ? 1 : 0; Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } } }
                                SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: root.ink; hovered: modeChoice.hovered; pressed: modeChoice.down; focused: modeChoice.visualFocus }
                            }
                            contentItem: SpunText { text: modeChoice.text; color: modeChoice.selected ? root.accent : root.mutedInk; font.pixelSize: SpunStyle.caption; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        }
                    }
                }
                SettingsAction { width: parent.width; visible: crossfadeMenu.service.audioError.length > 0; text: "Refresh audio settings"; glyphName: "refresh"; enabled: !crossfadeMenu.service.audioBusy; onTriggered: crossfadeMenu.service.refreshAudioOptions(); SpunToolTip { visible: parent.hovered; text: crossfadeMenu.service.audioError } }
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
        x: 94; y: 110; width: 342; height: 514; radius: 22
        visible: root.helpOpen; color: root.surface; border.width: 0
        SpunText { x: 24; y: 23; text: "Shortcuts"; font.family: SpunStyle.family; font.pixelSize: 22; color: root.ink }
        SpunText {
            x: 24; y: 70; width: 294; color: root.mutedInk; font.pixelSize: 12; lineHeight: 1.55
            text: "Space                  Play / pause\n← / →                 Seek 5 seconds\nCtrl + ← / →       Previous / next\n↑ / ↓                    Volume\nM                         Mute\nCtrl + O               Add music\nCtrl + L                Show queue\nCtrl + F                Search panel\nCtrl + K               Quick jump\nCtrl + M              Mini / full player\nCtrl + B              Browse Cider music\nCtrl + V              Open music link\nF                          Flip disc\nY                          Lyrics / album tracks\nCtrl / Shift + click    Select tracks\nCtrl + A / Space     Select all / toggle*\n*In a focused track list\n\nDouble-click to flip. Drag to move.\nScrub the outer rim to seek."
        }
        IconButton { x: 299; y: 8; glyphName: "close"; tip: "Close shortcuts"; ink: root.mutedInk; hoverFill: root.hoverFill; onClicked: root.helpOpen=false }
    }
}
