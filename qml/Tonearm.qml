import QtQuick

// Lightweight geometry with a bounded stylus gesture and damped return motion.
Item {
    id: arm
    objectName: "vinylTonearm"
    required property var app
    readonly property bool motion: app.animate && visible && app.visible && native.exposed
    readonly property bool engaged: visible && app.deckPlayer.count > 0 && (app.deckPlayer.playing || landing)
    readonly property bool canSeek: visible && app.deckPlayer.duration > 0 && (!app.useCider || app.ciderService.canSeek)
    property bool dragging: false
    property real dragAngle: -4
    property real rawAngle: -4
    property real grabOffset: 0
    property real pointerDistance: 210
    property string dragTrack: ""
    property bool wasPlaying: false
    property bool landing: false
    property real landingProgress: 0
    readonly property real previewProgress: Math.max(0, Math.min(1, (rawAngle - 6) / 20))
    property real lowered: dragging ? .12 : engaged ? 1 : 0
    property real groove: landing ? landingProgress : Math.max(0, Math.min(1, app.progress))
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
        app.deckPlayer.seek(Math.min(app.deckPlayer.duration - 1, app.deckPlayer.duration * position))
        app.deckPlayer.play()
    }
    function confirmLanding() {
        if (landing && app.deckPlayer.playing && Math.abs(app.progress - landingProgress) < .025) {
            landing = false; landingTimeout.stop()
        }
    }
    onVisibleChanged: if (!visible) cancelDrag(true)
    onCanSeekChanged: if (!canSeek) cancelDrag(false)
    Timer { id: landingTimeout; interval: 1200; onTriggered: arm.landing = false }
    Connections {
        target: arm.app
        function onCassetteTrackIdentityChanged() { arm.cancelDrag(false); arm.landing = false }
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
        parent: arm; x: 184; y: 60
        visible: needleHit.containsMouse || needleHit.activeFocus
        delay: arm.dragging ? 0 : 650
        text: arm.dragging ? arm.app.time(arm.previewProgress * arm.app.deckPlayer.duration) : "Drag the needle to seek"
    }
    // The pivot rests at the record's edge; the stylus stays outside the label.
    Item {
        x: 378; y: 100
        Rectangle { x: -15; y: -13; width: 30; height: 30; radius: 15; color: "#50000000" }
        Rectangle {
            x: -13; y: -13; width: 26; height: 26; radius: 13
            color: arm.app.surface; border.width: 1; border.color: arm.shade
        }
        Item {
            id: shaft
            objectName: "tonearmShaft"
            rotation: arm.armAngle
            transformOrigin: Item.TopLeft
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
                Rectangle { x: 6; y: 23; width: 2; height: 5; radius: 1; color: arm.glint }
            }

        }
        Rectangle { x: -7; y: -7; width: 14; height: 14; radius: 7; color: arm.gold; border.width: 1; border.color: arm.glint }
        Rectangle { x: -3; y: -1; width: 6; height: 2; radius: 1; rotation: -35; color: arm.shade }
    }
    // Pointer coordinates must stay fixed while the shaft rotates beneath them.
    MouseArea {
        id: needleHit; objectName: "needleHandle"
        anchors.fill: parent
        enabled: arm.canSeek; hoverEnabled: true; preventStealing: true
        acceptedButtons: Qt.LeftButton
        activeFocusOnTab: true
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
        Accessible.description: "Drag onto the grooves to seek and play. Arrow keys seek; Enter starts playback; Escape cancels dragging."
        onPressed: mouse => {
            if (Math.hypot(mouse.x - tip.x, mouse.y - tip.y) > 27) { mouse.accepted = false; return }
            forceActiveFocus()
            arm.dragAngle = arm.armAngle; arm.rawAngle = arm.armAngle
            arm.grabOffset = Math.atan2(378 - mouse.x, mouse.y - 100) * 180 / Math.PI - arm.armAngle
            arm.dragTrack = arm.app.cassetteTrackIdentity; arm.wasPlaying = arm.app.deckPlayer.playing
            arm.pointerDistance = Math.hypot(mouse.x - 378, mouse.y - 100)
            arm.dragging = true; arm.landing = false; landingTimeout.stop()
            if (arm.wasPlaying) arm.app.deckPlayer.pause()
        }
        onPositionChanged: mouse => {
            if (!pressed || !arm.dragging) return
            const dx = mouse.x - 378, dy = mouse.y - 100
            arm.pointerDistance = Math.hypot(dx, dy)
            arm.rawAngle = Math.atan2(-dx, dy) * 180 / Math.PI - arm.grabOffset
            arm.dragAngle = Math.max(-8, Math.min(38, arm.rawAngle))
        }
        onReleased: arm.dropNeedle()
        onCanceled: arm.cancelDrag(true)
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
            border.width: needleHit.activeFocus && !needleHit.containsMouse && !arm.dragging ? 1.5 : 0
            border.color: arm.app.accent
        }
    }

}
