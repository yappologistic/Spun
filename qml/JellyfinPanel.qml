import QtQuick
import QtQuick.Controls
import QtQml.Models

Rectangle {
    id: panel
    objectName: "jellyfinPanel"
    required property var app
    readonly property var server: jellyfin.server
    property string searchFilter: "albums"
    property var selected: ({})
    property int selectedIndex: -1
    property string renameId: ""
    property int revision: 0
    property int artworkRevision: 0
    readonly property bool actionsOpen: actions.visible || pageActions.visible || connection.visible || nameDialog.visible || playlistPicker.visible || deleteDialog.visible || folderMenu.visible || qualityMenu.visible
    readonly property bool playlistPage: jellyfin.page === "playlist"
    readonly property string selectedPage: jellyfin.page === "album" ? "albums" : jellyfin.page === "artist" ? "artists" : playlistPage ? "playlists" : jellyfin.page
    color: app.surface
    radius: SpunStyle.panelRadius
    function closeActions() {
        actions.close();
        pageActions.close();
        connection.close();
        nameDialog.close();
        playlistPicker.close();
        deleteDialog.close();
        folderMenu.close();
        qualityMenu.close();
    }
    function focusSearch() {
        if (server.connected) {
            search.forceActiveFocus();
            search.selectAll();
        } else
            connectButton.forceActiveFocus();
    }
    function openMenu(menu, anchor) {
        const point = anchor ? anchor.mapToItem(panel, anchor.width, anchor.height) : Qt.point(panel.width - 16, 156);
        menu.x = Math.max(8, Math.min(panel.width - menu.width - 8, point.x - menu.width));
        menu.y = Math.max(8, Math.min(panel.height - menu.height - 8, point.y));
        menu.open();
    }
    function menuFor(row, index, anchor) {
        selected = row;
        selectedIndex = index;
        openMenu(actions, anchor);
    }
    function settle() {
        entrance.stop();
        opacity = 1;
        offset.x = 0;
    }
    transform: Translate {
        id: offset
    }
    onVisibleChanged: {
        if (!visible)
            closeActions();
        settle();
        if (visible && SpunStyle.motion)
            entrance.start();
    }
    Connections {
        target: SpunStyle
        function onMotionChanged() {
            if (!SpunStyle.motion)
                panel.settle();
        }
    }
    Connections {
        target: jellyfin
        function onChanged() {
            panel.revision++;
        }
        function onArtworkChanged() {
            panel.artworkRevision++;
        }
    }
    ParallelAnimation {
        id: entrance
        NumberAnimation {
            target: panel
            property: "opacity"
            from: 0
            to: 1
            duration: SpunStyle.feedback
            easing.type: Easing.BezierSpline
            easing.bezierCurve: SpunStyle.effectsCurve
        }
        NumberAnimation {
            target: offset
            property: "x"
            from: 12
            to: 0
            duration: SpunStyle.enter
            easing.type: Easing.BezierSpline
            easing.bezierCurve: SpunStyle.enterCurve
        }
    }
    Row {
        id: tabs
        x: 12
        y: 12
        visible: server.connected
        function focusTab(i) {
            tabItems.itemAt(Math.max(0, Math.min(tabItems.count - 1, i))).forceActiveFocus(Qt.TabFocusReason);
        }
        Accessible.role: Accessible.PageTabList
        Accessible.name: "Jellyfin library"
        Repeater {
            id: tabItems
            model: ["albums", "artists", "songs", "playlists"]
            SpunChoiceButton {
                required property string modelData
                required property int index
                objectName: "jellyfinTab_" + modelData
                width: (panel.width - 24) / 4
                text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                ink: panel.app.ink
                mutedInk: panel.app.mutedInk
                accent: panel.app.accent
                selected: panel.selectedPage === modelData
                Accessible.role: Accessible.PageTab
                Accessible.selectable: true
                Accessible.selected: selected
                Keys.onShortcutOverride: event => {
                    if (event.modifiers === Qt.NoModifier && [Qt.Key_Left, Qt.Key_Right, Qt.Key_Home, Qt.Key_End].includes(event.key))
                        event.accepted = true;
                }
                Keys.onLeftPressed: tabs.focusTab(index - 1)
                Keys.onRightPressed: tabs.focusTab(index + 1)
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Home) {
                        tabs.focusTab(0);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_End) {
                        tabs.focusTab(3);
                        event.accepted = true;
                    } else
                        event.accepted = false;
                }
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                onClicked: {
                    panel.searchFilter = modelData;
                    search.text = "";
                    jellyfin.show(modelData);
                }
            }
        }
    }
    SpunText {
        visible: !server.connected
        x: 24
        y: 24
        text: "Jellyfin"
        font.pixelSize: SpunStyle.title
        color: panel.app.ink
    }
    SpunSearchField {
        id: search
        objectName: "jellyfinSearch"
        app: panel.app
        visible: server.connected
        x: 16
        y: 60
        width: parent.width - 32
        height: 44
        maximumLength: 512
        placeholderText: "Search " + panel.searchFilter
        onAccepted: jellyfin.search(text, panel.searchFilter)
        Keys.onDownPressed: {
            list.forceActiveFocus();
            if (list.count)
                list.currentIndex = 0;
        }
    }
    IconButton {
        visible: server.connected
        x: 8
        y: 114
        glyphName: "back"
        tip: "Back"
        ink: panel.app.ink
        enabled: jellyfin.canBack
        onClicked: jellyfin.back()
    }
    SpunText {
        visible: server.connected
        x: 52
        y: 120
        width: parent.width - 104
        height: 28
        verticalAlignment: Text.AlignVCenter
        text: jellyfin.heading
        font.pixelSize: SpunStyle.body
        font.weight: Font.Medium
        color: panel.app.ink
        elide: Text.ElideRight
    }
    IconButton {
        objectName: "jellyfinPageActions"
        x: parent.width - 48
        y: server.connected ? 114 : 16
        glyphName: "more"
        tip: "Library actions"
        ink: panel.app.ink
        onClicked: server.connected ? pageActions.open() : connection.open()
    }
    Column {
        x: 24
        y: 88
        width: parent.width - 48
        spacing: 16
        visible: !server.connected
        SpunText {
            width: parent.width
            text: server.error || "Connect your music library."
            wrapMode: Text.WordWrap
            color: server.error.length ? theme.colors.error : panel.app.mutedInk
            font.pixelSize: SpunStyle.body
        }
        SpunButton {
            id: connectButton
            objectName: "jellyfinConnect"
            text: server.connecting ? "Connecting…" : "Connect"
            enabled: !server.connecting
            tonal: true
            onClicked: connection.open()
        }
        SpunLoading {
            width: parent.width
            visible: server.connecting
            label: "Connecting to Jellyfin"
        }
    }
    SpunLoading {
        objectName: "jellyfinLoading"
        x: 24
        y: 160
        width: parent.width - 48
        visible: server.connected && jellyfin.busy
        label: "Loading music"
    }
    ListView {
        id: list
        objectName: "jellyfinResults"
        x: 8
        y: 164
        width: parent.width - 16
        height: parent.height - y - 56
        visible: server.connected && !jellyfin.error.length
        model: jellyfin.items
        clip: true
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        Keys.onReturnPressed: if (currentIndex >= 0)
            jellyfin.open(jellyfin.items[currentIndex])
        Keys.onEnterPressed: if (currentIndex >= 0)
            jellyfin.open(jellyfin.items[currentIndex])
        delegate: ItemDelegate {
            id: row
            required property var modelData
            required property int index
            objectName: "jellyfinResult_" + index
            width: list.width
            height: 68
            focusPolicy: Qt.StrongFocus
            Accessible.name: modelData.title + ", " + modelData.artist
            highlighted: ListView.isCurrentItem
            background: Rectangle {
                radius: SpunStyle.rowRadius
                color: row.highlighted ? panel.app.inset : "transparent"
                border.width: row.visualFocus ? 2 : 0
                border.color: panel.app.accent
                SpunStateLayer {
                    anchors.fill: parent
                    radius: parent.radius
                    pressed: row.down
                    hovered: row.hovered
                    focused: row.visualFocus
                    color: panel.app.ink
                }
            }
            onClicked: {
                list.currentIndex = index;
                jellyfin.open(modelData);
            }
            Image {
                x: 8
                y: 12
                width: 44
                height: 44
                source: {
                    panel.artworkRevision;
                    return jellyfin.artwork(row.modelData.id || "");
                }
                sourceSize: Qt.size(88, 88)
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                Rectangle {
                    anchors.fill: parent
                    visible: parent.status !== Image.Ready
                    radius: 6
                    color: panel.app.inset
                    Glyph {
                        anchors.centerIn: parent
                        name: "disc"
                        ink: panel.app.mutedInk
                        width: 24
                        height: 24
                    }
                }
            }
            SpunText {
                x: 62
                y: 12
                width: parent.width - 108
                text: row.modelData.title
                color: panel.app.ink
                font.pixelSize: SpunStyle.body
                elide: Text.ElideRight
            }
            SpunText {
                x: 62
                y: 37
                width: parent.width - 108
                text: row.modelData.artist || row.modelData.kind
                color: panel.app.mutedInk
                font.pixelSize: SpunStyle.caption
                elide: Text.ElideRight
            }
            IconButton {
                objectName: "jellyfinRowMenu_" + row.index
                x: parent.width - 44
                y: 12
                glyphName: "more"
                tip: "Item actions"
                ink: panel.app.mutedInk
                onClicked: panel.menuFor(row.modelData, row.index, this)
            }
        }
    }
    Column {
        x: 24
        y: 210
        width: parent.width - 48
        spacing: 12
        visible: server.connected && !jellyfin.busy && (!!jellyfin.error.length || !jellyfin.items.length)
        SpunText {
            width: parent.width
            text: jellyfin.error || "No results"
            wrapMode: Text.WordWrap
            color: jellyfin.error.length ? theme.colors.error : panel.app.mutedInk
            font.pixelSize: SpunStyle.body
        }
        SpunButton {
            text: "Retry"
            visible: !!jellyfin.error.length
            onClicked: jellyfin.reload()
        }
    }
    SpunButton {
        objectName: "jellyfinMore"
        visible: server.connected && jellyfin.more
        enabled: !jellyfin.busy
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height - 48
        text: jellyfin.busy ? "Loading…" : "Load more"
        onClicked: jellyfin.loadMore()
    }
    component Action: MenuItem {
        id: action
        implicitHeight: 40
        font.family: SpunStyle.family
        contentItem: SpunText {
            text: action.text
            color: panel.app.ink
            opacity: action.enabled ? 1 : SpunStyle.disabledOpacity
            font.pixelSize: SpunStyle.body
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 12
            color: "transparent"
            SpunStateLayer {
                anchors.fill: parent
                radius: 12
                color: panel.app.ink
                enabled: action.enabled
                pressed: action.down
                hovered: action.highlighted
                focused: action.visualFocus
            }
        }
    }
    component PopupMenu: Menu {
        width: Math.min(270, panel.width - 32)
        x: panel.width - width - 16
        y: 156
        enter: SpunPopupEnter {}
        exit: Transition {
            NumberAnimation {
                property: "opacity"
                to: 0
                duration: SpunStyle.exit
            }
        }
        background: Rectangle {
            color: SpunStyle.popup
            radius: SpunStyle.popupRadius
        }
    }
    PopupMenu {
        id: pageActions
        Action {
            text: jellyfin.more ? "Play listed songs" : "Play songs"
            enabled: !jellyfin.busy && jellyfin.items.some(row => row.kind === "song")
            onTriggered: jellyfin.playItems(jellyfin.items)
        }
        Action {
            text: "Favorites"
            onTriggered: jellyfin.show("favorites")
        }
        Action {
            text: "Genres"
            onTriggered: jellyfin.show("genres")
        }
        Action {
            text: "Recently played"
            onTriggered: jellyfin.show("recent")
        }
        Action {
            objectName: "jellyfinNewPlaylist"
            text: "New playlist…"
            onTriggered: {
                panel.renameId = "";
                nameField.text = "";
                nameDialog.open();
            }
        }
        Action {
            text: "Rename playlist…"
            visible: panel.playlistPage && !!jellyfin.collection.editable
            height: visible ? 40 : 0
            enabled: !jellyfin.actionBusy
            onTriggered: {
                panel.renameId = jellyfin.collection.remoteId;
                nameField.text = jellyfin.heading;
                nameDialog.open();
            }
        }
        Action {
            text: "Delete playlist…"
            visible: panel.playlistPage && !!jellyfin.collection.deletable
            height: visible ? 40 : 0
            enabled: !jellyfin.actionBusy
            onTriggered: deleteDialog.open()
        }
        Action {
            text: "Refresh"
            enabled: !jellyfin.busy
            onTriggered: jellyfin.reload()
        }
        Action {
            objectName: "jellyfinSettings"
            text: "Server settings…"
            onTriggered: connection.open()
        }
    }
    PopupMenu {
        id: actions
        Action {
            text: panel.selected.kind === "song" ? "Play" : "Open"
            onTriggered: jellyfin.open(panel.selected)
        }
        Action {
            text: "Add to queue"
            enabled: panel.selected.kind === "song"
            onTriggered: jellyfin.enqueue(panel.selected)
        }
        Action {
            text: {
                panel.revision;
                return jellyfin.favorite(panel.selected.id || "") ? "Remove from favorites" : "Add to favorites";
            }
            enabled: !jellyfin.actionBusy
            onTriggered: jellyfin.toggleFavorite(panel.selected)
        }
        Action {
            text: "Add to playlist…"
            enabled: panel.selected.kind === "song"
            onTriggered: playlistPicker.open()
        }
        Action {
            text: "Open album"
            enabled: !!panel.selected.albumId
            onTriggered: jellyfin.open({
                source: "jellyfin",
                server: panel.selected.server,
                kind: "album",
                remoteId: panel.selected.albumId,
                title: panel.selected.album
            })
        }
        Action {
            text: "Open artist"
            enabled: !!panel.selected.artistId
            onTriggered: jellyfin.open({
                source: "jellyfin",
                server: panel.selected.server,
                kind: "artist",
                remoteId: panel.selected.artistId,
                title: panel.selected.artist
            })
        }
        Action {
            text: "Remove from playlist"
            visible: panel.playlistPage && !!jellyfin.collection.editable
            height: visible ? 40 : 0
            enabled: !jellyfin.actionBusy
            onTriggered: jellyfin.removeFromPlaylist(panel.selected)
        }
        Action {
            text: "Move up"
            visible: panel.playlistPage && !!jellyfin.collection.editable
            height: visible ? 40 : 0
            enabled: !jellyfin.actionBusy && panel.selectedIndex > 0
            onTriggered: jellyfin.movePlaylistItem(panel.selectedIndex, panel.selectedIndex - 1)
        }
        Action {
            text: "Move down"
            visible: panel.playlistPage && !!jellyfin.collection.editable
            height: visible ? 40 : 0
            enabled: !jellyfin.actionBusy && panel.selectedIndex >= 0 && panel.selectedIndex < jellyfin.items.length - 1
            onTriggered: jellyfin.movePlaylistItem(panel.selectedIndex, panel.selectedIndex + 1)
        }
        Action {
            text: "Copy link"
            onTriggered: jellyfin.copyLink(panel.selected)
        }
    }
    component LibraryDialog: Dialog {
        id: dialog
        modal: true
        focus: true
        width: Math.min(360, Math.max(280, panel.width - 32))
        x: (panel.width - width) / 2
        y: Math.max(16, Math.min(150, panel.height - height - 16))
        padding: 24
        spacing: 16
        enter: SpunPopupEnter {}
        exit: Transition {
            NumberAnimation {
                property: "opacity"
                to: 0
                duration: SpunStyle.exit
            }
        }
        background: Rectangle {
            color: SpunStyle.popup
            radius: SpunStyle.dialogRadius
        }
        header: SpunText {
            text: dialog.title
            color: panel.app.ink
            font.pixelSize: SpunStyle.title
            font.weight: Font.Medium
            leftPadding: 24
            rightPadding: 24
            topPadding: 24
            wrapMode: Text.WordWrap
        }
        footer: DialogButtonBox {
            buttonLayout: DialogButtonBox.AndroidLayout
            standardButtons: dialog.standardButtons
            alignment: Qt.AlignRight
            spacing: 8
            leftPadding: 24
            rightPadding: 24
            bottomPadding: 24
            background: Item {}
            delegate: SpunButton {
                objectName: dialog.objectName + (DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole ? "Accept" : "Cancel")
                horizontalPadding: 16
            }
        }
    }
    LibraryDialog {
        id: connection
        objectName: "jellyfinConnection"
        title: server.connected ? "Jellyfin settings" : "Connect to Jellyfin"
        standardButtons: Dialog.Close
        onAboutToShow: {
            address.text = server.address;
            username.text = server.username;
            password.text = "";
        }
        onOpened: {
            address.forceActiveFocus();
            address.cursorPosition = 0;
        }
        onClosed: password.text = ""
        contentItem: ScrollView {
            implicitHeight: Math.min(connectionFields.implicitHeight, Math.max(180, panel.height - 190))
            contentWidth: availableWidth
            clip: true
            Column {
                id: connectionFields
                width: connection.availableWidth
                spacing: 12
                SpunSearchField {
                    id: address
                    objectName: "jellyfinAddress"
                    app: panel.app
                    searchIcon: false
                    width: parent.width
                    labelText: "Server URL"
                    Accessible.name: "Jellyfin server URL"
                    enabled: !server.connecting
                    maximumLength: 2048
                    onActiveFocusChanged: if (!activeFocus)
                        cursorPosition = 0
                }
                SpunSearchField {
                    id: username
                    objectName: "jellyfinUsername"
                    app: panel.app
                    searchIcon: false
                    width: parent.width
                    labelText: "Username"
                    enabled: !server.connecting
                    maximumLength: 256
                }
                SpunSearchField {
                    id: password
                    objectName: "jellyfinPassword"
                    app: panel.app
                    searchIcon: false
                    width: parent.width
                    labelText: "Password"
                    echoMode: TextInput.Password
                    enabled: !server.connecting
                    maximumLength: 1024
                    onAccepted: signIn.clicked()
                }
                CheckBox {
                    id: remember
                    objectName: "jellyfinRemember"
                    width: parent.width
                    implicitHeight: Math.max(48, rememberLabel.implicitHeight + 16)
                    text: "Remember connection"
                    enabled: server.keyringAvailable && !server.connecting
                    checked: server.keyringAvailable
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: text
                    padding: 0
                    background: Rectangle {
                        radius: SpunStyle.rowRadius
                        color: "transparent"
                        border.width: remember.visualFocus ? 2 : 0
                        border.color: panel.app.accent
                        SpunStateLayer {
                            anchors.fill: parent
                            radius: parent.radius
                            color: panel.app.ink
                            enabled: remember.enabled
                            pressed: remember.down
                            focused: remember.visualFocus
                            hovered: remember.hovered
                        }
                    }
                    indicator: Rectangle {
                        x: 14
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18
                        height: 18
                        radius: 2
                        color: remember.checked ? panel.app.accent : "transparent"
                        border.width: remember.checked ? 0 : 2
                        border.color: panel.app.mutedInk
                        opacity: remember.enabled ? 1 : SpunStyle.disabledOpacity
                        Behavior on color {
                            ColorAnimation { duration: SpunStyle.feedback }
                        }
                        Glyph {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            name: "check"
                            ink: theme.colors.onAccent
                            visible: remember.checked
                        }
                    }
                    contentItem: SpunText {
                        id: rememberLabel
                        leftPadding: 44
                        rightPadding: 12
                        text: remember.text
                        font.pixelSize: SpunStyle.body
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                        color: panel.app.ink
                        opacity: remember.enabled ? 1 : SpunStyle.disabledOpacity
                    }
                }
                SpunText {
                    visible: server.error.length > 0
                    width: parent.width
                    text: server.error
                    wrapMode: Text.WordWrap
                    color: theme.colors.error
                    font.pixelSize: SpunStyle.body
                }
                SpunButton {
                    id: signIn
                    objectName: "jellyfinSignIn"
                    text: server.connecting ? "Connecting…" : server.connected ? "Reconnect" : "Connect"
                    tonal: true
                    enabled: !server.connecting && address.text.trim().length > 0 && username.text.trim().length > 0
                    onClicked: {
                        server.connectServer(address.text, username.text, password.text, remember.checked);
                        password.text = "";
                    }
                }
                SpunButton {
                    visible: server.connected
                    width: parent.width
                    text: {
                        panel.revision;
                        const f = server.folders.find(row => row.id === server.folder);
                        return f ? f.name : "All music libraries";
                    }
                    onClicked: panel.openMenu(folderMenu, this)
                }
                SpunButton {
                    objectName: "jellyfinQuality"
                    visible: server.connected
                    width: parent.width
                    text: server.bitrate ? server.bitrate + " kbps" : "Original quality"
                    onClicked: panel.openMenu(qualityMenu, this)
                }
                PreferenceSwitch {
                    app: panel.app
                    width: parent.width
                    glyphName: "repeat"
                    visible: server.connected
                    text: "Report playback"
                    checked: server.scrobbling
                    onToggled: server.scrobbling = checked
                }
                SpunButton {
                    visible: server.connected
                    text: "Disconnect"
                    onClicked: {
                        server.disconnectServer();
                        connection.close();
                    }
                }
            }
        }
    }
    PopupMenu {
        id: folderMenu
        Action {
            text: "All music libraries"
            onTriggered: {
                server.folder = "";
                jellyfin.show("albums");
            }
        }
        Instantiator {
            model: server.folders
            delegate: Action {
                required property var modelData
                text: modelData.name
                onTriggered: {
                    server.folder = modelData.id;
                    jellyfin.show("albums");
                }
            }
            onObjectAdded: (index, object) => folderMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => folderMenu.removeItem(object)
        }
    }
    PopupMenu {
        id: qualityMenu
        Instantiator {
            model: [0, 128, 192, 320]
            delegate: Action {
                required property int modelData
                objectName: "jellyfinQuality_" + modelData
                text: modelData ? modelData + " kbps" : "Original quality"
                onTriggered: server.bitrate = modelData
            }
            onObjectAdded: (index, object) => qualityMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => qualityMenu.removeItem(object)
        }
    }
    LibraryDialog {
        id: nameDialog
        objectName: "jellyfinNameDialog"
        title: panel.renameId.length ? "Rename playlist" : "New playlist"
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: {
            nameField.forceActiveFocus();
            nameField.selectAll();
            standardButton(Dialog.Save).text = panel.renameId.length ? "Save" : "Create";
            standardButton(Dialog.Save).enabled = nameField.text.trim().length > 0;
        }
        onAccepted: {
            if (panel.renameId.length)
                jellyfin.renamePlaylist(panel.renameId, nameField.text);
            else
                jellyfin.createPlaylist(nameField.text);
        }
        contentItem: SpunSearchField {
            id: nameField
            objectName: "jellyfinPlaylistName"
            app: panel.app
            searchIcon: false
            labelText: "Playlist name"
            maximumLength: 120
            onTextChanged: if (nameDialog.visible)
                nameDialog.standardButton(Dialog.Save).enabled = text.trim().length > 0
        }
    }
    LibraryDialog {
        id: playlistPicker
        title: "Add to playlist"
        standardButtons: Dialog.Cancel
        contentItem: ListView {
            implicitHeight: Math.min(contentHeight, 220)
            model: server.playlists.filter(row => row.editable)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: SpunButton {
                required property var modelData
                width: playlistPicker.availableWidth
                text: modelData.title
                onClicked: {
                    jellyfin.addToPlaylist(modelData.remoteId, panel.selected);
                    playlistPicker.close();
                }
            }
        }
    }
    LibraryDialog {
        id: deleteDialog
        title: "Delete playlist?"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: standardButton(Dialog.Ok).text = "Delete"
        onAccepted: jellyfin.deletePlaylist(jellyfin.collection.remoteId)
        contentItem: SpunText {
            text: "This deletes the playlist from Jellyfin."
            wrapMode: Text.WordWrap
            color: panel.app.ink
            font.pixelSize: SpunStyle.body
        }
    }
}
