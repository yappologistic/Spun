import QtQuick
import QtQuick.Controls

Rectangle {
    id: panel
    objectName: "youtubePanel"
    required property var app
    color: app.surface
    radius: SpunStyle.panelRadius
    readonly property bool playlistPage: youtube.page.startsWith("playlist:")
    property var selected: ({})
    property int selectedIndex: -1
    property string renameId: ""
    property string searchFilter: "songs"
    property int revision: 0
    readonly property bool actionsOpen: actions.visible || pageActions.visible || nameDialog.visible || playlistPicker.visible || deleteDialog.visible || historyDialog.visible
    function closeActions(){actions.close();pageActions.close();nameDialog.close();playlistPicker.close();deleteDialog.close();historyDialog.close()}
    function settle() { entrance.stop(); opacity = 1; entranceOffset.x = 0 }
    onVisibleChanged: {
        if (!visible) closeActions()
        settle()
        if (visible && SpunStyle.motion) entrance.start()
    }
    transform: Translate { id: entranceOffset }
    Connections { target: SpunStyle; function onMotionChanged() { if (!SpunStyle.motion) panel.settle() } }
    ParallelAnimation {
        id: entrance
        NumberAnimation { target: panel; property: "opacity"; from: 0; to: 1; duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve }
        NumberAnimation { target: entranceOffset; property: "x"; from: 12; to: 0; duration: SpunStyle.enter; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
    }
    readonly property string selectedPage: playlistPage ? "playlists" : ["favorites", "playlists", "history"].includes(youtube.page) ? youtube.page : "search"
    Connections { target: youtube; function onChanged(){ panel.revision++; list.currentIndex=-1 } }
    function menuFor(row,index) { selected=row; selectedIndex=index; actions.open() }
    function focusSearch() { search.forceActiveFocus(); search.selectAll() }
    function playAll() { youtube.playItems(youtube.items) }

    Rectangle {
        SpunSpring { id: tabMotion; targetValue: 12 + ["search", "favorites", "playlists", "history"].indexOf(panel.selectedPage) * ((panel.width - 24) / 4) }
        x: tabMotion.value; y: 12; width: (panel.width - 24) / 4; height: 36
        radius: 18 * theme.radius; color: SpunStyle.selected
    }
    Row {
        id: tabs
        x: 12; y: 12
        Accessible.role: Accessible.PageTabList
        Accessible.name: "YouTube library"
        function focusTab(index) { tabItems.itemAt(Math.max(0, Math.min(tabItems.count - 1, index))).forceActiveFocus(Qt.TabFocusReason) }
        Repeater {
            id: tabItems
            model: [{name:"Search",page:"search"},{name:"Favorites",page:"favorites"},{name:"Playlists",page:"playlists"},{name:"History",page:"history"}]
            SpunChoiceButton {
                required property var modelData
                required property int index
                objectName: "youtubeTab_" + modelData.page
                width: (panel.width - 24) / 4; text: modelData.name; pill: false
                ink: panel.app.ink; mutedInk: panel.app.mutedInk; accent: panel.app.accent
                selected: panel.selectedPage === modelData.page
                Accessible.role: Accessible.PageTab
                Accessible.selectable: true; Accessible.selected: selected
                Keys.onShortcutOverride: event => {
                    if (event.modifiers === Qt.NoModifier && [Qt.Key_Left, Qt.Key_Right, Qt.Key_Home, Qt.Key_End].includes(event.key)) event.accepted = true
                }
                Keys.onLeftPressed: tabs.focusTab(index - 1)
                Keys.onRightPressed: tabs.focusTab(index + 1)
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Home) { tabs.focusTab(0); event.accepted = true }
                    else if (event.key === Qt.Key_End) { tabs.focusTab(tabItems.count - 1); event.accepted = true }
                    else event.accepted = false
                }
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                onClicked: youtube.show(modelData.page)
            }
        }
    }
    SpunSearchField {
        id: search; objectName: "youtubeSearch"
        app: panel.app; x: 16; y: 60; width: parent.width-32; height: 44
        maximumLength: 2048; placeholderText: "Search or paste a YouTube link"
        onAccepted: youtube.search(text,panel.searchFilter)
    }
    Row {
        x: 16; y: 108; spacing: 2
        Repeater {
            model: [{name:"Songs",filter:"songs"},{name:"Albums",filter:"albums"},{name:"Artists",filter:"artists"},{name:"Playlists",filter:"playlists"}]
            SpunChoiceButton {
                required property var modelData
                width: (panel.width - 38) / 4; height: 36; text: modelData.name; selected: panel.searchFilter===modelData.filter
                ink: panel.app.ink; mutedInk: panel.app.mutedInk; accent: panel.app.accent
                Accessible.role: Accessible.RadioButton; Accessible.checkable: true; Accessible.checked: selected
                onClicked: {panel.searchFilter=modelData.filter;if(search.text.trim().length)youtube.search(search.text,panel.searchFilter)}
            }
        }
    }
    IconButton { x: 8; y: 151; glyphName: "back"; tip: "Back"; ink: panel.app.ink; enabled: youtube.canBack; onClicked: youtube.back() }
    SpunText { x: 52; y: 156; width: parent.width-104; height: 36; text: youtube.heading; color: panel.app.ink; font.pixelSize: SpunStyle.body; font.weight: Font.Medium; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
    IconButton { objectName:"youtubePageActions"; x: parent.width-48; y:151; glyphName:"more"; tip:"Library actions"; ink:panel.app.ink; onClicked:pageActions.open() }
    SpunLoading { objectName: "youtubeLoading"; x: SpunStyle.outerInset; y: 214; width: parent.width - 2 * x; visible: youtube.busy; label: "Loading music" }
    ListView {
        id: list; objectName: "youtubeResults"
        x: 8; y: 200; width: parent.width-16; height: parent.height-y-SpunStyle.inset
        clip: true; spacing: 2; model: youtube.items; visible: !youtube.busy && !youtube.error.length
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        Keys.onReturnPressed: {if(currentIndex>=0)youtube.open(youtube.items[currentIndex])}
        Keys.onEnterPressed: {if(currentIndex>=0)youtube.open(youtube.items[currentIndex])}
        delegate: ItemDelegate {
            id: row
            required property var modelData
            required property int index
            objectName: "youtubeResult_"+index
            width: list.width; height: 68
            Accessible.name: modelData.title + ", " + modelData.artist
            focusPolicy: Qt.StrongFocus
            highlighted: ListView.isCurrentItem
            background: Rectangle {
                border.width: row.visualFocus ? 2 : 0; border.color: panel.app.accent
                radius: SpunStyle.rowRadius; color: row.highlighted ? panel.app.inset : "transparent"
                SpunStateLayer {anchors.fill:parent;radius:12;color:panel.app.ink;pressed:row.down;hovered:row.hovered;focused:row.visualFocus}
            }
            onClicked: {list.currentIndex=index;youtube.open(modelData)}
            Image {
                x: 8; y: 12; width: 44; height: 44
                source: row.modelData.art || ""; asynchronous: true; sourceSize: Qt.size(88,88); fillMode: Image.PreserveAspectCrop
                Rectangle {anchors.fill:parent;visible:parent.status!==Image.Ready;color:panel.app.inset;radius:6;Glyph {anchors.centerIn:parent;name: "disc";ink:panel.app.mutedInk;width:24;height:24}}
            }
            SpunText {x:62;y:12;width:parent.width-108;text:row.modelData.title;color:panel.app.ink;font.pixelSize:SpunStyle.body;elide:Text.ElideRight}
            SpunText {x:62;y:37;width:parent.width-108;text:row.modelData.artist || row.modelData.kind;color:panel.app.mutedInk;font.pixelSize:SpunStyle.caption;elide:Text.ElideRight}
            IconButton {objectName:"youtubeRowMenu_"+row.index;x:parent.width-44;y:12;glyphName:"more";tip:"Song actions";ink:panel.app.mutedInk;onClicked:panel.menuFor(row.modelData,row.index)}
        }
    }
    Column {
        x: 24; y: 230; width: parent.width-48; spacing: 16
        visible: !youtube.busy && (youtube.error.length>0 || youtube.items.length===0)
        SpunText {width:parent.width;text:youtube.error || (youtube.page==="search"?"Find a song, album, artist or playlist. No account needed.":"Nothing here yet.");wrapMode:Text.WordWrap;color:youtube.error.length?theme.colors.error:panel.app.mutedInk;font.pixelSize:SpunStyle.body}
        SpunButton {objectName:"youtubeRetry";text:"Retry";visible:youtube.error.length>0;onClicked:{if(!youtube.ready)youtube.check();else if(search.text.trim().length)youtube.search(search.text,panel.searchFilter);else youtube.show("home")}}
        SpunButton {objectName:"youtubeDiscover";text:"Discover music";visible:!youtube.error.length && youtube.page==="search";onClicked:youtube.show("home")}
    }

    component Action: MenuItem {
        id: action
        implicitHeight: 40
        font.family: SpunStyle.family
        contentItem: SpunText {text:action.text;color:panel.app.ink;opacity:action.enabled?1:SpunStyle.disabledOpacity;font.pixelSize:SpunStyle.body;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
        background:Rectangle {color:"transparent";radius:12;SpunStateLayer {anchors.fill:parent;radius:12;color:panel.app.ink;pressed:action.down;hovered:action.highlighted;focused:action.visualFocus;enabled:action.enabled}}
    }
    Menu {
        id: actions; width: 270
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: SpunStyle.exit } }
        x: 30; y: Math.min(panel.height-height-16,230)
        background: Rectangle {color:SpunStyle.popup;radius:SpunStyle.popupRadius}
        Action {text:"Play";enabled:panel.selected.kind==="song"||panel.selected.kind==="video";onTriggered:youtube.playItem(panel.selected)}
        Action {text:"Add to queue";enabled:panel.selected.kind==="song"||panel.selected.kind==="video";onTriggered:youtube.enqueue(panel.selected)}
        Action {text:{panel.revision;return youtube.favorite(panel.selected.id||"")?"Remove from favorites":"Save to favorites"} visible:panel.selected.kind!=="local-playlist";height:visible?implicitHeight:0;onTriggered:youtube.toggleFavorite(panel.selected)}
        Action {text:"Add to playlist…";enabled:panel.selected.kind==="song"||panel.selected.kind==="video";onTriggered:playlistPicker.open()}
        Action {text:"Song radio";enabled:panel.selected.kind==="song"||panel.selected.kind==="video";onTriggered:youtube.radio(panel.selected)}
        Action {text:"Open album";enabled:!!panel.selected.albumId;onTriggered:youtube.open({id:panel.selected.albumId,kind:"album",title:panel.selected.album})}
        Action {text:"Open artist";enabled:!!panel.selected.artistId;onTriggered:youtube.open({id:panel.selected.artistId,kind:"artist",title:panel.selected.artist})}
        Action {text:"Copy link";visible:panel.selected.kind!=="local-playlist";height:visible?implicitHeight:0;onTriggered:youtube.copyLink(panel.selected)}
        Action {text:"Remove from this playlist";visible:panel.playlistPage && panel.selectedIndex>=0;height:visible?implicitHeight:0;onTriggered:youtube.removeFromPlaylist(youtube.page.slice(9),panel.selectedIndex)}
        Action {text:"Rename playlist…";visible:panel.selected.kind==="local-playlist";height:visible?implicitHeight:0;onTriggered:{panel.renameId=panel.selected.id;nameField.text=panel.selected.title;nameDialog.open()}}
    }
    Menu {
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: SpunStyle.exit } }
        id: pageActions; width: 240; x:panel.width-width-8;y:196
        background:Rectangle {color:SpunStyle.popup;radius:SpunStyle.popupRadius}
        Action {text:"Play all";enabled:youtube.items.some(row=>row.kind==="song"||row.kind==="video");onTriggered:panel.playAll()}
        Action {objectName:"youtubeNewPlaylist";text:"New playlist…";onTriggered:{panel.renameId="";nameField.text="";nameDialog.open()}}
        Action {text:"Rename playlist…";visible:panel.playlistPage;height:visible?implicitHeight:0;onTriggered:{panel.renameId=youtube.page.slice(9);nameField.text=youtube.heading;nameDialog.open()}}
        Action {text:"Delete playlist…";visible:panel.playlistPage;height:visible?implicitHeight:0;onTriggered:deleteDialog.open()}
        Action {text:"Clear history…";visible:youtube.page==="history";height:visible?implicitHeight:0;onTriggered:historyDialog.open()}
    }
    component LibraryDialog: Dialog {
        id: libraryDialog
        padding: SpunStyle.outerInset
        spacing: SpunStyle.inset
        property string acceptText: ""
        enter: SpunPopupEnter {}
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: SpunStyle.exit } }
        onAboutToShow: { if (acceptText.length && standardButton(Dialog.Ok)) standardButton(Dialog.Ok).text = acceptText }
        background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.dialogRadius }
        header: SpunText {
            text: libraryDialog.title
            color: panel.app.ink; font.pixelSize: SpunStyle.title; font.weight: Font.Medium
            leftPadding: SpunStyle.outerInset; rightPadding: SpunStyle.outerInset; topPadding: SpunStyle.outerInset; bottomPadding: 0
            wrapMode: Text.WordWrap
        }
        footer: DialogButtonBox {
            buttonLayout: DialogButtonBox.AndroidLayout
            standardButtons: libraryDialog.standardButtons
            alignment: Qt.AlignRight
            spacing: SpunStyle.gap; leftPadding: SpunStyle.outerInset; rightPadding: SpunStyle.outerInset; topPadding: 0; bottomPadding: SpunStyle.outerInset
            background: Item {}
            delegate: SpunButton { objectName: libraryDialog.objectName + (DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole ? "Accept" : "Cancel"); horizontalPadding: 16 }
        }
    }
    LibraryDialog {
        id:nameDialog; objectName:"youtubeNameDialog"; palette.window:panel.app.surface;palette.windowText:panel.app.ink;palette.text:panel.app.ink;palette.base:panel.app.inset;palette.buttonText:panel.app.accent; title: panel.renameId.length?"Rename playlist":"New playlist";modal:true;focus:true;width:Math.min(320,panel.width-32);x:(panel.width-width)/2;y:Math.max(16,Math.min(180,panel.height-height-16))
        standardButtons:Dialog.Save|Dialog.Cancel
        onOpened:{nameField.forceActiveFocus();nameField.selectAll();standardButton(Dialog.Save).enabled=nameField.text.trim().length>0}
        onAccepted:{if(panel.renameId.length)youtube.renamePlaylist(panel.renameId,nameField.text);else {const id=youtube.createPlaylist(nameField.text);if(playlistPicker.visible){youtube.addToPlaylist(id,panel.selected);playlistPicker.close()}}}
        contentItem:SpunSearchField {app:panel.app;searchIcon:false;id:nameField;objectName:"youtubePlaylistName";color:panel.app.ink;placeholderTextColor:panel.app.mutedInk;maximumLength:100;labelText:"Playlist name";onTextChanged:if(nameDialog.visible)nameDialog.standardButton(Dialog.Save).enabled=text.trim().length>0}
    }
    LibraryDialog {
        id:playlistPicker; palette.window:panel.app.surface;palette.windowText:panel.app.ink;palette.text:panel.app.ink;palette.buttonText:panel.app.accent;title:"Add to playlist";modal:true;focus:true;width:Math.min(320,panel.width-32);x:(panel.width-width)/2;y:Math.max(16,Math.min(180,panel.height-height-16));standardButtons:Dialog.Cancel
        contentItem: Column {
            spacing: 6
            ListView {
                width: playlistPicker.availableWidth; implicitHeight: Math.min(contentHeight, Math.max(80, panel.height-360))
                model: youtube.playlists; clip: true; spacing: 4
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: SpunButton {
                    required property var modelData
                    width: playlistPicker.availableWidth; text: modelData.name
                    onClicked: {youtube.addToPlaylist(modelData.id,panel.selected);playlistPicker.close()}
                }
            }
            SpunButton {text:"New playlist…";onClicked:{panel.renameId="";nameField.text="";nameDialog.open()}}
        }
    }
    LibraryDialog {id:deleteDialog; acceptText:"Delete"; palette.window:panel.app.surface;palette.windowText:panel.app.ink;palette.buttonText:panel.app.accent;title:"Delete this playlist?";modal:true;focus:true;width:Math.min(320,panel.width-32);x:(panel.width-width)/2;y:Math.max(16,Math.min(180,panel.height-height-16));standardButtons:Dialog.Ok|Dialog.Cancel;onAccepted:youtube.deletePlaylist(youtube.page.slice(9));contentItem:SpunText {text:"This removes the saved list from this device.";wrapMode:Text.WordWrap;color:panel.app.ink}}
    LibraryDialog {id:historyDialog; acceptText:"Clear"; palette.window:panel.app.surface;palette.windowText:panel.app.ink;palette.buttonText:panel.app.accent;title:"Clear listening history?";modal:true;focus:true;width:Math.min(320,panel.width-32);x:(panel.width-width)/2;y:Math.max(16,Math.min(180,panel.height-height-16));standardButtons:Dialog.Ok|Dialog.Cancel;onAccepted:youtube.clearHistory();contentItem:SpunText {text:"This clears YouTube listening history stored on this device.";wrapMode:Text.WordWrap;color:panel.app.ink}}
}
