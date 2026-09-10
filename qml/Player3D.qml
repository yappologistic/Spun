import QtQuick
import QtQuick3D
import Spun 1.0

View3D {
    id: view
    required property var app
    required property Item surfaceItem
    objectName: "player3DView"
    clip: true
    property real vinylYaw: 0
    property real vinylPitch: 0
    readonly property bool inspectable: app.recorder || app.vinyl
    property real recorderYaw: 0
    property real recorderPitch: 0
    readonly property bool hardwareActive: !!(vinylScene.item && vinylScene.item.pressedControl>=0)
    function cancelHardware() { if(vinylScene.item)vinylScene.item.cancelHardware() }
    property bool orbiting: false
    property real pitchOffset: 0
    property real yawOffset: 0
    readonly property real progress: app.cassette ? app.cassetteVisualProgress : app.recordVisualProgress
    Behavior on pitchOffset { enabled: app.animate && !app.threeDGesture; NumberAnimation { duration: 140 } }
    Behavior on yawOffset { enabled: app.animate && !app.threeDGesture; NumberAnimation { duration: 140 } }
    function tilt(point) {
        if(view.inspectable || !app.animate || app.threeDGesture || app.menuOpen)return
        pitchOffset=(point.y/height-.5)*2
        yawOffset=(point.x/width-.5)*3
    }
    Connections { target: app; function onAnimateChanged(){if(!app.animate){view.pitchOffset=0;view.yawOffset=0}} }
    // Noctalia already exports the wallpaper palette. Reuse its accent without
    // reading wallpaper files or sampling the desktop on every frame.
    // A palette accent is a UI color, often pastel or dark. Preserve its hue
    // while giving the illumination enough brightness and chroma to be visible.
    readonly property color lightingColor: Qt.hsva(Math.max(0,app.accent.hsvHue), Math.min(.65,app.accent.hsvSaturation*1.8), 1, 1)
    property color keyTint: Qt.tint("#ffffff", Qt.alpha(lightingColor, .28))
    property color fillTint: Qt.tint("#ffffff", Qt.alpha(lightingColor, .85))
    property color ambientTint: Qt.tint("#202329", Qt.alpha(lightingColor, .16))
    Behavior on keyTint { enabled: app.animate; ColorAnimation { duration: 450 } }
    Behavior on fillTint { enabled: app.animate; ColorAnimation { duration: 450 } }
    Behavior on ambientTint { enabled: app.animate; ColorAnimation { duration: 450 } }
    camera: sceneCamera
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Transparent
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.Medium
        specularAAEnabled: true
        lightProbe: Texture { textureData: StudioTexture { tint: view.app.accent } }
        probeExposure: .55
        aoEnabled: true; aoStrength: 18; aoDistance: 12; aoSoftness: 50; aoSampleRate: 2
    }
    readonly property vector3d vinylCamera: {
        if(!app.vinyl)return Qt.vector3d(0,0,930)
        // Fit the closed and raised cover throughout inspection. The camera is
        // stable during the hinge animation, and only recalculates on rotation.
        const rotation=assembly.eulerRotation, origin=assembly.position
        let points=[]
        for(const x of [-270,270])for(const y of [-204,204])for(const z of [-63,65])points.push(assembly.mapPositionToScene(Qt.vector3d(x,y,z)))
        for(const degrees of [0,24,48,72]) {
            const a=-degrees*Math.PI/180
            for(const x of [-263,263])for(const y of [0,-373])for(const z of [0,72])points.push(assembly.mapPositionToScene(Qt.vector3d(x,187+y*Math.cos(a)-z*Math.sin(a),7+y*Math.sin(a)+z*Math.cos(a))))
        }
        const cx=(Math.min(...points.map(p=>p.x))+Math.max(...points.map(p=>p.x)))/2
        const cy=(Math.min(...points.map(p=>p.y))+Math.max(...points.map(p=>p.y)))/2
        const focal=height/(2*Math.tan(19*Math.PI/180))
        let distance=710
        for(const p of points)distance=Math.max(distance,p.z+Math.abs(p.x-cx)*focal/(width/2-20),p.z+Math.abs(p.y-cy)*focal/(height/2-16))
        return Qt.vector3d(cx,cy,distance)
    }
    PerspectiveCamera { id: sceneCamera; x: app.vinyl ? view.vinylCamera.x : 0; y: app.vinyl ? view.vinylCamera.y : 0; z: app.vinyl ? view.vinylCamera.z : 710+(app.recorder?0:app.lidOpen*65); clipNear: 10; clipFar: 1500; fieldOfView: 38 }
    DirectionalLight { objectName: "threeDKeyLight"; color: view.keyTint; eulerRotation: Qt.vector3d(-30,-35,0); brightness: 1; ambientColor: view.ambientTint; castsShadow: true; shadowFactor: 35; shadowMapQuality: Light.ShadowMapQualityMedium; shadowMapFar: 1200 }
    DirectionalLight { objectName: "threeDFillLight"; color: view.fillTint; eulerRotation: Qt.vector3d(-10,45,0); brightness: .65 }
    property color caseTint: Qt.tint(app.surface,Qt.alpha(app.accent,.07))
    property color plateTint: Qt.tint(app.surface,Qt.alpha(app.ink,app.light?.12:.045))
    property color metalTint: Qt.tint("#b8bdc0",Qt.alpha(app.accent,.13))
    property color glassTint: Qt.tint("#dce8ed",Qt.alpha(app.accent,.18))
    Behavior on caseTint { enabled: app.animate; ColorAnimation { duration: 450 } }
    Behavior on plateTint { enabled: app.animate; ColorAnimation { duration: 450 } }
    Behavior on metalTint { enabled: app.animate; ColorAnimation { duration: 450 } }
    Behavior on glassTint { enabled: app.animate; ColorAnimation { duration: 450 } }
    Texture { id: brushedGrain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "brushed" } }
    Texture { id: turnedGrain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "turned" } }
    Texture { id: metalMicroSurface; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "metal-normal" } }
    Texture { id: moldedGrain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "molded" } }
    Texture { id: vinylGrain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "vinyl" } }
    Texture { id: opticalFilm; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "disc" } }
    Texture { id: paperGrain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "paper" } }
    Texture { id: knurledGrain; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "knurled-normal" } }
    ReelGeometry { id: reelWeb }
    Texture {
        id: cover; generateMipmaps: true; mipFilter: Texture.Linear
        property CoverTexture imageData: CoverTexture { objectName: "currentCoverTexture"; artwork: presentation.artwork }
        textureData: imageData.texture
    }
    Texture {
        id: tapeLabel; generateMipmaps: true; mipFilter: Texture.Linear
        property CoverTexture imageData: CoverTexture { objectName: "cassetteCoverTexture"; artwork: presentation.artwork; paper: true; title: app.deckPlayer.album || app.deckPlayer.title; artist: app.deckPlayer.artist }
        textureData: imageData.texture
    }
    Texture {
        id: oldCover; generateMipmaps: true; mipFilter: Texture.Linear
        property CoverTexture imageData: CoverTexture { artwork: presentation.outgoing }
        textureData: imageData.texture
    }
    PrincipledMaterial { id: metal; baseColor: view.metalTint; metalness: .86; roughness: .36; roughnessMap: brushedGrain }
    PrincipledMaterial { id: rubber; baseColor: Qt.tint("#15191d",Qt.alpha(app.inset,.20)); roughness: .95; roughnessMap: moldedGrain }
    PrincipledMaterial { id: blackVinyl; baseColor: "#17191d"; metalness: 0; roughness: .48; roughnessMap: vinylGrain; clearcoatAmount: .14; clearcoatRoughnessAmount: .3 }
    PrincipledMaterial { id: smoothVinyl; baseColor: "#17191d"; metalness: 0; roughness: .32; roughnessMap: vinylGrain; clearcoatAmount: .14; clearcoatRoughnessAmount: .3 }
    PrincipledMaterial { id: gold; baseColor: Qt.tint("#d6ae58",Qt.alpha(app.accent,.045)); metalness: 1; roughness: .26; roughnessMap: brushedGrain; normalMap: metalMicroSurface; normalStrength: .12 }
    PrincipledMaterial { id: steel; baseColor: Qt.tint("#cbd0d4",Qt.alpha(app.accent,.025)); metalness: 1; roughness: .34; roughnessMap: turnedGrain; normalMap: metalMicroSurface; normalStrength: .24 }
    PrincipledMaterial { id: counterweightSteel; baseColor: Qt.tint("#c3c8cc",Qt.alpha(app.accent,.025)); metalness: 1; roughness: .31; roughnessMap: brushedGrain; normalMap: knurledGrain; normalStrength: .5 }
    PrincipledMaterial { id: platterMetal; baseColor: Qt.tint("#737b80",Qt.alpha(app.accent,.04)); metalness: 1; roughness: .5; roughnessMap: turnedGrain }
    PrincipledMaterial { id: label; baseColorMap: cover; roughness: app.vinyl?.92:.46; roughnessMap: paperGrain; clearcoatAmount: app.vinyl?0:.18; clearcoatRoughnessAmount: .3 }
    PrincipledMaterial { id: housingFinish; baseColor: view.caseTint; metalness: 0; roughness: .4; roughnessMap: moldedGrain; clearcoatAmount: .22; clearcoatRoughnessAmount: .4 }
    PrincipledMaterial { id: plateFinish; baseColor: view.plateTint; metalness: 0; roughness: .48; roughnessMap: moldedGrain }
    PrincipledMaterial { id: lidEdge; baseColor: view.glassTint; metalness: 0; roughness: .14; alphaMode: PrincipledMaterial.Blend; opacity: .32; clearcoatAmount: .7; clearcoatRoughnessAmount: .12 }
    PrincipledMaterial { id: tapePlastic; baseColor: "#d0cab9"; metalness: 0; roughness: .48; roughnessMap: moldedGrain }
    component Solid: Model {
        id: solidNode
        property vector3d size: Qt.vector3d(20,20,10)
        property real rounding: 2
        property color tint: "#30363b"
        geometry: DeckGeometry { dimensions: solidNode.size; radius: solidNode.rounding }
        property Material finish: PrincipledMaterial { baseColor: solidNode.tint; metalness: 0; roughness: .55; roughnessMap: moldedGrain }
        materials: [finish]
    }
    component Ring: Model {
        id: ringNode
        property real radius: 10
        property real hole: 3
        property real depth: 2
        property bool grooves: false
        geometry: RecordGeometry { radius: ringNode.radius; hole: ringNode.hole; depth: ringNode.depth; grooves: ringNode.grooves }
        property Material finish: rubber
        materials: [finish]
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
    Node {
        id: assembly
        y: app.vinyl ? -45 : 0
        eulerRotation: Qt.vector3d(app.vinyl?-42+view.vinylPitch:app.recorder?-12+view.recorderPitch:-24+view.pitchOffset,app.vinyl?-12+view.vinylYaw:app.recorder?-12+view.recorderYaw:-7+view.yawOffset,0)
        Loader3D { id: recorderScene; active: app.recorder; sourceComponent: Recorder3D { app: view.app; artworkTexture: cover } }
        Loader3D { id: vinylScene; active: app.vinyl; sourceComponent: Vinyl3D { app: view.app; artworkTexture: cover; outgoingTexture: oldCover } }
        Loader3D { id: legacyMedium; active: !app.recorder && !app.vinyl; sourceComponent: Node {
        property alias lidNode: lidPivot
        Solid {
            objectName: "threeDEnclosure"
            y: app.cassette?-4.5:0; z: -15
            size: app.cassette?Qt.vector3d(402,283,28):Qt.vector3d(428,428,28)
            rounding: app.cassette?23:app.vinyl?22:214; tint: view.caseTint
            finish: housingFinish
        }
        Solid {
            objectName: "threeDEnclosureSeam"
            y: app.cassette?-4.5:0; z: -24
            size: app.cassette?Qt.vector3d(402.3,283.3,1.1):Qt.vector3d(428.3,428.3,1.1)
            rounding: app.cassette?23:app.vinyl?22:214; finish: rubber
        }
        Solid {
            objectName: "threeDTopPlate"
            y: app.cassette?-4.5:0; z: .1
            size: app.cassette?Qt.vector3d(388,269,2):Qt.vector3d(414,414,2)
            rounding: app.cassette?19:app.vinyl?17:207; tint: view.plateTint
            finish: plateFinish
        }
        Repeater3D {
            model: app.vinyl?4:0
            Node {
                required property int index
                x: index%2?193:-193; y: index<2?193:-193; z: 2
                Ring { radius: 3.2; hole: .25; depth: 1.2; finish: metal }
                Solid { size: Qt.vector3d(4,.7,.3); z: .8; rounding: .2; tint: "#202228" }
            }
        }
        Repeater3D {
            model: 4
            Model {
                required property int index
                source: "#Cylinder"; x: (index%2?1:-1)*(app.cassette?157:app.vinyl?160:120)
                y: (index<2?1:-1)*(app.cassette?107:app.vinyl?162:120); z: -33
                eulerRotation.x: 90; scale: Qt.vector3d(.24,.1,.24); materials: rubber
            }
        }
        Ring { visible: !app.cassette; radius: 182; hole: 3; depth: 7; z: -1; finish: rubber }
        Ring { visible: !app.cassette; radius: 180; hole: 176.5; depth: 1.1; z: 2.1; finish: app.vinyl?platterMetal:metal }
        Ring { visible: !app.cassette; radius: 183; hole: 179; depth: 3; z: 0; finish: app.vinyl?platterMetal:metal }
        Ring { objectName: "threeDPlatterLip"; visible: app.vinyl; radius: 181.4; hole: 180.3; depth: .7; z: 2.8; finish: steel }
        Node {
            id: medium
            objectName: "threeDMedium"
            x: app.swapOffset*.86
            opacity: app.incomingOpacity
            Node {
                visible: !app.cassette
                eulerRotation.z: -app.spinAngle
                Ring { objectName: "threeDRecord"; radius: 176; hole: app.vinyl?3.5:15; depth: app.vinyl?2.8:1.2; grooves: app.vinyl; z: 4; finish: app.vinyl?blackVinyl:metal }
                Ring { objectName: "threeDRunout"; visible: app.vinyl; radius: 78; hole: 62.6; depth: .07; z: 5.47; finish: smoothVinyl }
                Ring { visible: app.vinyl; radius: 175.7; hole: 174; depth: .06; z: 5.46; finish: smoothVinyl }
                Repeater3D {
                    objectName: "threeDTrackBands"
                    model: app.recordMap ? app.recordMap.rows.length-1 : 0
                    Ring {
                        required property int index
                        readonly property real boundaryAngle: app.recordMap ? (6+20*app.recordMap.rows[index+1].start/app.recordMap.total)*Math.PI/180 : 0
                        radius: Math.hypot(173-197*Math.sin(boundaryAngle),-105+197*Math.cos(boundaryAngle))*.86
                        hole: radius-.8; depth: .08; z: 5.48
                        finish: PrincipledMaterial { baseColor: "#43484a"; metalness: .2; roughness: .38 }
                    }
                }
                Ring { objectName: "threeDRecordLabel"; radius: app.vinyl?62:169; hole: app.vinyl?3.5:32; depth: .25; z: app.vinyl?5.6:4.8; finish: label }
                Ring { visible: app.vinyl; radius: 62.6; hole: 61.8; depth: .3; z: 5.55; finish: blackVinyl }
                Ring { objectName: "threeDOpticalRim"; visible: !app.vinyl; radius: 175.5; hole: 169.2; depth: .18; z: 4.72; finish: PrincipledMaterial { baseColorMap: opticalFilm; metalness: .8; roughness: .19; clearcoatAmount: .65; clearcoatRoughnessAmount: .12 } }
                Ring { visible: !app.vinyl; radius: 33; hole: 31.5; depth: .2; z: 5; finish: metal }
                Ring { castsShadows: false; visible: !app.vinyl; radius: 32; hole: 15; depth: .4; z: 4.9; finish: PrincipledMaterial { baseColor: view.glassTint; metalness: .3; roughness: .15; alphaMode: PrincipledMaterial.Blend; opacity: .6 } }
                Ring { objectName: "threeDHubPressing"; visible: !app.vinyl; radius: 27.5; hole: 26.7; depth: .25; z: 5.15; finish: steel }
                Ring { visible: !app.vinyl; radius: 16.2; hole: 15; depth: .5; z: 5; finish: lidEdge; castsShadows: false }
            }
            Node {
                visible: app.cassette
                objectName: "threeDCassette"
                Solid { size: Qt.vector3d(346,214,12); y: 4; z: 4; rounding: 12; finish: PrincipledMaterial { baseColor: Qt.darker(app.cassetteShell,1.4); alphaMode: PrincipledMaterial.Blend; opacity: player.cassetteFinish === "clear" ? .68 : 1; roughness: .32 } }
                Solid { objectName: "threeDTapeWell"; size: Qt.vector3d(296,136,3); y: 13; z: 10.2; rounding: 25; finish: rubber }
                Solid { objectName: "threeDLabelBacking"; size: Qt.vector3d(320,36,.8); y: 91; z: 13.1; rounding: 6; tint: "#aa9e88" }
                Solid { size: Qt.vector3d(318,34,1.5); y: 91; z: 14; rounding: 6; finish: PrincipledMaterial { baseColorMap: tapeLabel; roughness: .88 } }
                Solid { size: Qt.vector3d(326,34,9); y: -76; z: 12; rounding: 6; tint: app.cassetteShell }
                Repeater3D {
                    model: 2
                    Node {
                        id: reel
                        required property int index
                        objectName: "threeDReel"+index
                        x: index?78.6:-78.6; y: 13; z: 13
                        readonly property real fill: index?view.progress:1-view.progress
                        Ring { radius: 60; hole: .2; depth: .6; z: -2; finish: rubber }
                        Ring { radius: Math.sqrt(1936+3993*reel.fill)*.77; hole: 31; depth: 5; grooves: true; finish: PrincipledMaterial { baseColor: "#32231b"; roughness: .64 } }
                        Ring { objectName: "threeDReelBezel"+reel.index; radius: 62; hole: 60; depth: 1.5; z: 8; finish: rubber }
                        Node {
                            objectName: "threeDHub"+reel.index
                            eulerRotation.z: -(reel.index?app.cassetteRightAngle:app.cassetteLeftAngle)
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
                        readonly property real packRadius: Math.sqrt(1936+3993*(index?view.progress:1-view.progress))*.77
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
                    Solid { required property int index; x: (index<4?-1:1)*(123+(index%4)*8); y: -73; z: 17; size: Qt.vector3d(2,20,1); rounding: .5; tint: Qt.darker(app.cassetteShell,1.3) }
                }
                Solid { size: Qt.vector3d(5,111,4); y: 13; z: 21; rounding: 2; tint: app.cassetteShell }
                Solid { objectName: "threeDTapeWindow"; castsShadows: false; receivesShadows: false; size: Qt.vector3d(296,136,1); y: 13; z: 23; rounding: 25; finish: PrincipledMaterial { baseColor: view.glassTint; metalness: 0; alphaMode: PrincipledMaterial.Blend; opacity: .065; roughness: .19; clearcoatAmount: .6; clearcoatRoughnessAmount: .15 } }
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
        Node {
            objectName: "threeDOutgoingMedium"
            visible: app.outgoingOpacity>0
            x: app.outgoingOffset*.86; opacity: app.outgoingOpacity
            Ring { visible: !app.cassette; radius: 176; hole: app.vinyl?3.5:15; depth: 2.8; grooves: app.vinyl; z: 4; finish: app.vinyl?blackVinyl:metal }
            Ring { visible: !app.cassette; radius: app.vinyl?62:169; hole: app.vinyl?3.5:32; depth: .25; z: 5.6; eulerRotation.z: -app.outgoingAngle; finish: PrincipledMaterial { baseColorMap: oldCover; roughness: .85 } }
            Solid { visible: app.cassette; size: Qt.vector3d(346,214,15); rounding: 12; z: 6; tint: app.cassetteShell }
        }
        Node {
            id: lidPivot
            visible: !app.vinyl
            y: app.cassette?108:194; z: 18
            eulerRotation.x: -app.lidOpen*40
            Solid {
                objectName: "threeDLid"
                castsShadows: false; receivesShadows: false
                y: app.cassette?-112.5:-194
                size: app.cassette?Qt.vector3d(356,225,2):Qt.vector3d(388,388,2)
                rounding: app.cassette?12:194
                finish: PrincipledMaterial { baseColor: view.glassTint; alphaMode: PrincipledMaterial.Blend; opacity: .065; metalness: .1; roughness: .16; cullMode: Material.NoCulling }
            }
            Ring { objectName: "threeDLidRim"; visible: !app.cassette; y: -194; radius: 193.5; hole: 190; depth: 3.5; finish: lidEdge; castsShadows: false; receivesShadows: false }
            Solid { objectName: "threeDLidLatch"; y: app.cassette?-224:-383; z: 1.5; size: Qt.vector3d(28,6,4); rounding: 2; finish: lidEdge; castsShadows: false }
        }
        Repeater3D {
            model: 2
            Model { required property int index; source: "#Cylinder"; x: (index?1:-1)*(app.cassette?124:app.vinyl?140:48); y: app.cassette?109:app.vinyl?207:191; z: 14; eulerRotation.z: 90; scale: Qt.vector3d(.065,.24,.065); materials: metal }
        }
        Model { visible: app.vinyl; source: "#Cylinder"; z: 9; eulerRotation.x: 90; scale: Qt.vector3d(.07,.16,.07); materials: steel }
        Model { visible: app.vinyl; source: "#Sphere"; z: 16; scale: Qt.vector3d(.07,.07,.07); materials: steel }
        Ring { visible: app.vinyl; radius: 4.7; hole: 3.5; depth: .6; z: 7; finish: steel }
        Node {
            objectName: "threeDCdSpindle"
            visible: !app.cassette && !app.vinyl
            z: 8
            Ring { radius: 13.8; hole: 3; depth: 6; finish: rubber }
            Ring { radius: 11.8; hole: 9.5; depth: .55; z: 3.2; finish: steel }
            Model { source: "#Cylinder"; eulerRotation.x: 90; scale: Qt.vector3d(.045,.073,.045); materials: steel }
            Repeater3D {
                model: 3
                Node {
                    required property int index
                    eulerRotation.z: index*120
                    Solid { objectName: "threeDSpindleClip"+index; x: 12.5; z: 2; size: Qt.vector3d(7.5,4,2.2); rounding: 1.5; finish: steel }
                }
            }
        }
        Node {
            visible: app.vinyl && !!app.tonearm
            objectName: "threeDTonearm"
            x: 135.88; y: 103.2; z: 11
            Model { source: "#Cylinder"; eulerRotation.x: 90; scale: Qt.vector3d(.25,.12,.25); materials: rubber }
            Ring { objectName: "threeDBearingCollar"; radius: 16.5; hole: 10; depth: 3; z: 1.5; finish: steel }
            Ring { objectName: "threeDBearingHousing"; radius: 15; hole: 9; depth: 10; z: 8; eulerRotation.x: 90; finish: steel }
            Repeater3D {
                model: 2
                Node {
                    required property int index
                    x: index?15:-15; z: 8; eulerRotation.y: 90
                    Ring { radius: 4; hole: .6; depth: 2; finish: steel }
                    Solid { size: Qt.vector3d(5,.6,.3); z: 1.1; rounding: .2; finish: rubber }
                }
            }
            Model { source: "#Cylinder"; z: 8; eulerRotation.x: 90; scale: Qt.vector3d(.16,.09,.16); materials: gold }
            Node {
                id: armPivot
                eulerRotation.z: app.tonearm?-app.tonearm.armAngle:4
                eulerRotation.x: app.tonearm?-6*(1-app.tonearm.lowered):0
                z: 12
                Model { objectName: "threeDArmTube"; source: "#Cylinder"; y: -79; scale: Qt.vector3d(.045,1.58,.045); materials: gold }
                Model { objectName: "threeDCounterweight"; source: "#Cylinder"; y: 23; scale: Qt.vector3d(.23,.24,.23); materials: counterweightSteel }
                Repeater3D {
                    model: 2
                    Ring { required property int index; radius: 11.6; hole: 10.6; depth: .8; y: 12+index*22; eulerRotation.x: 90; finish: steel }
                }
                Model { source: "#Cylinder"; y: 10; scale: Qt.vector3d(.10,.06,.10); materials: rubber }
                Model { source: "#Cylinder"; y: 34; scale: Qt.vector3d(.24,.03,.24); materials: rubber }
                Solid { objectName: "threeDFingerLift"; x: 9; y: -158; z: 6; size: Qt.vector3d(14,3,2); rounding: 1; eulerRotation.y: -18; finish: steel }
                Solid { objectName: "threeDHeadshell"; y: -159; z: 3; size: Qt.vector3d(15,27,2); rounding: 3; finish: platterMetal }
                Repeater3D {
                    model: 2
                    Model { required property int index; source: "#Cylinder"; x: index?3:-3; y: -160; z: 5; eulerRotation.x: 90; scale: Qt.vector3d(.022,.025,.022); materials: metal }
                }
                Solid { objectName: "threeDCartridge"; y: -165; z: -2; size: Qt.vector3d(12,21,11); rounding: 3; tint: Qt.tint(app.surface,Qt.alpha("#c6a25a",.15)) }
                Solid { y: -167; z: 4; size: Qt.vector3d(6,10,1); rounding: 1; finish: gold }
                Solid { y: -169.42; z: -7; size: Qt.vector3d(3,6,5); rounding: .5; tint: "#171b1d" }
                Model { objectName: "threeDStylus"; source: "#Cone"; y: -169.42; z: -13; eulerRotation.x: -90; scale: Qt.vector3d(.015,.09,.015); materials: metal }
            }
        }
        Node {
            visible: app.packageOpacity>0; opacity: app.packageOpacity
            x: 126; y: -107; z: 28; eulerRotation.z: -12
            Solid { objectName: "threeDAlbumCase"; size: Qt.vector3d(130,130,5); rounding: 3; finish: metal }
            Solid { size: Qt.vector3d(120,122,.5); x: 3; z: 3; rounding: 1; finish: label }
        }
        Model {
            visible: app.deckPlayer.count>0
            geometry: WaveGeometry { progress: view.progress; phase: app.wavePhase; amplitude: app.deckPlayer.playing?2.4:0; linear: app.cassette }
            materials: PrincipledMaterial { baseColor: app.accent; lighting: PrincipledMaterial.NoLighting; cullMode: Material.NoCulling }
        }
    }
    } }
    // Pointer math intersects the same plane as the physical controls. It never
    // installs a Qt Quick 3D transform on the shared 2D control tree.
    function projectSurface(x,y) { return projectSurfaceDepth(x,y,app.recorder?16:.5) }
    function projectSurfaceDepth(x,y,depth) {
        const space=app.vinyl && vinylScene.item ? vinylScene.item.recordSpace : assembly
        const p=mapFrom3DScene(space.mapPositionToScene(Qt.vector3d(x-265,294-y,depth)))
        return Qt.point(p.x,p.y)
    }
    function unprojectSurface(x,y) { return unprojectPlane(x,y,app.recorder?16:.5) }
    function unprojectPlane(x,y,depth) {
        const space=app.vinyl && vinylScene.item ? vinylScene.item.recordSpace : assembly
        const a=space.mapPositionFromScene(mapTo3DScene(Qt.vector3d(x,y,0)))
        const b=space.mapPositionFromScene(mapTo3DScene(Qt.vector3d(x,y,1000)))
        const t=(depth-a.z)/(b.z-a.z)
        return Qt.point(a.x+(b.x-a.x)*t+265,294-a.y-(b.y-a.y)*t)
    }
    function projectRecorderControl(index) { const p=mapFrom3DScene(recorderScene.item.controlScenePoint(index));return Qt.point(p.x,p.y) }
    function recorderHit(x,y) {
        const hit=pick(x,y)
        if(!hit.objectHit)return {control:null,body:false,depth:16}
        const index=hit.objectHit.recorderControl
        const controls=app.recorderView
        const control=controls && index>=0 && index<controls.controls.length ? controls.controls[index].pointerInput : controls && index===20 ? controls.wheelInput : null
        return {control:control && control.enabled ? control : null,body:true,depth:assembly.mapPositionFromScene(hit.scenePosition).z}
    }
    function resetRecorderOrientation() { recorderYaw=0;recorderPitch=0 }
    function resetVinylOrientation() { vinylYaw=0;vinylPitch=0 }
    function projectVinylNeedle() { const p=mapFrom3DScene(vinylScene.item.needlePoint());return Qt.point(p.x,p.y) }
    function projectVinylControl(index) { const p=mapFrom3DScene(vinylScene.item.controlPoint(index));return Qt.point(p.x,p.y) }
    function vinylHit(x,y) {
        if(app.menuOpen || app.swapRunning)return {control:null,body:false,depth:.5,hardware:-1}
        const hit=pick(x,y)
        if(!hit.objectHit)return {control:null,body:false,depth:.5,hardware:-1}
        const index=hit.objectHit.vinylControl
        if(index>=0 && index<=3)return {control:null,body:true,depth:.5,hardware:index}
        // A back or underside hit must not operate controls through the housing.
        const normal=assembly.mapDirectionToScene(Qt.vector3d(0,0,1))
        if(index===30 && normal.z>.12 && app.tonearm && app.tonearm.canSeek)
            return {control:app.tonearm.pointerInput,body:true,depth:vinylScene.item.recordSpace.mapPositionFromScene(hit.scenePosition).z,hardware:-1}
        const point=unprojectSurface(x,y)
        const control=normal.z>.12 ? app.controlAt3DPoint(point) : null
        return {control:control,body:true,depth:.5,hardware:-1}
    }
    function lidBounds() {
        if(!legacyMedium.item)return Qt.rect(0,0,0,0)
        let left=width,top=height,right=0,bottom=0
        for(let i=0;i<32;++i){const a=i*Math.PI/16;const point=app.cassette?Qt.vector3d(i%2?178:-178,i<16?0:-225,0):Qt.vector3d(194*Math.cos(a),-194+194*Math.sin(a),0);const p=mapFrom3DScene(legacyMedium.item.lidNode.mapPositionToScene(point));left=Math.min(left,p.x);right=Math.max(right,p.x);top=Math.min(top,p.y);bottom=Math.max(bottom,p.y)}
        return Qt.rect(left,top,right-left,bottom-top)
    }
    MouseArea {
        id: pointer
        objectName: "threeDPointer"
        property var hoveredControl: null
        property bool hintDismissed: false
        property point pressPoint
        property bool windowDragAllowed: false
        property bool hoveredBody: false
        property real startYaw: 0
        property real startPitch: 0
        property real grabDepth: 16
        property string hint: ""
        function updateHint(x,y) {
            const point=view.unprojectSurface(x,y)
            const hit=view.app.vinyl ? view.vinylHit(x,y) : view.app.recorder ? view.recorderHit(x,y) : null
            const target=hit ? hit.control : view.app.controlAt3DPoint(point)
            hoveredBody=!!hit && hit.body
            if(hit && hit.hardware>=0) {
                hoveredControl=null;hoveredBody=true
                hint=["33⅓ RPM", "45 RPM", view.app.deckPlayer.playing?"Lift needle and pause":"Lower needle and play", vinylScene.item.coverOpen?"Close dust cover":"Open dust cover"][hit.hardware]
                return
            }
            if(target!==hoveredControl)hintDismissed=false
            hoveredControl=target
            if(!target){hint=hoveredBody ? "Drag to rotate · double-click to reset" : "";return}
            if(view.app.recorder) { hint=target.hint || ""
            } else if(view.app.vinyl && view.app.tonearm && target===view.app.tonearm.pointerInput) {
                hint="Drag to seek · Shift for precision"
            } else {
                const p=view.app.pointerEvent(target,point,0)
                let fraction
                if(view.app.cassette)fraction=target.fraction(p.x)
                else { let a=Math.atan2(p.y-220,p.x-220)+Math.PI/2;if(a<0)a+=2*Math.PI;fraction=a/(2*Math.PI) }
                const track=view.app.recordTarget(fraction)
                hint=track?track.track.title+" · "+view.app.time(track.position):"Seek to "+view.app.time(fraction*view.app.deckPlayer.duration)
            }
        }
        anchors.fill: parent; hoverEnabled: true; preventStealing: true; acceptedButtons: Qt.AllButtons
        onPressed: mouse => {
            hintDismissed=true;pressPoint=Qt.point(mouse.x,mouse.y)
            const hit=view.app.vinyl ? view.vinylHit(mouse.x,mouse.y) : view.app.recorder ? view.recorderHit(mouse.x,mouse.y) : null
            grabDepth=hit ? hit.depth : .5
            if(mouse.button===Qt.LeftButton && hit && hit.hardware>=0) { vinylScene.item.beginHardware(hit.hardware);windowDragAllowed=false;return }
            if(mouse.button===Qt.LeftButton)view.app.begin3DPointer(view.unprojectPlane(mouse.x,mouse.y,grabDepth),mouse.modifiers,hit ? hit.control : null)
            view.orbiting=mouse.button===Qt.LeftButton && !!hit && hit.body && !view.app.threeDPointer && !view.app.menuOpen
            startYaw=view.app.vinyl?view.vinylYaw:view.recorderYaw;startPitch=view.app.vinyl?view.vinylPitch:view.recorderPitch
            windowDragAllowed=mouse.button===Qt.LeftButton && !view.app.threeDPointer && !view.orbiting
        }
        onPositionChanged: mouse => {
            if(pressed) {
                if(view.app.vinyl && vinylScene.item && vinylScene.item.pressedControl>=0) {
                    vinylScene.item.armed=view.vinylHit(mouse.x,mouse.y).hardware===vinylScene.item.pressedControl
                    return
                }
                view.app.move3DPointer(view.unprojectPlane(mouse.x,mouse.y,grabDepth),mouse.modifiers)
                if(view.orbiting) {
                    if(view.app.vinyl) {
                        view.vinylYaw=(startYaw+(mouse.x-pressPoint.x)*.55)%360
                        view.vinylPitch=Math.max(-35,Math.min(65,startPitch+(mouse.y-pressPoint.y)*.4))
                    } else {
                        view.recorderYaw=(startYaw+(mouse.x-pressPoint.x)*.65)%360
                        view.recorderPitch=Math.max(-65,Math.min(75,startPitch+(mouse.y-pressPoint.y)*.5))
                    }
                }
                if(windowDragAllowed && !view.app.threeDPointer && (pressedButtons & Qt.LeftButton) && Math.hypot(mouse.x-pressPoint.x,mouse.y-pressPoint.y)>12)view.app.startSystemMove()
            } else { updateHint(mouse.x,mouse.y);if(!hoveredControl)view.tilt(Qt.point(mouse.x,mouse.y)) }
        }
        onReleased: { if(vinylScene.item)vinylScene.item.endHardware();view.app.end3DPointer();view.orbiting=false }
        onCanceled: { if(vinylScene.item)vinylScene.item.cancelHardware();view.app.cancel3DPointer();view.orbiting=false }
        onClicked: mouse => { if(mouse.button===Qt.RightButton)view.app.openSettings() }
        onDoubleClicked: mouse => { if(mouse.button===Qt.LeftButton) { if(view.inspectable) { view.app.cancel3DPointer();view.orbiting=false;if(view.app.vinyl)view.resetVinylOrientation();else view.resetRecorderOrientation() } else if(!view.app.controlAt3DPoint(view.unprojectSurface(mouse.x,mouse.y)))view.app.flipDisc() } }
        onExited: { hoveredControl=null;hoveredBody=false;hint="";hintDismissed=false;if(!pressed){view.pitchOffset=0;view.yawOffset=0} }
        onWheel: wheel => { if(!view.app.menuOpen)view.app.deckPlayer.volume=Math.max(0,Math.min(1,view.app.deckPlayer.volume+wheel.angleDelta.y/2400));wheel.accepted=true }
        cursorShape: view.orbiting||view.app.threeDGesture?Qt.ClosedHandCursor:hoveredControl?Qt.PointingHandCursor:hoveredBody?Qt.OpenHandCursor:Qt.ArrowCursor
    }
    SpunToolTip {
        objectName: "threeDToolTip"
        parent: pointer
        x: Math.max(8,Math.min(view.width-width-8,pointer.mouseX-width/2))
        y: Math.max(8,pointer.mouseY-height-18)
        visible: pointer.containsMouse && (!!pointer.hoveredControl || pointer.hoveredBody) && !pointer.hintDismissed && !pointer.pressed && !view.app.menuOpen && !view.app.swapRunning && view.app.visible
        text: pointer.hint
        timeout: 2500
        onClosed: pointer.hintDismissed=true
    }
}
