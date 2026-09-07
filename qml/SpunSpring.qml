import QtQuick

// Own the value so stopping a spring preserves the consumer's target binding.
// A SpringAnimation inside a Behavior cannot be completed/stopped on demand.
QtObject {
    id: movement
    required property real targetValue
    property real value: 0
    property real epsilon: 0.25
    property bool ready: false
    function settle() { animation.stop(); value = targetValue }
    onTargetValueChanged: {
        if (!ready || !SpunStyle.motion) settle()
        else { animation.to = targetValue; animation.restart() }
    }
    Component.onCompleted: { settle(); ready = true }
    property SpringAnimation animation: SpringAnimation {
        target: movement; property: "value"
        spring: 4.5; damping: 0.3
        mass: Math.max(0.05, theme.motionScale * theme.motionScale)
        epsilon: movement.epsilon
    }
    property Connections motionGuard: Connections {
        target: SpunStyle
        function onMotionChanged() { if (!SpunStyle.motion) movement.settle() }
    }
}
