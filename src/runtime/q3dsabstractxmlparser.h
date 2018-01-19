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

#ifndef Q3DSABSTRACTXMLPARSER_H
#define Q3DSABSTRACTXMLPARSER_H

#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <QXmlStreamReader>
#include <QFileInfo>
#include <QFile>
#include <QElapsedTimer>

QT_BEGIN_NAMESPACE

class Q3DSV_EXPORT Q3DSAbstractXmlParser
{
public:
    virtual ~Q3DSAbstractXmlParser();

    QFileInfo *sourceInfo() { return &m_sourceInfo; }

    quint64 elapsedSinceSetSource() const { return m_parseTimer.elapsed(); }
    QString readerErrorString() const;

    QString assetFileName(const QString &xmlFileNameRef, int *part) const;

protected:
    bool setSource(const QString &filename);
    bool setSourceData(const QByteArray &data);
    QXmlStreamReader *reader() { return &m_reader; }

private:
    QXmlStreamReader m_reader;
    QFileInfo m_sourceInfo;
    QFile m_sourceFile;
    QElapsedTimer m_parseTimer;
};

QT_END_NAMESPACE

#endif // Q3DSABSTRACTXMLPARSER_H
