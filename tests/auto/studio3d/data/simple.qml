import QtQuick 2.0
import QtStudio3D 2.0

Rectangle {
    id: root
    color: "lightGray"

    Studio3D {
        id: s3d
        focus: true
        anchors.margins: 60
        anchors.fill: parent
        Presentation {
            id: s3dpres
            source: "qrc:/data/primitives.uip"
        }
    }
}
