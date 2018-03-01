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

#include "q3dsabstractxmlparser_p.h"
#include "q3dsutils_p.h"

QT_BEGIN_NAMESPACE

bool Q3DSAbstractXmlParser::setSource(const QString &filename)
{
    if (m_sourceFile.isOpen())
        m_sourceFile.close();

    m_sourceFile.setFileName(filename);
    if (!m_sourceFile.exists()) {
        Q3DSUtils::showMessage(QObject::tr("Source file %1 does not exist").arg(filename));
        return false;
    }
    if (!m_sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Q3DSUtils::showMessage(QObject::tr("Failed to open %1").arg(filename));
        return false;
    }

    m_parseTimer.start();

    m_sourceInfo = QFileInfo(filename);

    m_reader.setDevice(&m_sourceFile);

    return true;
}

bool Q3DSAbstractXmlParser::setSourceData(const QByteArray &data)
{
    m_parseTimer.start();

    m_reader.clear();
    m_reader.addData(data);

    // ### should have a way to set m_sourceInfo so that assetFileName can
    // provide results relative to something

    return true;
}

Q3DSAbstractXmlParser::~Q3DSAbstractXmlParser()
{

}

QString Q3DSAbstractXmlParser::readerErrorString() const
{
    return QObject::tr("Failed to parse %1: line %2: column %3: offset %4: %5")
            .arg(m_sourceInfo.fileName())
            .arg(m_reader.lineNumber())
            .arg(m_reader.columnNumber())
            .arg(m_reader.characterOffset())
            .arg(m_reader.errorString());
}

QT_END_NAMESPACE
