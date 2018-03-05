/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "q3dsbehavior_p.h"
#include "q3dsutils_p.h"
#include "q3dsabstractxmlparser_p.h"
#include <QFile>

QT_BEGIN_NAMESPACE

class BehaviorMetaDataParser : public Q3DSAbstractXmlParser
{
public:
    BehaviorMetaDataParser(const QString &data);

    bool parse();

    const QMap<QString, Q3DSBehavior::Property> &properties() const { return m_properties; }
    const QMap<QString, Q3DSBehavior::Handler> &handlers() const { return m_handlers; }

private:
    void registerProperty(const QXmlStreamAttributes &attrs);
    void registerHandler(const QXmlStreamAttributes &attrs);

    QString m_data;
    QMap<QString, Q3DSBehavior::Property> m_properties;
    QMap<QString, Q3DSBehavior::Handler> m_handlers;
};

BehaviorMetaDataParser::BehaviorMetaDataParser(const QString &data)
{
    setSourceData(data.toUtf8());
}

bool BehaviorMetaDataParser::parse()
{
    QXmlStreamReader *r = reader();
    if (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("BehaviorMeta")) {
            while (r->readNextStartElement()) {
                if (r->name() == QStringLiteral("Property"))
                    registerProperty(r->attributes());
                else if (r->name() == QStringLiteral("Handler"))
                    registerHandler(r->attributes());
                r->skipCurrentElement();
            }
        } else {
            r->raiseError(QObject::tr("Not a BehaviorMeta document"));
        }
    }

    if (r->hasError()) {
        Q3DSUtils::showMessage(readerErrorString());
        return false;
    }

    return true;
}

void BehaviorMetaDataParser::registerProperty(const QXmlStreamAttributes &attrs)
{
    Q3DSBehavior::Property p;
    p.name = attrs.value(QLatin1String("name")).toString();
    p.formalName = attrs.value(QLatin1String("formalName")).toString();
    Q3DS::convertToPropertyType(attrs.value(QLatin1String("type")), &p.type, nullptr, "property type", reader());
    p.defaultValue = attrs.value(QLatin1String("default")).toString();
    p.publishLevel = attrs.value(QLatin1String("publishLevel")).toString();
    p.description = attrs.value(QLatin1String("description")).toString();
    m_properties.insert(p.name, p);
}

void BehaviorMetaDataParser::registerHandler(const QXmlStreamAttributes &attrs)
{
    Q3DSBehavior::Handler h;
    h.name = attrs.value(QLatin1String("name")).toString();
    h.formalName = attrs.value(QLatin1String("formalName")).toString();
    h.category = attrs.value(QLatin1String("category")).toString();
    h.description = attrs.value(QLatin1String("description")).toString();
    m_handlers.insert(h.name, h);
}

Q3DSBehavior::Q3DSBehavior()
{
}

bool Q3DSBehavior::isNull() const
{
    return m_qmlCode.isEmpty();
}

Q3DSBehavior Q3DSBehaviorParser::parse(const QString &filename, bool *ok)
{
    if (ok)
        *ok = false;

    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Q3DSUtils::showMessage(QObject::tr("Failed to open %1").arg(filename));
        return Q3DSBehavior();
    }

    QString contents = QString::fromUtf8(f.readAll());
    if (contents.isEmpty())
        return Q3DSBehavior();

    Q3DSBehavior behavior;
    behavior.m_qmlCode = contents;
    behavior.m_qmlSourceUrl = QUrl::fromLocalFile(filename);

    // Find the metadata looking like this:
    // /*[[
    //     <Property name="cameraTarget" formalName="Camera Target" type="ObjectRef" default="Scene.Layer.Camera" description="Object in scene the camera should look at" />
    //     <Property name="startImmediately" formalName="Start Immediately?" type="Boolean" default="True" publishLevel="Advanced" description="Start immediately, or wait for the Enable action to be called?" />
    //
    //     <Handler name="start" formalName="Start" category="CameraLookAt" description="Begin looking the target" />
    //     <Handler name="stop" formalName="Stop" category="CameraLookAt" description="Stop looking the target" />
    // ]]*/

    const int metaStartPos = contents.indexOf(QLatin1String("/*[["));
    if (metaStartPos >= 0) {
        const int metaEndPos = contents.indexOf(QLatin1String("]]*/"), metaStartPos);
        if (metaEndPos > metaStartPos) {
            const QString prefix = QLatin1String("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<BehaviorMeta>\n");
            const QString suffix = QLatin1String("</BehaviorMeta>");
            const QString metadata = prefix + contents.mid(metaStartPos + 4, metaEndPos - metaStartPos - 4) + suffix;
            BehaviorMetaDataParser parser(metadata);
            if (parser.parse()) {
                behavior.m_properties = parser.properties();
                behavior.m_handlers = parser.handlers();
            }
        }
    }

    if (ok)
        *ok = true;

    return behavior;
}

QT_END_NAMESPACE
