import QtQuick
import QtQuick3D
import Spun 1.0

Node {
    id: recorder
    required property var app
    required property Texture artworkTexture
    readonly property var controls: app.recorderView
    Texture { id: fineGrain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "bead-normal" } scaleU: 2; scaleV: 2 }
    Texture { id: knurled; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "knurled-normal" } scaleU: 1.5; scaleV: 1 }
    PrincipledMaterial { id: aluminum; baseColor: "#c3c8cb"; metalness: .85; roughness: .46 }
    PrincipledMaterial { id: faceFinish; baseColor: "#c3c8cb"; metalness: .8; roughness: .52; normalMap: fineGrain; normalStrength: .16 }
    PrincipledMaterial { id: wheelFinish; baseColor: "#b9bfc5"; metalness: .84; roughness: .52; normalMap: fineGrain; normalStrength: .1 }
    PrincipledMaterial { id: polished; baseColor: "#d5dbe0"; metalness: 1; roughness: .27 }
    PrincipledMaterial { id: black; baseColor: "#22292e"; roughness: .62 }
    PrincipledMaterial { id: engraving; baseColor: "#657078"; metalness: .35; roughness: .65 }
    PrincipledMaterial { id: orange; baseColor: "#ed7e1f"; roughness: .63; normalMap: fineGrain; normalStrength: .55 }
    function surface(x,y,z) { return Qt.vector3d(x-205,205-y,z) }
    function controlScenePoint(index) {
        if(!controls || index<0 || index>=controls.controls.length)return Qt.vector3d(0,0,0)
        const key=controls.controls[index]
        return mapPositionToScene(surface(key.x+key.width/2-(index>=8?5:0),key.y+key.height/2,index>=7?-9:16))
    }
    component Plate: Model {
        id: plate
        pickable: true
        property int recorderControl: -1
        property vector3d size: Qt.vector3d(10,10,2)
        property real rounding: 2
        property Material finish: aluminum
        geometry: DeckGeometry { dimensions: plate.size; radius: plate.rounding }
        materials: [finish]
    }
    component Ring: Model {
        id: ring
        pickable: true
        property int recorderControl: -1
        property real radius: 10
        property real hole: .5
        property real depth: 2
        property Material finish: polished
        geometry: RecordGeometry { radius: ring.radius; hole: ring.hole; depth: ring.depth }
        materials: [finish]
    }
    // An open transport bay exposes the key guides when viewed from below.
    Plate { objectName: "threeDRecorderBody"; position: recorder.surface(207,153.5,-5); size: Qt.vector3d(244,267,34); rounding: 12 }
    Plate { position: recorder.surface(207,196,-26); size: Qt.vector3d(244,352,8); rounding: 12 }
    Plate { position: recorder.surface(301,329,-5); size: Qt.vector3d(56,86,34); rounding: 10 }
    Plate { position: recorder.surface(88,329,-5); size: Qt.vector3d(6,86,34); rounding: 3 }
    Plate { objectName: "threeDRecorderOrangeBack"; position: recorder.surface(207,196,-31.5); size: Qt.vector3d(240,348,3); rounding: 10; finish: orange }
    Plate { position: recorder.surface(207,196,12.15); size: Qt.vector3d(242,350,.6); rounding: 11; finish: black }
    Plate { position: recorder.surface(207,196,12.5); size: Qt.vector3d(240.8,348.8,.8); rounding: 10.4; finish: faceFinish }
    Plate { position: recorder.surface(116,12,-9); size: Qt.vector3d(8,24,15); rounding: 1.5; finish: polished }
    Ring { position: recorder.surface(205,160,13.15); radius: 114.5; depth: .6; finish: black }
    Node {
        objectName: "threeDRecorderWheel"
        position: recorder.surface(205,160,14.4)
        eulerRotation.z: -(recorder.controls ? recorder.controls.wheelAngle : recorder.app.spinAngle)
        Ring { objectName: "threeDRecorderWheelMesh"; recorderControl: 20; radius: 113; hole: 31; depth: 2; finish: wheelFinish }
        Ring { recorderControl: 20; radius: 113; hole: 112.5; depth: .22; z: .95; finish: polished }
        Plate { recorderControl: 20; y: 74; z: 1.06; size: Qt.vector3d(.32,73,.04); rounding: 0; finish: engraving }
        Plate { recorderControl: 20; y: -74; z: 1.06; size: Qt.vector3d(.32,73,.04); rounding: 0; finish: engraving }
        Ring { radius: 31; depth: 2.2; finish: black }
        Ring { radius: 30.9; hole: 30.35; depth: .22; z: 1.1 }
        Model {
            objectName: "threeDRecorderArtwork"
            z: 1.15
            geometry: RecordGeometry { radius: 26; hole: .5; depth: .06 }
            materials: PrincipledMaterial { baseColorMap: recorder.artworkTexture; roughness: .46; clearcoatAmount: .22; clearcoatRoughnessAmount: .2 }
        }
        Repeater3D {
            model: 3
            Node {
                required property int index
                x: Math.sin(index*Math.PI*2/3)*28.5; y: Math.cos(index*Math.PI*2/3)*28.5; z: 1.4
                Ring { radius: 1.15; hole: .25; depth: .35 }
                Plate { size: Qt.vector3d(1,.2,.06); z: .2; rounding: .08; finish: black }
            }
        }
    }
    Model {
        z: 12.95; source: "#Rectangle"; scale: Qt.vector3d(4.1,4.1,1)
        materials: PrincipledMaterial {
            baseColorMap: Texture { sourceItem: RecorderSurface { width: 410; height: 410; part: 2 } }
            alphaMode: PrincipledMaterial.Blend; lighting: PrincipledMaterial.NoLighting; depthDrawMode: Material.NeverDepthDraw
        }
    }
    Plate { position: recorder.surface(293,44.5,13); size: Qt.vector3d(55,32,.6); rounding: 4.7; finish: black }
    Plate { position: recorder.surface(293,44.5,13.26); size: Qt.vector3d(53,30,.5); rounding: 4; finish: polished }
    Model {
        objectName: "threeDRecorderDisplay"
        position: recorder.surface(293,44.5,13.55); source: "#Rectangle"; scale: Qt.vector3d(.52,.29,1)
        materials: PrincipledMaterial { baseColorMap: Texture { sourceItem: RecorderDisplay { app: recorder.app } } lighting: PrincipledMaterial.NoLighting }
    }
    Ring { position: recorder.surface(115,263,13.05); radius: 9; depth: .22; finish: PrincipledMaterial { baseColor: "#8c969d"; metalness: .65; roughness: .46 } }
    Plate { position: recorder.surface(178.5,329,13); size: Qt.vector3d(187,86,.8); rounding: 1.8; finish: black }
    Repeater3D {
        model: 3
        Node {
            required property int index
            readonly property var control: recorder.controls ? recorder.controls.keyControls[index] : null
            position: control ? recorder.surface(control.x+control.width/2,control.y+control.height/2,15-1.35*control.pressDepth) : Qt.vector3d(0,0,0)
            eulerRotation.x: control ? 1.2*control.pressDepth : 0
            Model {
                objectName: "threeDRecorderKeyGuide"+parent.index
                source: "#Cylinder"; z: -20; eulerRotation.x: 90; scale: Qt.vector3d(.18,.36,.18)
                materials: polished
            }
            Model { source: "#Cylinder"; z: -31; eulerRotation.x: 90; scale: Qt.vector3d(.23,.04,.23); materials: black }
            Plate { objectName: "threeDRecorderKey"+parent.index; recorderControl: parent.index; size: Qt.vector3d(parent.control?parent.control.width-1.4:60,parent.control?parent.control.height-1.4:83,3.8); rounding: 2 }
            Model {
                source: "#Rectangle"; y: 22; z: 1.94; scale: Qt.vector3d(.13,.13,1)
                materials: PrincipledMaterial { baseColorMap: Texture { sourceItem: Glyph { width: 48; height: 48; name: index===0?"previous":index===2?"next":recorder.app.deckPlayer.playing?"pause":"play"; ink: index===0?"#d76d24":"#252d34" } } alphaMode: PrincipledMaterial.Blend; roughness: .65 }
            }
        }
    }
    Plate { position: recorder.surface(298,255.5,13.01); eulerRotation.z: -46.3; size: Qt.vector3d(25,55,.18); rounding: 12.5; finish: polished }
    Plate { position: recorder.surface(298,255.5,13.12); eulerRotation.z: -46.3; size: Qt.vector3d(23.7,53.7,.18); rounding: 11.8 }
    Repeater3D {
        model: 2
        Node {
            required property int index
            readonly property var control: recorder.controls ? recorder.controls.controls[3+index] : null
            position: control ? recorder.surface(control.x+control.width/2,control.y+control.height/2,14.5-1.0*control.pressDepth) : Qt.vector3d(0,0,0)
            Plate { recorderControl: parent.index+3; size: Qt.vector3d(20,20,2.6); rounding: 10 }
            Ring { recorderControl: parent.index+3; radius: 10; hole: 9.65; depth: .25; z: 1.2 }
            Plate { recorderControl: parent.index+3; z: 1.35; size: Qt.vector3d(6,.4,.06); rounding: .1; finish: black }
            Plate { recorderControl: parent.index+3; visible: parent.index===1; z: 1.35; size: Qt.vector3d(.4,6,.06); rounding: .1; finish: black }
        }
    }
    Node {
        position: recorder.surface(70,162,-1)
        eulerRotation.x: recorder.controls ? 3*(recorder.controls.controls[5].pressDepth-recorder.controls.controls[6].pressDepth) : 0
        Plate { recorderControl: 6; y: 47.5; size: Qt.vector3d(10,91,28); rounding: 4; finish: polished }
        Plate { recorderControl: 5; y: -47.5; size: Qt.vector3d(10,91,28); rounding: 4; finish: polished }
        Plate { x: 3; size: Qt.vector3d(18,17,28); rounding: 8; finish: polished }
        Ring { x: 2; z: 14.2; radius: 3.3; hole: .7; depth: .5 }
        Plate { x: 10; z: -10; size: Qt.vector3d(15,6,8); rounding: 1 }
    }
    Repeater3D {
        model: 2
        Plate { required property int index; position: recorder.surface(294.8+index*14,324.5,13); size: Qt.vector3d(1.6,69,.12); rounding: .75; finish: engraving }
    }
    Plate { position: recorder.surface(301,314,13.02); size: Qt.vector3d(36,.25,.03); rounding: .1; finish: black }
    Model {
        objectName: "threeDRecorderKnob"
        pickable: true; property int recorderControl: 7
        source: "#Cylinder"; position: recorder.surface(294.5,389.5,-9); scale: Qt.vector3d(.35,.29,.35)
        eulerRotation.y: recorder.app.deckPlayer.volume*270
        materials: PrincipledMaterial { baseColor: "#c3cbd1"; metalness: 1; roughness: .34; normalMap: knurled; normalStrength: .65 }
    }
    Model { pickable: true; property int recorderControl: 7; source: "#Cylinder"; position: recorder.surface(294.5,372,-9); scale: Qt.vector3d(.31,.06,.31); materials: polished }
    Model { pickable: true; property int recorderControl: 7; source: "#Cylinder"; position: recorder.surface(294.5,376,-9); scale: Qt.vector3d(.35,.009,.35); materials: polished }
    Model { pickable: true; property int recorderControl: 7; source: "#Cylinder"; position: recorder.surface(294.5,403,-9); scale: Qt.vector3d(.35,.009,.35); materials: polished }
    Model { pickable: true; property int recorderControl: 7; source: "#Cylinder"; position: recorder.surface(294.5,404.4,-9); scale: Qt.vector3d(.31,.006,.31); materials: polished }
    Model { pickable: true; property int recorderControl: 7; source: "#Cylinder"; position: recorder.surface(294.5,404.8,-9); scale: Qt.vector3d(.1,.006,.1); materials: black }
    // Side keys remain useful while inspecting the recorder from an angle.
    Repeater3D {
        model: 3
        Plate {
            required property int index
            readonly property var control: recorder.controls ? recorder.controls.controls[index+8] : null
            objectName: "threeDRecorderSide"+(index+8); recorderControl: index+8
            position: control ? recorder.surface(control.x+control.width/2-5-1.5*control.pressDepth,control.y+control.height/2,-9) : Qt.vector3d(0,0,0)
            size: Qt.vector3d(8,index===0?32:18,index===0?22:18); rounding: index===0?3:8.9
        }
    }
    // Three recessed jacks and a USB port in the upper edge.
    Repeater3D {
        model: 3
        Node {
            required property int index
            position: recorder.surface(178+index*29,19.5,-9)
            eulerRotation.x: 90
            Ring { radius: 4.2; hole: 3.1; depth: .8 }
            Ring { radius: 3.05; depth: .15; finish: black }
        }
    }
    Plate { position: recorder.surface(279,19.5,-9); size: Qt.vector3d(24,6,1); eulerRotation.x: 90; rounding: 2.8; finish: black }
    Repeater3D {
        model: 4
        Node {
            required property int index
            x: (index%2?1:-1)*106+2; y: index<2?174:-148; z: -33.3; eulerRotation.y: 180
            Ring { radius: 2.2; hole: .4; depth: .5 }
            Plate { z: .3; size: Qt.vector3d(2.1,.35,.08); rounding: .15; finish: black }
        }
    }
    Model {
        objectName: "threeDRecorderProgress"
        visible: recorder.app.deckPlayer.count>0
        position: recorder.surface(205,160,16.3); scale: Qt.vector3d(116/186,116/186,1)
        geometry: WaveGeometry { progress: recorder.app.trackVisualProgress; phase: recorder.app.wavePhase; amplitude: recorder.app.deckPlayer.playing?1.6:0 }
        materials: PrincipledMaterial { baseColor: recorder.app.accent; lighting: PrincipledMaterial.NoLighting; cullMode: Material.NoCulling }
    }
}
