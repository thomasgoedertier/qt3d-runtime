/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt 3D Studio.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "q3dssubpresentationsettings_p.h"
#include <private/q3dsinlineqmlsubpresentation_p.h>

QT_BEGIN_NAMESPACE

Q3DSSubPresentationSettings::Q3DSSubPresentationSettings(QObject *parent)
    : QObject(parent)
{
}

Q3DSSubPresentationSettings::~Q3DSSubPresentationSettings()
{
    qDeleteAll(m_list);
}

QQmlListProperty<Q3DSInlineQmlSubPresentation> Q3DSSubPresentationSettings::qmlStreams()
{
    return QQmlListProperty<Q3DSInlineQmlSubPresentation>(this, m_list);
}

/*!
    \qmltype SubPresentationSettings
    \inqmlmodule QtStudio3D
    \ingroup 3dstudioruntime2
    \brief Settings for sub-presentations.

    This type enables specifying a QML sub-presentation directly in the QML.

    A QML sub-presentation is a Qt Quick scene that is rendered to a texture
    and then used either as the contents of Qt 3D Studio layer or as a texture
    map. The contents of such a sub-presentation can be provided in two ways:
    by a separate \c{.qml} file, or directly under the Studio3D element.
    SubPresentationSettings is the enabler for the latter.

    \section2 Usage

    \qml
    Studio3D {
        id: studio3D
        Presentation {
            source: "file:///presentation.uia"
            SubPresentationSettings {
                qmlStreams: [
                    QmlStream {
                        presentationId: "sub-presentation-id"
                        Item {
                            width: 1024
                            height: 1024
                            // ... other Qt Quick items
                        }
                    }
                ]
            }
        }
    }
    \endqml
 */

/*!
    \qmlproperty list<QmlStream> SubPresentationSettings::qmlStreams

    Holds the list of QmlStream children.
 */

/*!
    \qmltype QmlStream
    \inqmlmodule QtStudio3D
    \ingroup 3dstudioruntime2
    \brief QML stream.

    This type allows attaching QML sub-presentation with a quick item. The item is rendered to a
    texture and used as a part of a Qt 3D Studio presentation.

    The sub-presentation element must be specified in the \e assets element of the presentation
    .uia file:

     \badcode
     <assets ...>
         <presentation-qml id="presentation-id" args="preview-presentation.qml" />
     </assets>
     \endcode

    The \c presentation-id attribute must contain a unique ID of the sub-presentation.
    The \c args attribute may contain an optional preview version of the item, which is only
    used in the Viewer application.
 */

/*!
    \qmlproperty string QmlStream::presentationId

    Holds the string ID of the sub-presentation the item is attached to. The id must be one of
    the \c presentation-qml IDs specified in the .uia file.
 */

/*!
    \qmlproperty Item QmlStream::item

    Holds the item attached to the sub-presentation. The item size is used as the the size of the
    texture the item is rendered to. Default values \c{(256, 256)} are used if the item doesn't
    specify a size.
 */

QT_END_NAMESPACE
