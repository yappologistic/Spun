import QtQuick
import QtQuick.Controls

Popup {
    id: picker
    required property var app
    required property var preferences
    objectName: "fontPicker"
    popupType: Popup.Item
    focus: true
    modal: true; dim: false
    width: Math.min(352, (app.layoutWidth || app.width) - 24)
    height: Math.min(Math.max(424, preferences.height), (app.layoutHeight || app.height) - 24)
    padding: 12
    parent: app.contentItem
    x: Math.max(12, preferences.x)
    y: Math.max(12, preferences.y + preferences.height - height)
    font.family: SpunStyle.family
    background: Rectangle { color: SpunStyle.popup; radius: SpunStyle.popupRadius }
    enter: SpunPopupEnter {}
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: SpunStyle.exit; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve } }
    readonly property var matches: {
        const query = search.text.trim().toLocaleLowerCase()
        return typography.families.filter(family => family.toLocaleLowerCase().includes(query))
    }
    function choose(family) { if (typography.select(family)) close() }
    onAboutToShow: { typography.loadFamilies(); search.text = ""; fonts.currentIndex = -1 }
    onOpened: search.forceActiveFocus()
    contentItem: Item {
        SpunText { x: 12; y: 10; text: "Font"; color: picker.app.ink; font.pixelSize: SpunStyle.heading; font.weight: Font.Medium }
        IconButton { objectName: "closeFontPicker"; anchors.right: parent.right; glyphName: "close"; tip: "Close font picker"; ink: picker.app.ink; onClicked: picker.close() }
        SpunSearchField {
            id: search
            objectName: "fontSearch"
            app: picker.app
            x: 0; y: 48; width: parent.width
            placeholderText: "Search fonts"
            onTextChanged: fonts.currentIndex = -1
            Keys.onDownPressed: if (fonts.count) { fonts.currentIndex = 0; fonts.forceActiveFocus() }
            onAccepted: if (fonts.count) picker.choose(picker.matches[0])
            IconButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; visible: search.text.length > 0; glyphName: "close"; tip: "Clear search"; ink: picker.app.mutedInk; onClicked: { search.clear(); search.forceActiveFocus() } }
        }
        ItemDelegate {
            id: systemChoice
            objectName: "systemFontChoice"
            y: search.y + search.height + 8; width: parent.width; height: SpunStyle.target
            text: "System default"
            Accessible.role: Accessible.RadioButton
            Accessible.checkable: true; Accessible.checked: typography.selectedFamily.length === 0
            font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
            onClicked: picker.choose("")
            contentItem: SpunText { text: systemChoice.text; font: systemChoice.font; color: picker.app.ink; verticalAlignment: Text.AlignVCenter; rightPadding: 32; elide: Text.ElideRight }
            background: Rectangle {
                radius: SpunStyle.rowRadius; color: typography.selectedFamily.length === 0 ? SpunStyle.selected : "transparent"
                border.width: systemChoice.visualFocus ? 2 : 0; border.color: picker.app.accent
                SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: picker.app.ink; hovered: systemChoice.hovered; pressed: systemChoice.down; focused: systemChoice.visualFocus }
            }
            Glyph { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; visible: typography.selectedFamily.length === 0; name: "check"; ink: picker.app.accent }
        }
        SpunText {
            id: missing
            x: 12; y: systemChoice.y + systemChoice.height + 4; width: parent.width - 24
            visible: typography.missing
            height: visible ? implicitHeight : 0
            text: "Saved font unavailable. Using " + typography.systemFamily + "."
            font.pixelSize: SpunStyle.caption; color: picker.app.mutedInk
            wrapMode: Text.WordWrap
        }
        ListView {
            id: fonts
            objectName: "fontList"
            x: 0; y: missing.y + missing.height + 8
            width: parent.width; height: parent.height - y
            clip: true; reuseItems: true
            model: picker.matches
            currentIndex: -1
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            Keys.onReturnPressed: if (currentIndex >= 0) picker.choose(picker.matches[currentIndex])
            Keys.onEnterPressed: if (currentIndex >= 0) picker.choose(picker.matches[currentIndex])
            Keys.onUpPressed: event => { if (currentIndex <= 0) { currentIndex = -1; search.forceActiveFocus() } else { currentIndex--; positionViewAtIndex(currentIndex, ListView.Contain) } }
            delegate: ItemDelegate {
                id: row
                required property string modelData
                required property int index
                objectName: "fontRow" + index
                width: fonts.width; height: SpunStyle.target
                text: modelData
                highlighted: fonts.activeFocus && fonts.currentIndex === index
                readonly property bool selected: typography.selectedFamily === modelData
                font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
                Accessible.name: modelData
                Accessible.role: Accessible.RadioButton
                Accessible.checkable: true; Accessible.checked: selected
                onClicked: picker.choose(modelData)
                contentItem: SpunText { text: row.text; color: picker.app.ink; font: row.font; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; rightPadding: 32 }
                background: Rectangle {
                    radius: SpunStyle.rowRadius; color: row.selected ? SpunStyle.selected : "transparent"
                    border.width: row.highlighted || row.visualFocus ? 2 : 0; border.color: picker.app.accent
                    SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: picker.app.ink; hovered: row.hovered; pressed: row.down; focused: row.highlighted || row.visualFocus }
                }
                Glyph { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; visible: row.selected; name: "check"; ink: picker.app.accent }
            }
            SpunText { anchors.centerIn: parent; visible: fonts.count === 0; text: "No matching fonts"; color: picker.app.mutedInk; font.pixelSize: SpunStyle.body }
        }
    }
}
