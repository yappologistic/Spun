import QtQuick

// Lightweight geometry with a bounded stylus gesture and damped return motion.
Item {
    id: arm
    objectName: "vinylTonearm"
    required property var app
    readonly property bool motion: app.animate && visible && app.visible && native.exposed
    readonly property bool engaged: visible && app.deckPlayer.count > 0 && ((app.deckPlayer.playing && !app.swapRunning) || landing)
    readonly property bool canSeek: visible && app.deckPlayer.duration > 0 && (!app.useCider || (app.ciderService.canSeek && !app.listeningService.busy))
    property bool dragging: false
    property real dragAngle: -4
    property real rawAngle: -4
    property real lastPointerAngle: 0
    property real pointerDistance: 210
    property string dragTrack: ""
    property bool wasPlaying: false
    property bool landing: false
    property real landingProgress: 0
    readonly property real previewProgress: Math.max(0, Math.min(1, (rawAngle - 6) / 20))
    property real lowered: dragging ? .12 : engaged ? 1 : 0
    property real groove: landing ? landingProgress : Math.max(0, Math.min(1, app.recordProgress))
    property real armAngle: dragging ? dragAngle : engaged ? 6 + 20 * groove : -4
    Behavior on armAngle {
        enabled: arm.motion && !arm.dragging
        SpringAnimation { id: angleSpring; spring: 6; damping: .55; epsilon: .01 }
    }
    function cancelDrag(resume) {
        if (!dragging) return
        dragging = false; landing = false
        if (resume && wasPlaying && dragTrack === app.cassetteTrackIdentity) app.deckPlayer.play()
    }
    function dropNeedle() {
        if (!dragging) return
        const valid = canSeek && dragTrack === app.cassetteTrackIdentity && rawAngle >= 4 && rawAngle <= 28 && pointerDistance > 120 && pointerDistance < 290
        const position = previewProgress
        if (!valid) { dragging = false; return }
        landingProgress = position; landing = true; landingTimeout.restart()
        dragging = false
        if (app.recordMap) {
            if (!app.dropRecordNeedle(position)) { landing = false; landingTimeout.stop() }
        } else {
            app.deckPlayer.seek(Math.min(app.deckPlayer.duration - 1, app.deckPlayer.duration * position))
            app.deckPlayer.play()
        }
    }
    function confirmLanding() {
        if (landing && app.deckPlayer.playing && Math.abs(app.recordProgress - landingProgress) < .025) {
            landing = false; landingTimeout.stop()
        }
    }
    onVisibleChanged: if (!visible) cancelDrag(true)
    onCanSeekChanged: if (!canSeek) cancelDrag(false)
    Timer { id: landingTimeout; interval: 1200; onTriggered: arm.landing = false }
    Connections {
        target: arm.app
        function onCassetteTrackIdentityChanged() { arm.cancelDrag(false); arm.landing = false }
        function onRecordKeyChanged() { arm.cancelDrag(true); arm.landing = false }
    }
    Connections {
        target: arm.app.deckPlayer
        function onPositionChanged() { arm.confirmLanding() }
        function onPlayingChanged() { arm.confirmLanding() }
    }
    readonly property color gold: Qt.tint("#c6a25a", Qt.alpha(app.accent, .12))
    readonly property color glint: Qt.lighter(gold, 1.5)
    readonly property color shade: Qt.darker(gold, 1.6)
    Behavior on lowered {
        enabled: arm.motion && !arm.dragging
        NumberAnimation { id: liftMotion; duration: SpunStyle.hero; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.standardCurve }
    }
    onMotionChanged: if (!motion) {
        liftMotion.complete(); angleSpring.complete()
    }
    SpunToolTip {
        objectName: "needleToolTip"
        parent: arm; x: 184; y: 60
        visible: arm.canSeek && arm.app.visible && (arm.dragging || (!needleHit.hintDismissed && needleHit.hoveringNeedle) || (needleHit.activeFocus && !needleHit.pointerFocus))
        delay: arm.dragging ? 0 : 650
        text: {
            const target = arm.app.recordTarget(arm.previewProgress)
            if (arm.dragging && target) return target.track.title + " · " + arm.app.time(target.position)
            if (arm.dragging) return arm.app.time(arm.previewProgress * arm.app.deckPlayer.duration)
            return arm.app.recordMap ? "Drag to choose an album track · Shift for precision" : "Drag to seek · Shift for precision"
        }
    }
    Rectangle {
        x: 377; y: 105; width: 14; height: 35; radius: 5
        visible: arm.app.bodyVisible
        color: arm.app.surface; border.width: 1; border.color: arm.shade
        rotation: -4
    }
    // The pivot rests at the record's edge; the stylus stays outside the label.
    Item {
        x: 378; y: 100
        Rectangle { x: -21; y: -17; width: 42; height: 42; radius: 21; color: "#80000000" }
        Rectangle {
            x: -19; y: -19; width: 38; height: 38; radius: 19
            gradient: Gradient { GradientStop { position: 0; color: "#a0a5a5" } GradientStop { position: .12; color: "#555c5e" } GradientStop { position: .6; color: "#252a2e" } GradientStop { position: 1; color: "#101619" } }
            border.width: .8; border.color: "#747b7b"
            Rectangle { anchors.centerIn: parent; width: 31; height: 31; radius: 15.5; color: "#252b2d"; border.width: 1; border.color: "#121718" }
        }
        Rectangle {
            x: -13; y: -13; width: 26; height: 26; radius: 13
            color: arm.app.surface; border.width: 1; border.color: arm.shade
        }
        Item {
            id: shaft
            objectName: "tonearmShaft"
            rotation: arm.armAngle
            transformOrigin: Item.TopLeft
            Rectangle { visible: arm.app.bodyVisible; x: -3; y: -47; width: 6; height: 39; radius: 2; color: arm.shade }
            Rectangle {
                objectName: "tonearmCounterweight"
                visible: arm.app.bodyVisible
                x: -15; y: -45; width: 30; height: 27; radius: 5
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#242b2e" }
                    GradientStop { position: .16; color: "#737b7d" }
                    GradientStop { position: .34; color: "#c0c6c6" }
                    GradientStop { position: .65; color: "#6e7678" }
                    GradientStop { position: 1; color: "#1c2326" }
                }
                border.width: .7; border.color: "#7a8486"
                Repeater {
                    model: 7
                    Rectangle { required property int index; x: 2; y: 3+index*3; width: 26; height: .6; color: "#70000000" }
                }
                Rectangle { x: 1; y: 23; width: 28; height: 3; radius: 1; color: "#262d30" }
            }
            // A displaced hard shadow makes the lift legible without a blur pass.
            Rectangle { x: 1 + (1 - arm.lowered) * 2; y: 5; width: 7; height: 187; radius: 3.5; color: "#48000000" }
            Rectangle { x: (1 - arm.lowered) * 2; y: 188; width: 14; height: 24; radius: 4; color: "#48000000" }
            Rectangle {
                x: -7; y: -14; width: 14; height: 22; radius: 5
                gradient: Gradient {
                    GradientStop { position: 0; color: arm.shade }
                    GradientStop { position: .4; color: arm.glint }
                    GradientStop { position: 1; color: arm.gold }
                }
            }
            Rectangle {
                x: -3; y: 0; width: 6; height: 187; radius: 3
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: arm.shade }
                    GradientStop { position: .36; color: arm.glint }
                    GradientStop { position: .65; color: arm.gold }
                    GradientStop { position: 1; color: arm.shade }
                }
            }
            Rectangle { x: -4; y: 178; width: 8; height: 10; radius: 2; color: arm.gold }
            Rectangle {
                x: -7; y: 185; width: 14; height: 24; radius: 4
                color: Qt.tint(arm.app.surface, Qt.alpha(arm.gold, .15))
                border.width: 1; border.color: arm.gold
                Rectangle { x: 4; y: 5; width: 6; height: 2; radius: 1; color: arm.gold }
                Rectangle { x: 4; y: 10; width: 6; height: 2; radius: 1; color: arm.gold }
                Rectangle { x: 2; y: 18; width: 10; height: 5; radius: 1; color: "#171b1d" }
                Rectangle { x: 6; y: 23; width: 1.2; height: 5; radius: .6; color: "#d5dcde" }
                Rectangle { x: 5.5; y: 27; width: 2; height: 1.5; radius: .5; color: "#d6c5a8" }
                Rectangle { x: 12; y: 4; width: 9; height: 2; radius: 1; rotation: -20; color: arm.gold }
                Repeater {
                    model: 2
                    Rectangle { required property int index; x: 2+index*8; y: 2; width: 2; height: 2; radius: 1; color: "#d1d4ca" }
                }
            }

        }
        Rectangle { x: -7; y: -7; width: 14; height: 14; radius: 7; color: arm.gold; border.width: 1; border.color: arm.glint }
        Rectangle { x: -3; y: -1; width: 6; height: 2; radius: 1; rotation: -35; color: arm.shade }
    }
    HoverHandler { id: needleHover; blocking: false; onHoveredChanged: if (!hovered) needleHit.hintDismissed = false }
    // Pointer coordinates must stay fixed while the shaft rotates beneath them.
    MouseArea {
        id: needleHit; objectName: "needleHandle"
        anchors.fill: parent
        enabled: arm.canSeek; hoverEnabled: true; preventStealing: true
        acceptedButtons: Qt.LeftButton
        activeFocusOnTab: true
        property bool pointerFocus: false
        property bool hintDismissed: false
        readonly property bool hoveringNeedle: needleHover.hovered && Math.hypot(needleHover.point.position.x-tip.x, needleHover.point.position.y-tip.y) <= 27
        onHoveringNeedleChanged: if (!hoveringNeedle) hintDismissed = false
        onActiveFocusChanged: if (!activeFocus) pointerFocus = false
        readonly property point tip: {
            const angle = arm.armAngle * Math.PI / 180
            return Qt.point(378 - 197 * Math.sin(angle), 100 + 197 * Math.cos(angle))
        }
        containmentMask: QtObject {
            function contains(point) { return Math.hypot(point.x - needleHit.tip.x, point.y - needleHit.tip.y) <= 27 }
        }
        cursorShape: arm.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        Accessible.role: Accessible.Slider
        Accessible.name: "Needle position"
        Accessible.description: "Drag onto the grooves to seek and play. Hold Shift for precision. Arrow keys seek; Enter starts playback; Escape cancels dragging."
        onPressed: mouse => {
            if (Math.hypot(mouse.x - tip.x, mouse.y - tip.y) > 27) { mouse.accepted = false; return }
            pointerFocus = true
            forceActiveFocus()
            arm.dragAngle = arm.armAngle; arm.rawAngle = arm.armAngle
            arm.lastPointerAngle = Math.atan2(378 - mouse.x, mouse.y - 100) * 180 / Math.PI
            arm.dragTrack = arm.app.cassetteTrackIdentity; arm.wasPlaying = arm.app.deckPlayer.playing
            arm.pointerDistance = Math.hypot(mouse.x - 378, mouse.y - 100)
            arm.dragging = true; arm.landing = false; landingTimeout.stop()
            if (arm.wasPlaying) arm.app.deckPlayer.pause()
        }
        onPositionChanged: mouse => {
            if (!pressed || !arm.dragging) return
            const dx = mouse.x - 378, dy = mouse.y - 100
            arm.pointerDistance = Math.hypot(dx, dy)
            const pointer = Math.atan2(-dx, dy) * 180 / Math.PI
            let delta=pointer-arm.lastPointerAngle
            if(delta>180)delta-=360;else if(delta < -180)delta+=360
            arm.rawAngle += delta*((mouse.modifiers & Qt.ShiftModifier)?.1:1)
            arm.lastPointerAngle=pointer
            arm.dragAngle = Math.max(-8, Math.min(38, arm.rawAngle))
        }
        onReleased: { hintDismissed = true; arm.dropNeedle() }
        onCanceled: { hintDismissed = true; arm.cancelDrag(true) }
        onWheel: wheel => { wheel.accepted = false }
        Keys.onShortcutOverride: event => { if ([Qt.Key_Escape,Qt.Key_Left,Qt.Key_Right,Qt.Key_Return,Qt.Key_Enter].includes(event.key)) event.accepted = true }
        Keys.onEscapePressed: arm.cancelDrag(true)
        Keys.onLeftPressed: arm.app.deckPlayer.seek(Math.max(0,arm.app.deckPlayer.position - 5000))
        Keys.onRightPressed: arm.app.deckPlayer.seek(Math.min(arm.app.deckPlayer.duration,arm.app.deckPlayer.position + 5000))
        Keys.onReturnPressed: arm.app.deckPlayer.play()
        Keys.onEnterPressed: arm.app.deckPlayer.play()
        Rectangle {
            x: needleHit.tip.x - 22; y: needleHit.tip.y - 25; width: 44; height: 50
            rotation: arm.armAngle; radius: 10; color: "transparent"
            border.width: needleHit.activeFocus && !needleHit.pointerFocus && !arm.dragging ? 1.5 : 0
            border.color: arm.app.accent
        }
    }

}
