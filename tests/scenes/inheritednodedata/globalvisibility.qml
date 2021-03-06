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

/*[[
    <Property name="target" formalName="Target" type="ObjectRef" default="Scene.Layer.Camera" description="Some object" />
    <Property name="startImmediately" formalName="Start Immediately?" type="Boolean" default="True" publishLevel="Advanced" description="Start immediately, or wait for the Enable action to be called?" />

    <Handler name="start" formalName="Start" category="Dummy" description="Start" />
    <Handler name="stop" formalName="Stop" category="Dummy" description="Stop" />
    <Handler name="cube_eyeball_0" formalName="cube_eyeball_0" category="actionevent" description="Make cube invisible" />
    <Handler name="cube_eyeball_1" formalName="cube_eyeball_1" category="actionevent" description="Make cube visible" />
    <Handler name="cone_eyeball_0" formalName="cone_eyeball_0" category="actionevent" description="Make cone invisible" />
    <Handler name="cone_eyeball_1" formalName="cone_eyeball_1" category="actionevent" description="Make cone visible" />
    <Handler name="sphere2_eyeball_0" formalName="sphere2_eyeball_0" category="actionevent" description="Make sphere invisible" />
    <Handler name="sphere2_eyeball_1" formalName="sphere2_eyeball_1" category="actionevent" description="Make sphere visible" />
]]*/

import QtStudio3D.Behavior 1.1

Behavior {
    property string target
    property bool startImmediately

    function cube_eyeball_0() {
        setAttribute("Cube", "eyeball", false)
    }

    function cube_eyeball_1() {
        setAttribute("Cube", "eyeball", true)
    }

    function cone_eyeball_0() {
        setAttribute("Cone", "eyeball", false)
    }

    function cone_eyeball_1() {
        setAttribute("Cone", "eyeball", true)
    }

    function sphere2_eyeball_0() {
        setAttribute("Sphere2", "eyeball", false)
    }

    function sphere2_eyeball_1() {
        setAttribute("Sphere2", "eyeball", true)
    }
}
