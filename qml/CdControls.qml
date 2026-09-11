import QtQuick

Item {
    id: controls
    required property var app
    objectName: "cdControls"
    property int pressedControl: -1
    property bool armed: false
    property bool doorOpen: false
    property real startY: 0
    property real startVolume: 0
    readonly property var keyItems: [play, stop, settings, previous, next, mute, volume, latch, shuffle, repeat]
    readonly property bool available: app.cd && app.bodyVisible && !app.discFlipped && !app.menuOpen && !app.swapRunning
    property real doorAngle: app.swapRunning ? Math.max(doorOpen ? 70 : 0, app.lidOpen*70) : doorOpen ? 70 : 0
    Behavior on doorAngle { enabled: controls.app.animate && !controls.app.swapRunning; NumberAnimation { duration: 320; easing.type: Easing.InOutCubic } }
    function canUse(index) {
        const transport=index===0 || index===1 || index===3 || index===4 || index>=8
        return available && index>=0 && index<keyItems.length
            && (!transport || (app.deckPlayer.count>0 && (!app.useCider || !app.ciderService.controlBusy)))
    }
    function cancel() { pressedControl=-1;armed=false }
    function begin(index,y) { cancel();if(!canUse(index))return;pressedControl=index;armed=true;startY=y;startVolume=app.deckPlayer.volume }
    function move(y,index) { if(pressedControl===6)app.deckPlayer.volume=Math.max(0,Math.min(1,startVolume+(startY-y)/150));else armed=index===pressedControl }
    function activate(index) {
        if(!canUse(index))return
        if(index===0){doorOpen=false;app.deckPlayer.toggle()}
        else if(index===1){app.deckPlayer.pause();if(!app.useCider || app.ciderService.canSeek)app.deckPlayer.seek(0)}
        else if(index===2)app.openSettings()
        else if(index===3)app.deckPlayer.previous()
        else if(index===4)app.deckPlayer.next()
        else if(index===5)app.toggleMute()
        else if(index===7){if(!doorOpen)app.deckPlayer.pause();doorOpen=!doorOpen}
        else if(index===8)app.deckPlayer.shuffle=!app.deckPlayer.shuffle
        else if(index===9)app.deckPlayer.repeatMode=(app.deckPlayer.repeatMode+1)%3
    }
    function end(){const index=pressedControl,run=armed;cancel();if(run&&index!==6)activate(index)}
    onAvailableChanged: if(!available)cancel()
    Connections { target: controls.app.deckPlayer; function onPlayingChanged(){if(controls.app.deckPlayer.playing)controls.doorOpen=false} function onTrackChanged(){controls.cancel()} }
    Connections { target: controls.app; function onUseCiderChanged(){controls.cancel();controls.doorOpen=false} }
    Rectangle { x: 54; y: 489; width: 422; height: 39; radius: 6; border.color: "#b9c0c4"; border.width: .8; gradient: Gradient { GradientStop { position: 0; color: "#dce0e3" } GradientStop { position: .2; color: "#c3c8cb" } GradientStop { position: 1; color: "#889298" } } }
    Rectangle { x: 61; y: 492; width: 408; height: 31; radius: 14; color: "#101619"; border.color: "#69747a"; border.width: .7 }
    Repeater { model: 2; Rectangle { required property int index; x: 72+index*24; y: 502; width: index?9:12; height: width; radius: width/2; color: "#05080a"; border.color: "#879298"; border.width: 1.2 } }
    CdDisplay { app: controls.app; x: 196; y: 493; scale: .43; transformOrigin: Item.TopLeft }
    Rectangle {
        x: 250; y: 279; width: 30; height: 30; radius: 15; border.width: 2; border.color: "#455159"
        gradient: Gradient { GradientStop { position: 0; color: "#cfd7dc" } GradientStop { position: .48; color: "#8e9ca5" } GradientStop { position: .52; color: "#39464e" } GradientStop { position: 1; color: "#aebbc2" } }
        Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 2.5; color: "#26343b" }
        Repeater { model: 3; Rectangle { required property int index; x: 13.5+12*Math.sin(index*2*Math.PI/3); y: 13.5-12*Math.cos(index*2*Math.PI/3); width: 3; height: 3; radius: 1.5; color: "#e1e7eb"; border.width: .5; border.color: "#6b7b85" } }
    }
    component Key: Item {
        id: key
        required property int index
        required property string caption
        property string icon: ""
        property bool knob: false
        property bool selector: false
        readonly property real pressure: Math.max(0,Math.min(1.03,travel.value))
        SpunSpring { id: travel; targetValue: controls.pressedControl===key.index&&controls.armed?1:0; epsilon: .005; animation.spring: 5.2; animation.damping: .4 }
        enabled: controls.canUse(index); activeFocusOnTab: true
        Accessible.role: knob?Accessible.Slider:Accessible.Button; Accessible.name: caption
        Accessible.onPressAction: controls.activate(index)
        Keys.onReturnPressed: controls.activate(index)
        Keys.onSpacePressed: controls.activate(index)
        Keys.onUpPressed: if(knob)controls.app.deckPlayer.volume=Math.min(1,controls.app.deckPlayer.volume+.05)
        Keys.onDownPressed: if(knob)controls.app.deckPlayer.volume=Math.max(0,controls.app.deckPlayer.volume-.05)
        Rectangle { anchors.fill: parent; anchors.margins: 1; radius: key.selector?2:width/2; color: "#030607" }
        Rectangle {
            x: 2; y: 1+key.pressure*1.5; width: parent.width-4; height: parent.height-4
            radius: key.selector?1:width/2; border.width: .6; border.color: key.activeFocus?controls.app.accent:"#8b979d"
            gradient: Gradient { GradientStop { position: 0; color: key.knob?"#e4e7e9":"#68757c" } GradientStop { position: .5; color: key.knob?"#c4cbce":"#3b464d" } GradientStop { position: 1; color: key.knob?"#87959e":"#202c32" } }
            Glyph { anchors.centerIn: parent; width: key.knob?10:7; height: width; name: key.icon; ink: "#bac4c8"; visible: key.icon!==""&&key.icon!=="stop" }
            Rectangle { anchors.centerIn: parent; width: 3.5; height: 3.5; color: "#bac4c8"; visible: key.icon==="stop" }
            Rectangle { visible: key.knob; x: parent.width/2-.6; y: 2; width: 1.2; height: 4; color: "#424c52"; transform: Rotation { origin.x: .6; origin.y: parent.height/2-2; angle: -135+270*controls.app.deckPlayer.volume } }
        }
        MouseArea {
            anchors.fill: parent; preventStealing: true; hoverEnabled: true; cursorShape: key.knob?Qt.SizeVerCursor:Qt.PointingHandCursor
            onPressed: mouse=>{key.forceActiveFocus();controls.begin(key.index,mapToItem(controls,mouse.x,mouse.y).y)}
            onPositionChanged: mouse=>controls.move(mapToItem(controls,mouse.x,mouse.y).y,containsMouse?key.index:-1)
            onReleased: controls.end(); onCanceled: controls.cancel()
            onWheel: wheel=>{if(key.knob)controls.app.deckPlayer.volume=Math.max(0,Math.min(1,controls.app.deckPlayer.volume+wheel.angleDelta.y/2400));wheel.accepted=key.knob}
            SpunToolTip { visible: parent.containsMouse&&!parent.pressed&&!controls.app.threeDActive; text: key.caption; timeout: 2500 }
        }
    }
    Key { id: play; objectName: "cdPlay"; index: 0; caption: controls.app.deckPlayer.playing?"Pause":"Play"; icon: controls.app.deckPlayer.playing?"pause":"play"; x: 326; y: 494; width: 17; height: 16 }
    Key { id: stop; objectName: "cdStop"; index: 1; caption: "Stop and return to start"; icon: "stop"; x: 354; y: 494; width: 17; height: 16 }
    Key { id: settings; objectName: "cdSettings"; index: 2; caption: "Settings"; icon: "more"; x: 382; y: 494; width: 17; height: 16 }
    Key { id: previous; objectName: "cdPrevious"; index: 3; caption: "Previous track"; icon: "previous"; x: 326; y: 510; width: 17; height: 16 }
    Key { id: next; objectName: "cdNext"; index: 4; caption: "Next track"; icon: "next"; x: 354; y: 510; width: 17; height: 16 }
    Key { id: mute; objectName: "cdMute"; index: 5; caption: "Mute or restore volume"; icon: "volume"; x: 382; y: 510; width: 17; height: 16 }
    Key { id: volume; objectName: "cdVolume"; index: 6; caption: "Volume, drag up or down"; knob: true; x: 430; y: 493; width: 32; height: 32 }
    Key { id: latch; objectName: "cdLatch"; index: 7; caption: controls.doorOpen?"Close CD lid":"Open CD lid"; x: 251; y: 482; width: 28; height: 8; selector: true }
    Key { id: shuffle; objectName: "cdShuffle"; index: 8; caption: "Shuffle"; icon: "shuffle"; x: 122; y: 499; width: 23; height: 21; selector: true }
    Key { id: repeat; objectName: "cdRepeat"; index: 9; caption: "Repeat mode"; icon: "repeat"; x: 157; y: 499; width: 23; height: 21; selector: true }
}
