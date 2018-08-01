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
#include "q3dslogging_p.h"
#include "q3dsenummaps_p.h"
#include <QLoggingCategory>

QT_BEGIN_NAMESPACE

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
Q3DSUipPresentation *Q3DSUipParser::parse(const QString &filename, const QString &presentationName)
{
    if (!setSource(filename))
        return nullptr;

    return createPresentation(presentationName);
}

Q3DSUipPresentation *Q3DSUipParser::parseData(const QByteArray &data, const QString &presentationName)
{
    if (!setSourceData(data))
        return nullptr;

    return createPresentation(presentationName);
}

Q3DSUipPresentation *Q3DSUipParser::createPresentation(const QString &presentationName)
{
    // reset (not owned by Q3DSUipParser)
    m_presentation.reset(new Q3DSUipPresentation);

    m_presentation->setSourceFile(sourceInfo()->absoluteFilePath());
    m_presentation->setName(presentationName.isEmpty() ? QLatin1String("main") : presentationName);

    QXmlStreamReader *r = reader();
    if (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("UIP")
                && (r->attributes().value(QLatin1String("version")) == QStringLiteral("4")
                    || r->attributes().value(QLatin1String("version")) == QStringLiteral("3"))) {
            parseUIP();
        } else {
            r->raiseError(QObject::tr("UIP version is too low, and is no longer supported."));
        }
    }

    if (r->hasError()) {
        Q3DSUtils::showMessage(readerErrorString());
        return nullptr;
    }

    resolveReferences(m_presentation->scene());
    resolveReferences(m_presentation->masterSlide());

    m_presentation->resolveAliases();
    m_presentation->updateObjectStateForSubTrees();
    m_presentation->addImplicitPropertyChanges();

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
        if (r->name() == QStringLiteral("CustomMaterial")) {
            parseExternalFileRef(std::bind(&Q3DSUipPresentation::loadCustomMaterial, m_presentation.data(),
                                           std::placeholders::_1, std::placeholders::_2));
        } else if (r->name() == QStringLiteral("Effect")) {
            parseExternalFileRef(std::bind(&Q3DSUipPresentation::loadEffect, m_presentation.data(),
                                           std::placeholders::_1, std::placeholders::_2));
        } else if (r->name() == QStringLiteral("Behavior")) {
            parseExternalFileRef(std::bind(&Q3DSUipPresentation::loadBehavior, m_presentation.data(),
                                           std::placeholders::_1, std::placeholders::_2));
        } else if (r->name() == QStringLiteral("RenderPlugin")) {
            r->raiseError(QObject::tr("RenderPlugin not supported"));
        } else {
            r->skipCurrentElement();
        }
    }
}

