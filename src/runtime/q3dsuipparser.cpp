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

#include "q3dsuipparser_p.h"
#include "q3dspresentationcommon_p.h"
#include "q3dsutils_p.h"
#include "q3dsenummaps_p.h"
#include <QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcUip, "q3ds.uip")
Q_LOGGING_CATEGORY(lcUipProp, "q3ds.uipprop")
Q_DECLARE_LOGGING_CATEGORY(lcPerf)

/*
 Basic structure:

<?xml version="1.0" encoding="UTF-8" ?>
<UIP version="3" >
    <Project >
        <ProjectSettings author="" company="" presentationWidth="800" presentationHeight="480"
            presentationRotation="None" maintainAspect="False" />
        <Classes >
            <CustomMaterial id="aluminum" name="aluminum" sourcepath=".\aluminum.material" />
            <Effect id="dof" name="Depth Of Field HQ Blur" sourcepath=".\effects\Depth Of Field HQ Blur.effect" />
            ...
        </Classes>
        <BufferData>
            <ImageBuffer sourcepath="..." hasTransparency="true" />
        </BufferData>
        <Graph>
            <Scene id="Scene">
                <Layer id="layer"> scene can only have layer children
                    <Camera id="Camera" />
                    <Light id="Light" />
                    ...
                    <Model id="Cube">
                        <Material id="Material" />
                    </Model>
                </Layer>
                ... more layers
            </Scene>
        </Graph>
        <Logic>
            <State name="Master" component="#Scene"> master slide
                <Add ref="#Layer />
                <Add ref="#Camera" />
                <Add ref="#Light" />
                <State id=... name=... playmode="..."> normal slides
                    <Add ref="#Cube" name="Cube" rotation="-52.4901 -10.7851 -20.7213" scale="2 2 2" sourcepath="#Cube">
                      ...
                    </Add>
                    <Add ref="#Material" />
                </State>
                ... more States (slides)
            </State>
        </Logic>
    </Project>
</UIP>
 */

// Returns a Presentation and does not maintain ownership
Q3DSUipPresentation *Q3DSUipParser::parse(const QString &filename)
{
    if (!setSource(filename))
        return nullptr;

    return createPresentation();
}

Q3DSUipPresentation *Q3DSUipParser::parseData(const QByteArray &data)
{
    if (!setSourceData(data))
        return nullptr;

    return createPresentation();
}

Q3DSUipPresentation *Q3DSUipParser::createPresentation()
{
    // reset (not owned by Q3DSUipParser)
    m_presentation.reset(new Q3DSUipPresentation);

    m_presentation->setSourceFile(sourceInfo()->absoluteFilePath());

    QXmlStreamReader *r = reader();
    if (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("UIP") && r->attributes().value(QLatin1String("version")) == QStringLiteral("3"))
            parseUIP();
        else
            r->raiseError(QObject::tr("Not a UIP version 3 document."));
    }

    if (r->hasError()) {
        Q3DSUtils::showMessage(readerErrorString());
        return nullptr;
    }

    resolveReferences(m_presentation->scene());

    qint64 loadTime = elapsedSinceSetSource();
    qCDebug(lcPerf, "Presentation %s loaded in %lld ms", qPrintable(m_presentation->sourceFile()), loadTime);
    m_presentation->setLoadTime(loadTime);

    return m_presentation.take();
}

void Q3DSUipParser::parseUIP()
{
    QXmlStreamReader *r = reader();
    int projectCount = 0;
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Project")) {
            ++projectCount;
            if (projectCount == 1)
                parseProject();
            else
                r->raiseError(QObject::tr("Multiple Project elements found."));
        } else {
            r->skipCurrentElement();
        }
    }
}

void Q3DSUipParser::parseProject()
{
    QXmlStreamReader *r = reader();
    bool foundGraph = false;
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("ProjectSettings"))
            parseProjectSettings();
        else if (r->name() == QStringLiteral("Classes"))
            parseClasses();
        else if (r->name() == QStringLiteral("BufferData"))
            parseBufferData();
        else if (r->name() == QStringLiteral("Graph")) {
            if (!foundGraph) {
                foundGraph = true;
                parseGraph();
            } else {
                r->raiseError(QObject::tr("Multiple Graph elements found."));
            }
        } else if (r->name() == QStringLiteral("Logic")) {
            if (foundGraph)
                parseLogic();
            else
                r->raiseError(QObject::tr("Encountered Logic element before Graph."));
        } else {
            r->skipCurrentElement();
        }
    }
}

