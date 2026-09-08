import QtQuick

Rectangle {
    id: layer
    objectName: "interactionStateLayer"
    property bool pressed: false
    property bool focused: false
    property bool hovered: false
    color: theme.colors.text
    // Keep RGB fixed. Interpolating transparent black into a light hover color
    // creates a dark intermediate flash, especially on light surfaces.
    opacity: enabled ? (pressed ? SpunStyle.pressOpacity : focused ? SpunStyle.focusOpacity : hovered ? SpunStyle.hoverOpacity : 0) : 0
    Behavior on opacity {
        NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve }
    }
}
