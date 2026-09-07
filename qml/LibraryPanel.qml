import QtQuick
import QtQuick.Controls

Rectangle {
    id: panel
    required property var app
    property var browser: library
    readonly property bool detail: Object.keys(browser.collection).length > 0
    readonly property bool linkPreview: browser.section === "search" && browser.query.trim().startsWith("https://music.apple.com/")
    readonly property bool actionsOpen: trackMenu.visible
    function closeActions() { trackMenu.close() }
    function showActions(item, anchor) {
        trackMenu.selection = Object.assign({}, item)
        const point = anchor.mapToItem(panel, anchor.width, anchor.height)
        trackMenu.x = Math.max(8, Math.min(panel.width - trackMenu.width - 8, point.x - trackMenu.width))
        trackMenu.y = Math.max(8, Math.min(panel.height - trackMenu.height - 8, point.y))
        trackMenu.open()
    }
    TrackMenu { id: trackMenu; objectName: "browserTrackMenu"; app: panel.app }
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
        NumberAnimation { target: panel; property: "opacity"; from: 0; to: 1; duration: panel.feedbackTime }
        NumberAnimation { target: entranceOffset; property: "x"; from: 12; to: 0; duration: panel.transitionTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
    }
    ParallelAnimation {
        id: listEntrance
        NumberAnimation { target: list; property: "opacity"; from: 0; to: 1; duration: panel.feedbackTime }
        NumberAnimation { target: listOffset; property: "x"; from: panel.detail ? 12 : 0; to: 0; duration: panel.transitionTime; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
    }
    color: app.surface; radius: SpunStyle.panelRadius
    width: 310
    objectName: "libraryPanel"
    function focusSearch() { Qt.callLater(function() { const field = panel.detail ? collectionSearch : search; field.forceActiveFocus(); field.selectAll() }) }
    function activate(index) {
        if (index < 0) return
        if (!detail) savedY = list.contentY
        browser.open(index)
        if (detail) list.forceActiveFocus()
    }
    Connections {
        target: panel.browser
        function onItemsChanging(append) { panel.closeActions(); panel.appending = append; panel.pageY = append ? list.contentY : 0; panel.pageIndex = append ? list.currentIndex : -1 }
        function onItemsChanged() { Qt.callLater(function() { list.contentY = panel.pageY; list.currentIndex = panel.pageIndex; if (!panel.appending && panel.app.animate && panel.visible && list.count) listEntrance.restart() }) }
        function onReturned() { Qt.callLater(function() { list.contentY = panel.savedY; list.forceActiveFocus() }) }
    }
    component SmallButton: AbstractButton {
        id: control
        property bool selected: false
        property bool pill: true
        implicitHeight: 36
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true
        Accessible.name: text
        background: Rectangle {
            radius: 18 * theme.radius
            color: "transparent"
            Rectangle {
                anchors.fill: parent; radius: parent.radius; color: SpunStyle.selected; opacity: control.selected && control.pill ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: SpunStyle.feedback } }
            }
            SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: panel.app.ink; enabled: control.enabled; pressed: control.down; focused: control.visualFocus; hovered: control.hovered }
            border.width: control.visualFocus ? 2 : 0
            border.color: panel.app.accent
        }
        contentItem: SpunText {
            text: control.text; color: control.selected ? panel.app.accent : panel.app.mutedInk
            font.family: SpunStyle.family; font.pixelSize: SpunStyle.body; font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            opacity: control.enabled ? control.down ? .65 : 1 : .4
            Behavior on color { ColorAnimation { duration: panel.feedbackTime } }
            Behavior on opacity { NumberAnimation { duration: panel.feedbackTime } }
        }
    }
    Rectangle {
        visible: !panel.detail
        SpunSpring { id: sectionMotion; targetValue: 20 + ["search", "songs", "albums", "playlists"].indexOf(panel.browser.section === "recent" ? "songs" : panel.browser.section) * 68 }
        x: sectionMotion.value
        y: 16; width: 66; height: 36; radius: 18 * theme.radius; color: SpunStyle.selected
    }
    Row {
        visible: !panel.detail
        x: 20; y: 16; spacing: 2
        Repeater {
            model: ["search", "songs", "albums", "playlists"]
            SmallButton {
                required property string modelData
                objectName: "libraryTab_" + modelData
                pill: false; width: 66; text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                selected: panel.browser.section === modelData || (modelData === "songs" && panel.browser.section === "recent")
                onClicked: { panel.browser.section = modelData; search.forceActiveFocus() }
            }
        }
    }
    SpunSearchField {
        app: panel.app
        id: search
        objectName: "librarySearchInput"
        visible: !panel.detail
        x: SpunStyle.outerInset; y: 64; width: parent.width - 2 * SpunStyle.outerInset; height: SpunStyle.target
        text: panel.browser.query
        onTextEdited: panel.browser.query = text
        maximumLength: 2048
        placeholderText: panel.browser.section === "search" ? "Search or paste a link" : panel.browser.section === "recent" ? "Search recently played" : "Search your " + panel.browser.section
        Accessible.name: placeholderText
        IconButton {
            visible: search.text.length > 0 || panel.browser.section !== "search"
            x: parent.width - SpunStyle.target; y: 0
            glyphName: search.text.length ? "close" : "refresh"; tip: search.text.length ? "Clear search" : "Refresh library"; ink: panel.app.mutedInk; hoverFill: panel.app.hoverFill
            onClicked: { if (search.text.length) panel.browser.query = ""; else panel.browser.reload(); search.forceActiveFocus() }
        }
        onAccepted: { panel.browser.reload(); list.forceActiveFocus() }
        Keys.onDownPressed: { list.forceActiveFocus(); if (list.count) list.currentIndex = 0 }
    }
    Row {
        visible: !panel.detail && !panel.linkPreview && panel.browser.section === "search"
        x: SpunStyle.outerInset; y: 112; spacing: SpunStyle.smallGap
        Repeater {
            model: ["songs", "albums", "playlists"]
            SmallButton {
                required property string modelData
                objectName: "libraryKind_" + modelData
                width: 84; height: 32; text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                selected: panel.browser.kind === modelData
                onClicked: panel.browser.kind = modelData
            }
        }
    }
    Row {
        visible: !panel.detail && (panel.browser.section === "songs" || panel.browser.section === "recent")
        x: SpunStyle.outerInset; y: 112; spacing: SpunStyle.smallGap
        SmallButton { objectName: "allSongsTab"; width: 100; height: 32; text: "All songs"; selected: panel.browser.section === "songs"; onClicked: panel.browser.section = "songs" }
        SmallButton { objectName: "recentSongsTab"; width: 155; height: 32; text: "Recently played"; selected: panel.browser.section === "recent"; onClicked: panel.browser.section = "recent" }
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
        placeholderText: "Search tracks"
        Accessible.name: "Search this collection"
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
        visible: panel.detail; width: parent.width; height: 144
        IconButton {
            objectName: "libraryBack"
            x: 16; y: 14; glyphName: "back"; tip: "Back · Esc"
            ink: panel.app.ink; hoverFill: panel.app.hoverFill
            onClicked: panel.browser.back()
        }
        SpunText {
            x: 64; y: 20; width: parent.width - x - SpunStyle.outerInset; height: 28
            text: panel.browser.collection.title || ""; color: panel.app.ink
            font.family: SpunStyle.family; font.pixelSize: SpunStyle.heading; elide: Text.ElideRight
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
            x: 101; y: 94; width: 87; height: 34
            text: panel.browser.starting ? "Starting…" : "Play"
            enabled: !!panel.browser.collection.playable && !panel.browser.starting
            selected: true
            onClicked: panel.browser.playCollection()
        }
        SpunText {
            x: 195; y: 104; width: 52; horizontalAlignment: Text.AlignRight
            text: panel.browser.collection.trackCount >= 0 ? panel.browser.collection.trackCount + " tracks" : ""
            color: panel.app.mutedInk; font.pixelSize: SpunStyle.caption; elide: Text.ElideRight
        }
    }
    IconButton {
        objectName: "collectionActions"
        visible: panel.detail; enabled: !!panel.browser.collection.playable
        x: parent.width - SpunStyle.outerInset - SpunStyle.target; y: 92
        glyphName: "more"; tip: "Collection actions"; ink: panel.app.mutedInk; hoverFill: panel.app.hoverFill
        onClicked: panel.showActions(panel.browser.collection, this)
    }
    ListView {
        id: list
        objectName: "libraryList"
        x: SpunStyle.inset; y: panel.detail ? collectionSearch.y + collectionSearch.height + SpunStyle.gap : (!panel.linkPreview && panel.browser.section === "search") || panel.browser.section === "songs" || panel.browser.section === "recent" ? 152 : search.y + search.height + SpunStyle.gap
        width: parent.width - 2 * SpunStyle.inset; height: footer.y - SpunStyle.gap - y
        model: panel.browser.items; clip: true; spacing: 4
        transform: Translate { id: listOffset }
        currentIndex: -1; boundsBehavior: Flickable.StopAtBounds
        onCountChanged: if (currentIndex >= count) currentIndex = -1
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                const row = itemAtIndex(currentIndex)
                if (row) panel.showActions(row.modelData, row)
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (panel.detail) panel.browser.play(currentIndex); else panel.activate(currentIndex)
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
            objectName: "libraryRow" + index
            width: list.width; height: SpunStyle.trackHeight
            enabled: !song || (modelData.playable && !panel.browser.starting)
            HoverHandler { id: rowHover }
            Accessible.name: (song ? "Play " : "Open ") + modelData.title + ", " + modelData.artist
            onClicked: { list.currentIndex = index; if (panel.detail) panel.browser.play(index); else panel.activate(index) }
            background: Rectangle {
                radius: SpunStyle.rowRadius
                color: "transparent"
                SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: panel.app.ink; enabled: row.enabled; pressed: row.down; focused: list.activeFocus && list.currentIndex === row.index; hovered: rowHover.hovered }
            }
            contentItem: TrackContent {
                app: panel.app; opacity: row.enabled ? 1 : .4
                title: row.modelData.title
                subtitle: row.modelData.artist || (row.modelData.trackCount >= 0 ? row.modelData.trackCount + " tracks" : "Playlist")
                artwork: row.modelData.artwork || ""; artworkName: "libraryArtwork" + row.index
                fallback: row.song || row.modelData.type.endsWith("albums") ? "disc" : "queue"
            }
            IconButton {
                id: rowActions
                objectName: "libraryActions" + row.index
                x: parent.width - SpunStyle.target; anchors.verticalCenter: parent.verticalCenter
                enabled: !!row.modelData.playable && !panel.app.actionService.busy
                glyphName: "more"; tip: "Track actions"; ink: panel.app.mutedInk; hoverFill: panel.app.hoverFill
                onClicked: { list.currentIndex = row.index; panel.showActions(row.modelData, this) }
            }
            TapHandler { acceptedButtons: Qt.RightButton; onTapped: panel.showActions(row.modelData, rowActions) }
            Keys.onPressed: event => { if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) { panel.showActions(row.modelData, rowActions); event.accepted = true } }
            SpunToolTip { visible: row.hovered && !rowActions.hovered && !trackMenu.visible && row.visible && !row.down; text: row.modelData.title + (row.modelData.artist ? "\n" + row.modelData.artist : "") + (row.song && !row.modelData.playable ? "\nUnavailable" : "") }
        }
        SpunText {
            anchors.centerIn: parent; width: 222
            visible: list.count === 0
            text: panel.browser.busy ? "Loading…" : panel.browser.error || (panel.browser.section === "search" && !panel.browser.query.trim().length && !panel.detail ? "" : (panel.detail ? panel.browser.collectionQuery.trim().length : panel.browser.query.trim().length) ? "No matches" : panel.detail ? "No tracks available" : panel.browser.section === "recent" ? "No recently played songs" : "No " + panel.browser.section + " in your library")
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: panel.app.mutedInk; font.pixelSize: 12; lineHeight: 1.3
        }
    }
    Item {
        id: footer
        objectName: "libraryFooter"
        x: SpunStyle.outerInset; y: parent.height - height - SpunStyle.inset; width: parent.width - 2 * SpunStyle.outerInset; height: 48
        SpunText {
            anchors.fill: parent
            visible: panel.browser.playError.length > 0
            text: panel.browser.playError; color: theme.colors.error
            font.pixelSize: SpunStyle.caption; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter
        }
        SmallButton {
            objectName: "libraryMore"
            anchors.centerIn: parent; width: parent.width; height: SpunStyle.target
            visible: !panel.browser.playError.length && (panel.browser.hasMore || panel.browser.error.length > 0)
            enabled: !panel.browser.busy
            text: panel.browser.busy ? ((panel.detail && panel.browser.collectionQuery.length) || (panel.browser.section === "recent" && panel.browser.query.length) ? "Searching tracks…" : "Loading…") : panel.browser.needsConnection ? "Connection settings" : panel.browser.error.length ? ((panel.detail && panel.browser.collectionQuery.length) || (panel.browser.section === "recent" && panel.browser.query.length) ? "Search incomplete · Retry" : "Try again") : "Load more"
            selected: true
            onClicked: {
                if (panel.browser.needsConnection) panel.app.toggleQueue()
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