void Q3DSUipParser::parseProjectSettings()
{
    QXmlStreamReader *r = reader();
    for (const QXmlStreamAttribute &attr : r->attributes()) {
        if (attr.name() == QStringLiteral("author")) {
            m_presentation->setAuthor(attr.value().toString());
        } else if (attr.name() == QStringLiteral("company")) {
            m_presentation->setCompany(attr.value().toString());
        } else if (attr.name() == QStringLiteral("presentationWidth")) {
            int w;
            if (Q3DS::convertToInt(attr.value(), &w, "presentation width", r))
                m_presentation->setPresentationWidth(w);
        } else if (attr.name() == QStringLiteral("presentationHeight")) {
            int h;
            if (Q3DS::convertToInt(attr.value(), &h, "presentation height", r))
                m_presentation->setPresentationHeight(h);
        } else if (attr.name() == QStringLiteral("presentationRotation")) {
            Q3DSUipPresentation::Rotation v;
            if (Q3DSEnumMap::enumFromStr(attr.value(), &v))
                m_presentation->setPresentationRotation(v);
            else
                r->raiseError(QObject::tr("Invalid presentation rotation \"%1\"").arg(attr.value().toString()));
        } else if (attr.name() == QStringLiteral("maintainAspect")) {
            bool v;
            if (Q3DS::convertToBool(attr.value(), &v, "maintainAspect value", r))
                m_presentation->setMaintainAspectRatio(v);
        }
    }
    r->skipCurrentElement();
}

void Q3DSUipParser::parseClasses()
{
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("CustomMaterial"))
            parseCustomMaterial();
        else if (r->name() == QStringLiteral("Effect"))
            parseEffect();
        else if (r->name() == QStringLiteral("RenderPlugin"))
            r->raiseError(QObject::tr("RenderPlugin not supported"));
        else
            r->skipCurrentElement();
    }
}

void Q3DSUipParser::parseCustomMaterial()
{
    QXmlStreamReader *r = reader();
    auto a = r->attributes();

    QStringRef id = a.value(QStringLiteral("id"));
    QStringRef name = a.value(QStringLiteral("name"));
    QStringRef sourcePath = a.value(QStringLiteral("sourcepath"));

    const QString src = assetFileName(sourcePath.toString(), nullptr);
    if (!m_presentation->loadCustomMaterial(id, name, src))
        r->raiseError(QObject::tr("Failed to parse custom material %1").arg(src));

    r->skipCurrentElement();
}

void Q3DSUipParser::parseEffect()
{
    QXmlStreamReader *r = reader();
    auto a = r->attributes();

    QStringRef id = a.value(QStringLiteral("id"));
    QStringRef name = a.value(QStringLiteral("name"));
    QStringRef sourcePath = a.value(QStringLiteral("sourcepath"));

    const QString src = assetFileName(sourcePath.toString(), nullptr);
    if (!m_presentation->loadEffect(id, name, src))
        r->raiseError(QObject::tr("Failed to parse effect %1").arg(src));

    r->skipCurrentElement();
}

void Q3DSUipParser::parseBufferData()
{
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("ImageBuffer"))
            parseImageBuffer();
        else
            r->skipCurrentElement();
    }
}

void Q3DSUipParser::parseImageBuffer()
{
    QXmlStreamReader *r = reader();
    auto a = r->attributes();
    const QStringRef &sourcePath = a.value(QStringLiteral("sourcepath"));
    const QStringRef &hasTransparency = a.value(QStringLiteral("hasTransparency"));

    if (!sourcePath.isEmpty() && !hasTransparency.isEmpty())
        m_presentation->registerImageBuffer(sourcePath.toString(), hasTransparency.compare(QStringLiteral("True")) == 0);

    r->skipCurrentElement();
}