void Q3DSUipParser::parseExternalFileRef(ExternalFileLoadCallback callback)
{
    QXmlStreamReader *r = reader();
    auto a = r->attributes();

    const QStringRef id = a.value(QStringLiteral("id"));
    const QStringRef sourcePath = a.value(QStringLiteral("sourcepath"));

    // custommaterial/effect/behavior all expect ids to be prefixed with #
    const QByteArray decoratedId = QByteArrayLiteral("#") + id.toUtf8();
    const QString src = m_presentation->assetFileName(sourcePath.toString(), nullptr);
    if (!callback(decoratedId, src))
        r->raiseError(QObject::tr("Failed to load external file %1").arg(src));

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
    scene->addDataInputControlledProperties(getDataInputControlledProperties());
    m_presentation->registerObject(id, scene);
    m_presentation->setScene(scene);

    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Layer") || r->name() == QStringLiteral("Behavior"))
            parseObjects(scene);
        else
            r->raiseError(QObject::tr("Scene can only have Layer or Behavior children."));
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
    else if (r->name() == QStringLiteral("Behavior"))
        obj = new Q3DSBehaviorInstance;
    else if (r->name() == QStringLiteral("Image"))
        obj = new Q3DSImage;
    else if (r->name() == QStringLiteral("Alias"))
        obj = new Q3DSAliasNode;

    if (!obj) {
        r->skipCurrentElement();
        return;
    }

    obj->setProperties(r->attributes(), Q3DSGraphObject::PropSetDefaults);
    obj->addDataInputControlledProperties(getDataInputControlledProperties());
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
    slide->addDataInputControlledProperties(getDataInputControlledProperties());
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
            if (attr.name() == QStringLiteral("sourcepath")) {
                QString absoluteFileName =
                        m_presentation->assetFileName(attr.value().toString(), nullptr);
                changeList->append(Q3DSPropertyChange(attr.name().toString(), absoluteFileName));
            } else {
                changeList->append(Q3DSPropertyChange(attr.name().toString(), attr.value().toString()));
            }
        }
        if (!changeList->isEmpty())
            slide->addPropertyChanges(obj, changeList.take());
    }

    // controlledproperty attributes may be present in the Logic section as well.
    obj->addDataInputControlledProperties(getDataInputControlledProperties());

    // Store animations and actions.
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
                slide->addAnimation(animTrack);
        } else if (r->name() == QStringLiteral("Action")) {
            Q3DSAction action;
            action.owner = obj;
            action.id = getId(QStringRef(), false);
            // we only support simple, static action definitions; no changes afterwards
            if (!action.id.isEmpty()) {
                for (const QXmlStreamAttribute &attr : r->attributes()) {
                    if (attr.name() == QStringLiteral("eyeball")) {
                        Q3DS::convertToBool(attr.value(), &action.eyeball, "'eyeball' attribute value", r);
                    } else if (attr.name() == QStringLiteral("triggerObject")) {
                        action.triggerObject_unresolved = attr.value().trimmed().toString();
                    } else if (attr.name() == QStringLiteral("event")) {
                        action.event = attr.value().trimmed().toString();
                    } else if (attr.name() == QStringLiteral("targetObject")) {
                        action.targetObject_unresolved = attr.value().trimmed().toString();
                    } else if (attr.name() == QStringLiteral("handler")) {
                        if (!Q3DSEnumMap::enumFromStr(attr.value(), &action.handler)) {
                            action.handler = Q3DSAction::BehaviorHandler;
                            action.behaviorHandler = attr.value().trimmed().toString();
                        }
                    }
                }
                // Parse the HandlerArgument children.
                while (r->readNextStartElement()) {
                    if (r->name() == QStringLiteral("HandlerArgument")) {
                        Q3DSAction::HandlerArgument ha;
                        for (const QXmlStreamAttribute &attr : r->attributes()) {
                            if (attr.name() == QStringLiteral("name")) {
                                ha.name = attr.value().trimmed().toString();
                            } else if (attr.name() == QStringLiteral("type")) {
                                if (!Q3DS::convertToPropertyType(attr.value(), &ha.type, nullptr, "handler type", r))
                                    r->raiseError(QObject::tr("Invalid action handler type %1").arg(attr.value().toString()));
                            } else if (attr.name() == QStringLiteral("argtype")) {
                                if (!Q3DSEnumMap::enumFromStr(attr.value(), &ha.argType))
                                    r->raiseError(QObject::tr("Invalid action handler argtype %1").arg(attr.value().toString()));
                            } else if (attr.name() == QStringLiteral("value")) {
                                ha.value = attr.value().trimmed().toString();
                            }
                        }
                        action.handlerArgs.append(ha);
                    }
                    r->skipCurrentElement();
                }
                slide->addAction(action);
            } else {
                r->skipCurrentElement();
            }
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

Q3DSGraphObject::DataInputControlledProperties Q3DSUipParser::getDataInputControlledProperties()
{
    Q3DSGraphObject::DataInputControlledProperties dataInputControlledProperties;
    const QStringRef cp = reader()->attributes().value(QLatin1String("controlledproperty"));
    if (!cp.isEmpty()) {
        QVector<QStringRef> nameTargetPairs = cp.trimmed().split(' ', QString::SkipEmptyParts);
        for (int i = 0; i < nameTargetPairs.count(); i += 2) {
            if (i + 1 == nameTargetPairs.count()) {
                reader()->raiseError(QObject::tr("Malformed controlledproperty attribute: %1").arg(cp.toString()));
                return dataInputControlledProperties;
            }
            // remove prefix '$' from datainput controller name
            QString controller = nameTargetPairs[i].toString();
            if (controller.at(0) == QLatin1Char('$'))
                controller.remove(0, 1);

            dataInputControlledProperties.insert(controller, nameTargetPairs[i + 1].toString());
        }
    }
    return dataInputControlledProperties;
}

void Q3DSUipParser::resolveReferences(Q3DSGraphObject *obj)
{
    while (obj) {
        obj->resolveReferences(*m_presentation);
        m_presentation->registerDataInputTarget(obj);

        resolveReferences(obj->firstChild());
        obj = obj->nextSibling();
    }
}

QT_END_NAMESPACE
