/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt 3D Studio.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtStudio3D 2.0
import QtQuick.Window 2.3
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Qt.labs.platform 1.0

Rectangle {
    id: root
    color: "lightGray"

    MessageDialog {
        id: errorDialog
    }

    Studio3D {
        id: s3d
        focus: true
        anchors.margins: 60
        anchors.fill: parent
        Presentation {
            id: s3dpres
            source: "qrc:/presentation/barrel.uip"
            onCustomSignalEmitted: customSignalName.text = Date.now() + ": " + name
            onSlideEntered: slideEnter.text = "Entered slide " + name + "(index " + index + ") on " + elementPath
            onSlideExited: slideExit.text = "Exited slide " + name + "(index " + index + ") on " + elementPath
        }
        ignoredEvents: mouseEvCb.checked ? Studio3D.EnableAllEvents : (Studio3D.IgnoreMouseEvents | Studio3D.IgnoreWheelEvents)
        onRunningChanged: console.log("running: " + s3d.running)
        onPresentationReady: console.log("presentationReady")
        onErrorChanged: {
            if (s3d.error !== "") {
                errorDialog.text = s3d.error;
                errorDialog.open();
            }
        }

        property int frameCount: 0
        onFrameUpdate: frameCount += 1

        Timer {
            running: true
            repeat: true
            interval: 1000
            onTriggered: {
                fpsCount.text = "~" + s3d.frameCount + " FPS";
                s3d.frameCount = 0;
            }
        }

        NumberAnimation on opacity {
            id: opacityAnimation
            from: 1
            to: 0
            duration: 5000
            running: false
            onStopped: s3d.opacity = 1
        }
    }

    Window {
        id: w
        visible: true
        width: 500
        height: 500
        Item {
            id: wroot
            anchors.fill: parent
        }
        title: "Second window"
    }

    RowLayout {
        Button {
            text: "Move to other window"
            onClicked: if (s3d.parent === wroot) s3d.parent = root; else s3d.parent = wroot
        }
        Button {
            text: "Open barrel without background"
            onClicked: s3dpres.source = "qrc:/presentation/barrel_no_background.uip"
        }
        Button {
            text: "Animate opacity"
            onClicked: opacityAnimation.running = true
        }
        Button {
            text: "Reload"
            onClicked: s3dpres.reload()
        }
        Button {
            text: "Open"
            onClicked: openDialog.open()
        }
        CheckBox {
            id: mouseEvCb
            text: "Let mouse events through"
            checked: true
        }
        Button {
            text: "Fire event"
            // Here we could open a Dialog to specify a target object and event
            // name but creating a working dialog with Quick Controls 2 is way
            // beyond my modest skills, apparently.
            onClicked: s3dpres.fireEvent("Scene.Layer.Camera", "customCameraEvent") // in actionevent.uip this will change the sphere's color
        }
        Button {
            text: "Toggle camera"
            onClicked: {
                var v = s3dpres.getAttribute("Scene.Layer.Camera", "eyeball")
                s3dpres.setAttribute("Scene.Layer.Camera", "eyeball", !v)
            }
        }
    }

    Text {
        id: fpsCount
        text: "0 FPS"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
    }
    Text {
        id: customSignalName
        anchors.bottom: parent.bottom
        anchors.left: fpsCount.right
        anchors.leftMargin: 8
    }
    Text {
        id: slideEnter
        anchors.bottom: parent.bottom
        anchors.left: customSignalName.right
        anchors.leftMargin: 8
    }
    Text {
        id: slideExit
        anchors.bottom: parent.bottom
        anchors.left: slideEnter.right
        anchors.leftMargin: 8
    }

    FileDialog {
        id: openDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["UIP files (*.uip)", "UIA files (*.uia)", "All files (*)"]
        onAccepted: s3dpres.source = file
    }
}
