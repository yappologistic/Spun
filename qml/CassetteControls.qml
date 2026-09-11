import QtQuick

Item {
    id: controls
    required property var app
    objectName: "cassetteControls"
    property int pressedControl: -1
    property bool armed: false
    property bool doorOpen: false
    property real startY: 0
    property real startVolume: 0
    readonly property var keyItems: [play, rewind, forward, eject, volume, latch]
    readonly property bool available: app.cassette && app.bodyVisible && !app.discFlipped && !app.menuOpen && !app.swapRunning
    property real doorAngle: app.swapRunning ? app.lidOpen*55 : doorOpen ? 58 : 0
    Behavior on doorAngle { enabled: controls.app.animate; NumberAnimation { duration: 300; easing.type: Easing.InOutCubic } }
    function canUse(index) {
        return available && (index>=3 || (app.deckPlayer.count>0 && (index===0 || !app.useCider || app.ciderService.canSeek)))
    }
    function cancel() { pressedControl=-1;armed=false }
    function begin(index,y) { cancel();if(!canUse(index))return;pressedControl=index;armed=true;startY=y;startVolume=app.deckPlayer.volume }
    function move(y,index) {
        if(pressedControl===4)app.deckPlayer.volume=Math.max(0,Math.min(1,startVolume+(startY-y)/150))
        else armed=index===pressedControl
    }
    function activate(index) {
        if(!canUse(index))return
        if(index===0) { doorOpen=false;app.deckPlayer.toggle() }
        else if(index===1)app.deckPlayer.seek(Math.max(0,app.deckPlayer.position-10000))
        else if(index===2)app.deckPlayer.seek(Math.min(app.deckPlayer.duration,app.deckPlayer.position+10000))
        else if(index===3) {
            if(app.deckPlayer.playing)app.deckPlayer.pause()
            else doorOpen=!doorOpen
        } else if(index===5) { if(!doorOpen)app.deckPlayer.pause();doorOpen=!doorOpen }
    }
    function end() { const index=pressedControl,run=armed;cancel();if(run && index!==4)activate(index) }
    onAvailableChanged: if(!available)cancel()
    Connections { target: controls.app.deckPlayer; function onPlayingChanged() { if(controls.app.deckPlayer.playing)controls.doorOpen=false } }
    Connections { target: controls.app; function onCassetteTrackIdentityChanged(){controls.cancel()} }
    component Key: Item {
        id: key
        required property int index
        required property string caption
        property string icon: ""
        property bool knob: false
        readonly property real pressure: Math.max(0,Math.min(1.04,travel.value))
        SpunSpring { id: travel; targetValue: controls.pressedControl===key.index && controls.armed ? 1 : 0; epsilon: .005; animation.spring: 5.2; animation.damping: .38 }
        enabled: controls.canUse(index)
        activeFocusOnTab: true
        Accessible.role: knob ? Accessible.Slider : Accessible.Button
        Accessible.name: caption
        Accessible.onPressAction: controls.activate(index)
        Keys.onReturnPressed: controls.activate(index)
        Keys.onSpacePressed: controls.activate(index)
        Keys.onUpPressed: if(knob)controls.app.deckPlayer.volume=Math.min(1,controls.app.deckPlayer.volume+.05)
        Keys.onDownPressed: if(knob)controls.app.deckPlayer.volume=Math.max(0,controls.app.deckPlayer.volume-.05)
        Rectangle {
            anchors.fill: parent; anchors.margins: 1
            radius: key.knob ? 6 : 5
            color: "#32383d"
        }
        Rectangle {
            x: 1; y: 1+key.pressure*3; width: parent.width-2; height: parent.height-3
            radius: key.knob ? 6 : 5
            border.width: key.activeFocus ? 1.5 : .6; border.color: key.activeFocus ? controls.app.accent : "#8b989f"
            gradient: Gradient { GradientStop { position: 0; color: "#e9edf0" } GradientStop { position: .18; color: "#c8cfd4" } GradientStop { position: .78; color: "#adb6bd" } GradientStop { position: 1; color: "#707d87" } }
            Glyph { anchors.centerIn: parent; width: 13; height: 13; name: key.icon; ink: "#38434b"; visible: key.icon!=="" && key.icon!=="stop" }
            Rectangle { anchors.centerIn: parent; width: 8; height: 8; color: "#38434b"; visible: key.icon==="stop" }
            Repeater { model: key.knob ? 7 : 0; Rectangle { required property int index; x: 3; y: 4+index*3.5; width: parent.width-6; height: .65; color: "#6e7d85"; opacity: .55 } }
        }
        MouseArea {
            anchors.fill: parent; preventStealing: true
            cursorShape: key.knob ? Qt.SizeVerCursor : Qt.PointingHandCursor
            onPressed: mouse => { key.forceActiveFocus();controls.begin(key.index,mapToItem(controls,mouse.x,mouse.y).y) }
            onPositionChanged: mouse => controls.move(mapToItem(controls,mouse.x,mouse.y).y,containsMouse?key.index:-1)
            onReleased: controls.end()
            onCanceled: controls.cancel()
            onWheel: wheel => { controls.app.deckPlayer.volume=Math.max(0,Math.min(1,controls.app.deckPlayer.volume+wheel.angleDelta.y/2400));wheel.accepted=true }
            hoverEnabled: true
            SpunToolTip { visible: parent.containsMouse && !parent.pressed && !controls.app.threeDActive; text: key.caption; timeout: 2500 }
        }
    }
    Key { id: play; objectName: "cassettePlay"; index: 0; x: 123; y: 137; width: 34; height: 21; caption: controls.app.deckPlayer.playing?"Pause":"Play"; icon: controls.app.deckPlayer.playing?"pause":"play" }
    Key { id: rewind; objectName: "cassetteRewind"; index: 1; x: 172; y: 137; width: 34; height: 21; caption: "Back 10 seconds"; icon: "previous" }
    Key { id: forward; objectName: "cassetteForward"; index: 2; x: 221; y: 137; width: 34; height: 21; caption: "Forward 10 seconds"; icon: "next" }
    Key { id: eject; objectName: "cassetteStop"; index: 3; x: 270; y: 137; width: 34; height: 21; caption: controls.app.deckPlayer.playing?"Stop":"Open or close cassette door"; icon: "stop" }
    Key { id: volume; objectName: "cassetteVolume"; index: 4; x: 462; y: 203; width: 18; height: 34; caption: "Volume, drag up or down"; knob: true }
    Key { id: latch; objectName: "cassetteLatch"; index: 5; x: 250; y: 160; width: 30; height: 9; caption: controls.doorOpen?"Close cassette door":"Open cassette door" }
}
