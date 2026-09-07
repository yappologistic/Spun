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
    opacity: enabled ? (pressed || focused ? .12 : hovered ? .08 : 0) : 0
    Behavior on opacity {
        NumberAnimation { duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.standardCurve }
    }
}
