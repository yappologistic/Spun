import QtQuick
import QtQuick.Controls

Popup {
    id: picker
    required property var app
    property var service: null
    property var tracks: []
    property var rows: []
    property var selected: ({})
    property string error: ""
    property bool skipDuplicates: true
    objectName: "savedQueuePicker"
    popupType: Popup.Item; focus: true; modal: true; dim: true; padding: 24
    parent: Overlay.overlay
    x: ((app.layoutWidth || app.width) - width) / 2
    y: ((app.layoutHeight || app.height) - height) / 2
    width: Math.min(360, (app.layoutWidth || app.width) - 32)
    height: Math.min(438, Math.max(310, 252 + rows.length * 62), (app.layoutHeight || app.height) - 48)
    background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.dialogRadius }
    Overlay.modal: Rectangle { color: Qt.alpha("black", .32) }
    enter: SpunPopupEnter {}
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
    function refresh() {
        const result = service.savedQueueChoices()
        rows = result.rows || []; error = result.error || ""; selected = ({})
    }
    function show(songs, source) { tracks = songs.slice(); service = source; skipDuplicates = true; refresh(); open() }
    function selectRow(index) {
        if (index < 0 || index >= rows.length) return
        selected = Object.assign({}, rows[index]); error = ""
    }
    function submit() {
        if (!selected.id || !tracks.length) return
        const result = service.appendSavedQueue(selected, tracks, skipDuplicates)
        if (result.ok) close()
        else error = result.error || "Couldn’t update this saved queue. Try again."
    }
    onOpened: if (contentItem.item) contentItem.item.focusFirst()
    onClosed: { tracks = []; rows = []; selected = ({}); error = "" }
    contentItem: Loader {
        active: picker.visible
        sourceComponent: Item {
            function focusFirst() { closePicker.forceActiveFocus(Qt.TabFocusReason) }
            IconButton { id: closePicker; objectName: "closeSavedQueuePicker"; x: parent.width - 40; y: -8; glyphName: "close"; tip: "Close"; ink: picker.app.mutedInk; onClicked: picker.close() }
            SpunText { text: "Add to saved queue"; color: picker.app.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
            SpunText { y: 30; text: picker.tracks.length + (picker.tracks.length === 1 ? " song selected" : " songs selected"); color: picker.app.mutedInk; font.pixelSize: SpunStyle.caption }
            ListView {
                id: choices; objectName: "savedQueueChoices"
                y: 62; width: parent.width; height: Math.max(60, parent.height - 212)
                clip: true; model: picker.rows; spacing: 4; boundsBehavior: Flickable.StopAtBounds
                currentIndex: -1; activeFocusOnTab: true
                Keys.onReturnPressed: picker.selectRow(currentIndex)
                Keys.onEnterPressed: picker.selectRow(currentIndex)
                Keys.onSpacePressed: picker.selectRow(currentIndex)
                ScrollBar.vertical: ScrollBar {}
                delegate: AbstractButton {
                    id: choice
                    required property int index
                    required property var modelData
                    objectName: "savedQueueChoice" + index
                    width: choices.width; height: 58; hoverEnabled: true
                    Accessible.name: modelData.title + ", " + modelData.trackCount + " tracks"
                    Accessible.checkable: true; Accessible.checked: picker.selected.id === modelData.id
                    onClicked: { choices.currentIndex = index; picker.selectRow(index) }
                    background: Rectangle {
                        border.width: choice.visualFocus || (choices.activeFocus && choices.currentIndex === choice.index) ? 2 : 0
                        border.color: picker.app.accent
                        radius: SpunStyle.rowRadius; color: picker.selected.id === choice.modelData.id ? SpunStyle.selected : "transparent"
                        Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: picker.app.ink; pressed: choice.down; hovered: choice.hovered; focused: choice.visualFocus || (choices.activeFocus && choices.currentIndex === choice.index) }
                    }
                    contentItem: Item {
                        SpunText { height: 20; maximumLineCount: 1; x: 12; y: 8; width: parent.width - 54; text: choice.modelData.title; color: picker.app.ink; font.pixelSize: SpunStyle.body; elide: Text.ElideRight }
                        SpunText { x: 12; y: 31; text: choice.modelData.trackCount + (choice.modelData.trackCount === 1 ? " track" : " tracks"); color: picker.app.mutedInk; font.pixelSize: SpunStyle.caption }
                        Glyph { visible: picker.selected.id === choice.modelData.id; anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; name: "check"; ink: picker.app.accent }
                    }
                }
                SpunText { anchors.centerIn: parent; width: parent.width - 24; visible: !choices.count; text: picker.error.length ? "" : "No saved queues yet. Save one from Queue → ⋯."; color: picker.app.mutedInk; font.pixelSize: SpunStyle.body; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
            }
            CheckBox {
                id: skip; objectName: "skipSavedDuplicates"
                y: choices.y + choices.height + 10; width: parent.width; height: 40
                text: "Skip duplicates"; checked: picker.skipDuplicates; onToggled: picker.skipDuplicates = checked
                spacing: 12; padding: 8; hoverEnabled: true
                Accessible.name: text
                indicator: Rectangle {
                    x: 8; anchors.verticalCenter: parent.verticalCenter; width: 20; height: 20; radius: 4
                    color: skip.checked ? picker.app.accent : "transparent"; border.width: skip.checked ? 0 : 2; border.color: picker.app.mutedInk
                    Behavior on color { ColorAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
                    Glyph { anchors.centerIn: parent; width: 16; height: 16; visible: skip.checked; name: "check"; ink: theme.colors.onAccent }
                }
                contentItem: SpunText { leftPadding: 32; text: skip.text; color: picker.app.ink; font.pixelSize: SpunStyle.body; verticalAlignment: Text.AlignVCenter }
                background: SpunStateLayer { radius: SpunStyle.rowRadius; color: picker.app.ink; pressed: skip.down; hovered: skip.hovered; focused: skip.visualFocus }
            }
            SpunText { objectName: "savedQueueError"; y: skip.y + skip.height + 4; width: parent.width - 44; height: 42; text: picker.error; color: theme.colors.error; font.pixelSize: SpunStyle.caption; wrapMode: Text.WordWrap; elide: Text.ElideRight }
            IconButton { objectName: "refreshSavedQueueChoices"; visible: picker.error.length > 0; x: parent.width - width; y: skip.y + skip.height + 4; glyphName: "refresh"; tip: "Refresh saved queues"; ink: picker.app.accent; onClicked: picker.refresh() }
            SpunButton { objectName: "cancelSavedQueuePicker"; x: parent.width - 188; y: parent.height - 40; width: 80; text: "Cancel"; onClicked: picker.close() }
            SpunButton { objectName: "appendSavedQueueConfirm"; x: parent.width - width; y: parent.height - 40; width: 100; tonal: true; text: "Add songs"; enabled: !!picker.selected.id && !picker.error.length; onClicked: picker.submit() }
        }
    }
}
