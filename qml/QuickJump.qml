import QtQuick
import QtQuick.Controls

Popup {
    id: jump
    required property var app
    property var service: app.savedService
    property var targets: []
    property string query: ""
    property string notice: ""
    property int revision: -1
    property bool waitingForQueue: false
    readonly property var results: {
        const words = query.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase().trim().split(/\s+/).filter(Boolean)
        return targets.filter(row => words.every(word => row.match.includes(word))).slice(0, 40)
    }
    objectName: "quickJump"
    parent: Overlay.overlay
    x: (parent.width - width) / 2
    y: Math.max(12, (app.height - 430) / 2)
    width: Math.min(440, app.width - 24)
    height: Math.min(430, app.height - 24, Math.max(214, 126 + Math.max(1, results.length) * 66 + (notice.length ? 50 : 0)))
    Behavior on height { NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.standardCurve } }
    padding: 16; focus: true; modal: true; dim: false; popupType: Popup.Item
    background: Rectangle { radius: SpunStyle.popupRadius; color: SpunStyle.popup }
    enter: SpunPopupEnter {}
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
    function show(preserveQuery) {
        const saved = service.savedQueueChoices()
        notice = saved.error || ""; if (!preserveQuery) query = ""
        const rows = []
        const add = (kind, key, title, subtitle, artwork, index, identity) => rows.push({kind: kind, key: key, title: title || "Untitled", subtitle: subtitle, artwork: artwork || "", index: index, identity: identity || "", match: ((title || "") + " " + subtitle).normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase()})
        for (const row of service.pins) add("pin", row.path, row.title, row.type === "artists" ? "Pinned artist" : row.type === "stations" ? "Pinned station" : row.type.includes("albums") ? "Pinned album · " + (row.artist || "") : "Pinned playlist", row.artwork, -1)
        for (const row of saved.rows || []) add("saved", row.id, row.title, "Saved queue · " + row.trackCount + (row.trackCount === 1 ? " track" : " tracks"), "", -1)
        for (const row of app.listeningService.bookmarks) add("bookmark", row.key, row.title, "Bookmark · " + app.time(row.position) + " · " + (row.artist || ""), row.artwork, -1)
        if (app.listeningService.session.trackCount) add("recovery", "", "Recover listening session", app.listeningService.session.title || "", "", -1)
        targets = rows; open()
        if (app.useCider) { waitingForQueue = true; app.ciderService.refreshQueue(); receiveQueue() }
        else addQueue()
    }
    function addQueue() {
        const rows = targets.filter(row => row.kind !== "queue")
        const queue = app.deckPlayer.queue
        revision = app.useCider ? app.ciderService.queueRevision : -1
        for (let i = 0; i < queue.length; i++) {
            const row = queue[i], title = row.title || "Untitled", subtitle = "Queue · " + (row.artist || "")
            rows.push({kind: "queue", key: "", title: title, subtitle: subtitle, artwork: row.artwork || "", index: i, identity: app.useCider ? row.type + ":" + row.id : row.path, match: (title + " " + subtitle).normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase()})
        }
        targets = rows
    }
    function receiveQueue() {
        if (!visible || !waitingForQueue || app.ciderService.queueBusy || app.ciderService.controlBusy) return
        waitingForQueue = false
        if (app.ciderService.queueReady && !app.ciderService.queueError.length) addQueue()
        else notice = "Queue unavailable. Your pins and saved queues are still here."
    }
    Connections { target: jump.app.ciderService; function onQueueStatusChanged() { jump.receiveQueue() } }

    function activate(row) {
        if (!visible || !row) return
        if (row.kind === "queue") {
            if (app.useCider && (revision !== app.ciderService.queueRevision || !app.queueControlsReady)) { notice = "Queue changed. Refresh before choosing a song."; return }
            const current = app.deckPlayer.queue[row.index]
            if (!current || (app.useCider ? current.type + ":" + current.id : current.path) !== row.identity) { notice = "Queue changed. Refresh before choosing a song."; return }
            close(); app.deckPlayer.select(row.index)
            if (!app.miniMode) app.revealQueueTrack(row.index)
        } else if (row.kind === "bookmark") {
            if (app.actionService.busy || app.ciderService.controlBusy || app.listeningService.busy) { notice = "Cider is busy. Try again when it finishes."; return }
            close(); app.useCider = true; app.listeningService.playBookmark(row.key)
        } else if (row.kind === "recovery") {
            close(); app.useCider = true; app.showRecovery()
        } else {
            const kind = row.kind, key = row.key
            close(); app.useCider = true; app.libraryOpen = true; service.openQuickTarget(kind, key)
        }
    }
    onClosed: { waitingForQueue = false; targets = []; query = ""; notice = "" }
    contentItem: Loader {
        active: jump.visible
        sourceComponent: Item {
            Component.onCompleted: Qt.callLater(function() { search.forceActiveFocus() })
            SpunText { text: "Quick jump"; color: jump.app.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
            IconButton { x: parent.width - 36; y: -8; width: 36; height: 36; glyphName: "close"; tip: "Close · Esc"; ink: jump.app.mutedInk; onClicked: jump.close() }
            SpunSearchField {
                id: search; objectName: "quickJumpSearch"; app: jump.app
                y: 40; width: parent.width; placeholderText: "Find music…"; maximumLength: 160
                text: jump.query; onTextEdited: { jump.query = text; choices.currentIndex = 0 }
                onAccepted: jump.activate(jump.results[choices.currentIndex])
                Keys.onDownPressed: { choices.currentIndex = Math.min(choices.count - 1, choices.currentIndex + 1); choices.positionViewAtIndex(choices.currentIndex, ListView.Contain) }
                Keys.onUpPressed: { choices.currentIndex = Math.max(0, choices.currentIndex - 1); choices.positionViewAtIndex(choices.currentIndex, ListView.Contain) }
                IconButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 36; height: 36; visible: search.text.length > 0; glyphName: "close"; tip: "Clear search"; ink: jump.app.mutedInk; onClicked: { search.clear(); jump.query = ""; choices.currentIndex = 0; search.forceActiveFocus() } }
            }
            ListView {
                id: choices; objectName: "quickJumpResults"
                y: 94; width: parent.width; height: parent.height - y - (jump.notice.length ? 50 : 0)
                model: jump.results; currentIndex: 0; clip: true; spacing: 4; boundsBehavior: Flickable.StopAtBounds
                activeFocusOnTab: true
                Keys.onReturnPressed: jump.activate(jump.results[currentIndex])
                Keys.onEnterPressed: jump.activate(jump.results[currentIndex])
                ScrollBar.vertical: ScrollBar {}
                delegate: AbstractButton {
                    id: row
                    required property int index
                    required property var modelData
                    objectName: "quickJumpRow" + index
                    width: choices.width; height: 62; hoverEnabled: true
                    Accessible.name: modelData.title + ", " + modelData.subtitle
                    Accessible.selectable: true; Accessible.selected: row.index === choices.currentIndex
                    onClicked: jump.activate(modelData)
                    background: Rectangle {
                        radius: SpunStyle.rowRadius; color: row.index === choices.currentIndex ? SpunStyle.selected : "transparent"
                        border.width: row.visualFocus || (choices.activeFocus && row.index === choices.currentIndex) ? 2 : 0
                        border.color: jump.app.accent
                        Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: jump.app.ink; hovered: row.hovered; pressed: row.down; focused: row.visualFocus }
                    }
                    contentItem: Item {
                        Rectangle { x: 8; y: 9; width: 44; height: 44; radius: 8; color: jump.app.inset
                            Image { id: art; anchors.fill: parent; source: row.modelData.artwork; sourceSize: Qt.size(88,88); asynchronous: true; cache: false; fillMode: Image.PreserveAspectCrop }
                            Glyph { anchors.centerIn: parent; visible: art.status !== Image.Ready; name: row.modelData.kind === "queue" || row.modelData.kind === "saved" ? "queue" : "disc"; ink: jump.app.mutedInk }
                        }
                        IconButton { objectName: "removeBookmark" + row.index; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 36; height: 36; visible: row.modelData.kind === "bookmark"; glyphName: "close"; ink: jump.app.mutedInk; tip: "Remove bookmark"; enabled: !jump.app.listeningService.busy; onClicked: { jump.app.listeningService.removeBookmark(row.modelData.key); jump.show(true) } }
                        SpunText { objectName: "quickJumpTitle" + row.index; height: 20; maximumLineCount: 1; x: 64; y: 9; width: parent.width - (row.modelData.kind === "bookmark" ? 112 : 76); text: row.modelData.title; color: jump.app.ink; font.pixelSize: SpunStyle.body; elide: Text.ElideRight }
                        SpunText { height: 16; maximumLineCount: 1; x: 64; y: 33; width: parent.width - (row.modelData.kind === "bookmark" ? 112 : 76); text: row.modelData.subtitle; color: jump.app.mutedInk; font.pixelSize: SpunStyle.caption; elide: Text.ElideRight }
                    }
                }
                SpunText { objectName: "quickJumpEmptyState"; anchors.centerIn: parent; width: parent.width - 24; visible: choices.count === 0; text: jump.waitingForQueue ? "Loading queue…" : jump.query.trim().length ? "No matches" : "Your pins, bookmarks, saved queues and loaded queue appear here."; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: jump.app.mutedInk; font.pixelSize: SpunStyle.body }
            }
            SpunText { anchors.bottom: parent.bottom; height: 42; width: parent.width - 44; text: jump.notice; visible: text.length > 0; wrapMode: Text.WordWrap; font.pixelSize: SpunStyle.caption; color: jump.app.mutedInk }
            IconButton { anchors.right: parent.right; anchors.bottom: parent.bottom; visible: jump.notice.length > 0; glyphName: "refresh"; tip: "Refresh Quick jump"; ink: jump.app.accent; objectName: "quickJumpRefresh"; onClicked: jump.show(true) }
        }
    }
}
