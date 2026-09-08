import QtQuick

Transition {
    ParallelAnimation {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: SpunStyle.feedback; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.effectsCurve }
        NumberAnimation { property: "scale"; from: SpunStyle.motion ? .97 : 1; to: 1; duration: SpunStyle.enter; easing.type: Easing.BezierSpline; easing.bezierCurve: SpunStyle.enterCurve }
    }
}
