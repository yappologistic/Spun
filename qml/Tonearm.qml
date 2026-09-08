import QtQuick

// Decorative scene-graph geometry: no images, effects, timers or pointer handlers.
Item {
    id: arm
    objectName: "vinylTonearm"
    required property var app
    readonly property bool motion: app.animate && visible && app.visible && native.exposed
    readonly property bool engaged: visible && app.deckPlayer.count > 0 && app.deckPlayer.playing
    property real lowered: engaged ? 1 : 0
    property real groove: Math.max(0, Math.min(1, app.progress))
    readonly property real armAngle: -4 + lowered * (10 + 20 * groove)
    readonly property color gold: Qt.tint("#c6a25a", Qt.alpha(app.accent, .12))
    readonly property color glint: Qt.lighter(gold, 1.5)
    readonly property color shade: Qt.darker(gold, 1.6)
    Behavior on lowered {
        enabled: arm.motion
        NumberAnimation { id: liftMotion; duration: SpunStyle.hero; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.standardCurve }
    }
    Behavior on groove {
        enabled: arm.motion && arm.engaged
        NumberAnimation { id: grooveMotion; duration: SpunStyle.navigate; easing.type: Easing.Linear }
    }
    onMotionChanged: if (!motion) {
        liftMotion.complete(); grooveMotion.complete()
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
}
