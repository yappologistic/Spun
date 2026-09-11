import QtQuick
import QtQuick3D
import Spun 1.0

Node {
    id: mixer

    required property var app
    readonly property var controls: app.tx6Controls

    function surface(x, y, z) {
        return Qt.vector3d(x - 122, 176 - y, z);
    }

    function controlPoint(i) {
        const p = controls.position(i);
        return mapPositionToScene(surface(p.x, p.y, i < 18 ? 25 : i === 30 ? 29 : i === 34 ? -8 : 17));
    }

    objectName: "tx6Model"

    Texture {
        id: grain

        generateMipmaps: true
        mipFilter: Texture.Linear
        scaleU: 2
        scaleV: 2

        textureData: SurfaceTexture {
            kind: "bead-normal"
        }

    }

    Texture {
        id: knurl

        generateMipmaps: true
        mipFilter: Texture.Linear
        scaleU: 1.5
        scaleV: 1

        textureData: SurfaceTexture {
            kind: "knurled-normal"
        }

    }

    Texture { id: satin; generateMipmaps: true; mipFilter: Texture.Linear; textureData: SurfaceTexture { kind: "satin" } }
    Texture { id: leather; generateMipmaps: true; mipFilter: Texture.Linear; scaleU: 3; scaleV: 4; textureData: SurfaceTexture { kind: "leather-normal" } }

    PrincipledMaterial {
        id: aluminum

        baseColor: "#c7cbcf"
        metalness: 0.84
        roughness: 0.39
        roughnessMap: satin
        normalMap: grain
        normalStrength: 0.12
    }

    PrincipledMaterial {
        id: edge

        baseColor: "#d4d9db"
        metalness: 0.96
        roughness: 0.24
    }

    PrincipledMaterial {
        id: cap

        baseColor: "#d1d4d5"
        metalness: 0.84
        roughness: 0.34
        roughnessMap: satin
        normalMap: grain
        normalStrength: 0.13
    }

    PrincipledMaterial {
        id: knurled

        baseColor: "#c3cace"
        metalness: 0.9
        roughness: 0.4
        normalMap: knurl
        normalStrength: 0.28
    }

    PrincipledMaterial {
        id: black

        baseColor: "#182128"
        roughness: 0.69
    }

    PrincipledMaterial {
        id: slot

        baseColor: "#0c1014"
        roughness: 0.91
    }

    PrincipledMaterial {
        id: orange

        baseColor: "#f27226"
        roughness: 0.55
    }

    PrincipledMaterial {
        id: white

        baseColor: "#eeeae3"
        roughness: 0.5
    }

    Plate {
        z: -5
        size: Qt.vector3d(244, 352, 36)
        rounding: 9
    }

    Plate {
        z: 13.1
        size: Qt.vector3d(241.8, 349.8, 0.8)
        rounding: 8
        finish: edge
    }

    Plate {
        z: 13.5
        size: Qt.vector3d(240.4, 348.4, 0.8)
        rounding: 7.6
    }

    Plate {
        z: -23.8
        size: Qt.vector3d(240, 348, 1.4)
        rounding: 8
        finish: edge
    }

    Plate {
        x: 10
        z: -24.8
        size: Qt.vector3d(218, 339, 1.5)
        rounding: 5

        finish: PrincipledMaterial {
            baseColor: "#14212d"
            roughness: 0.78
            normalMap: leather
            normalStrength: 0.55
        }

    }

    Repeater3D {
        model: 2

        Node {
            required property int index

            position: mixer.surface(14, index === 0 ? 44 : 304, -25)
            eulerRotation.y: 180

            Ring {
                radius: 3.4
                depth: 0.8
            }

            Ring {
                radius: 1.5
                depth: 1
                z: 0.3
                finish: black
            }

        }

    }

    Model {
        position: mixer.surface(122, 176, 14.02)
        source: "#Rectangle"
        scale: Qt.vector3d(2.44, 3.52, 1)

        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            alphaMode: PrincipledMaterial.Blend
            roughness: 0.8

            baseColorMap: Texture {
                generateMipmaps: true
                mipFilter: Texture.Linear

                sourceItem: Tx6Labels {
                }

            }

        }

    }

    Plate {
        position: mixer.surface(210, 78, 14.1)
        size: Qt.vector3d(42, 57, 1)
        rounding: 4
        finish: edge
    }

    Model {
        position: mixer.surface(210, 78, 14.7)
        source: "#Rectangle"
        scale: Qt.vector3d(0.4, 0.55, 1)

        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            roughness: 0.55

            baseColorMap: Texture {

                sourceItem: Tx6Display {
                    controls: mixer.controls
                }

            }

        }

    }

    Repeater3D {
        model: 6

        Node {
            required property int index

            x: 22 + index * 29.3 - 122

            Plate {
                y: 176 - 245
                z: 14.1
                size: Qt.vector3d(10.2, 89, 0.5)
                rounding: 5
                finish: edge
            }

            Plate {
                y: 176 - 245
                z: 14.4
                size: Qt.vector3d(7, 86, 0.4)
                rounding: 3.4
                finish: slot
            }

            Plate {
                x: -1.8
                y: 176 - 245
                z: 14.7
                size: Qt.vector3d(0.7, 81, 0.3)
                rounding: 0.3
                finish: aluminum
            }

            Ring {
                y: 176 - 309
                z: 14
                radius: 1
                depth: 0.2
                finish: tx6.channels[index].solo ? orange : black
            }

        }

    }

    Ring {
        position: mixer.surface(210, 148, 14.3)
        radius: 24
        depth: 0.65
        finish: black
    }

    Ring {
        position: mixer.surface(210, 148, 14.7)
        radius: 23.6
        depth: 0.6
    }

    Repeater3D {
        model: 35

        Node {
            id: part

            required property int index
            readonly property var key: mixer.controls ? mixer.controls.keyItems[index] : null
            readonly property point point: mixer.controls ? mixer.controls.position(index) : Qt.point(0, 0)
            readonly property real pressure: key ? key.pressure : 0

            position: mixer.surface(point.x - (index === 34 ? pressure * 1.2 : 0), point.y, index === 34 ? -7 : 14)
            eulerRotation.z: index < 18 || index === 30 ? -(key ? key.rotationValue : 0) : 0

            Node {
                z: part.index === 34 ? 0 : -part.pressure * 1.25
                eulerRotation.y: part.index >= 24 && part.index < 34 ? (mixer.controls ? mixer.controls.contactX : 0) * part.pressure * 1.1 : 0

                // Each index has a fixed physical type. Instantiate only its
                // meshes; hidden alternative types used to allocate full geometry
                // for every control. The shared pressure transform stays above.
                Loader3D {
                    active: part.index < 18 || part.index === 30
                    sourceComponent: Node {
                        Ring {
                            tx6Control: part.index
                            radius: part.index === 30 ? 11 : 4
                            depth: 6; z: 3; finish: edge
                        }
                        Ring {
                            tx6Control: part.index
                            radius: part.index === 30 ? 13.5 : 6.7
                            depth: part.index === 30 ? 9 : 5
                            z: part.index === 30 ? 10 : 8
                            finish: knurled
                        }
                        Ring {
                            tx6Control: part.index
                            radius: part.index === 30 ? 13.3 : 6.55
                            depth: 1.2
                            z: part.index === 30 ? 14.7 : 10.6
                            finish: cap
                        }
                        Ring {
                            visible: part.index < 18
                            tx6Control: part.index
                            radius: 2.8; depth: .18; z: 11.3
                            finish: part.index < 6 ? black : part.index < 12 ? orange : white
                        }
                        Plate {
                            tx6Control: part.index
                            y: part.index === 30 ? 9 : 3.6
                            z: part.index === 30 ? 15.4 : 11.3
                            size: Qt.vector3d(part.index === 30 ? 1.3 : 1.8, part.index === 30 ? 5 : 6, .24)
                            rounding: .5
                            finish: part.index < 6 ? black : part.index < 12 ? orange : part.index < 18 ? white : black
                        }
                    }
                }
                Loader3D {
                    active: part.index >= 18 && part.index < 24
                    sourceComponent: Ring {
                        tx6Control: part.index
                        radius: 4.8; depth: 1.2; z: 3; finish: cap
                    }
                }
                Loader3D {
                    active: part.index >= 24 && part.index < 30 || part.index === 33
                    sourceComponent: Node {
                        Plate {
                            tx6Control: part.index
                            z: 1.8
                            size: Qt.vector3d(part.index === 33 ? 39 : 10, part.index === 33 ? 20 : 23, 3.5)
                            rounding: 2; finish: cap
                        }
                        Ring {
                            visible: part.index === 33
                            tx6Control: part.index
                            radius: 2.5; z: 3.7; depth: .15
                            finish: mixer.controls && mixer.controls.shiftHeld ? orange : white
                        }
                    }
                }
                Loader3D {
                    active: part.index === 31 || part.index === 32
                    sourceComponent: Node {
                        Ring {
                            tx6Control: part.index
                            radius: 20; depth: 3.4; z: 2.4; finish: cap
                        }
                        Plate {
                            tx6Control: part.index
                            z: 4.25; size: Qt.vector3d(28, 12, .24); rounding: 1.5
                            finish: part.index === 32 ? orange : black
                        }
                        Plate {
                            tx6Control: part.index
                            x: part.index === 32 ? -2 : 0
                            z: 4.5; size: Qt.vector3d(.8, 8, .1); rounding: .2; finish: white
                        }
                        Plate {
                            visible: part.index === 32
                            tx6Control: part.index
                            x: 2; z: 4.5; size: Qt.vector3d(.8, 8, .1); rounding: .2; finish: white
                        }
                    }
                }
                Loader3D {
                    active: part.index === 34
                    sourceComponent: Plate {
                        tx6Control: part.index
                        size: Qt.vector3d(8, 35, 14); rounding: 3; finish: cap
                    }
                }

            }

        }

    }
    // Numbered TRS inputs and USB-C occupy the real upper sidewall.

    Repeater3D {
        model: 6

        Node {
            required property int index

            position: mixer.surface(22 + index * 29.3, 0, -9)
            eulerRotation.x: -90

            Ring {
                radius: 4.4
                hole: 3.5
                depth: 0.5
            }

            Ring {
                radius: 3.5
                depth: 0.45
                z: -0.2
                finish: slot
            }

        }

    }

    Plate {
        position: mixer.surface(207, 0, -9)
        eulerRotation.x: -90
        size: Qt.vector3d(27, 8, 0.5)
        rounding: 3.8
        finish: black
    }

    Plate {
        position: mixer.surface(207, -0.3, -9)
        eulerRotation.x: -90
        size: Qt.vector3d(18, 1.1, 0.5)
        rounding: 0.5
        finish: aluminum
    }

    Repeater3D {
        model: 2

        Node {
            required property int index

            position: mixer.surface(140 + index * 29, 352, -9)
            eulerRotation.x: 90

            Ring {
                radius: 5
                hole: 3.8
                depth: 0.8
            }

            Ring {
                radius: 3.8
                depth: 0.5
                finish: slot
            }

        }

    }

    Node {
        position: mixer.surface(210, 356, -9)
        eulerRotation.x: 90

        Ring {
            radius: 17
            hole: 4.5
            depth: 8
        }

        Ring {
            z: 20
            radius: 19
            hole: 4.5
            depth: 34
            finish: knurled
        }

        Ring {
            z: 37.2
            radius: 18.2
            hole: 4.5
            depth: 1.2
        }

    }

    component Plate: Model {
        id: plate

        property vector3d size: Qt.vector3d(244, 352, 36)
        property real rounding: 7
        property var finish: aluminum
        property int tx6Control: -1

        pickable: true
        materials: finish

        geometry: DeckGeometry {
            dimensions: plate.size
            radius: plate.rounding
        }

    }

    component Ring: Model {
        id: ring

        property real radius: 8
        property real hole: 0.001
        property real depth: 3
        property var finish: edge
        property int tx6Control: -1

        pickable: true
        materials: finish

        geometry: RecordGeometry {
            radius: ring.radius
            hole: ring.hole
            depth: ring.depth
        }

    }

    Repeater3D {
        model: 2
        Node {
            required property int index
            position:mixer.surface(66+index*29,353,-9)
            eulerRotation.x:90
            Ring {radius:7;depth:1.6;finish:cap;property string tx6Info:"Aux and cue outputs are not connected in the software mixer"}
        }
    }
    Model {
        position:mixer.surface(122,352.3,-9);eulerRotation.x:90
        source:"#Rectangle";scale:Qt.vector3d(2.44,.32,1)
        materials:PrincipledMaterial {
            lighting:PrincipledMaterial.NoLighting;alphaMode:PrincipledMaterial.Blend
            baseColorMap:Texture {
                sourceItem:Item {
                    width:244;height:32
                    Text {x:58;y:1;text:"aux";font.pixelSize:8;color:"#424a4d"}
                    Text {x:87;y:1;text:"cue";font.pixelSize:8;color:"#424a4d"}
                    Text {x:131;y:1;text:"AUX    CUE";font.pixelSize:6;color:"#424a4d"}
                }
            }
        }
    }

}
