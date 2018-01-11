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

import QtQuick 2.7

Rectangle {
    width: 1024
    height: 512
    color: "black"
    property int lines: 166

    Text {
        id: text
        x: 10
        y: 0
        color: "white"

        font.pointSize: 16
        text: "import QtQuick 2.7
import QtStudio3D 1.0

Item {
    id: mainview
    width: 1280
    height: 768
    visible: true

    // Qt 3D Studio element
    //
    // The presentation displayed in this example consists of three simple slides with ping-pong
    // animations:
    //
    // Slide 1: Move ball to the right side of the screen
    // Slide 2: Move ball to center of the screen and scale it up
    // Slide 3: Move ball back to the initial position on the left side of the screen
    //
    // Because the animations in the presentation are defined dynamic, the current position
    // and scale of the ball are used as the initial keyframes for animations whenever
    // a slide is changed.
    Studio3D {
        id: studio3D
        anchors.fill: parent

        // ViewerSettings item is used to specify presentation independent viewer settings.
        ViewerSettings {
            scaleMode: ViewerSettings.ScaleModeFill
            showRenderStats: false
        }

        // Presentation item is used to control the presentation.
        Presentation {
            source: \"qrc:/presentation/dyn_key.uip\"

            // SceneElement item is used to listen for slide changes of a scene in the presentation.
            // You can also change the slides via its properties.
            SceneElement {
                id: scene
                elementPath: \"Scene\"
                onCurrentSlideIndexChanged: {
                    console.log(\"Current slide : \" + currentSlideIndex + \" (\"
                                + currentSlideName + \")\");
                }
                onPreviousSlideIndexChanged: {
                    console.log(\"Previous slide: \" + previousSlideIndex + \" (\"
                                + previousSlideName + \")\");
                }
            }

            // Element item is used to change element attributes
            Element {
                id: materialElement
                elementPath: \"Scene.Layer.Sphere.Material\"
            }

            property int desiredSlideIndex: 1
            property int colorIndex: 0
            property var colorArray: [
                [1.0, 1.0, 1.0],
                [1.0, 0.0, 0.0],
                [0.0, 1.0, 0.0],
                [0.0, 0.0, 1.0],
                [0.0, 1.0, 1.0],
                [1.0, 0.0, 1.0],
                [1.0, 1.0, 0.0]
            ]

            function nextSlide() {
                // Separate desiredSlideIndex variable is used to keep track of the desired slide,
                // because SceneElement's currentSlideIndex property works asynchronously.
                // This way the button click always changes the slide even if you click
                // it twice during the same frame.
                desiredSlideIndex = desiredSlideIndex != 3 ? desiredSlideIndex + 1 : 1;
                scene.currentSlideIndex = desiredSlideIndex
                slideButtonText.text = \"Change Slide (\" + desiredSlideIndex + \")\"
            }

            function resetTime() {
                scene.goToTime(0);
            }

            function changeColor() {
                colorIndex = colorIndex >= colorArray.length - 1 ? colorIndex = 0 : colorIndex + 1;
                materialElement.setAttribute(\"diffuse.r\", colorArray[colorIndex][0]);
                materialElement.setAttribute(\"diffuse.g\", colorArray[colorIndex][1]);
                materialElement.setAttribute(\"diffuse.b\", colorArray[colorIndex][2]);
                changeColorButton.color = Qt.rgba(colorArray[colorIndex][0],
                                                  colorArray[colorIndex][1],
                                                  colorArray[colorIndex][2], 1.0);
            }
        }
        onRunningChanged: console.log(\"Presentation running\")
    }

    // Some buttons to control the scene
    Rectangle {
        id: slideButton
        anchors.left: parent.left
        anchors.top: parent.top
        width: 200
        height: 50
        border.color: \"red\"
        border.width: 3
        color: \"white\"
        Text {
            id: slideButtonText
            anchors.fill: parent
            text: \"Change Slide (1)\"
            font.pointSize: 16
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                studio3D.presentation.nextSlide();
            }
        }
    }
    Rectangle {
        id: resetTimeButton
        anchors.left: parent.left
        anchors.top: slideButton.bottom
        width: 200
        height: 50
        border.color: \"red\"
        border.width: 3
        color: \"white\"
        Text {
            anchors.fill: parent
            text: \"Reset Time\"
            font.pointSize: 16
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                studio3D.presentation.resetTime();
            }
        }
    }
    Rectangle {
        id: changeColorButton
        anchors.left: parent.left
        anchors.top: resetTimeButton.bottom
        width: 200
        height: 50
        border.color: \"red\"
        border.width: 3
        color: \"white\"
        Text {
            anchors.fill: parent
            text: \"Change Color\"
            font.pointSize: 16
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                studio3D.presentation.changeColor();
            }
        }
    }
}"
    }

    PropertyAnimation {
        target: text
        property: "y"
        from: 0
        to: (lines-27) * -16
        duration: lines * 600
        loops: Animation.Infinite
        running: true
    }
}
