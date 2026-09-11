import QtQuick
import QtQuick3D
import Spun 1.0

Node {
    id: deck
    objectName: "vinylTurntable"
    required property var app
    required property Texture artworkTexture
    required property Texture outgoingTexture
    property alias recordSpace: recordSpace
    property alias lidNode: lid
    property bool coverOpen: true
    property int pressedControl: -1
    property bool armed: false
    readonly property real lidAngle: app.swapRunning ? 72 : coverOpen ? 68 : 0
    property real coverAngle: lidAngle
    Behavior on coverAngle { enabled: deck.app.animate; NumberAnimation { duration: 360; easing.type: Easing.InOutCubic } }
    function beginHardware(index) { pressedControl=index;armed=true }
    function cancelHardware() { pressedControl=-1;armed=false }
    function endHardware() {
        const index=pressedControl, activate=armed
        cancelHardware()
        if(!activate)return
        if(index===0)player.vinylSpeed=33
        else if(index===1)player.vinylSpeed=45
        else if(index===2 && app.deckPlayer.count>0)app.deckPlayer.toggle()
        else if(index===3)coverOpen=!coverOpen
    }
    function needlePoint() { return stylus.mapPositionToScene(Qt.vector3d(0,0,0)) }
    function controlPoint(index) {
        if(index<2)return mapPositionToScene(Qt.vector3d(-217+index*40,-200,-21))
        if(index===2)return cueCap.mapPositionToScene(Qt.vector3d(0,0,3.2))
        return lid.mapPositionToScene(Qt.vector3d(0,-180,70))
    }
    Texture { id: grain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "bead-normal" } }
    Texture { id: molded; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "molded" } }
    Texture { id: brushed; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "brushed" } }
    Texture { id: knurl; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "knurled-normal" } }
    Texture { id: microGrooves; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "vinyl-normal" } }
    Texture { id: lacquer; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "lacquer" } }
    Texture { id: powder; generateMipmaps: true; mipFilter: Texture.Linear; scaleU: 4; scaleV: 4; textureData: SurfaceTexture { kind: "powder" } }
    Texture { id: fineMetal; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "metal-normal" } }
    Texture { id: paperGrain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "paper" } }
    PrincipledMaterial { id: enclosure; baseColor: "#202328"; roughness: .64; metalness: .03; roughnessMap: powder; normalMap: grain; normalStrength: .14 }
    PrincipledMaterial { id: topFinish; baseColor: "#25292d"; roughness: .49; roughnessMap: powder; normalMap: grain; normalStrength: .17 }
    PrincipledMaterial { id: rubber; baseColor: "#121519"; roughness: .92; roughnessMap: molded }
    PrincipledMaterial { id: armFinish; baseColor: "#33393e"; metalness: .85; roughness: .3; roughnessMap: brushed; normalMap: fineMetal; normalStrength: .18 }
    PrincipledMaterial { id: steel; baseColor: "#bac3ca"; metalness: 1; roughness: .23; roughnessMap: brushed; normalMap: fineMetal; normalStrength: .16 }
    PrincipledMaterial { id: accentMetal; baseColor: Qt.tint("#be965c",Qt.alpha(app.accent,.5)); metalness: .88; roughness: .3; roughnessMap: brushed; normalMap: fineMetal; normalStrength: .22 }
    PrincipledMaterial { id: discFinish; baseColor: "#101317"; metalness: 0; roughness: .23; roughnessMap: lacquer; normalMap: microGrooves; normalStrength: .18; clearcoatAmount: .4; clearcoatRoughnessAmount: .2 }
    PrincipledMaterial { id: runoutFinish; baseColor: "#13171b"; roughness: .28; clearcoatAmount: .2 }
    PrincipledMaterial { id: paper; baseColorMap: deck.artworkTexture; roughness: .92; roughnessMap: paperGrain }
    PrincipledMaterial { id: glass; baseColor: Qt.tint("#afbfc4",Qt.alpha(app.accent,.09)); alphaMode: PrincipledMaterial.Blend; opacity: .065; roughness: .16; clearcoatAmount: .2; clearcoatRoughnessAmount: .18; cullMode: Material.BackFaceCulling; depthDrawMode: Material.NeverDepthDraw }
    PrincipledMaterial { id: glassEdge; baseColor: "#9eacb0"; alphaMode: PrincipledMaterial.Blend; opacity: .3; roughness: .18; depthDrawMode: Material.NeverDepthDraw }
    component Plate: Model {
        id: plate
        property int vinylControl: -1
        property vector3d size: Qt.vector3d(20,20,4)
        property real rounding: 2
        property Material finish: enclosure
        pickable: true
        geometry: DeckGeometry { dimensions: plate.size; radius: plate.rounding }
        materials: [finish]
    }
    component Ring: Model {
        id: ring
        property int vinylControl: -1
        property real radius: 10
        property real hole: .2
        property real depth: 2
        property bool grooves: false
        property Material finish: armFinish
        pickable: true
        geometry: RecordGeometry { radius: ring.radius; hole: ring.hole; depth: ring.depth; grooves: ring.grooves }
        materials: [finish]
    }
    component Screw: Node {
        Ring { radius: 2.5; hole: .3; depth: 1.1; finish: steel }
        Plate { size: Qt.vector3d(3.4,.65,.25); z: .65; rounding: .2; finish: rubber }
    }
    Plate { objectName: "threeDEnclosure"; size: Qt.vector3d(535,398,42); z: -23; rounding: 3 }
    Plate { objectName: "threeDEnclosureSeam"; size: Qt.vector3d(533,396,1.1); z: -4.2; rounding: 2; finish: rubber }
    Plate { objectName: "threeDTopPlate"; size: Qt.vector3d(535,398,2.5); z: -1.2; rounding: 2; finish: topFinish }
    Plate { size: Qt.vector3d(511,378,4); z: -45; rounding: 5; finish: rubber }
    Repeater3D {
        model: 4
        Screw { required property int index; x: index%2?178:-178; y: index<2?124:-124; z: -47.5; eulerRotation.x: 180 }
    }
    Repeater3D {
        model: 12
        Plate { required property int index; x: -44+index*8; y: 60; z: -47.1; size: Qt.vector3d(3,52,.4); rounding: 1.5; finish: enclosure }
    }
    Model { objectName: "threeDFrontRibs"; geometry: TurntableDetailGeometry { fascia: true } x: 4; y: -199.5; z: -23; eulerRotation.x: 90; materials: armFinish }
    Repeater3D {
        model: 4
        Node {
            required property int index
            x: index%2?214:-214; y: index<2?148:-148; z: -52
            Ring { radius: 28; depth: 14; finish: rubber }
            Ring { radius: 25.5; depth: 3; z: 6; finish: armFinish }
            Ring { radius: 27.8; hole: 26.5; depth: .8; z: -3; finish: armFinish }
            Ring { radius: 25; depth: 2.5; z: -8; finish: rubber }
        }
    }
    Repeater3D {
        model: 2
        Node {
            id: speedKey
            required property int index
            readonly property bool selected: player.vinylSpeed===(index?45:33)
            readonly property real pressure: Math.max(0,travel.value)
            property SpunSpring travel: SpunSpring { targetValue: deck.pressedControl===speedKey.index && deck.armed?1:0; epsilon: .005 }
            x: -217+index*40; y: -199.5; z: -20; eulerRotation.x: 90
            Ring { radius: 12.5; depth: .8; finish: rubber }
            Plate { objectName: "vinylSpeedKey"+speedKey.index; vinylControl: speedKey.index; size: Qt.vector3d(22,22,2.5); rounding: 11; z: 1.5-speedKey.pressure; finish: armFinish }
            Ring { radius: 1.2; depth: .1; z: 2.85-speedKey.pressure; finish: PrincipledMaterial { baseColor: speedKey.selected?deck.app.accent:"#59616a"; lighting: PrincipledMaterial.NoLighting } }
            Model { source: "#Rectangle"; y: -17; z: 1; scale: Qt.vector3d(.23,.085,1); materials: PrincipledMaterial {
                lighting: PrincipledMaterial.NoLighting; alphaMode: PrincipledMaterial.Blend; baseColorMap: Texture { sourceItem: Text { width: 92; height: 34; text: speedKey.index?"45":"33⅓"; font.pixelSize: 26; font.family: "sans-serif"; color: "#b7bfc2"; horizontalAlignment: Text.AlignHCenter } }
            } }
        }
    }
    // Local record coordinates retain the same artwork and seek ring contract.
    Node {
        id: recordSpace
        x: -50; y: 0; z: 16
        Ring { radius: 155; depth: 3; z: -15; finish: rubber }
        Ring { objectName: "threeDPlatter"; radius: 182.5; depth: 15; z: -7; finish: accentMetal }
        Ring { objectName: "threeDPlatterLip"; radius: 182.5; hole: 181.5; depth: .7; z: .6; finish: steel }
        Ring { radius: 180.5; depth: 2; z: 1.1; finish: rubber }
        Node {
            objectName: "threeDMedium"
            x: deck.app.swapOffset*.86; opacity: deck.app.incomingOpacity
            eulerRotation.z: -deck.app.spinAngle
            Ring { objectName: "threeDRecord"; radius: 176; hole: 3.5; depth: 2.8; grooves: true; z: 4; finish: discFinish }
            Ring { objectName: "threeDRunout"; radius: 78; hole: 62.6; depth: .07; z: 5.47; finish: runoutFinish }
            Ring { radius: 175.7; hole: 174; depth: .06; z: 5.46; finish: runoutFinish }
            Repeater3D {
                objectName: "threeDTrackBands"
                model: deck.app.recordMap ? deck.app.recordMap.rows.length-1 : 0
                Ring {
                    required property int index
                    readonly property real angle: (20+22*deck.app.recordMap.rows[index+1].start/deck.app.recordMap.total)*Math.PI/180
                    radius: Math.hypot(202.1-240.8*Math.sin(angle),120.4-240.8*Math.cos(angle))
                    hole: radius-.65; depth: .06; z: 5.46; finish: runoutFinish
                }
            }
            Ring { objectName: "threeDRecordLabel"; radius: 62; hole: 3.5; depth: .25; z: 5.6; finish: paper }
            Ring { radius: 62.6; hole: 61.8; depth: .3; z: 5.55; finish: discFinish }
        }
        Node {
            objectName: "threeDOutgoingMedium"
            visible: deck.app.outgoingOpacity>0; x: deck.app.outgoingOffset*.86; opacity: deck.app.outgoingOpacity
            eulerRotation.z: -deck.app.outgoingAngle
            Ring { radius: 176; hole: 3.5; depth: 2.8; z: 4; grooves: true; finish: discFinish }
            Ring { radius: 62; hole: 3.5; depth: .25; z: 5.6; finish: PrincipledMaterial { baseColorMap: deck.outgoingTexture; roughness: .92 } }
        }
        Ring { radius: 5.5; hole: 3; depth: .8; z: 6; finish: steel }
        Model { source: "#Cylinder"; z: 10; eulerRotation.x: 90; scale: Qt.vector3d(.055,.1,.055); materials: steel }
        Model { source: "#Cone"; z: 16; eulerRotation.x: 90; scale: Qt.vector3d(.055,.04,.055); materials: steel }
        Model {
            visible: deck.app.deckPlayer.count>0
            geometry: WaveGeometry { progress: deck.app.recordVisualProgress; phase: deck.app.wavePhase; amplitude: deck.app.deckPlayer.playing?2.4:0 }
            materials: PrincipledMaterial { baseColor: deck.app.accent; lighting: PrincipledMaterial.NoLighting; cullMode: Material.NoCulling }
        }
        Node {
            objectName: "threeDTonearm"
            x: 202.1; y: 120.4; z: -2
            Ring { radius: 32; depth: 8; z: -8; finish: armFinish }
            Ring { objectName: "threeDBearingCollar"; radius: 22; depth: 8; z: 0; finish: armFinish }
            Ring { radius: 19; hole: 16; depth: 1; z: 4.6; finish: steel }
            Plate { objectName: "threeDBearingHousing"; size: Qt.vector3d(34,30,26); z: 20; rounding: 9; finish: armFinish }
            Repeater3D { model: 2; Node { required property int index; x: index?18:-18; z: 22; eulerRotation.y: 90; Ring { radius: 6; depth: 1.5; finish: armFinish } Screw { z: 1 } } }
            Node {
                id: armPivot
                z: 24
                eulerRotation.z: deck.app.tonearm ? -deck.app.tonearm.armAngle : 4
                eulerRotation.x: deck.app.tonearm ? -5*(1-deck.app.tonearm.lowered) : 0
                Model { objectName: "threeDArmTube"; pickable: true; property int vinylControl: 30; source: "#Cylinder"; y: -105; scale: Qt.vector3d(.065,2.1,.065); materials: armFinish }
                Model { source: "#Cylinder"; y: 25; scale: Qt.vector3d(.07,.5,.07); materials: steel }
                Model { objectName: "threeDCounterweight"; source: "#Cylinder"; y: 33; scale: Qt.vector3d(.31,.26,.31); materials: PrincipledMaterial { baseColor: "#282e34"; metalness: .8; roughness: .4; normalMap: knurl; normalStrength: .25 } }
                Ring { radius: 15.8; hole: 14.6; depth: 1; y: 46.5; eulerRotation.x: 90; finish: armFinish }
                Model { source: "#Cylinder"; y: 18; scale: Qt.vector3d(.27,.045,.27); materials: rubber }
                Repeater3D { model: 12; Plate { required property int index; x: Math.sin(index*Math.PI/6)*13.6; z: Math.cos(index*Math.PI/6)*13.6; y: 18; size: Qt.vector3d(.45,2,.5); rounding: .1; eulerRotation.y: index*30; finish: steel } }
                Model { source: "#Cylinder"; y: -206; scale: Qt.vector3d(.10,.1,.10); materials: PrincipledMaterial { baseColor: "#41484d"; metalness: .9; roughness: .34; normalMap: knurl; normalStrength: .4 } }
                Node {
                    y: -223; z: -1
                    Model { objectName: "threeDHeadshell"; property int vinylControl: 30; pickable: true; geometry: TurntableDetailGeometry {} materials: armFinish }
                    Plate { vinylControl: 30; y: 12; size: Qt.vector3d(22,6,2); rounding: 1.2; finish: armFinish }
                    Plate { vinylControl: 30; y: -15; size: Qt.vector3d(22,12,2); rounding: 1.4; finish: armFinish }
                    Repeater3D { model: 2; Plate { required property int index; vinylControl: 30; x: index?10:-10; size: Qt.vector3d(2,20,2); rounding: .6; finish: armFinish } }
                    Screw { x: -6.5; y: -13; z: 1.8 } Screw { x: 6.5; y: -13; z: 1.8 }
                    Plate { objectName: "threeDFingerLift"; vinylControl: 30; x: 17; y: -5; z: 3; size: Qt.vector3d(17,3,2); rounding: 1; eulerRotation.y: -18; finish: armFinish }
                    Plate { vinylControl: 30; x: 24; y: -5; z: 5; size: Qt.vector3d(3,3,5); rounding: 1; finish: armFinish }
                    Repeater3D {
                        model: 4
                        Node {
                            required property int index
                            x: -4.5+index*3; y: 4; z: -4
                            Model { source: "#Cylinder"; scale: Qt.vector3d(.013,.09,.013); eulerRotation.x: -28; materials: PrincipledMaterial { baseColor: ["#743f38","#d6d2bb","#3e6571","#456951"][index]; roughness: .72 } }
                            Ring { y: 4; z: -2; radius: 1.1; depth: 2; eulerRotation.x: 90; finish: steel }
                        }
                    }
                    Plate { objectName: "threeDCartridge"; vinylControl: 30; y: -10; z: -6; size: Qt.vector3d(15,23,10); rounding: 1.3; finish: rubber }
                    Plate { vinylControl: 30; y: -16; z: -10; size: Qt.vector3d(16,11,7); rounding: 1; finish: PrincipledMaterial { baseColor: "#d4cbb5"; roughness: .62 } }
                    Plate { y: -17.8; z: -13.5; size: Qt.vector3d(1,8,1); rounding: .3; eulerRotation.x: 12; finish: steel }
                    Model { id: stylus; objectName: "threeDStylus"; source: "#Cone"; y: -17.8; z: -15.5; eulerRotation.x: -90; scale: Qt.vector3d(.012,.027,.012); materials: steel }
                }
            }
        }
    }
    // The cue lever shares playback state with the needle and transport bar.
    Node {
        id: cue; x: 199; y: 104; z: 8
        Ring { radius: 8; depth: 12; finish: armFinish }
        Node {
            z: 6; eulerRotation.x: tilt.value
            property SpunSpring tilt: SpunSpring { targetValue: (deck.app.deckPlayer.playing?-24:22)+(deck.pressedControl===2&&deck.armed?7:0); epsilon: .01 }
            Plate { vinylControl: 2; size: Qt.vector3d(5,5,24); z: 12; rounding: 1; finish: armFinish }
            Plate { id: cueCap; objectName: "vinylCueLever"; vinylControl: 2; size: Qt.vector3d(10,9,6); z: 24; rounding: 3; finish: rubber }
        }
    }
    Node { x: 162; y: 70; z: 8; Ring { radius: 5; depth: 16; finish: armFinish } Plate { size: Qt.vector3d(14,4,4); z: 9; rounding: 1; finish: rubber } }
    Node { x: 204; y: 143; z: 6; Ring { radius: 9; depth: 7; finish: accentMetal } Ring { radius: 6.5; depth: 1; z: 4; finish: armFinish } }
    Repeater3D {
        model: 2
        Node {
            required property int index
            x: index?194:-194; y: 187; z: 7
            Plate { size: Qt.vector3d(35,22,9); rounding: 2; finish: rubber }
            Model { source: "#Cylinder"; eulerRotation.z: 90; scale: Qt.vector3d(.09,.29,.09); materials: steel }
            Plate { size: Qt.vector3d(32,4,31); y: 6; z: 10; rounding: 2; finish: armFinish }
        }
    }
    Node {
        id: lid
        objectName: "vinylDustCover"
        y: 187; z: 7; eulerRotation.x: -deck.coverAngle
        Model { property int vinylControl: 3; pickable: true; objectName: "threeDLid"; y: -185; z: 70; source: "#Cube"; scale: Qt.vector3d(5.24,3.74,.016); materials: glass; castsShadows: false; receivesShadows: false }
        Model { property int vinylControl: 3; pickable: true; x: -261; y: -185; z: 35; source: "#Cube"; scale: Qt.vector3d(.7,3.71,.016); eulerRotation.y: 90; materials: glass; castsShadows: false; receivesShadows: false }
        Model { property int vinylControl: 3; pickable: true; x: 261; y: -185; z: 35; source: "#Cube"; scale: Qt.vector3d(.7,3.71,.016); eulerRotation.y: -90; materials: glass; castsShadows: false; receivesShadows: false }
        Model { property int vinylControl: 3; pickable: true; y: -371; z: 35; source: "#Cube"; scale: Qt.vector3d(5.21,.7,.016); eulerRotation.x: 90; materials: glass; castsShadows: false; receivesShadows: false }
        Repeater3D { model: 2; Plate { vinylControl: 3; required property int index; x: index?261:-261; y: -185; z: 70; size: Qt.vector3d(1.2,369,1.2); rounding: .5; finish: glassEdge; castsShadows: false } }
        Plate { vinylControl: 3; y: -371; z: 70; size: Qt.vector3d(511,1.5,1.5); rounding: .7; finish: glassEdge; castsShadows: false }
        Plate { vinylControl: 3; y: -372; z: 7; size: Qt.vector3d(31,4,5); rounding: 2; finish: glassEdge; castsShadows: false }
    }
    // Rear connections are modeled as sockets, without vendor marks or radio claims.
    Node {
        y: 200; z: -23; eulerRotation.x: -90
        Plate { size: Qt.vector3d(218,29,2); rounding: 4; finish: rubber }
        Repeater3D { model: 2; Node { required property int index; x: -70+index*28; Ring { radius: 7; hole: 3; depth: 3; finish: steel } Ring { radius: 5.6; hole: 3; depth: 3.2; finish: PrincipledMaterial { baseColor: index?"#cbc9b9":"#a34236"; roughness: .6 } } } }
        Ring { x: 68; radius: 7; hole: 3.5; depth: 2; finish: armFinish }
        Screw { x: -98; z: 1.5 } Screw { x: 98; z: 1.5 }
    }
}
