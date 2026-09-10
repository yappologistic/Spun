import QtQuick
import Spun 1.0

SpunSlider {
    id: control
    required property var app
    property alias pointerInput: seekPointer
    enabled: visible && !app.swapRunning && app.deckPlayer.duration > 0 && (!app.useCider || app.ciderService.canSeek)
    property bool scrubbing: false
    property real previewValue: 0
    property real lastPointer: 0
    property real windDirection: 0
    property double lastMove: 0
    property string seekTrack: ""
    previewing: scrubbing
    value: scrubbing ? previewValue : app.trackVisualProgress
    function cancelSeek() {
        scrubbing=false; windDirection=0
        if(app.activeSeekControl===control)app.activeSeekControl=null
    }
    onEnabledChanged: if(!enabled)cancelSeek()
    onVisibleChanged: if(!visible)cancelSeek()
    Component.onDestruction: if(app.activeSeekControl===control)app.activeSeekControl=null
    valueText: app.time(value * app.deckPlayer.duration)
    Accessible.name: "Playback position"
    onMoved: if(!scrubbing)app.deckPlayer.seek(value * app.deckPlayer.duration)
    MouseArea {
        id: seekPointer; objectName: control.objectName + "Pointer"
        anchors.fill: parent; preventStealing: true; acceptedButtons: Qt.LeftButton
        cursorShape: Qt.PointingHandCursor
        function fraction(x) { return Math.max(0, Math.min(1,(x-control.leftPadding)/(control.availableWidth-control.handle.width))) }
        function beginPointer(mouse) {
            control.forceActiveFocus(Qt.MouseFocusReason)
            control.seekTrack=control.app.cassetteTrackIdentity
            control.previewValue=(mouse.modifiers & Qt.ShiftModifier)?control.app.progress:fraction(mouse.x)
            control.lastPointer=mouse.x;control.windDirection=Math.sign(control.previewValue-control.app.progress);control.lastMove=Date.now()
            control.scrubbing=true;control.app.activeSeekControl=control
            if(control.app.cassette)tapeSound.transport()
        }
        function movePointer(mouse) {
            if(!control.scrubbing)return
            const delta=(mouse.x-control.lastPointer)/(control.availableWidth-control.handle.width)
            control.previewValue=Math.max(0,Math.min(1,control.previewValue+delta*((mouse.modifiers & Qt.ShiftModifier)?.1:1)))
            control.lastPointer=mouse.x
            if(delta!==0){control.windDirection=Math.sign(delta);control.lastMove=Date.now()}
        }
        function endPointer() {
            if(!control.scrubbing)return
            const valid=control.enabled && control.seekTrack===control.app.cassetteTrackIdentity
            const position=control.previewValue
            control.cancelSeek()
            if(valid){control.app.deckPlayer.seek(position*control.app.deckPlayer.duration);if(control.app.cassette)tapeSound.transport()}
        }
        function cancelPointer() { control.cancelSeek() }
        onPressed: mouse => beginPointer(mouse)
        onPositionChanged: mouse => { if(pressed)movePointer(mouse) }
        onReleased: endPointer()
        onCanceled: cancelPointer()
        onWheel: wheel => wheel.accepted=false
    }
    Keys.onShortcutOverride: event => { if(event.key===Qt.Key_Escape && scrubbing)event.accepted=true }
    Keys.onEscapePressed: cancelSeek()
    background: Item {
        x: 4; y: 4; width: control.availableWidth; height: control.availableHeight
        readonly property real head: parent.visualPosition * (width - 4) + 2
        opacity: parent.enabled ? 1 : SpunStyle.disabledOpacity
        ProgressRing {
            objectName: control.objectName + "Wave"
            width: Math.max(0, parent.head - 5); height: parent.height
            visible: width > 0
            linear: true; progress: 1
            phase: control.app.wavePhase
            amplitude: control.app.deckPlayer.playing ? 2.8 : 0
            accent: control.app.accent
            Behavior on amplitude { NumberAnimation { duration: SpunStyle.enter } }
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: Math.min(parent.width, parent.head + 5); width: parent.width - x
            height: 4; radius: 2; color: theme.colors.outline
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
            width: 3; height: 3; radius: 1.5; color: control.app.accent
            visible: control.app.progress < .95
        }
    }
}
