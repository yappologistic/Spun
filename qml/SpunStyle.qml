pragma Singleton
import QtQuick
QtObject {
    readonly property string family: typography.family
    readonly property int caption: 12
    readonly property int body: 14
    readonly property int heading: 16
    readonly property int title: 22
    readonly property int smallGap: 4
    readonly property int gap: 8
    readonly property int textGap: 12
    readonly property int inset: 16
    readonly property int outerInset: 24
    readonly property int target: 40
    readonly property int icon: 24
    readonly property int smallIcon: 20
    readonly property int trackHeight: 64
    readonly property int artwork: 40
    readonly property real rowRadius: 12 * theme.radius
    readonly property real popupRadius: 16 * theme.radius
    readonly property real panelRadius: 28 * theme.radius
    readonly property real dialogRadius: 28 * theme.radius
    readonly property real disabledOpacity: .38
    readonly property real hoverOpacity: .08
    readonly property real focusOpacity: .10
    readonly property real pressOpacity: .10
    readonly property bool motion: player.motion && theme.motionScale > 0
    readonly property int feedback: motion ? Math.round(150 * theme.motionScale) : 0
    readonly property int enter: motion ? Math.round(200 * theme.motionScale) : 0
    readonly property int exit: motion ? Math.round(100 * theme.motionScale) : 0
    readonly property int navigate: motion ? Math.round(250 * theme.motionScale) : 0
    readonly property int hero: motion ? Math.round(350 * theme.motionScale) : 0
    // Material's bounded effects and emphasized entrance/exit easing, in Qt's cubic format.
    // M3 motion physics: effects stay bounded; spatial feedback uses springs.
    // Official fast-effects curve approximation, used for color/opacity.
    readonly property var effectsCurve: [0.31, 0.94, 0.34, 1, 1, 1]
    readonly property var standardCurve: [0.2, 0, 0, 1, 1, 1]
    readonly property var enterCurve: [0.05, 0.7, 0.1, 1, 1, 1]
    readonly property var exitCurve: [0.3, 0, 0.8, 0.15, 1, 1]
    readonly property color selected: Qt.tint(theme.colors.card, Qt.alpha(theme.colors.accent, .13))
    readonly property color popup: Qt.tint(theme.colors.card, Qt.alpha(theme.colors.text, .035))
}
