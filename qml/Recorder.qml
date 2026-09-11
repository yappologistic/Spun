import QtQuick
import Spun 1.0

Item {
    id: recorder
    required property var app
    objectName: "recorderFace"
    readonly property var controls: [previous, play, next, quieter, louder, rewind, forward, knob, details, queue, settings]
    readonly property var keyControls: [previous, play, next]
    readonly property var wheelInput: wheelPointer
    readonly property real wheelAngle: wheel.scrubbing ? wheel.baseAngle + (wheel.previewValue-wheel.baseValue)*360 : app.activeSeekControl ? app.trackVisualProgress*360 : app.spinAngle
    function controlAt(point) {
        for(const button of controls) {
            const p=button.mapFromItem(recorder,point.x,point.y)
            if(button.enabled && p.x>=0 && p.y>=0 && p.x<button.width && p.y<button.height)return button.pointerInput
        }
        const p=wheel.mapFromItem(recorder,point.x,point.y)
        return wheel.enabled && wheel.onWheel(p.x,p.y) ? wheelPointer : null
    }
    function cancelInput() { wheel.cancelSeek(); for(const button of controls)button.pointerInput.cancelPointer() }
    onVisibleChanged: if(!visible)cancelInput()
    Connections {
        target: recorder.app
        function onMenuOpenChanged() { if(recorder.app.menuOpen)recorder.cancelInput() }
        function onDiscFlippedChanged() { recorder.cancelInput() }
        function onCassetteTrackIdentityChanged() { recorder.cancelInput() }
    }
    RecorderSurface { anchors.fill: parent }
    RecorderSurface { objectName: "recorderWheelSurface"; anchors.fill: parent; part: 1; artwork: presentation.artwork; transform: Rotation { origin.x: 205; origin.y: 160; angle: recorder.wheelAngle } }
    RecorderSurface { anchors.fill: parent; part: 2 }
    RecorderSurface {
        anchors.fill: parent; part: 3
        transform: Rotation { origin.x: 70; origin.y: 162; axis.x: 1; axis.y: 0; axis.z: 0; angle: 7*(rewind.pressDepth-forward.pressDepth) }
    }
    Rectangle { x: 65; y: 69; width: 10; height: 91; radius: 4; color: "#25323d"; opacity: .18*Math.max(0,forward.pressDepth) }
    Rectangle { x: 65; y: 164; width: 10; height: 91; radius: 4; color: "#25323d"; opacity: .18*Math.max(0,rewind.pressDepth) }
    RecorderDisplay { x: 267; y: 30; width: 52; height: 29; app: recorder.app }
    ProgressRing {
        objectName: "recorderProgress"
        x: 83; y: 38; width: 244; height: 244
        visible: recorder.app.deckPlayer.count>0
        progress: recorder.app.trackVisualProgress; phase: recorder.app.wavePhase
        amplitude: recorder.app.deckPlayer.playing ? 1.1 : 0; accent: recorder.app.accent
        Behavior on amplitude { NumberAnimation { duration: SpunStyle.enter } }
    }
    component HardwareKey: Item {
        id: key
        property string caption
        property string glyphName
        property bool round: false
        property bool rocker: false
        property bool orange: false
        property bool isKnob: false
        property bool side: false
        property bool requiresTrack: true
        property alias pointerInput: input
        property bool held: false
        property bool armed: false
        property real initialY: 0
        property real initialVolume: 0
        property bool keyboardDown: false
        readonly property real pressDepth: Math.max(-.025,Math.min(1,keyTravel.value))
        SpunSpring {
            id: keyTravel
            targetValue: (key.held && key.armed) || key.keyboardDown ? 1 : 0
            epsilon: .005
            animation.spring: 4.5
            animation.damping: .3
            animation.mass: .7
        }
        Timer { id: keyboardRelease; interval: 90; onTriggered: key.keyboardDown=false }
        function keyboardActivate() { if(!enabled || isKnob)return;keyboardDown=true;keyboardRelease.restart();triggered() }
        signal triggered()
        enabled: (!requiresTrack || recorder.app.deckPlayer.count>0) && !recorder.app.discFlipped && !recorder.app.menuOpen
        activeFocusOnTab: true
        Accessible.role: isKnob ? Accessible.Slider : Accessible.Button
        Accessible.name: caption
        Accessible.onPressAction: keyboardActivate()
        Keys.onSpacePressed: keyboardActivate()
        Keys.onReturnPressed: keyboardActivate()
        Item {
            objectName: key.objectName+"Face"
            anchors.fill: parent
            transform: Translate { x: key.side ? -key.pressDepth : 0; y: key.side || key.rocker || key.isKnob ? 0 : 1.8*key.pressDepth }
            scale: key.side || key.rocker || key.isKnob ? 1 : 1-.008*key.pressDepth
        Rectangle {
            anchors.fill: parent; anchors.margins: key.round ? 3 : .7
            anchors.leftMargin: key.side ? 8 : key.round ? 4 : .7; anchors.rightMargin: key.side ? 8 : key.round ? 4 : .7
            radius: key.round ? width/2 : 2
            visible: !key.rocker && !key.isKnob
            border.color: key.activeFocus ? recorder.app.accent : "#7d858b"; border.width: key.activeFocus ? 2 : .45
            gradient: Gradient { GradientStop { position: 0; color: key.held ? "#b8bec4" : "#e3e7ea"; Behavior on color { ColorAnimation { duration: SpunStyle.feedback } } } GradientStop { position: .08; color: "#c9ced2" } GradientStop { position: .94; color: "#b9c0c6" } GradientStop { position: 1; color: "#98a2aa" } }
        }
        Glyph { anchors.horizontalCenter: parent.horizontalCenter; y: key.rocker ? (parent.height-height)/2 : key.round ? (parent.height-height)/2 : 12; width: key.rocker ? 11 : 15; height: width; name: key.glyphName; ink: key.orange ? "#d76d24" : "#414951"; visible: !key.isKnob && !key.side }
        }
        MouseArea {
            id: input
            objectName: key.objectName + "Pointer"
            property string hint: key.caption
            anchors.fill: parent; cursorShape: key.isKnob ? Qt.SizeVerCursor : Qt.PointingHandCursor
            preventStealing: true
            function beginPointer(mouse) { if(!key.enabled){mouse.accepted=false;return};key.forceActiveFocus();key.initialY=mouse.y;key.initialVolume=recorder.app.deckPlayer.volume;key.held=true;key.armed=true }
            function movePointer(mouse) {
                if(!key.held)return
                if(key.isKnob)recorder.app.deckPlayer.volume=Math.max(0,Math.min(1,key.initialVolume+(key.initialY-mouse.y)/150))
                else key.armed=mouse.x>=-4&&mouse.y>=-4&&mouse.x<=width+4&&mouse.y<=height+4
            }
            function endPointer() { const activate=key.held&&key.armed&&key.enabled&&!key.isKnob;cancelPointer();if(activate)key.triggered() }
            function cancelPointer() { key.held=false;key.armed=false;key.keyboardDown=false }
            onPressed: mouse => beginPointer(mouse)
            onPositionChanged: mouse => movePointer(mouse)
            onReleased: endPointer()
            onCanceled: cancelPointer()
            onWheel: wheel => { recorder.app.deckPlayer.volume=Math.max(0,Math.min(1,recorder.app.deckPlayer.volume+wheel.angleDelta.y/2400));wheel.accepted=true }
        }
        property bool hintDismissed: false
        HoverHandler { id: keyHover; onHoveredChanged: if(!hovered)key.hintDismissed=false }
        SpunToolTip {
            visible: keyHover.hovered && !key.held && !key.hintDismissed && !recorder.app.threeDActive && !recorder.app.menuOpen
            text: key.caption; timeout: 2500
            onClosed: key.hintDismissed=true
        }
        onEnabledChanged: if(!enabled)input.cancelPointer()
        Keys.onUpPressed: if(isKnob)recorder.app.deckPlayer.volume=Math.min(1,recorder.app.deckPlayer.volume+.05)
        Keys.onDownPressed: if(isKnob)recorder.app.deckPlayer.volume=Math.max(0,recorder.app.deckPlayer.volume-.05)
    }
    HardwareKey { id: previous; objectName: "recorderPrevious"; x: 86; y: 287; width: 61; height: 84; caption: "Previous track"; glyphName: "previous"; orange: true; enabled: !recorder.app.menuOpen && !recorder.app.discFlipped && (recorder.app.useCider ? recorder.app.ciderService.canPrevious : recorder.app.deckPlayer.count>0); onTriggered: recorder.app.deckPlayer.previous() }
    HardwareKey { id: play; objectName: "recorderPlay"; x: 148; y: 287; width: 61; height: 84; caption: recorder.app.deckPlayer.playing ? "Pause" : "Play"; glyphName: recorder.app.deckPlayer.playing ? "pause" : "play"; onTriggered: recorder.app.deckPlayer.toggle() }
    HardwareKey { id: next; objectName: "recorderNext"; x: 210; y: 287; width: 61; height: 84; caption: "Next track"; glyphName: "next"; enabled: !recorder.app.menuOpen && !recorder.app.discFlipped && (recorder.app.useCider ? recorder.app.ciderService.canNext : recorder.app.deckPlayer.count>0); onTriggered: recorder.app.deckPlayer.next() }
    HardwareKey { id: quieter; requiresTrack: false; objectName: "recorderQuieter"; x: 273; y: 253; width: 28; height: 26; round: true; caption: "Volume down"; glyphName: "minus"; onTriggered: recorder.app.deckPlayer.volume=Math.max(0,recorder.app.deckPlayer.volume-.05) }
    HardwareKey { id: louder; requiresTrack: false; objectName: "recorderLouder"; x: 295; y: 232; width: 28; height: 26; round: true; caption: "Volume up"; glyphName: "plus"; onTriggered: recorder.app.deckPlayer.volume=Math.min(1,recorder.app.deckPlayer.volume+.05) }
    HardwareKey { id: rewind; objectName: "recorderRewind"; x: 55; y: 164; width: 30; height: 91; rocker: true; caption: "Back 10 seconds"; glyphName: "previous"; enabled: wheel.enabled; onTriggered: recorder.app.deckPlayer.seek(Math.max(0,recorder.app.deckPlayer.position-10000)) }
    HardwareKey { id: forward; objectName: "recorderForward"; x: 55; y: 69; width: 30; height: 91; rocker: true; caption: "Forward 10 seconds"; glyphName: "next"; enabled: wheel.enabled; onTriggered: recorder.app.deckPlayer.seek(Math.min(recorder.app.deckPlayer.duration,recorder.app.deckPlayer.position+10000)) }
    HardwareKey { id: knob; requiresTrack: false; objectName: "recorderVolumeKnob"; x: 276; y: 373; width: 37; height: 33; isKnob: true; caption: "Volume · drag up or down" }
    HardwareKey { id: details; objectName: "recorderDetails"; x: 326; y: 82; width: 22; height: 38; side: true; caption: "Album details · F"; onTriggered: recorder.app.flipDisc() }
    HardwareKey { id: queue; requiresTrack: false; objectName: "recorderQueue"; x: 326; y: 133; width: 22; height: 24; side: true; round: true; caption: "Queue"; onTriggered: recorder.app.queueOpen=!recorder.app.queueOpen }
    HardwareKey { id: settings; requiresTrack: false; objectName: "recorderSettings"; x: 326; y: 173; width: 22; height: 24; side: true; round: true; caption: "Player menu"; onTriggered: recorder.app.openSettings() }
    Item {
        id: wheel; objectName: "recorderWheel"
        x: 83; y: 38; width: 244; height: 244
        enabled: recorder.app.deckPlayer.count>0 && recorder.app.deckPlayer.duration>0 && !recorder.app.discFlipped && !recorder.app.menuOpen && (!recorder.app.useCider || recorder.app.ciderService.canSeek)
        property bool scrubbing: false
        property real previewValue: 0
        property real lastFraction: 0
        property real baseAngle: 0
        property real baseValue: 0
        property string track: ""
        activeFocusOnTab: true
        Accessible.role: Accessible.Slider; Accessible.name: "Recorder wheel, playback position"
        function onWheel(x,y) { const r=Math.hypot(x-122,y-122);return r>32&&r<122 }
        function fraction(x,y) { let angle=Math.atan2(y-122,x-122)+Math.PI/2;if(angle<0)angle+=Math.PI*2;return angle/(Math.PI*2) }
        function cancelSeek() { scrubbing=false;if(recorder.app.activeSeekControl===wheel)recorder.app.activeSeekControl=null }
        onEnabledChanged: if(!enabled)cancelSeek()
        Component.onDestruction: if(recorder.app.activeSeekControl===wheel)recorder.app.activeSeekControl=null
        Keys.onLeftPressed: recorder.app.deckPlayer.seek(Math.max(0,recorder.app.deckPlayer.position-5000))
        Keys.onRightPressed: recorder.app.deckPlayer.seek(Math.min(recorder.app.deckPlayer.duration,recorder.app.deckPlayer.position+5000))
        Keys.onEscapePressed: cancelSeek()
        MouseArea {
            id: wheelPointer; objectName: "recorderWheelPointer"
            property string hint: "Drag wheel to seek · Shift for precision"
            anchors.fill: parent; preventStealing: true; cursorShape: wheel.scrubbing ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            containmentMask: QtObject { function contains(point: point): bool { return wheel.onWheel(point.x,point.y) } }
            function beginPointer(mouse) {
                if(!wheel.enabled||!wheel.onWheel(mouse.x,mouse.y)){mouse.accepted=false;return}
                wheel.forceActiveFocus();wheel.track=recorder.app.cassetteTrackIdentity
                wheel.baseAngle=recorder.app.spinAngle;wheel.lastFraction=wheel.fraction(mouse.x,mouse.y)
                wheel.previewValue=(mouse.modifiers&Qt.ShiftModifier)?recorder.app.progress:wheel.lastFraction
                wheel.baseValue=wheel.previewValue;wheel.scrubbing=true;recorder.app.activeSeekControl=wheel
            }
            function movePointer(mouse) {
                if(!wheel.scrubbing)return
                const next=wheel.fraction(mouse.x,mouse.y);let delta=next-wheel.lastFraction
                if(delta>.5)delta-=1;else if(delta<-.5)delta+=1
                wheel.lastFraction=next;wheel.previewValue=Math.max(0,Math.min(1,wheel.previewValue+delta*((mouse.modifiers&Qt.ShiftModifier)? 0.1:1)))
            }
            function endPointer() { const valid=wheel.scrubbing&&wheel.enabled&&wheel.track===recorder.app.cassetteTrackIdentity;const fraction=wheel.previewValue;wheel.cancelSeek();if(valid)recorder.app.deckPlayer.seek(fraction*recorder.app.deckPlayer.duration) }
            function cancelPointer() { wheel.cancelSeek() }
            onPressed: mouse => beginPointer(mouse)
            onPositionChanged: mouse => movePointer(mouse)
            onReleased: endPointer()
            onCanceled: cancelPointer()
            onDoubleClicked: recorder.app.flipDisc()
        }
    }
}
