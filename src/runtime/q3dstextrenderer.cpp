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

#include "q3dstextrenderer_p.h"
#include "q3dsuippresentation_p.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QRawFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QLoggingCategory>
#include <qmath.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcScene)

void Q3DSTextRenderer::registerFonts(const QStringList &dirs)
{
    const QStringList nameFilters = { QStringLiteral("*.ttf"), QStringLiteral("*.otf") };

    for (const QString &dir : dirs) {
        QDir fontDir(dir);
        if (!fontDir.exists()) {
            qWarning("Attempted to register invalid font directory: %s", qPrintable(dir));
            continue;
        }

        const QStringList entryList = fontDir.entryList(nameFilters);

        for (QString entry : entryList) {
            entry = fontDir.absoluteFilePath(entry);
            const QString fontName = QFileInfo(entry).baseName();
            if (std::find_if(m_fonts.cbegin(), m_fonts.cend(),
                             [fontName](const Font &f) { return f.fontName == fontName; }) != m_fonts.cend())
            {
                // already registered
                continue;
            }
            QFile file(entry);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray rawData = file.readAll();
                int fontId = QFontDatabase::addApplicationFontFromData(rawData);
                if (fontId < 0) {
                    qWarning("Failed to register font %s", qPrintable(entry));
                } else {
                    Font f;
                    f.fontName = fontName;
                    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
                    if (!families.isEmpty()) {
                        f.fontFamily = families.first();
                        f.font.setFamily(f.fontFamily);
                    }
                    QRawFont rawFont(rawData, 1.0);
                    if (rawFont.isValid()) {
                        f.font.setStyle(rawFont.style());
                        f.font.setWeight(rawFont.weight());
                    } else {
                        qWarning("Failed to construct QRawFont from %s", qPrintable(entry));
                    }
                    m_fonts.append(f);
                    qCDebug(lcScene, "Registered font %s with family %s", qPrintable(f.fontName), qPrintable(f.fontFamily));
                }
            } else {
                qWarning("Failed to open %s", qPrintable(entry));
            }
        }
    }
}

Q3DSTextRenderer::Font *Q3DSTextRenderer::findFont(const QString &name)
{
    for (Font &f : m_fonts) {
        if (f.fontName == name)
            return &f;
    }

    // Pick the fallback, default font if it was needed before.
    for (Font &f : m_fonts) {
        if (f.fontName.isEmpty())
            return &f;
    }

    // Register the fallback, default font, and return that.
    Font fallback;
    m_fonts.append(fallback);
    return &m_fonts[m_fonts.count() - 1];
}

void Q3DSTextRenderer::updateFontInfo(Font *font, Q3DSTextNode *text3DS)
{
    font->font.setPointSizeF(qreal(text3DS->size()));
    font->font.setLetterSpacing(QFont::AbsoluteSpacing, qreal(text3DS->tracking()));
}

QRectF Q3DSTextRenderer::textBoundingBox(Q3DSTextNode *text3DS, const QFontMetricsF &fm,
                                         const QStringList &lineList, QVector<float> *lineWidths)
{
    QRectF boundingBox;
    boundingBox.setHeight(lineList.size() * fm.height() + qCeil(qreal(lineList.size() - 1) * qreal(text3DS->leading())));

    lineWidths->resize(lineList.size());

    for (int i = 0; i < lineList.size(); ++i) {
        const float width = float(fm.width(lineList[i]));
        const float right = float(fm.boundingRect(lineList[i]).right());
        const float lineWidth = qMax(width, right);
        (*lineWidths)[i] = lineWidth;
        if (float(boundingBox.width()) < lineWidth)
            boundingBox.setWidth(qreal(lineWidth));
    }

    boundingBox.setRight(qMax(boundingBox.left(), boundingBox.right() - qFloor(qreal(text3DS->tracking()))));

    return boundingBox;
}

static constexpr int nextMultipleOf4(int v)
{
    return (v >= 0 && v % 4) ? v += 4 - (v % 4) : v;
}

static inline int mapVertAlign(Q3DSTextNode *text3DS)
{
    switch (text3DS->verticalAlignment()) {
    case Q3DSTextNode::Top:
        return Qt::AlignTop;
    case Q3DSTextNode::Bottom:
        return Qt::AlignBottom;
    default:
        return Qt::AlignVCenter;
    }
}

QSize Q3DSTextRenderer::textImageSize(Q3DSTextNode *text3DS)
{
    Q3DSTextRenderer::Font *font = findFont(text3DS->font());
    Q_ASSERT(font);

    updateFontInfo(font, text3DS);

    QFontMetricsF fm(font->font);
    const QStringList lineList = text3DS->text().split('\n');
    QVector<float> lineWidths;
    QRectF boundingBox = textBoundingBox(text3DS, fm, lineList, &lineWidths);
    if (boundingBox.isEmpty())
        boundingBox.setSize(QSizeF(4, 4));

    return QSize(nextMultipleOf4(int(boundingBox.width())), nextMultipleOf4(int(boundingBox.height())));
}

void Q3DSTextRenderer::renderText(QPainter *painter, Q3DSTextNode *text3DS)
{
    Q3DSTextRenderer::Font *font = findFont(text3DS->font());
    Q_ASSERT(font);

    updateFontInfo(font, text3DS);

    QFontMetricsF fm(font->font);
    const QStringList lineList = text3DS->text().split('\n');
    QVector<float> lineWidths;
    QRectF boundingBox = textBoundingBox(text3DS, fm, lineList, &lineWidths);
    if (boundingBox.isEmpty())
        boundingBox.setSize(QSizeF(4, 4));

    const QSize sz(nextMultipleOf4(int(boundingBox.width())), nextMultipleOf4(int(boundingBox.height())));
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->fillRect(0, 0, sz.width(), sz.height(), Qt::transparent);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);

    painter->setPen(Qt::white); // coloring is done in the shader
    painter->setFont(font->font);

    qreal tracking = 0.0;
    switch (text3DS->horizontalAlignment()) {
    case Q3DSTextNode::Center:
        tracking += qreal(text3DS->tracking()) / 2.0;
        break;
    case Q3DSTextNode::Right:
        tracking += qreal(text3DS->tracking());
        break;
    default:
        break;
    }

    const qreal lineHeight = fm.height();
    qreal nextHeight = 0.0;
    for (int i = 0; i < lineList.size(); ++i) {
        const QString &line = lineList.at(i);
        qreal xTranslation = tracking;
        switch (text3DS->horizontalAlignment()) {
        case Q3DSTextNode::Center:
            xTranslation += (boundingBox.width() - qreal(lineWidths.at(i))) / 2.0;
            break;
        case Q3DSTextNode::Right:
            xTranslation += boundingBox.width() - qreal(lineWidths.at(i));
            break;
        default:
            break;
        }
        QRectF bound(xTranslation, nextHeight, qreal(lineWidths.at(i)), lineHeight);
        QRectF actualBound;
        painter->drawText(bound, mapVertAlign(text3DS) | Qt::TextDontClip | Qt::AlignLeft, line, &actualBound);
        nextHeight += lineHeight + qreal(text3DS->leading());
    }
}

QT_END_NAMESPACE
