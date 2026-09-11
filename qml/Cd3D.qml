import QtQuick
import QtQuick3D
import Spun 1.0

Node {
    id: deck
    objectName: "cdPlayer3D"
    required property var app
    required property Texture artworkTexture
    required property Texture outgoingTexture
    property alias lidNode: lid
    readonly property real doorAngle: app.cdControls ? app.cdControls.doorAngle : 0
    function controlPoint(index) {
        if(index<6)return front.mapPositionToScene(Qt.vector3d(81+28*(index%3),index<3?10:-10,5))
        if(index===6)return front.mapPositionToScene(Qt.vector3d(184,0,12))
        if(index===7)return lid.mapPositionToScene(Qt.vector3d(0,-394,2))
        return front.mapPositionToScene(Qt.vector3d(index===8?-126:-91,0,4))
    }
    Texture { id: brushed; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "brushed" } }
    Texture { id: turned; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "turned" } }
    Texture { id: micro; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "metal-normal" } }
    Texture { id: molded; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "molded" } }
    Texture { id: optical; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "disc" } }
    Texture { id: knurl; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "knurled-normal" } }
    PrincipledMaterial { id: aluminum; baseColor: "#cbd0d3"; metalness: .86; roughness: .34; roughnessMap: brushed; normalMap: micro; normalStrength: .11 }
    PrincipledMaterial { id: polished; baseColor: "#d0d6d8"; metalness: 1; roughness: .22; roughnessMap: turned }
    PrincipledMaterial { id: rubber; baseColor: "#12171b"; roughness: .85; roughnessMap: molded }
    PrincipledMaterial { id: graphite; baseColor: "#343e44"; metalness: .72; roughness: .36; roughnessMap: brushed }
    PrincipledMaterial { id: tray; baseColor: "#aab2b8"; metalness: .68; roughness: .43; roughnessMap: brushed }
    PrincipledMaterial { id: glass; baseColor: Qt.tint("#dce6e9",Qt.alpha(app.accent,.07)); alphaMode: PrincipledMaterial.Blend; opacity: .065; roughness: .13; metalness: 0; clearcoatAmount: .65; clearcoatRoughnessAmount: .12; cullMode: Material.BackFaceCulling; depthDrawMode: Material.NeverDepthDraw }
    PrincipledMaterial { id: glassEdge; baseColor: "#a9bec4"; alphaMode: PrincipledMaterial.Blend; opacity: .34; roughness: .16; metalness: .18; cullMode: Material.NoCulling }
    PrincipledMaterial { id: discSilver; baseColorMap: optical; metalness: .86; roughness: .22; clearcoatAmount: .75; clearcoatRoughnessAmount: .13 }
    component Solid: Model {
        id: solid
        property vector3d size: Qt.vector3d(20,20,10)
        property real rounding: 2
        property Material finish: aluminum
        property int cdControl: -1
        pickable: true
        geometry: DeckGeometry { dimensions: solid.size; radius: solid.rounding }
        materials: [finish]
    }
    component Ring: Model {
        id: ring
        property real radius: 10
        property real hole: .2
        property real depth: 2
        property int cdControl: -1
        property Material finish: rubber
        pickable: true
        geometry: RecordGeometry { radius: ring.radius; hole: ring.hole; depth: ring.depth }
        materials: [finish]
    }
    component Screw: Node {
        Ring { radius: 2.3; depth: 1; finish: polished }
        Solid { size: Qt.vector3d(3,.65,.25); rounding: .2; z: .6; finish: rubber }
        Solid { size: Qt.vector3d(.65,3,.25); rounding: .2; z: .6; finish: rubber }
    }
    // Separate extrusions leave a real recessed well below the disc and lid.
    Solid { objectName: "threeDEnclosure"; size: Qt.vector3d(426,420,8); z: -51; rounding: 7 }
    Repeater3D { model: 2; Solid { required property int index; x: index?209:-209; z: -23; size: Qt.vector3d(8,412,56); rounding: 3 } }
    Repeater3D { model: 2; Solid { required property int index; y: index?206:-206; z: -23; size: Qt.vector3d(420,8,56); rounding: 3 } }
    Solid { objectName: "threeDTopPlate"; size: Qt.vector3d(410,404,4); z: -10; rounding: 4; finish: tray }
    Ring { radius: 188; hole: 181; depth: 11; z: -3; finish: graphite }
    Ring { radius: 181; depth: 2; z: -6; finish: rubber }
    Ring { radius: 187.8; hole: 186.7; depth: .5; z: 2.8; finish: polished }
    // Finger scoops and suspension pads outside the CD recess.
    Repeater3D { model: 2; Ring { required property int index; x: index?135:-135; y: -135; z: -6.7; radius: 19; depth: 1; finish: rubber } }
    Repeater3D { model: 4; Screw { required property int index; x: index%2?197:-197; y: index<2?193:-193; z: -6.5 } }
    Solid { objectName: "cdLaserSled"; x: 0; y: 83; z: -3; size: Qt.vector3d(26,104,4); rounding: 3; finish: graphite }
    Repeater3D { model: 2; Solid { required property int index; x: index?9:-9; y: 82; z: -.5; size: Qt.vector3d(1.6,97,1.6); rounding: .7; finish: polished } }
    Ring { y: 57; z: 1.5; radius: 5; depth: 1.8; finish: PrincipledMaterial { baseColor: "#466a85"; metalness: .6; roughness: .13; clearcoatAmount: 1 } }
    Node {
        objectName: "threeDMedium"; visible: app.deckPlayer.count>0
        x: app.swapOffset*.86; z: app.lidOpen*24; opacity: app.incomingOpacity
        eulerRotation.z: -app.spinAngle
        Ring { objectName: "threeDRecord"; radius: 176; hole: 15; depth: 1.2; z: 5; finish: discSilver }
        Ring { objectName: "threeDRecordLabel"; radius: 169; hole: 32; depth: .22; z: 5.76; finish: PrincipledMaterial { baseColorMap: deck.artworkTexture; roughness: .53; clearcoatAmount: .18; clearcoatRoughnessAmount: .3 } }
        Ring { objectName: "threeDOpticalRim"; radius: 175.6; hole: 169.2; depth: .12; z: 5.66; finish: discSilver }
        Ring { radius: 33; hole: 31.4; depth: .25; z: 5.85; finish: polished }
        Ring { radius: 31.4; hole: 15; depth: .8; z: 5.4; finish: glassEdge; castsShadows: false }
        Ring { radius: 26; hole: 25.4; depth: .25; z: 5.9; finish: polished }
    }
    Node {
        objectName: "threeDOutgoingMedium"; visible: app.outgoingOpacity>0
        x: app.outgoingOffset*.86; z: app.lidOpen*24; opacity: app.outgoingOpacity; eulerRotation.z: -app.outgoingAngle
        Ring { radius: 176; hole: 15; depth: 1.2; z: 5; finish: discSilver }
        Ring { radius: 169; hole: 32; depth: .22; z: 5.76; finish: PrincipledMaterial { baseColorMap: deck.outgoingTexture; roughness: .53 } }
    }
    Node {
        objectName: "threeDCdSpindle"; z: 7; eulerRotation.z: -app.spinAngle
        Ring { radius: 14.3; hole: 2; depth: 9; finish: graphite }
        Ring { radius: 11.8; hole: 3.2; depth: .7; z: 4.9; finish: polished }
        Ring { radius: 2.8; depth: 9.6; finish: polished }
        Repeater3D { model: 3; Node { required property int index; eulerRotation.z: index*120; Model { objectName: "threeDSpindleClip"+index; source: "#Sphere"; x: 13; z: 1; scale: Qt.vector3d(.05,.05,.05); materials: polished } Screw { x: 7; z: 5.4 } } }
    }
    Repeater3D {
        model: 2
        Node {
            required property int index; x: index?137:-137; y: 196; z: 11
            Solid { size: Qt.vector3d(35,15,8); rounding: 2; finish: graphite }
            Ring { radius: 5.5; depth: 31; eulerRotation.y: 90; finish: polished }
            Repeater3D { model: 3; Ring { required property int index; radius: 5.65; hole: 4.7; depth: .6; x: -9+index*9; eulerRotation.y: 90; finish: graphite } }
        }
    }
    Node {
        id: lid; y: 197; z: 16; eulerRotation.x: -deck.doorAngle
        Solid { objectName: "threeDLid"; size: Qt.vector3d(407,399,2.4); y: -197; rounding: 4; finish: glass; castsShadows: false; receivesShadows: false }
        Repeater3D { model: 2; Solid { required property int index; x: index?203:-203; y: -197; size: Qt.vector3d(4,397,4.5); rounding: 1.2; finish: polished; castsShadows: false } }
        Repeater3D { model: 2; Solid { required property int index; y: index?-395:1; size: Qt.vector3d(405,4,4.5); rounding: 1.2; finish: polished; castsShadows: false } }
        Repeater3D { model: 2; Solid { required property int index; x: index?200:-200; y: -197; z: .1; size: Qt.vector3d(.9,391,2.8); rounding: .3; finish: glassEdge; castsShadows: false } }
        Solid { objectName: "cdLidLatch3D"; cdControl: 7; y: -394; z: 1.4; size: Qt.vector3d(31,11,5); rounding: 2; finish: graphite }
        Solid { cdControl: 7; y: -394; z: 4.1; size: Qt.vector3d(17,1.1,.5); rounding: .4; finish: polished }
    }
    Node {
        id: front; y: -211; z: -24; eulerRotation.x: 90
        Solid { objectName: "cdFrontGlass"; size: Qt.vector3d(411,48,2.3); rounding: 17; finish: PrincipledMaterial { baseColor: "#0c1419"; roughness: .18; metalness: .14; clearcoatAmount: .9; clearcoatRoughnessAmount: .1 } }
        Repeater3D { model: 2; Node { required property int index; x: index?-154:-183; z: 2; Ring { radius: index?5.8:8; hole: index?3.8:5.5; depth: 2; finish: polished } Ring { radius: index?3.8:5.5; depth: .4; finish: rubber } } }
        Model {
            objectName: "cdDisplay3D"; source: "#Rectangle"; x: -7; z: 1.5; scale: Qt.vector3d(1.14,.36,1)
            materials: PrincipledMaterial { lighting: PrincipledMaterial.NoLighting; baseColorMap: Texture { sourceItem: CdDisplay { app: deck.app } } }
        }
        Repeater3D {
            model: 6
            Node {
                id: key; required property int index; x: 81+28*(index%3); y: index<3?10:-10
                readonly property real pressure: deck.app.cdControls ? deck.app.cdControls.keyItems[index].pressure : 0
                Ring { cdControl: key.index; radius: 8; depth: 1; z: 1.8; finish: rubber }
                Ring { objectName: "cdKey3D"+key.index; cdControl: key.index; radius: 6.6; depth: 3.7; z: 3.5-1.6*key.pressure; finish: graphite }
                Model {
                    property int cdControl: key.index; pickable: true; source: "#Rectangle"; z: 5.5-1.6*key.pressure; scale: Qt.vector3d(.065,.065,1)
                    materials: PrincipledMaterial { lighting: PrincipledMaterial.NoLighting; alphaMode: PrincipledMaterial.Blend; baseColorMap: Texture { sourceItem: Item { width: 32; height: 32; Glyph { anchors.fill: parent; visible: key.index!==1; name: [deck.app.deckPlayer.playing?"pause":"play","play","more","previous","next","volume"][key.index]; ink: "#c0cbcf" } Rectangle { anchors.centerIn: parent; width: 16; height: 16; visible: key.index===1; color: "#c0cbcf" } } } }
                }
            }
        }
        Node {
            x: 184; eulerRotation.z: 135-270*deck.app.deckPlayer.volume
            Ring { cdControl: 6; radius: 19; depth: 1.5; z: 1.8; finish: rubber }
            Ring { objectName: "cdVolume3D"; cdControl: 6; radius: 17.6; depth: 8; z: 6; finish: PrincipledMaterial { baseColor: "#cad0d3"; metalness: .9; roughness: .33; roughnessMap: brushed; normalMap: knurl; normalStrength: .22 } }
            Ring { cdControl: 6; radius: 16.6; depth: .8; z: 10.5; finish: polished }
            Solid { cdControl: 6; y: 12; z: 11.1; size: Qt.vector3d(1.1,5,.4); rounding: .3; finish: graphite }
        }
        Repeater3D {
            model: 2
            Node {
                required property int index; x: index?-91:-126
                Solid { cdControl: index+8; size: Qt.vector3d(9,24,2); rounding: 2; z: 1.8; finish: rubber }
                Solid { cdControl: index+8; size: Qt.vector3d(6.5,8,3); rounding: 1.4; z: 3.1; y: index ? (deck.app.deckPlayer.repeatMode-1)*6 : deck.app.deckPlayer.shuffle?6:-6; finish: polished }
                Model { source: "#Rectangle"; x: 10; z: 1.6; scale: Qt.vector3d(.07,.07,1); materials: PrincipledMaterial { lighting: PrincipledMaterial.NoLighting; alphaMode: PrincipledMaterial.Blend; baseColorMap: Texture { sourceItem: Glyph { width: 32; height: 32; name: index?"repeat":"shuffle"; ink: "#aeb9bd" } } } }
            }
        }
    }
    Node {
        y: 211; z: -24; eulerRotation.x: -90
        Solid { size: Qt.vector3d(346,40,1.3); rounding: 4; finish: graphite }
        Repeater3D { model: 2; Node { required property int index; x: index?139:100; z: 1.7; Ring { radius: index?7.5:6; hole: index?5:3.8; depth: 2; finish: polished } Ring { radius: index?5:3.8; depth: .3; finish: rubber } } }
        Repeater3D { model: 2; Node { required property int index; x: index?0:-120; z: 1.8; Solid { size: Qt.vector3d(21,8,2); rounding: 3.8; finish: rubber } Solid { size: Qt.vector3d(14,2,1); rounding: .6; z: 1.4; finish: tray } } }
        Repeater3D { model: 2; Solid { required property int index; x: index?52:-58; z: 1.6; size: Qt.vector3d(17,7,2); rounding: 1; finish: rubber } }
        Repeater3D { model: 2; Screw { required property int index; x: index?163:-163; z: 2 } }
    }
    Repeater3D {
        model: 4
        Node { required property int index; x: index%2?167:-167; y: index<2?162:-162; z: -56; Ring { radius: 17; depth: 6; finish: graphite } Ring { radius: 15; depth: 4; z: -4; finish: rubber } Screw { z: -6.5; eulerRotation.x: 180 } }
    }
    // The transport perimeter uses the same progress preview as the horizontal bar.
    Model { visible: app.deckPlayer.count>0; geometry: WaveGeometry { progress: app.recordVisualProgress; phase: app.wavePhase; amplitude: app.deckPlayer.playing?1.3:0 } materials: PrincipledMaterial { baseColor: app.accent; lighting: PrincipledMaterial.NoLighting; cullMode: Material.NoCulling } }
}