void Q3DSUipParser::parseGraph()
{
    QXmlStreamReader *r = reader();
    int sceneCount = 0;
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Scene")) {
            ++sceneCount;
            if (sceneCount == 1)
                parseScene();
            else
                r->raiseError(QObject::tr("Multiple Scene elements found."));
        } else {
            r->skipCurrentElement();
        }
    }
}

void Q3DSUipParser::parseScene()
{
    QXmlStreamReader *r = reader();
    QByteArray id = getId(r->name());
    if (id.isEmpty())
        return;

    auto scene = new Q3DSScene;
    scene->setProperties(r->attributes(), Q3DSGraphObject::PropSetDefaults);
    m_presentation->registerObject(id, scene);
    m_presentation->setScene(scene);

    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Layer"))
            parseObjects(scene);
        else
            r->raiseError(QObject::tr("Scene can only have Layer children."));
    }
}

void Q3DSUipParser::parseObjects(Q3DSGraphObject *parent)
{
    QXmlStreamReader *r = reader();
    Q_ASSERT(parent);
    QByteArray id = getId(r->name());
    if (id.isEmpty())
        return;

    Q3DSGraphObject *obj = nullptr;

    if (r->name() == QStringLiteral("Layer"))
        obj = new Q3DSLayerNode;
    else if (r->name() == QStringLiteral("Camera"))
        obj = new Q3DSCameraNode;
    else if (r->name() == QStringLiteral("Light"))
        obj = new Q3DSLightNode;
    else if (r->name() == QStringLiteral("Model"))
        obj = new Q3DSModelNode;
    else if (r->name() == QStringLiteral("Group"))
        obj = new Q3DSGroupNode;
    else if (r->name() == QStringLiteral("Component"))
        obj = new Q3DSComponentNode;
    else if (r->name() == QStringLiteral("Text"))
        obj = new Q3DSTextNode;
    else if (r->name() == QStringLiteral("Material"))
        obj = new Q3DSDefaultMaterial;
    else if (r->name() == QStringLiteral("ReferencedMaterial"))
        obj = new Q3DSReferencedMaterial;
    else if (r->name() == QStringLiteral("CustomMaterial"))
        obj = new Q3DSCustomMaterialInstance;
    else if (r->name() == QStringLiteral("Effect"))
        obj = new Q3DSEffectInstance;
    else if (r->name() == QStringLiteral("Image"))
        obj = new Q3DSImage;

    if (!obj) {
        r->skipCurrentElement();
        return;
    }

    obj->setProperties(r->attributes(), Q3DSGraphObject::PropSetDefaults);
    m_presentation->registerObject(id, obj);
    parent->appendChildNode(obj);

    while (r->readNextStartElement())
        parseObjects(obj);
}

void Q3DSUipParser::parseLogic()
{
    QXmlStreamReader *r = reader();
    int masterCount = 0;
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("State")) {
            QStringRef compRef = r->attributes().value(QLatin1String("component"));
            if (!compRef.startsWith('#')) {
                r->raiseError(QObject::tr("Invalid ref '%1' in State").arg(compRef.toString()));
                return;
            }
            Q3DSGraphObject *slideTarget = m_presentation->object(compRef.mid(1).toUtf8());
            if (!slideTarget) {
                r->raiseError(QObject::tr("State references unknown object '%1'").arg(compRef.mid(1).toString()));
                return;
            }
            const QByteArray idPrefix = compRef.mid(1).toUtf8();
            if (slideTarget->type() == Q3DSGraphObject::Scene) {
                if (++masterCount == 1) {
                    auto masterSlide = parseSlide(nullptr, idPrefix);
                    Q_ASSERT(masterSlide);
                    m_presentation->setMasterSlide(masterSlide);
                } else {
                    r->raiseError(QObject::tr("Multiple State (master slide) elements found."));
                }
            } else {
                Q3DSSlide *componentMasterSlide = parseSlide(nullptr, idPrefix);
                Q_ASSERT(componentMasterSlide);
                static_cast<Q3DSComponentNode *>(slideTarget)->m_masterSlide = componentMasterSlide; // transfer ownership
            }
        } else {
            r->raiseError(QObject::tr("Logic can only have State children."));
        }
    }
}

