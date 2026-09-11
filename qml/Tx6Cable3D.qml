import QtQuick
import QtQuick3D
import Spun 1.0

Node {
    id: cable

    // Each connector enters the USB-C socket on the TOP edge, not an audio jack.
    property vector3d start: Qt.vector3d(-79.1, 157.7, -7.65)
    property vector3d end: Qt.vector3d(217.25, 149.6, -7.65)

    objectName: "tx6UsbLink"

    PrincipledMaterial {
        id: metal

        baseColor: "#c7cdd1"
        metalness: 0.85
        roughness: 0.36
    }

    PrincipledMaterial {
        id: boot

        baseColor: "#d8d8d5"
        roughness: 0.56
    }

    PrincipledMaterial {
        id: rubber

        baseColor: "#131b20"
        roughness: 0.84
        normalStrength: 0.2

        normalMap: Texture {
            generateMipmaps: true
            mipFilter: Texture.Linear

            textureData: SurfaceTexture {
                kind: "knurled-normal"
            }

        }

    }

    Model {
        objectName: "tx6BraidedCable"
        materials: rubber

        geometry: CableGeometry {
            start: cable.start.plus(Qt.vector3d(0, 28, 0))
            end: cable.end.plus(Qt.vector3d(0, 28, 0))
        }

    }

    Repeater3D {
        model: 2

        Node {
            required property int index

            position: index === 0 ? cable.start : cable.end

            Model {
                y: 3
                materials: metal

                geometry: DeckGeometry {
                    dimensions: Qt.vector3d(16, 9, 5.6)
                    radius: 2
                }

            }

            Model {
                y: 14
                materials: boot

                geometry: DeckGeometry {
                    dimensions: Qt.vector3d(19, 18, 8)
                    radius: 3
                }

            }

            Model {
                y: 24
                materials: boot

                geometry: DeckGeometry {
                    dimensions: Qt.vector3d(7, 12, 6)
                    radius: 2
                }

            }

            Model {
                y: 15
                z: 4.05
                source: "#Rectangle"
                scale: Qt.vector3d(0.1, 0.02, 1)

                materials: PrincipledMaterial {
                    baseColor: "#98a1a5"
                    roughness: 0.8
                }

            }

        }

    }

}
