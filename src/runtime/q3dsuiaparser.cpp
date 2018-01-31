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

#include "q3dsuiaparser.h"
#include "q3dsutils_p.h"
#include <QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcUip)

Q3DSUiaParser::Uia Q3DSUiaParser::parse(const QString &filename)
{
    Q3DSUiaParser::Uia invalidUia;

    if (!setSource(filename))
        return invalidUia;

    m_uia = Q3DSUiaParser::Uia();

    QXmlStreamReader *r = reader();
    if (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("application"))
            parseApplication();
        else
            r->raiseError(QObject::tr("Not a valid uia document: %1").arg(filename));
    }
    if (r->hasError()) {
        Q3DSUtils::showMessage(readerErrorString());
        return invalidUia;
    }

    m_uia.loadTimeMsecs = elapsedSinceSetSource();
    qCDebug(lcUip, "%s loaded in %lld ms", qPrintable(filename), m_uia.loadTimeMsecs);
    return m_uia;
}

void Q3DSUiaParser::parseApplication()
{
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("assets")) {
            QStringRef initialId = r->attributes().value(QLatin1String("initial"));
            if (!initialId.isEmpty())
                m_uia.initialPresentationId = initialId.toString();
            parsePresentations();
        } else {
            // statemachine not supported
            r->skipCurrentElement();
        }
    }
}

void Q3DSUiaParser::parsePresentations()
{
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("presentation")) {
            QXmlStreamAttributes attrs = r->attributes();
            QStringRef id = attrs.value(QLatin1String("id"));
            QStringRef src = attrs.value(QLatin1String("src"));
            if (!id.isEmpty() && !src.isEmpty()) {
                Uia::Presentation pres;
                pres.type = Uia::Presentation::Uip;
                pres.id = id.toString();
                pres.source = src.toString();
                m_uia.presentations.append(pres);
                if (m_uia.initialPresentationId.isEmpty())
                    m_uia.initialPresentationId = pres.id;
            } else {
                r->raiseError(QObject::tr("Malformed presentation element"));
            }
        } else if (r->name() == QStringLiteral("presentation-qml")) {
            QXmlStreamAttributes attrs = r->attributes();
            QStringRef id = attrs.value(QLatin1String("id"));
            QStringRef args = attrs.value(QLatin1String("args"));
            if (!id.isEmpty()) {
                Uia::Presentation pres;
                pres.type = Uia::Presentation::Qml;
                pres.id = id.toString();
                pres.source = args.toString();
                m_uia.presentations.append(pres);
            }
        }
        r->skipCurrentElement();
    }
}

QT_END_NAMESPACE
