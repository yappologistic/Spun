import QtQuick
import QtQuick3D
import Spun 1.0

View3D {
    id: view
    required property var app
    required property Item surfaceItem
    objectName: "player3DView"
    clip: true
    // Match device pixels for every enlarged deck, with the same bounded target.
    readonly property real cdRenderScale: Math.min(Math.min(1.5, Math.max(1, app.uiScale)) * Screen.devicePixelRatio, 1536 / Math.max(1, width, height))
    explicitTextureWidth: Math.ceil(width * cdRenderScale)
    explicitTextureHeight: Math.ceil(height * cdRenderScale)
    property real vinylYaw: 0
    property real vinylPitch: 0
    readonly property bool inspectable: true
    property real cdYaw: 0
    property real cdPitch: 0
    property real cassetteYaw: 0
    property real cassettePitch: 0
    property real recorderYaw: 0
    property real recorderPitch: 0
    readonly property bool hardwareActive: !!(vinylScene.item && vinylScene.item.pressedControl>=0) || !!(app.cassetteControls && app.cassetteControls.pressedControl>=0) || !!(app.cdControls && app.cdControls.pressedControl>=0)
    function cancelHardware() { if(vinylScene.item)vinylScene.item.cancelHardware();if(app.cassetteControls)app.cassetteControls.cancel();if(app.cdControls)app.cdControls.cancel() }
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
        tonemapMode: SceneEnvironment.TonemapModeAces
        lightProbe: Texture { textureData: StudioTexture { tint: view.app.accent; ambientLift: view.app.cassette ? .16 : view.app.cd ? .09 : view.app.tx6Visible ? .1 : .035 } }
        probeExposure: .7
        // Tabletop and upright products need different softbox elevation.
        probeOrientation: Qt.vector3d(view.app.vinyl || view.app.cd ? 270 : 0, 0, 0)
        // Small contact shadows under caps and seams, without broad dirty halos.
        aoEnabled: true; aoStrength: 22; aoDistance: 7; aoSoftness: 65; aoBias: .15; aoSampleRate: 2
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
    readonly property vector3d cassetteCamera: {
        if(!app.cassette)return Qt.vector3d(0,0,710)
        const rotation=assembly.eulerRotation
        let points=[]
        for(const x of [-217,217])for(const y of [-143,164])for(const z of [-56,34])points.push(assembly.mapPositionToScene(Qt.vector3d(x,y,z)))
        // Fit both hinge endpoints and the middle of the arc. Opening the door
        // never changes the zoom, even after the user rotates the enclosure.
        for(const degrees of [0,29,58]) {
            const a=degrees*Math.PI/180
            for(const x of [-193,193])for(const y of [0,269])for(const z of [-7,7])points.push(assembly.mapPositionToScene(Qt.vector3d(x,-133+y*Math.cos(a)-z*Math.sin(a),30+y*Math.sin(a)+z*Math.cos(a))))
        }
        const cx=(Math.min(...points.map(p=>p.x))+Math.max(...points.map(p=>p.x)))/2
        const cy=(Math.min(...points.map(p=>p.y))+Math.max(...points.map(p=>p.y)))/2
        const focal=height/(2*Math.tan(19*Math.PI/180))
        let distance=710
        for(const p of points)distance=Math.max(distance,p.z+Math.abs(p.x-cx)*focal/Math.max(1,width/2-20),p.z+Math.abs(p.y-cy)*focal/Math.max(1,height/2-18))
        return Qt.vector3d(cx,cy,distance)
    }
    readonly property vector3d cdCamera: {
        if(!app.cd)return Qt.vector3d(0,0,830)
        const rotation=assembly.eulerRotation
        let points=[]
        for(const x of [-217,217])for(const y of [-225,214])for(const z of [-66,22])points.push(assembly.mapPositionToScene(Qt.vector3d(x,y,z)))
        for(const degrees of [0,35,70]) {
            const a=-degrees*Math.PI/180
            for(const x of [-206,206])for(const y of [2,-400])points.push(assembly.mapPositionToScene(Qt.vector3d(x,197+y*Math.cos(a),16+y*Math.sin(a))))
        }
        const cx=(Math.min(...points.map(p=>p.x))+Math.max(...points.map(p=>p.x)))/2
        const cy=(Math.min(...points.map(p=>p.y))+Math.max(...points.map(p=>p.y)))/2
        const focal=height/(2*Math.tan(19*Math.PI/180))
        let distance=730
        for(const p of points)distance=Math.max(distance,p.z+Math.abs(p.x-cx)*focal/Math.max(1,width/2-18),p.z+Math.abs(p.y-cy)*focal/Math.max(1,height/2-16))
        return Qt.vector3d(cx,cy,distance)
    }
    readonly property vector3d pairedCamera: {
        if(!app.tx6Visible)return Qt.vector3d(0,0,710)
        const rotation=assembly.eulerRotation
        const points=[]
        // Fit the two housings and the cable separately, without reserving a
        // large empty box above the TP-7. Keep the controls large during orbit.
        for(const box of [[-278,-19,-203,184,-30,34],[35,254,-199,158,-30,34],[-84,223,174,279,-24,-5]])
            for(const x of [box[0],box[1]])for(const y of [box[2],box[3]])for(const z of [box[4],box[5]])
                points.push(assembly.mapPositionToScene(Qt.vector3d(x,y,z)))
        const cx=(Math.min(...points.map(p=>p.x))+Math.max(...points.map(p=>p.x)))/2
        const cy=(Math.min(...points.map(p=>p.y))+Math.max(...points.map(p=>p.y)))/2
        const focal=height/(2*Math.tan(19*Math.PI/180))
        let distance=620
        for(const p of points)distance=Math.max(distance,p.z+Math.abs(p.x-cx)*focal/(width/2-24),p.z+Math.abs(p.y-cy)*focal/(height/2-14))
        return Qt.vector3d(cx,cy,distance)
    }
    PerspectiveCamera { id: sceneCamera; x: app.vinyl ? view.vinylCamera.x : app.cassette ? view.cassetteCamera.x : app.cd ? view.cdCamera.x : app.tx6Visible ? view.pairedCamera.x : 0; y: app.vinyl ? view.vinylCamera.y : app.cassette ? view.cassetteCamera.y : app.cd ? view.cdCamera.y : app.tx6Visible ? view.pairedCamera.y : 0; z: app.vinyl ? view.vinylCamera.z : app.cassette ? view.cassetteCamera.z : app.cd ? view.cdCamera.z : app.tx6Visible ? view.pairedCamera.z : 710; clipNear: 10; clipFar: 1500; fieldOfView: 38 }
    DirectionalLight { objectName: "threeDKeyLight"; color: view.keyTint; eulerRotation: Qt.vector3d(-30,-35,0); brightness: .9; ambientColor: view.ambientTint; castsShadow: true; shadowFactor: 42; softShadowQuality: Light.PCF8; pcfFactor: 3; shadowBias: 1.5; shadowMapQuality: Light.ShadowMapQualityHigh; shadowMapFar: 1200 }
    DirectionalLight { objectName: "threeDFillLight"; color: view.fillTint; eulerRotation: Qt.vector3d(-10,45,0); brightness: view.app.vinyl ? 1.3 : .55 }
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
    Node {
        id: assembly
        y: app.vinyl ? -45 : 0
        eulerRotation: Qt.vector3d(app.vinyl?-42+view.vinylPitch:app.recorder?-12+view.recorderPitch:app.cassette?18+view.cassettePitch:-44+view.cdPitch,app.vinyl?-12+view.vinylYaw:app.recorder?-12+view.recorderYaw:app.cassette?-24+view.cassetteYaw:-16+view.cdYaw,0)
        Loader3D { id: recorderScene; active: app.recorder; sourceComponent: Recorder3D { app: view.app; artworkTexture: cover; x: view.app.tx6Visible?-142:0; scale: view.app.tx6Visible?Qt.vector3d(.85,.85,.85):Qt.vector3d(1,1,1) } }
        Loader3D { id: tx6Scene; active: app.tx6Visible; sourceComponent: Tx6_3D { app:view.app;x:145;scale:Qt.vector3d(.85,.85,.85) } }
        Loader3D { active:app.tx6Visible;sourceComponent:Tx6Cable3D {} }
        Loader3D { id: cassetteScene; active: app.cassette; sourceComponent: Cassette3D { app: view.app; labelTexture: tapeLabel } }
        Loader3D { id: vinylScene; active: app.vinyl; sourceComponent: Vinyl3D { app: view.app; artworkTexture: cover; outgoingTexture: oldCover } }
        Loader3D { id: cdScene; active: app.cd; sourceComponent: Cd3D { app: view.app; artworkTexture: cover; outgoingTexture: oldCover } }
    }
    // Pointer math intersects the same plane as the physical controls. It never
    // installs a Qt Quick 3D transform on the shared 2D control tree.
    function projectSurface(x,y) { return projectSurfaceDepth(x,y,app.recorder?16:app.cassette?36:app.cd?5:.5) }
    function projectSurfaceDepth(x,y,depth) {
        const space=app.vinyl && vinylScene.item ? vinylScene.item.recordSpace : app.recorder && recorderScene.item ? recorderScene.item : assembly
        const rp=app.recorder && app.recorderView ? app.recorderView.mapFromItem(surfaceItem,x,y) : Qt.point(x-60,y-89)
        const p=mapFrom3DScene(space.mapPositionToScene(Qt.vector3d(app.recorder?rp.x-205:x-265,app.recorder?205-rp.y:294-y,depth)))
        return Qt.point(p.x,p.y)
    }
    function unprojectSurface(x,y) { return unprojectPlane(x,y,app.recorder?16:app.cassette?36:app.cd?5:.5) }
    function unprojectPlane(x,y,depth) {
        const space=app.vinyl && vinylScene.item ? vinylScene.item.recordSpace : app.recorder && recorderScene.item ? recorderScene.item : assembly
        const a=space.mapPositionFromScene(mapTo3DScene(Qt.vector3d(x,y,0)))
        const b=space.mapPositionFromScene(mapTo3DScene(Qt.vector3d(x,y,1000)))
        const t=(depth-a.z)/(b.z-a.z)
        if(app.recorder && app.recorderView)return app.recorderView.mapToItem(surfaceItem,a.x+(b.x-a.x)*t+205,205-a.y-(b.y-a.y)*t)
        return Qt.point(a.x+(b.x-a.x)*t+265,294-a.y-(b.y-a.y)*t)
    }
    function projectRecorderControl(index) { const p=mapFrom3DScene(recorderScene.item.controlScenePoint(index));return Qt.point(p.x,p.y) }
    function projectTx6Control(index) {const p=mapFrom3DScene(tx6Scene.item.controlPoint(index));return Qt.point(p.x,p.y)}
    function projectTx6Face(x,y) {
        const p=mapFrom3DScene(tx6Scene.item.mapPositionToScene(tx6Scene.item.surface(x,y,14)))
        return Qt.point(p.x,p.y)
    }
    function tx6FacePoint(x,y) {
        const space=tx6Scene.item
        const a=space.mapPositionFromScene(mapTo3DScene(Qt.vector3d(x,y,0)))
        const b=space.mapPositionFromScene(mapTo3DScene(Qt.vector3d(x,y,1000)))
        const t=(14-a.z)/(b.z-a.z)
        return Qt.point(a.x+(b.x-a.x)*t+122,176-a.y-(b.y-a.y)*t)
    }
    function assistedTx6Hit(x,y,object) {
        if(!app.tx6Visible || !tx6Scene.item || !app.tx6Controls || app.menuOpen)return null
        // Assistance belongs only to the visible mixer face. Never reach
        // through its back, a cable, or the other device to find a control.
        if(tx6Scene.item.mapDirectionToScene(Qt.vector3d(0,0,1)).z<.3)return null
        let parent=object
        while(parent && parent!==tx6Scene.item)parent=parent.parent
        if(!parent)return null
        const local=tx6FacePoint(x,y)
        if(local.x<0 || local.x>244 || local.y<0 || local.y>352)return null
        let nearest=-1,score=Infinity
        for(let i=0;i<34;i++) {
            if(!app.tx6Controls.canUse(i))continue
            const center=projectTx6Control(i)
            const distance=Math.hypot(x-center.x,y-center.y)
            // Nearest-center resolution keeps neighboring knobs distinct even
            // when their invisible hit areas overlap at an oblique angle.
            const radius=i<24 ? 14 : 18
            if(distance<=radius && distance<score){nearest=i;score=distance}
        }
        if(nearest>=0)return {control:null,body:true,depth:16,hardware:-1,tx6:nearest}
        if(local.y>=201 && local.y<=290) {
            const channel=Math.round((local.x-22)/29.3)
            if(channel>=0 && channel<6 && Math.abs(local.x-(22+channel*29.3))<=12 && app.tx6Controls.canUse(18+channel))
                return {control:null,body:true,depth:16,hardware:-1,tx6:18+channel,faderLevel:Math.max(0,Math.min(1,(285-local.y)/77))}
        }
        return null
    }
    function recorderHit(x,y) {
        const hit=pick(x,y)
        if(!hit.objectHit)return {control:null,body:false,depth:16}
        if(hit.objectHit.tx6Info)return {control:null,body:true,depth:16,hardware:-1,info:hit.objectHit.tx6Info}
        const txIndex=hit.objectHit.tx6Control
        if(txIndex>=0)return {control:null,body:true,depth:16,hardware:-1,tx6:txIndex}
        const assisted=assistedTx6Hit(x,y,hit.objectHit)
        if(assisted)return assisted
        const index=hit.objectHit.recorderControl
        const controls=app.recorderView
        const control=controls && index>=0 && index<controls.controls.length ? controls.controls[index].pointerInput : controls && index===20 ? controls.wheelInput : null
        return {control:control && control.enabled ? control : null,body:true,depth:recorderScene.item.mapPositionFromScene(hit.scenePosition).z}
    }
    function resetCdOrientation() { cdYaw=0;cdPitch=0 }
    function projectCdControl(index) { const p=mapFrom3DScene(cdScene.item.controlPoint(index));return Qt.point(p.x,p.y) }
    function cdHit(x,y) {
        const miss={control:null,body:false,depth:5,hardware:-1}
        if(app.menuOpen || app.swapRunning)return miss
        const hit=pick(x,y)
        const index=hit.objectHit ? hit.objectHit.cdControl : -1
        if(index>=0 && app.cdControls && app.cdControls.canUse(index))return {control:null,body:true,depth:5,hardware:index}
        const front=assembly.mapDirectionToScene(Qt.vector3d(0,0,1)).z>.18
        const control=front ? app.controlAt3DPoint(unprojectSurface(x,y)) : null
        return {control:control,body:!!hit.objectHit,depth:5,hardware:-1}
    }
    function resetCassetteOrientation() { cassetteYaw=0;cassettePitch=0 }
    function projectCassetteControl(index) { const p=mapFrom3DScene(cassetteScene.item.controlPoint(index));return Qt.point(p.x,p.y) }
    function cassetteHit(x,y) {
        const miss={control:null,body:false,depth:36,hardware:-1}
        if(app.menuOpen || app.swapRunning)return miss
        const hit=pick(x,y)
        const index=hit.objectHit ? hit.objectHit.cassetteControl : -1
        if(index>=0 && app.cassetteControls && app.cassetteControls.canUse(index))return {control:null,body:true,depth:36,hardware:index}
        const front=assembly.mapDirectionToScene(Qt.vector3d(0,0,1)).z>.18
        const control=front ? app.controlAt3DPoint(unprojectSurface(x,y)) : null
        return {control:control,body:!!hit.objectHit,depth:36,hardware:-1}
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
        if(app.cassette && cassetteScene.item) {
            const points=[]
            for(const x of [-191,191])for(const y of [0,266])points.push(mapFrom3DScene(cassetteScene.item.lidNode.mapPositionToScene(Qt.vector3d(x,y,0))))
            const left=Math.min(...points.map(p=>p.x)),top=Math.min(...points.map(p=>p.y))
            return Qt.rect(left,top,Math.max(...points.map(p=>p.x))-left,Math.max(...points.map(p=>p.y))-top)
        }
        if(!cdScene.item)return Qt.rect(0,0,0,0)
        const points=[]
        for(const x of [-206,206])for(const y of [2,-400])points.push(mapFrom3DScene(cdScene.item.lidNode.mapPositionToScene(Qt.vector3d(x,y,0))))
        const left=Math.min(...points.map(p=>p.x)),top=Math.min(...points.map(p=>p.y))
        return Qt.rect(left,top,Math.max(...points.map(p=>p.x))-left,Math.max(...points.map(p=>p.y))-top)
    }

    MouseArea {
        id: pointer
        objectName: "threeDPointer"
        property var hoveredControl: null
        property bool hintDismissed: false
        property point pressPoint
        property bool windowDragAllowed: false
        property bool hoveredBody: false
        property int hoveredHardware: -1
        property int hoveredTx6: -1
        property bool channelMenuPress: false
        property real startYaw: 0
        property real startPitch: 0
        property real grabDepth: 16
        property string hint: ""
        function updateHint(x,y) {
            hoveredTx6=-1
            const point=view.unprojectSurface(x,y)
            const hit=view.app.vinyl ? view.vinylHit(x,y) : view.app.recorder ? view.recorderHit(x,y) : view.app.cassette ? view.cassetteHit(x,y) : view.cdHit(x,y)
            if(hit && hit.info){hoveredBody=true;hoveredControl=null;hint=hit.info;return}
            if(hit && hit.tx6>=0) {hoveredTx6=hit.tx6;hoveredBody=true;hoveredControl=null;hoveredHardware=-1;hint=view.app.tx6Controls.caption(hit.tx6);return}
            const target=hit ? hit.control : view.app.controlAt3DPoint(point)
            hoveredBody=!!hit && hit.body
            hoveredHardware=hit && hit.hardware!==undefined ? hit.hardware : -1
            if(hit && hit.hardware>=0) {
                hoveredControl=null;hoveredBody=true
                hint=view.app.cd ? view.app.cdControls.keyItems[hit.hardware].caption : view.app.cassette ? view.app.cassetteControls.keyItems[hit.hardware].caption : ["33⅓ RPM", "45 RPM", view.app.deckPlayer.playing?"Lift needle and pause":"Lower needle and play", vinylScene.item.coverOpen?"Close dust cover":"Open dust cover"][hit.hardware]
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
            hintDismissed=true;pressPoint=Qt.point(mouse.x,mouse.y);channelMenuPress=false
            const hit=view.app.vinyl ? view.vinylHit(mouse.x,mouse.y) : view.app.recorder ? view.recorderHit(mouse.x,mouse.y) : view.app.cassette ? view.cassetteHit(mouse.x,mouse.y) : view.cdHit(mouse.x,mouse.y)
            grabDepth=hit ? hit.depth : .5
            if(hit && hit.tx6>=0) {
                if(mouse.button===Qt.RightButton && hit.tx6<30){const i=hit.tx6;channelMenuPress=true;view.app.tx6Controls.openChannel(i<18?i%6:i<24?i-18:i-24);windowDragAllowed=false;return}
                if(mouse.button===Qt.LeftButton){
                    const p=view.tx6FacePoint(mouse.x,mouse.y)
                    if(hit.faderLevel!==undefined)view.app.tx6Controls.setValue(hit.tx6,hit.faderLevel)
                    const center=view.app.tx6Controls.position(hit.tx6)
                    view.app.tx6Controls.begin(hit.tx6,p.y,(p.x-center.x)/18);windowDragAllowed=false;return
                }
            }
            if(mouse.button===Qt.LeftButton && hit && hit.hardware>=0) { if(view.app.cd)view.app.cdControls.begin(hit.hardware,mouse.y);else if(view.app.cassette)view.app.cassetteControls.begin(hit.hardware,mouse.y);else vinylScene.item.beginHardware(hit.hardware);windowDragAllowed=false;return }
            if(mouse.button===Qt.LeftButton)view.app.begin3DPointer(view.unprojectPlane(mouse.x,mouse.y,grabDepth),mouse.modifiers,hit ? hit.control : null)
            view.orbiting=mouse.button===Qt.LeftButton && !!hit && hit.body && !view.app.threeDPointer && !view.app.menuOpen
            startYaw=view.app.vinyl?view.vinylYaw:view.app.cassette?view.cassetteYaw:view.app.cd?view.cdYaw:view.recorderYaw;startPitch=view.app.vinyl?view.vinylPitch:view.app.cassette?view.cassettePitch:view.app.cd?view.cdPitch:view.recorderPitch
            windowDragAllowed=mouse.button===Qt.LeftButton && !view.app.threeDPointer && !view.orbiting
        }
        onPositionChanged: mouse => {
            if(pressed) {
                if(view.app.tx6Controls && view.app.tx6Controls.pressedControl>=0){view.app.tx6Controls.move(view.tx6FacePoint(mouse.x,mouse.y).y,view.recorderHit(mouse.x,mouse.y).tx6,mouse.modifiers&Qt.ShiftModifier);return}
                if(view.app.cd && view.app.cdControls && view.app.cdControls.pressedControl>=0) { view.app.cdControls.move(mouse.y,view.cdHit(mouse.x,mouse.y).hardware);return }
                if(view.app.cassette && view.app.cassetteControls && view.app.cassetteControls.pressedControl>=0) { view.app.cassetteControls.move(mouse.y,view.cassetteHit(mouse.x,mouse.y).hardware);return }
                if(view.app.vinyl && vinylScene.item && vinylScene.item.pressedControl>=0) {
                    vinylScene.item.armed=view.vinylHit(mouse.x,mouse.y).hardware===vinylScene.item.pressedControl
                    return
                }
                view.app.move3DPointer(view.unprojectPlane(mouse.x,mouse.y,grabDepth),mouse.modifiers)
                if(view.orbiting) {
                    if(view.app.vinyl) {
                        view.vinylYaw=(startYaw+(mouse.x-pressPoint.x)*.55)%360
                        view.vinylPitch=Math.max(-35,Math.min(65,startPitch+(mouse.y-pressPoint.y)*.4))
                    } else if(view.app.cassette) {
                        view.cassetteYaw=(startYaw+(mouse.x-pressPoint.x)*.65)%360
                        view.cassettePitch=Math.max(-60,Math.min(70,startPitch+(mouse.y-pressPoint.y)*.5))
                    } else if(view.app.cd) {
                        view.cdYaw=(startYaw+(mouse.x-pressPoint.x)*.6)%360
                        view.cdPitch=Math.max(-50,Math.min(75,startPitch+(mouse.y-pressPoint.y)*.45))
                    } else {
                        view.recorderYaw=(startYaw+(mouse.x-pressPoint.x)*.65)%360
                        view.recorderPitch=Math.max(-65,Math.min(75,startPitch+(mouse.y-pressPoint.y)*.5))
                    }
                }
                if(windowDragAllowed && !view.app.threeDPointer && (pressedButtons & Qt.LeftButton) && Math.hypot(mouse.x-pressPoint.x,mouse.y-pressPoint.y)>12)view.app.startSystemMove()
            } else { updateHint(mouse.x,mouse.y);if(!hoveredControl)view.tilt(Qt.point(mouse.x,mouse.y)) }
        }
        onReleased: mouse=> { if(view.app.tx6Controls)view.app.tx6Controls.end(mouse.modifiers);if(view.app.cdControls)view.app.cdControls.end();if(view.app.cassetteControls)view.app.cassetteControls.end();if(vinylScene.item)vinylScene.item.endHardware();view.app.end3DPointer();view.orbiting=false }
        onCanceled: { if(vinylScene.item)vinylScene.item.cancelHardware();view.app.cancel3DPointer();view.orbiting=false }
        onClicked: mouse => { if(mouse.button===Qt.RightButton && !channelMenuPress)view.app.openSettings() }
        onDoubleClicked: mouse => { if(mouse.button===Qt.LeftButton) { if(view.app.tx6Visible){const hit=view.recorderHit(mouse.x,mouse.y);if(hit.tx6>=0){view.app.tx6Controls.cancel();view.app.tx6Controls.resetControl(hit.tx6);return}}if(view.inspectable) { view.app.cancel3DPointer();view.orbiting=false;if(view.app.vinyl)view.resetVinylOrientation();else if(view.app.cassette)view.resetCassetteOrientation();else if(view.app.cd)view.resetCdOrientation();else view.resetRecorderOrientation() } else if(!view.app.controlAt3DPoint(view.unprojectSurface(mouse.x,mouse.y)))view.app.flipDisc() } }
        onExited: { hoveredControl=null;hoveredBody=false;hoveredHardware=-1;hoveredTx6=-1;hint="";hintDismissed=false;if(!pressed){view.pitchOffset=0;view.yawOffset=0} }
        onWheel: wheel => { if(view.app.tx6Visible){const hit=view.recorderHit(wheel.x,wheel.y);if(hit.tx6>=0){view.app.tx6Controls.scroll(hit.tx6,wheel.angleDelta.y/120);wheel.accepted=true;return}}if(!view.app.menuOpen)view.app.deckPlayer.volume=Math.max(0,Math.min(1,view.app.deckPlayer.volume+wheel.angleDelta.y/2400));wheel.accepted=true }
        cursorShape: view.orbiting||view.app.threeDGesture?Qt.ClosedHandCursor:hoveredTx6>=0?(view.app.tx6Controls.canUse(hoveredTx6)?Qt.PointingHandCursor:Qt.ForbiddenCursor):(hoveredControl||hoveredHardware>=0)?Qt.PointingHandCursor:hoveredBody?Qt.OpenHandCursor:Qt.ArrowCursor
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