Q3DSSlide *Q3DSUipParser::parseSlide(Q3DSSlide *parent, const QByteArray &idPrefix)
{
    QXmlStreamReader *r = reader();
    QByteArray id = getId(r->name(), false);
    const bool isMaster = !parent;
    if (isMaster) {
        // The master slide may not have an id.
        if (id.isEmpty())
            id = idPrefix + QByteArrayLiteral("-Master");
    }
    if (id.isEmpty())
        return nullptr;

    Q3DSSlide *slide = new Q3DSSlide;
    slide->setProperties(r->attributes(), Q3DSGraphObject::PropSetDefaults);
    m_presentation->registerObject(id, slide);
    if (parent)
        parent->appendChildNode(slide);

    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("State")) {
            if (isMaster)
                parseSlide(slide);
            else
                r->raiseError(QObject::tr("Encountered sub-slide in sub-slide."));
        } else if (r->name() == QStringLiteral("Add")) {
            parseAddSet(slide, false, isMaster);
        } else if (r->name() == QStringLiteral("Set")) {
            parseAddSet(slide, true, isMaster);
        } else {
            r->skipCurrentElement();
        }
    }

    return slide;
}

void Q3DSUipParser::parseAddSet(Q3DSSlide *slide, bool isSet, bool isMaster)
{
    QXmlStreamReader *r = reader();
    QStringRef ref = r->attributes().value(QLatin1String("ref"));
    if (!ref.startsWith('#')) {
        r->raiseError(QObject::tr("Invalid ref '%1' in Add/Set").arg(ref.toString()));
        return;
    }

    Q3DSGraphObject *obj = m_presentation->object(ref.mid(1).toUtf8());
    if (!obj) {
        r->raiseError(QObject::tr("Add/Set references unknown object '%1'").arg(ref.mid(1).toString()));
        return;
    }

    if (!isSet) {
        // Add: register the object for this slide
        slide->addObject(obj);
        // and set the properties on the object right away.
        Q3DSGraphObject::PropSetFlags flags = 0;
        if (isMaster)
            flags |= Q3DSGraphObject::PropSetOnMaster;
        obj->setProperties(r->attributes(), flags);
    } else {
        // Set: store the property changes
        QScopedPointer<Q3DSPropertyChangeList> changeList(new Q3DSPropertyChangeList);
        for (const QXmlStreamAttribute &attr : r->attributes()) {
            if (attr.name() == QStringLiteral("ref"))
                continue;
            changeList->append(Q3DSPropertyChange(attr.name().toString(), attr.value().toString()));
        }
        if (!changeList->isEmpty())
            slide->m_propChanges.insert(obj, changeList.take());
    }

    // Store animations.
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("AnimationTrack")) {
            Q3DSAnimationTrack animTrack;
            animTrack.m_target = obj;
            for (const QXmlStreamAttribute &attr : r->attributes()) {
                if (attr.name() == QStringLiteral("property")) {
                    animTrack.m_property = attr.value().toString().trimmed();
                } else if (attr.name() == QStringLiteral("type")) {
                    if (!Q3DSEnumMap::enumFromStr(attr.value(), &animTrack.m_type))
                        r->raiseError(QObject::tr("Unknown animation type %1").arg(attr.value().toString()));
                } else if (attr.name() == QStringLiteral("dynamic")) {
                    Q3DS::convertToBool(attr.value(), &animTrack.m_dynamic, "'dynamic' attribute value", r);
                }
            }
            parseAnimationKeyFrames(r->readElementText(QXmlStreamReader::SkipChildElements).trimmed(), &animTrack);
            if (!animTrack.m_keyFrames.isEmpty())
                slide->m_anims.append(animTrack);
        } else {
            r->skipCurrentElement();
        }
    }
}

