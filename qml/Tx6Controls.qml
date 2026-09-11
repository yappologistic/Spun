import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Spun 1.0

Item {
    id: mixer

    required property var app
    property int pressedControl: -1
    property bool armed: false
    property int selected: 30
    property real startY: 0
    property real startValue: 0
    property real contactX: 0
    property bool shiftHeld: false
    property int loadChannel: 1
    readonly property bool popupOpen: channelMenu.visible || stemFile.visible
    readonly property var keyItems: {
        let a = [];
        for (let i = 0; i < keys.count; i++) a.push(keys.itemAt(i))
        return a;
    }
    readonly property bool operable: tx6.powered && tx6.available && !app.menuOpen && !app.discFlipped
    readonly property string displayTitle: selected < 18 ? ("CH" + (selected % 6 + 1) + " " + ["HIGH", "MID", "LOW"][Math.floor(selected / 6)]) : selected < 24 ? "CH" + (selected - 17) + " LEVEL" : selected < 30 ? "CH" + (selected - 23) + (tx6.channels[selected - 24].solo ? " SOLO" : " MUTE") : selected === 31 ? "DELAY" : selected === 32 ? "COMP" : "MASTER"
    readonly property string displayValue: selected < 18 ? Math.round(value(selected)).toString() : selected < 24 || selected === 30 ? Math.round(value(selected) * 100).toString() : selected === 31 ? (tx6.delay ? "ON" : "OFF") : selected === 32 ? (tx6.compressor ? "ON" : "OFF") : selected < 30 ? (tx6.channels[selected - 24].mute ? "OFF" : "ON") : Math.round(app.deckPlayer.volume * 100).toString()

    function position(i) {
        if (i < 18)
            return Qt.point(22 + (i % 6) * 29.3, 60 + Math.floor(i / 6) * 44);

        if (i < 24)
            return Qt.point(22 + (i - 18) * 29.3, 285 - 77 * tx6.channels[i - 18].level);

        if (i < 30)
            return Qt.point(22 + (i - 24) * 29.3, 324);

        return [Qt.point(210, 148), Qt.point(210, 220), Qt.point(210, 268), Qt.point(210, 324), Qt.point(-6, 283)][i - 30];
    }

    function value(i) {
        return i < 18 ? tx6.channels[i % 6][["high", "mid", "low"][Math.floor(i / 6)]] : i < 24 ? tx6.channels[i - 18].level : i === 30 ? app.deckPlayer.volume : 0;
    }

    function setValue(i, v) {
        if (i < 18)
            tx6.setEq(i % 6, Math.floor(i / 6), Math.max(-12, Math.min(12, v)));
        else if (i < 24)
            tx6.setLevel(i - 18, Math.max(0, Math.min(1, v)));
        else if (i === 30)
            app.deckPlayer.volume = Math.max(0, Math.min(1, v));
    }

    function caption(i) {
        if (i === 34)
            return tx6.powered ? "Power off · bypass mixer" : "Power on";

        if (i===30)return "Master volume · drag or scroll";
        if (!tx6.available)
            return "Local audio mixing · switch from Cider to Local";

        if (i < 18)
            return "Channel " + (i % 6 + 1) + " · " + ["High", "Mid", "Low"][Math.floor(i / 6)] + " EQ · drag or scroll · double-click to reset";

        if (i < 24)
            return "Channel " + (i - 17) + " · level · " + (tx6.channels[i - 18].name || "No input");

        if (i < 30)
            return "Channel " + (i - 23) + " · mute · Shift for solo · right-click to load audio";

        return ["Master volume · drag or scroll", "FX I · stereo delay", "FX II · compressor", "Shift · tap, then choose a channel to solo"][i - 30];
    }

    function canUse(i) {
        return i === 34 ? !app.menuOpen : i === 30 ? tx6.powered && !app.menuOpen : operable;
    }

    function begin(i, y, x) {
        if (!canUse(i))
            return ;

        const soloArmed = shiftHeld;
        cancel();
        shiftHeld = soloArmed;
        selected = i;
        pressedControl = i;
        armed = true;
        startY = y;
        startValue = value(i);
        contactX = Math.max(-1, Math.min(1, x || 0));
    }

    function move(y, hit, fine) {
        if (pressedControl < 0)
            return ;

        const i = pressedControl;
        if (i < 24 || i === 30)
            setValue(i, startValue + (startY - y) * (fine ? 0.2 : 1) * (i < 18 ? 0.15 : i < 24 ? 1 / 77 : 0.006));
        else
            armed = hit === i;
    }

    function end(modifiers) {
        const i = pressedControl;
        const activate = armed;
        pressedControl = -1;
        armed = false;
        if (i === 33) {
            if (activate)
                shiftHeld = !shiftHeld;
            return ;
        }
        if (!activate || i < 24 || i === 30)
            return ;

        if (i < 30) {
            if (shiftHeld || (modifiers & Qt.ShiftModifier))
                tx6.toggleSolo(i - 24);
            else
                tx6.toggleMute(i - 24);
            shiftHeld = false;
        } else if (i === 31)
            tx6.delay = !tx6.delay;
        else if (i === 32)
            tx6.compressor = !tx6.compressor;
        else if (i === 34)
            tx6.powered = !tx6.powered;
    }

    function cancel() {
        pressedControl = -1;
        armed = false;
        shiftHeld = false;
    }

    function scroll(i, delta) {
        if (!canUse(i))
            return ;

        selected = i;
        setValue(i, value(i) + (i < 18 ? 0.5 : 0.025) * delta);
    }

    function resetControl(i) {
        if (i < 18)
            setValue(i, 0);
        else if (i < 24)
            setValue(i, 1);
        else if (i === 30)
            setValue(i, 0.65);
    }

    function openChannel(i) {
        loadChannel = i;
        channelMenu.popup();
    }

    objectName: "tx6Controls"
    width: 244
    height: 400
    onVisibleChanged: {
        if (!visible) {
            cancel();
        }
    }

    Connections {
        function onMenuOpenChanged() {
            if (app.menuOpen)
                mixer.cancel();

        }

        function onDiscFlippedChanged() {
            mixer.cancel();
        }

        target: app
    }

    Connections {
        function onChanged() {
            if (!tx6.powered || !tx6.available)
                mixer.cancel();

        }

        target: tx6
    }

    Rectangle {
        x: 1
        y: 5
        width: 244
        height: 352
        radius: 10
        color: "#32000000"
    }

    Rectangle {
        width: 244
        height: 352
        radius: 10
        border.color: "#62666a"
        border.width: 0.7

        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#e2e3e4"
            }

            GradientStop {
                position: 0.2
                color: "#bfc1c4"
            }

            GradientStop {
                position: 0.7
                color: "#cfd0d1"
            }

            GradientStop {
                position: 1
                color: "#afb1b4"
            }

        }

    }

    Rectangle {
        x: 1.5
        y: 1.5
        width: 241
        height: 349
        radius: 9
        color: "transparent"
        border.color: "#ddf5f6f8"
        border.width: 0.8
    }

    Tx6Labels {
    }

    Tx6Display {
        x: 190
        y: 51
        controls: mixer
    }

    Repeater {
        model: 6

        Item {
            required property int index

            x: 22 + index * 29.3
            y: 201

            Rectangle {
                x: -5.2
                width: 10.4
                height: 89
                radius: 5
                color: "#75787a"
                border.color: "#e5e6e6"
                border.width: 0.7
            }

            Rectangle {
                x: -3
                y: 2
                width: 6
                height: 85
                radius: 3
                color: "#161b1e"
                border.color: "#383d3e"
            }

            Rectangle {
                x: -1.7
                y: 4
                width: 1
                height: 81
                color: "#566065"
            }

            Rectangle {
                x: -1
                y: 110
                width: 2
                height: 2
                radius: 1
                color: tx6.channels[index].solo ? "#f47822" : tx6.channels[index].mute ? "#a14b32" : "#9caaa5"
            }

        }

    }
    // These objects own pressure and rotation in both renderers. 3D picks call

    // the same gesture functions, so a press never competes with orbiting.
    Repeater {
        id: keys

        model: 35

        Item {
            id: key

            required property int index
            readonly property point center: mixer.position(index)
            readonly property bool round: index < 24 || (index >= 30 && index <= 32)
            readonly property bool latched: index >= 24 && index < 30 ? tx6.channels[index - 24].mute : index === 31 ? tx6.delay : index === 32 ? tx6.compressor : index === 33 ? mixer.shiftHeld : false
            readonly property real pressure: Math.max(-0.025, Math.min(1.03, press.value))
            readonly property real turn: mixer.value(index) * (index < 18 ? 11 : 270) - (index === 30 ? 135 : 0)
            property real rotationValue: turn

            objectName: "tx6Control" + index
            width: index < 18 ? 15 : index < 24 ? 10 : index < 30 ? 13 : index === 30 ? 50 : index === 33 ? 42 : index === 34 ? 10 : 42
            height: index < 18 ? 15 : index < 24 ? 10 : index < 30 ? 26 : index === 33 ? 22 : index === 34 ? 35 : width
            x: center.x - width / 2
            y: center.y - height / 2
            activeFocusOnTab: mixer.canUse(index)
            Accessible.role: key.index < 24 || key.index === 30 ? Accessible.Slider : Accessible.Button
            Accessible.name: mixer.caption(index)
            Keys.onPressed: (event) => {
                if (event.isAutoRepeat)
                    return ;

                if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                    mixer.begin(index, 0, 0);
                    event.accepted = true;
                } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                    mixer.scroll(index, event.key === Qt.Key_Up ? 1 : -1);
                    event.accepted = true;
                }
            }
            Keys.onReleased: (event) => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                    mixer.end(event.modifiers);
                    event.accepted = true;
                }
            }

            SpunSpring {
                id: press

                targetValue: mixer.pressedControl === key.index && mixer.armed ? 1 : key.latched ? 0.16 : 0
                epsilon: 0.002
                animation.spring: 5.5
                animation.damping: 0.42
                animation.mass: 0.8
            }

            Rectangle {
                visible: key.index===30
                anchors.centerIn: parent
                width: 50; height: 50; radius:25
                color:"#b5b9ba";border.width:.7;border.color:"#73797a"
                Rectangle {anchors.fill:parent;anchors.margins:1;color:"transparent";radius:24;border.color:"#e1e4e5";border.width:.7}
            }
            Rectangle {
                x: (key.index===30?12:1) + key.pressure
                y: (key.index===30?13.5:2.5) - key.pressure * 1.5
                width: key.index===30?28:parent.width
                height: key.index===30?28:parent.height
                radius: key.round ? width / 2 : 2
                color: "#54000000"
            }

            Rectangle {
                x: (key.index===30?11:0) + key.pressure * 0.25
                y: (key.index===30?11:0) + key.pressure * 1.4
                width: key.index===30?28:parent.width
                height: key.index===30?28:parent.height
                radius: key.round ? width / 2 : 2
                border.width: 0.65
                border.color: "#717778"

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: parent.radius
                    color: "transparent"
                    border.width: 0.6
                    border.color: "#c9ffffff"
                }

                Item {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    rotation: key.rotationValue
                    visible: key.index < 18 || key.index === 30

                    Rectangle {
                        visible: key.index<18
                        anchors.centerIn: parent
                        width: 6; height: 6; radius: 3
                        color: key.index<6?"#20242b":key.index<12?"#f67628":"#f6f4ee"
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 1.2
                        width: key.index === 30 ? 1.5 : 2
                        height: key.index === 30 ? 6 : 6.5
                        radius: 0.8
                        color: key.index < 6 ? "#171d24" : key.index < 12 ? "#f67628" : key.index < 18 ? "#f7f5ef" : "#61696f"
                    }

                }

                Rectangle {
                    visible: key.index === 31 || key.index === 32
                    anchors.centerIn: parent
                    width: 28
                    height: 12
                    radius: 1.8
                    color: key.index === 32 ? "#f76a20" : "#24272d"

                    Text {
                        anchors.centerIn: parent
                        text: key.index === 31 ? "I" : "II"
                        color: "#e8e4dd"
                        font.pixelSize: 12
                    }

                }

                Rectangle {
                    visible: key.index === 33
                    anchors.centerIn: parent
                    width: 5
                    height: 5
                    radius: 2.5
                    color: mixer.shiftHeld ? "#f67628" : "#f3f1ed"
                }

                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: "#f4f5f5"
                    }

                    GradientStop {
                        position: 0.3
                        color: "#d7d8d9"
                    }

                    GradientStop {
                        position: 1
                        color: "#a3a7aa"
                    }

                }

            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                radius: parent.width / 2
                color: "transparent"
                border.width: key.activeFocus ? 1 : 0
                border.color: mixer.app.accent
            }

            MouseArea {
                id: pointer

                anchors.fill: parent
                anchors.margins: key.index < 24 ? -4 : 0
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                hoverEnabled: true
                preventStealing: true
                cursorShape: mixer.canUse(key.index) ? Qt.PointingHandCursor : Qt.ArrowCursor
                onPressed: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        if (key.index >= 24 && key.index < 30)
                            mixer.openChannel(key.index - 24);

                        return ;
                    }
                    key.forceActiveFocus();
                    const p = mapToItem(mixer, mouse.x, mouse.y);
                    mixer.begin(key.index, p.y, (mouse.x - width / 2) / (width / 2));
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return ;

                    const p = mapToItem(mixer, mouse.x, mouse.y);
                    mixer.move(p.y, containsMouse ? key.index : -1, mouse.modifiers & Qt.ShiftModifier);
                }
                onReleased: (mouse) => {
                    return mixer.end(mouse.modifiers);
                }
                onCanceled: mixer.cancel()
                onWheel: (wheel) => {
                    mixer.scroll(key.index, wheel.angleDelta.y / 120);
                    wheel.accepted = true;
                }
                onDoubleClicked: (mouse) => {
                    if (mouse.button === Qt.LeftButton)
                        mixer.resetControl(key.index);

                }
            }

            SpunToolTip {
                visible: pointer.containsMouse && !pointer.pressed
                text: mixer.caption(key.index)
            }

            Behavior on rotationValue {
                enabled: mixer.app.animate && mixer.pressedControl !== key.index

                NumberAnimation {
                    duration: 90
                    easing.type: Easing.OutCubic
                }

            }

        }

    }

    Rectangle {
        x: 191
        y: 352
        width: 37
        height: 8
        color: "#92989b"
        border.color: "#e1e4e5"
    }

    Rectangle {
        x: 190
        y: 359
        width: 39
        height: 40
        radius: 3

        Repeater {
            model: 16

            Rectangle {
                required property int index

                x: 1 + index * 2.4
                y: 2
                width: 0.7
                height: 36
                color: "#6b727a"
                opacity: 0.5
            }

        }

        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0
                color: "#989fa2"
            }

            GradientStop {
                position: 0.3
                color: "#eaeeee"
            }

            GradientStop {
                position: 1
                color: "#8a9298"
            }

        }

    }

    Repeater {
        model:2
        Rectangle {
            required property int index
            x:61+index*29;y:352;width:10;height:4;radius:2
            color:"#b8c0c3";border.color:"#737b80";border.width:.6
        }
    }
    Menu {
        id: channelMenu
        objectName: "tx6ChannelMenu"
        popupType: Popup.Item

        MenuItem {
            text: mixer.loadChannel === 0 ? "TP-7 playback · USB channels 1/2" : tx6.channels[mixer.loadChannel].name || "Empty input"
            enabled: false
        }

        MenuItem {
            text: "Load local audio…"
            enabled: mixer.loadChannel > 0
            onTriggered: stemFile.open()
        }

        MenuItem {
            text: "Remove audio"
            enabled: mixer.loadChannel > 0 && tx6.channels[mixer.loadChannel].loaded
            onTriggered: tx6.unload(mixer.loadChannel)
        }

        MenuItem {
            objectName: "tx6SoloAction"
            text: "Solo"
            checkable: true
            checked: tx6.channels[mixer.loadChannel].solo
            onTriggered: tx6.toggleSolo(mixer.loadChannel)
        }

        MenuItem {
            text: "Reset mixer"
            onTriggered: tx6.reset()
        }

    }

    FileDialog {
        id: stemFile

        title: "Load channel " + (mixer.loadChannel + 1) + " audio"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Audio (*.wav *.flac *.mp3 *.m4a *.ogg *.opus *.aiff)"]
        onAccepted: tx6.load(mixer.loadChannel, selectedFile)
    }

}
