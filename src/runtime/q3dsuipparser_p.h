/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

#ifndef Q3DSUIPPARSER_P_H
#define Q3DSUIPPARSER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "q3dsruntimeglobal_p.h"
#include "q3dsabstractxmlparser_p.h"
#include "q3dspresentation_p.h"

QT_BEGIN_NAMESPACE

class Q3DSV_PRIVATE_EXPORT Q3DSUipParser : public Q3DSAbstractXmlParser
{
public:
    Q3DSPresentation *parse(const QString &filename);
    Q3DSPresentation *parseData(const QByteArray &data);

private:
    Q3DSPresentation *createPresentation();
    void parseUIP();
    void parseProject();
    void parseProjectSettings();
    void parseClasses();
    void parseCustomMaterial();
    void parseEffect();
    void parseBufferData();
    void parseImageBuffer();
    void parseGraph();
    void parseScene();
    void parseObjects(Q3DSGraphObject *parent);
    void parseLogic();
    Q3DSSlide *parseSlide(Q3DSSlide *parent = nullptr, const QByteArray &idPrefix = QByteArray());
    void parseAddSet(Q3DSSlide *slide, bool isSet, bool isMaster);
    void parseAnimationKeyFrames(const QString &data, Q3DSAnimationTrack *animTrack);

    QByteArray getId(const QStringRef &desc, bool required = true);
    void resolveReferences(Q3DSGraphObject *obj);

    QScopedPointer<Q3DSPresentation> m_presentation;
};

QT_END_NAMESPACE

#endif
