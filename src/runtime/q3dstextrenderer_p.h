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

#ifndef Q3DSTEXTRENDERER_P_H
#define Q3DSTEXTRENDERER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QVector>
#include <QStringList>
#include <QFont>

QT_BEGIN_NAMESPACE

class Q3DSTextNode;
class Q3DSSceneManager;
class QFontMetricsF;
class QPainter;

class Q3DSTextRenderer
{
public:
    Q3DSTextRenderer(Q3DSSceneManager *sceneManager);
    void registerFonts(const QStringList &dirs);
    QSize textImageSize(Q3DSTextNode *text3DS);
    void renderText(QPainter *painter, Q3DSTextNode *text3DS);

private:
    struct Font {
        QString fontName;
        QString fontFamily;
        QFont font;
    };

    Font *findFont(const QString &name);
    void updateFontInfo(Font *font, Q3DSTextNode *text3DS);
    QRectF textBoundingBox(Q3DSTextNode *text3DS, const QFontMetricsF &fm,
                           const QStringList &lineList, QVector<float> *lineWidths);

    QVector<Font> m_fonts;
    Q3DSSceneManager *m_sceneManager;
};

QT_END_NAMESPACE

#endif // Q3DSTEXTRENDERER_P_H