void Q3DSUipParser::parseAnimationKeyFrames(const QString &data, Q3DSAnimationTrack *animTrack)
{
    QXmlStreamReader *r = reader();
    QString spaceOnlyData = data;
    spaceOnlyData.replace('\n', ' ');
    const QStringList values = spaceOnlyData.split(' ', QString::SkipEmptyParts);
    if (values.isEmpty() || values.first().isEmpty())
        return;

    switch (animTrack->type()) {
    case Q3DSAnimationTrack::Linear:
        if (!(values.count() % 2)) {
            for (int i = 0; i < values.count() / 2; ++i) {
                Q3DSAnimationTrack::KeyFrame kf;
                if (!Q3DS::convertToFloat(&values[i * 2], &kf.time, "keyframe time", r))
                    continue;
                if (!Q3DS::convertToFloat(&values[i * 2 + 1], &kf.value, "keyframe value", r))
                    continue;
                animTrack->m_keyFrames.append(kf);
            }
        } else {
            r->raiseError(QObject::tr("Invalid Linear animation track: %1").arg(spaceOnlyData));
            return;
        }
        break;
    case Q3DSAnimationTrack::EaseInOut:
        if (!(values.count() % 4)) {
            for (int i = 0; i < values.count() / 4; ++i) {
                Q3DSAnimationTrack::KeyFrame kf;
                if (!Q3DS::convertToFloat(&values[i * 4], &kf.time, "keyframe time", r))
                    continue;
                if (!Q3DS::convertToFloat(&values[i * 4 + 1], &kf.value, "keyframe value", r))
                    continue;
                if (!Q3DS::convertToFloat(&values[i * 4 + 2], &kf.easeIn, "keyframe EaseIn", r))
                    continue;
                if (!Q3DS::convertToFloat(&values[i * 4 + 3], &kf.easeOut, "keyframe EaseOut", r))
                    continue;
                animTrack->m_keyFrames.append(kf);
            }
        } else {
            r->raiseError(QObject::tr("Invalid EaseInOut animation track: %1").arg(spaceOnlyData));
            return;
        }
        break;
    case Q3DSAnimationTrack::Bezier:
        if (!(values.count() % 6)) {
            for (int i = 0; i < values.count() / 6; ++i) {
                Q3DSAnimationTrack::KeyFrame kf;
                if (!Q3DS::convertToFloat(&values[i * 6], &kf.time, "keyframe time", r))
                    continue;
                kf.time *= 1000.0f;
                if (!Q3DS::convertToFloat(&values[i * 6 + 1], &kf.value, "keyframe value", r))
                    continue;
                if (i < values.count() / 6 - 1) {
                    if (!Q3DS::convertToFloat(&values[i * 6 + 2], &kf.c2time, "keyframe C2 time", r))
                        continue;
                    kf.c2time *= 1000.0f;
                    if (!Q3DS::convertToFloat(&values[i * 6 + 3], &kf.c2value, "keyframe C2 value", r))
                        continue;
                } else { // last keyframe
                    kf.c2time = kf.c2value = 0.0f;
                }
                if (!Q3DS::convertToFloat(&values[i * 6 + 4], &kf.c1time, "keyframe C1 time", r))
                    continue;
                kf.c1time *= 1000.0f;
                if (!Q3DS::convertToFloat(&values[i * 6 + 5], &kf.c1value, "keyframe C1 value", r))
                    continue;
                animTrack->m_keyFrames.append(kf);
            }
        } else {
            r->raiseError(QObject::tr("Invalid Bezier animation track: %1").arg(spaceOnlyData));
            return;
        }
        break;
    default:
        break;
    }
}

QByteArray Q3DSUipParser::getId(const QStringRef &desc, bool required)
{
    QByteArray id = reader()->attributes().value(QLatin1String("id")).toUtf8();
    if (id.isEmpty() && required)
        reader()->raiseError(QObject::tr("Missing %1 id.").arg(desc.toString()));
    return id;
}

void Q3DSUipParser::resolveReferences(Q3DSGraphObject *obj)
{
    if (!obj)
        return;

    obj->resolveReferences(*m_presentation, *this);

    for (int i = 0, ie = obj->childCount(); i != ie; ++i)
        resolveReferences(obj->childAtIndex(i));
}

QT_END_NAMESPACE
