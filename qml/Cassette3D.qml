import QtQuick
import QtQuick3D
import Spun 1.0

Node {
    id: deck
    objectName: "cassettePlayer3D"
    required property var app
    required property Texture labelTexture
    property alias lidNode: door
    readonly property color glassColor: Qt.tint("#c5d2d8",Qt.alpha(app.accent,.08))
    readonly property real doorAngle: app.cassetteControls ? app.cassetteControls.doorAngle : 0
    function controlPoint(index) {
        if(index<4)return mapPositionToScene(Qt.vector3d(-125+index*49,157.3,-12))
        if(index===4)return mapPositionToScene(Qt.vector3d(214.5,75,-12))
        return door.mapPositionToScene(Qt.vector3d(0,264,3))
    }
    Texture { id: brushed; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "brushed" } }
    Texture { id: turned; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "turned" } }
    Texture { id: beads; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "bead-normal" } }
    Texture { id: fineMetal; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "metal-normal" } }
    Texture { id: knurl; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "knurled-normal" } }
    Texture { id: molded; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "molded" } }
    PrincipledMaterial { id: aluminum; baseColor: "#cdd2d5"; metalness: .82; roughness: .38; roughnessMap: brushed; normalMap: beads; normalStrength: .14 }
    PrincipledMaterial { id: metal; baseColor: "#a8b1b7"; metalness: .85; roughness: .4; roughnessMap: brushed; normalMap: fineMetal; normalStrength: .12 }
    PrincipledMaterial { id: steel; baseColor: "#d6dadd"; metalness: 1; roughness: .24; roughnessMap: turned }
    PrincipledMaterial { id: stamped; baseColor: "#b7bec2"; metalness: .88; roughness: .36; roughnessMap: brushed; normalMap: fineMetal; normalStrength: .1 }
    PrincipledMaterial { id: rubber; baseColor: "#15191c"; roughness: .84; roughnessMap: molded }
    PrincipledMaterial { id: tapePlastic; baseColor: "#dedbcc"; roughness: .54; roughnessMap: molded }
    PrincipledMaterial { id: gold; baseColor: "#c89d57"; metalness: .95; roughness: .29; roughnessMap: turned; normalMap: fineMetal; normalStrength: .13 }
    PrincipledMaterial { id: glass; baseColor: deck.glassColor; alphaMode: PrincipledMaterial.Blend; opacity: .06; roughness: .13; clearcoatAmount: .3; clearcoatRoughnessAmount: .16; depthDrawMode: Material.NeverDepthDraw }
    PrincipledMaterial { id: glassEdge; baseColor: "#a5b9bf"; alphaMode: PrincipledMaterial.Blend; opacity: .4; roughness: .2; depthDrawMode: Material.NeverDepthDraw }
    ReelGeometry { id: reelWeb }
    CassetteGearGeometry { id: gearMesh }
    component Solid: Model {
        id: solid
        property vector3d size: Qt.vector3d(20,20,4)
        property real rounding: 2
        property color tint: "#30363b"
        property int cassetteControl: -1
        property Material finish: PrincipledMaterial { baseColor: solid.tint; roughness: .55; roughnessMap: molded }
        pickable: true
        geometry: DeckGeometry { dimensions: solid.size; radius: solid.rounding }
        materials: [finish]
    }
    component Ring: Model {
        id: ring
        property real radius: 10
        property real hole: .2
        property real depth: 2
        property bool grooves: false
        property int cassetteControl: -1
        property Material finish: rubber
        pickable: true
        geometry: RecordGeometry { radius: ring.radius; hole: ring.hole; depth: ring.depth; grooves: ring.grooves }
        materials: [finish]
    }
    component Screw: Node {
        Ring { radius: 2.7; hole: .25; depth: 1.2; finish: steel }
        Solid { size: Qt.vector3d(3.5,.7,.25); z: .7; rounding: .2; finish: rubber }
        Solid { size: Qt.vector3d(.7,3.5,.25); z: .7; rounding: .2; finish: rubber }
    }
    component TapeSpan: Model {
        property vector2d start
        property vector2d end
        source: "#Cube"
        x: (start.x+end.x)/2; y: (start.y+end.y)/2; z: 14
        eulerRotation.z: Math.atan2(end.y-start.y,end.x-start.x)*180/Math.PI
        scale: Qt.vector3d(Math.hypot(end.x-start.x,end.y-start.y)/100,.012,.03)
        materials: PrincipledMaterial { baseColor: "#352017"; roughness: .47 }
    }
    // An open frame keeps the front tape and rear transport in separate cavities.
    Solid { objectName: "threeDEnclosure"; size: Qt.vector3d(400,282,5); z: -14; rounding: 7; finish: aluminum }
    Repeater3D { model: 2; Solid { required property int index; x: index?198:-198; z: -12; size: Qt.vector3d(8,280,78); rounding: 3; finish: aluminum } }
    Repeater3D { model: 2; Solid { required property int index; y: index?138:-138; z: -12; size: Qt.vector3d(394,8,78); rounding: 3; finish: aluminum } }
    Repeater3D { model: 2; Solid { required property int index; x: index?192:-192; z: 25.8; size: Qt.vector3d(1.2,267,1.2); rounding: .5; finish: steel } }
    Repeater3D { model: 2; Solid { required property int index; y: index?132:-132; z: 25.8; size: Qt.vector3d(385,1.2,1.2); rounding: .5; finish: steel } }
    Solid { objectName: "threeDTopPlate"; size: Qt.vector3d(375,251,1.5); z: -10.5; rounding: 4; finish: metal }
    Repeater3D { model: 4; Screw { required property int index; x: index%2?182:-182; y: index<2?122:-122; z: -8.8 } }
    // Recesses and stamped brackets remain visible around a loaded cassette.
    Repeater3D { model: 2; Node { required property int index; x: index?178:-178; z: -2; Solid { size: Qt.vector3d(9,187,12); rounding: 2; finish: rubber } Solid { size: Qt.vector3d(7,49,2); y: 48; z: 8; finish: steel } Screw { y: 58; z: 10 } } }
    Repeater3D { model: 2; Node { required property int index; x: index?78.6:-78.6; y: 13; z: -4; Ring { radius: 15; depth: 7; finish: rubber } Ring { radius: 6; depth: 12; finish: steel } } }
    Node {
        objectName: "threeDMedium"
        x: app.swapOffset*.86; opacity: app.incomingOpacity
            Node {
                visible: deck.app.deckPlayer.count>0
                objectName: "threeDCassette"
                Solid { objectName: "cassetteShell3D"; size: Qt.vector3d(346,214,12); y: 4; z: 4; rounding: 12; finish: PrincipledMaterial { baseColor: Qt.darker(deck.app.cassetteShell,1.4); alphaMode: PrincipledMaterial.Blend; opacity: player.cassetteFinish === "clear" ? .68 : 1; roughness: .32 } }
                Solid { objectName: "threeDTapeWell"; size: Qt.vector3d(296,136,3); y: 13; z: 10.2; rounding: 25; finish: rubber }
                Solid { objectName: "threeDLabelBacking"; size: Qt.vector3d(320,36,.8); y: 91; z: 13.1; rounding: 6; tint: "#aa9e88" }
                Solid { size: Qt.vector3d(318,34,1.5); y: 91; z: 14; rounding: 6; finish: PrincipledMaterial { baseColorMap: deck.labelTexture; roughness: .88 } }
                Solid { size: Qt.vector3d(326,34,9); y: -76; z: 12; rounding: 6; tint: deck.app.cassetteShell }
                Repeater3D {
                    model: 2
                    Node {
                        id: reel
                        required property int index
                        objectName: "threeDReel"+index
                        x: index?78.6:-78.6; y: 13; z: 13
                        readonly property real fill: index?deck.app.cassetteVisualProgress:1-deck.app.cassetteVisualProgress
                        Ring { radius: 60; hole: .2; depth: .6; z: -2; finish: rubber }
                        Ring { radius: Math.sqrt(1936+3993*reel.fill)*.77; hole: 31; depth: 5; grooves: true; finish: PrincipledMaterial { baseColor: "#32231b"; roughness: .64 } }
                        Ring { objectName: "threeDReelBezel"+reel.index; radius: 62; hole: 60; depth: 1.5; z: 8; finish: rubber }
                        Node {
                            objectName: "threeDHub"+reel.index
                            eulerRotation.z: -(reel.index?deck.app.cassetteRightAngle:deck.app.cassetteLeftAngle)
                            Model { objectName: "threeDReelWeb"+reel.index; geometry: reelWeb; z: 2; materials: tapePlastic }
                            Ring { radius: 27.5; hole: 25.5; depth: .8; z: 5.2; finish: tapePlastic }
                        }
                    }
                }
                Solid { objectName: "threeDTapeRibbon"; size: Qt.vector3d(286,1.2,3); y: -58; z: 14; rounding: .3; tint: "#352017" }
                Repeater3D {
                    model: 2
                    Node {
                        id: tapeGuide
                        required property int index
                        readonly property real side: index?1:-1
                        readonly property real packRadius: Math.sqrt(1936+3993*(index?deck.app.cassetteVisualProgress:1-deck.app.cassetteVisualProgress))*.77
                        readonly property real tangent: Math.sqrt(64.4*64.4+61*61-Math.pow(packRadius-10,2))/(64.4*64.4+61*61)
                        readonly property real radial: (packRadius-10)/(64.4*64.4+61*61)
                        readonly property vector2d normal: Qt.vector2d(side*(64.4*radial+61*tangent),-61*radial+64.4*tangent)
                        TapeSpan {
                            objectName: "threeDTapeFeed"+tapeGuide.index
                            start: Qt.vector2d(tapeGuide.side*78.6+tapeGuide.packRadius*tapeGuide.normal.x,13+tapeGuide.packRadius*tapeGuide.normal.y)
                            end: Qt.vector2d(tapeGuide.side*143+10*tapeGuide.normal.x,-48+10*tapeGuide.normal.y)
                        }
                        Ring { x: tapeGuide.side*143; y: -59; z: 14; radius: 4; hole: 1.2; depth: 5; finish: steel }
                    }
                }
                Repeater3D {
                    model: 2
                    Node {
                        required property int index
                        x: index?143:-143; y: -48; z: 13
                        Ring { radius: 10; hole: 3; depth: 7; finish: rubber }
                        Ring { radius: 3; hole: .5; depth: 8; finish: metal }
                    }
                }
                Solid { objectName: "threeDTapePressureSpring"; size: Qt.vector3d(44,3,1); y: -61; z: 12; rounding: .5; finish: gold }
                Solid { size: Qt.vector3d(18,4,3); y: -60; z: 13; rounding: 1; tint: "#948372" }
                Solid { size: Qt.vector3d(36,18,1); y: -83; z: 17; rounding: 3; finish: rubber }
                Solid { objectName: "threeDTapeHead"; size: Qt.vector3d(26,13,5); y: -83; z: 19; rounding: 3; finish: steel }
                Solid { size: Qt.vector3d(.7,11,.3); y: -83; z: 21.6; rounding: .2; tint: "#34383b" }
                Repeater3D {
                    model: 8
                    Solid { required property int index; x: (index<4?-1:1)*(123+(index%4)*8); y: -73; z: 17; size: Qt.vector3d(2,20,1); rounding: .5; tint: Qt.darker(deck.app.cassetteShell,1.3) }
                }
                Solid { size: Qt.vector3d(5,111,4); y: 13; z: 21; rounding: 2; tint: deck.app.cassetteShell }
                Solid { objectName: "threeDTapeWindow"; castsShadows: false; receivesShadows: false; size: Qt.vector3d(296,136,1); y: 13; z: 23; rounding: 25; finish: PrincipledMaterial { baseColor: deck.glassColor; metalness: 0; alphaMode: PrincipledMaterial.Blend; opacity: .065; roughness: .19; clearcoatAmount: .6; clearcoatRoughnessAmount: .15 } }
                Repeater3D {
                    model: 4
                    Node {
                        required property int index
                        x: (index%2?1:-1)*159; y: (index<2?1:-1)*94; z: 13
                        Ring { radius: 3.8; hole: .4; depth: 2; finish: metal }
                        Solid { size: Qt.vector3d(5,1,1); z: 1.2; rounding: .2; tint: "#202428" }
                    }
                }
            }
    }
    // The real transport has a heavy flywheel and a belt. They are visible from the back.
    Node {
        objectName: "cassetteRearMechanism"; z: -20; eulerRotation.y: 180
        Solid { size: Qt.vector3d(365,241,3); rounding: 5; finish: rubber }
        Model { objectName: "cassetteStampedCarrier"; x: 20; y: 49; z: 4; geometry: CassettePlateGeometry {} materials: stamped }
        // Turned-up flanges and bearing bosses give the carrier a physical edge.
        Solid { x: 20; y: 104; z: 7; size: Qt.vector3d(250,2,9); rounding: .8; finish: stamped }
        Solid { x: -108; y: 25; z: 7; size: Qt.vector3d(2,53,9); rounding: .8; finish: stamped }
        Solid { x: 78; y: 37; z: 7; size: Qt.vector3d(2,37,8); rounding: .6; finish: stamped }
        Repeater3D { model: 3; Node { required property int index; x: [-64,23,137][index]; y: [89,29,90][index]; z: 7; Ring { radius: 5.5; hole: 2; depth: 4; finish: stamped } Screw { z: 2.6 } } }
        Solid { x: 55; y: -30; z: 4; size: Qt.vector3d(28,96,4); rounding: 2; finish: steel; eulerRotation.z: -24 }
        Solid { x: -64; y: 28; z: 5; size: Qt.vector3d(15,110,3); rounding: 2; finish: metal; eulerRotation.z: 28 }
        Node {
            objectName: "cassetteFlywheel"; x: -114; y: -28; z: 13; eulerRotation.z: app.cassetteLeftAngle*1.8
            Ring { radius: 48; depth: 12; finish: gold }
            Ring { radius: 48.9; hole: 47.4; depth: 2.2; z: 8; finish: rubber }
            Ring { radius: 46.8; hole: 45.8; depth: .7; z: 6.3; finish: steel }
            Ring { radius: 7; depth: 1; z: 6.5; finish: steel }
            Screw { z: 7.3 }
        }
        Node {
            x: 87; y: 68; z: 12
            Ring { radius: 24; depth: 18; finish: rubber }
            Ring { radius: 19; depth: 1.8; z: 9.5; finish: stamped }
            Ring { radius: 10; depth: 20; finish: gold }
            Ring { radius: 10.8; hole: 9.5; depth: 2.2; z: 9; finish: rubber }
            Ring { radius: 3; depth: 22; finish: steel }
            Repeater3D { model: 2; Screw { required property int index; x: index?15:-15; z: 11 } }
        }
        // Tangent belt runs between the motor pulley and the flywheel.
        Solid { x: -22.5; y: 47; z: 21; size: Qt.vector3d(227.6,1.8,2.2); eulerRotation.z: 15.8; rounding: .5; finish: rubber }
        Solid { x: -1; y: -6; z: 21; size: Qt.vector3d(225.3,1.8,2.2); eulerRotation.z: 35.2; rounding: .5; finish: rubber }
        Repeater3D {
            model: [{x:-32,y:-64,s:1.35},{x:13,y:-50,s:1.05},{x:40,y:-67,s:.72},{x:79,y:-56,s:1.4}]
            Node {
                required property var modelData; required property int index
                x: modelData.x; y: modelData.y; z: 9
                Model { objectName: "cassetteGear"+index; geometry: gearMesh; scale: Qt.vector3d(modelData.s,modelData.s,1); eulerRotation.z: (index%2?1:-1)*app.cassetteRightAngle/modelData.s; materials: index===3?rubber:tapePlastic }
                Ring { radius: 3; depth: 6; z: 1; finish: steel }
            }
        }
        Repeater3D { model: 6; Screw { required property int index; x: [-172,169,-93,20,153,60][index]; y: [102,99,-102,91,-99,22][index]; z: 7 } }
        // Tension spring between the selector rail and its return anchor.
        Node {
            x: -63; y: 63; z: 12; eulerRotation.z: -28
            Solid { size: Qt.vector3d(1.1,45,1.1); rounding: .4; finish: steel }
            Repeater3D { model: 10; Ring { required property int index; radius: 2.6; hole: 1.6; depth: .65; y: -12+index*2.7; eulerRotation.x: 90; finish: steel } }
        }
        Solid { x: 137; y: 70; z: 6; size: Qt.vector3d(19,58,2); rounding: 2; eulerRotation.z: -20; tint: "#a36a2d" }
        Repeater3D { model: 5; Solid { required property int index; x: 132+index*2; y: 72; z: 7.3; size: Qt.vector3d(.7,45,.3); rounding: .2; eulerRotation.z: -20; finish: gold } }
        Solid { objectName: "cassetteRearGlass"; size: Qt.vector3d(388,268,1.6); z: 33; rounding: 6; finish: glass; castsShadows: false; receivesShadows: false }
        Repeater3D { model: 4; Screw { required property int index; x: index%2?184:-184; y: index<2?124:-124; z: 34.4 } }
    }
    Repeater3D {
        model: 4
        Node {
            id: button; required property int index
            readonly property real pressure: deck.app.cassetteControls ? deck.app.cassetteControls.keyItems[index].pressure : 0
            x: -125+index*49; y: 142; z: -12; eulerRotation.x: -90
            Ring { cassetteControl: button.index; radius: 18; depth: 1.4; finish: rubber }
            Ring { objectName: "cassetteKey3D"+button.index; cassetteControl: button.index; radius: 16.2; depth: 14; z: 8-3.6*button.pressure; finish: aluminum }
            Node {
                z: 15.2-3.6*button.pressure
                // Raised inlaid legends remain legible while viewing the top edge.
                Model { source: "#Rectangle"; property int cassetteControl: button.index; pickable: true; scale: Qt.vector3d(.15,.15,1); materials: PrincipledMaterial { lighting: PrincipledMaterial.NoLighting; alphaMode: PrincipledMaterial.Blend; baseColorMap: Texture { sourceItem: Item { width: 48; height: 48; Glyph { anchors.fill: parent; visible: button.index<3; name: ["play","previous","next","play"][button.index]; ink: "#424b51" } Rectangle { anchors.centerIn: parent; width: 22; height: 22; color: "#424b51"; visible: button.index===3 } } } } }
            }
        }
    }
    Node {
        x: 201; y: 75; z: -12; eulerRotation.y: 90
        Ring { radius: 19; depth: 1.5; finish: rubber }
        Ring { objectName: "cassetteVolume3D"; cassetteControl: 4; radius: 17; depth: 12; z: 7; finish: PrincipledMaterial { baseColor: "#c7cdd0"; metalness: .88; roughness: .37; normalMap: knurl; normalStrength: .26; roughnessMap: brushed } }
        Ring { cassetteControl: 4; radius: 15.2; depth: .7; z: 13.4; finish: steel }
        Solid { cassetteControl: 4; x: 10; z: 14; size: Qt.vector3d(5,1,.4); rounding: .3; finish: rubber }
    }
    Node {
        x: 202.5; y: -23; z: -12; eulerRotation.y: 90
        Solid { size: Qt.vector3d(38,139,1); rounding: 14; finish: metal }
        Solid { y: 8; z: 1; size: Qt.vector3d(8,27,2); rounding: 3.8; finish: rubber }
        Solid { y: 8; z: 2.1; size: Qt.vector3d(1.8,17,1); rounding: .6; finish: metal }
        Ring { y: -42; z: 1.5; radius: 9; hole: 4.8; depth: 2; finish: rubber }
        Ring { y: -42; z: 3; radius: 5.5; hole: 4.8; depth: .5; finish: steel }
        Ring { y: 46; z: 1; radius: 1.3; depth: .8; finish: PrincipledMaterial { baseColor: deck.app.deckPlayer.playing?deck.app.accent:"#343b40"; lighting: PrincipledMaterial.NoLighting } }
    }
    Repeater3D { model: 2; Node { required property int index; x: index?142:-142; y: -133; z: 28; Ring { radius: 4.3; depth: 27; eulerRotation.y: 90; finish: metal } } }
    Repeater3D { model: 2; Node { required property int index; x: index?159:-159; y: -133; z: 28; eulerRotation.y: 90; Ring { radius: 2; depth: 2; finish: steel } } }
    Node {
        id: door; objectName: "cassetteDoor3D"
        y: -133; z: 30; eulerRotation.x: deck.doorAngle
        Solid { objectName: "threeDLid"; pickable: false; y: 133; size: Qt.vector3d(382,264,1.8); rounding: 6; finish: glass; castsShadows: false; receivesShadows: false }
        Repeater3D { model: 2; Solid { required property int index; x: index?190:-190; y: 133; size: Qt.vector3d(3,260,4.5); rounding: 1; finish: glassEdge; castsShadows: false } }
        Repeater3D { model: 2; Solid { required property int index; y: index?263:3; size: Qt.vector3d(379,3,4.5); rounding: 1; finish: glassEdge; castsShadows: false } }
        Solid { objectName: "cassetteDoorLatch3D"; cassetteControl: 5; y: 264; z: 3; size: Qt.vector3d(30,12,8); rounding: 2; finish: aluminum }
        Repeater3D { model: 2; Solid { required property int index; x: index?183:-183; y: 152; z: -3; size: Qt.vector3d(7,33,8); rounding: 1.5; finish: glassEdge; castsShadows: false } }
    }
    Model {
        objectName: "cassetteProgress3D"; y: -15.2; z: 31
        visible: app.deckPlayer.count>0
        geometry: WaveGeometry { progress: app.cassetteVisualProgress; phase: app.wavePhase; amplitude: app.deckPlayer.playing?2.4:0; linear: true }
        materials: PrincipledMaterial { baseColor: app.accent; lighting: PrincipledMaterial.NoLighting; cullMode: Material.NoCulling }
    }
}
