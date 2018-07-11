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

#include "q3dsconsolecommands_p.h"
#include "q3dsscenemanager_p.h"
#include "q3dsslideplayer_p.h"
#include "q3dslogging_p.h"
#if QT_CONFIG(q3ds_profileui)
#include "profileui/q3dsconsole_p.h"
#endif

#include <QFile>
#include <QDir>
#include <QTimer>

#include <Qt3DRender/QParameter>

QT_BEGIN_NAMESPACE

Q3DSConsoleCommands::Q3DSConsoleCommands(Q3DSSceneManager *mainPresSceneManager)
    : m_sceneManager(mainPresSceneManager)
{
}

static const QColor responseColor = Qt::cyan;
static const QColor longResponseColor = Qt::lightGray;
static const QColor errorColor = Qt::red;

static QByteArray unquote(const QByteArray &s)
{
    QByteArray r = s.trimmed();
    if (r.startsWith('\''))
        r = r.mid(1, r.lastIndexOf('\'') - 1);
    else if (r.startsWith('\"'))
        r = r.mid(1, r.lastIndexOf('\"') - 1);
    return r;
}

static QString printObject(Q3DSGraphObject *obj)
{
    return QString(QLatin1String("%1 id='%2' name='%3'"))
            .arg(QString::fromLatin1(obj->typeAsString())).arg(QString::fromUtf8(obj->id())).arg(obj->name());
}

static void graphPrinter(QString *dst, Q3DSGraphObject *obj, int indent)
{
    while (obj) {
        QString spaces;
        spaces.fill(QLatin1Char(' '), indent);
        *dst += spaces + printObject(obj) + QLatin1Char('\n');
        graphPrinter(dst, obj->firstChild(), indent + 2);
        if (indent != 0)
            obj = obj->nextSibling();
        else
            break; // skip siblings of the root node (relevant when printing a subtree, i.e. root != pres->scene())
    }
}

static QString printGraph(Q3DSGraphObject *root)
{
    QString result;
    graphPrinter(&result, root, 0);
    return result;
}

static QString printMatrix(const QMatrix4x4 &m)
{
    QString result = QLatin1String("( ");
    const float *p = m.constData();
    for (int i = 0; i < 16; ++i) {
        result += QString::number(*p++);
        result += QLatin1Char(' ');
    }
    result += QLatin1Char(')');
    return result;
}

static void removeFromSlide_helper(Q3DSGraphObject *obj, Q3DSSlide *slide)
{
    while (slide) {
        if (slide->objects().contains(obj)) {
            slide->removeObject(obj);
            return;
        }
        removeFromSlide_helper(obj, static_cast<Q3DSSlide *>(slide->firstChild()));
        slide = static_cast<Q3DSSlide *>(slide->nextSibling());
    }
}

void Q3DSConsoleCommands::setupConsole(Q3DSConsole *console)
{
#if QT_CONFIG(q3ds_profileui)
    m_console = console;
    m_console->addMessageFmt(responseColor, "Qt 3D Studio Console, 2nd Edition Ver. 2.31");
    // start with the main presentation active
    setCurrentPresentation(m_sceneManager);
    // in immediate mode
    m_console->setRecording(false);

    m_console->addCommand(Q3DSConsole::makeCommand("help", [this](const QByteArray &) {
        m_console->addMessageFmt(responseColor, "Available commands:\n");
        m_console->addMessageFmt(longResponseColor,
                               "help - Shows this text.\n"
                               "clear - Clears the console. [R]\n"
                               "\n"
                               "presentations - Lists all presentations. (main+sub) [R]\n"
                               "pres(id) - Changes to the given presentation. (id == the id from the .uia or main) Default is main. [R]\n"
                               "\n"
                               "scenegraph - Prints the scene graph in the current presentation. [R]\n"
                               "scenegraph(obj) - Prints the scene graph subtree starting from the given object in the current presentation. [R]\n"
                               "slidegraph - Prints the slide graph in the current presentation. [R]\n"
                               "slidegraph(obj) - Prints the slide graph in the given component node. [R]\n"
                               "datainput - Lists data input entry - object connections. [R]\n"
                               "\n"
                               "properties(obj) - Prints the properties for the given object. [R]\n"
                               "info(obj) - Prints additional properties for the given object (node or slide). [R]\n"
                               "get(obj, property) - Prints the property value. [R]\n"
                               "set(obj, property, value) - Applies and notifies a change to the given property. [R]\n"
                               "\n"
                               "slide(ctxObj) - Prints the current slide on the time context (scene or component object). [R]\n"
                               "setslide(ctxObj, name) - Changes the current slide on the time context (scene or component object). [R]\n"
                               "\n"
                               "object(id, name, type, parentObj, slide) - Creates a new object. (type == Model, DefaultMaterial, ...; parent and slide may be null) [R]\n"
                               "primitive(id, name, source, parentObj, slide) - Shortcut to add a model with a default material. (source == #Cube, #Cone, etc.) [R]\n"
                               "setparent(obj, parentObj, [insertAfterObj]) - Sets the parent (may be null). Appends to child list, unless insertAfterObj is set. [R]\n"
                               "kill(obj) - Removes a node from the scene graph (and from the slides' object list). [R]\n"
                               "objslideadd(obj, slide) - Associates the object with the given slide. [R]\n"
                               "objslideremove(obj, slide) - Removes the object from the slide's objject list. [R]\n"
                               "\n"
                               "record - Switches to recording mode.\n"
                               "immed - Switches to immediate mode (the default).\n"
                               "prgnew - Clears recorded commands and switches to recording mode.\n"
                               "prglist - Lists recorded commands.\n"
                               "prgsave(fn) - Saves recorded commands to the specified file.\n"
                               "prgload(fn) - Loads the specified file.\n"
                               "prgrun - Runs the recorded commands.\n"
                               );
        m_console->addMessageFmt(responseColor, "\n[R] = recordable\nObject references (obj) are either by id ('#id') or by name ('name')\n");
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("clear", [this](const QByteArray &) {
        m_console->clear();
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("presentations", [this](const QByteArray &) {
        m_console->addMessageFmt(longResponseColor, "%s", qPrintable(m_sceneManager->m_presentation->name()));
        for (const Q3DSSubPresentation &subPres : m_sceneManager->m_subPresentations) {
            if (subPres.sceneManager)
                m_console->addMessageFmt(longResponseColor, "%s", qPrintable(subPres.sceneManager->m_presentation->name()));
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("pres", [this](const QByteArray &args) {
        const QString id = QString::fromUtf8(args);
        bool found = false;
        if (id == m_sceneManager->m_presentation->name()) {
            setCurrentPresentation(m_sceneManager);
            found = true;
        } else {
            for (const Q3DSSubPresentation &subPres : m_sceneManager->m_subPresentations) {
                if (subPres.sceneManager && subPres.id == id) {
                    setCurrentPresentation(subPres.sceneManager);
                    found = true;
                    break;
                }
            }
        }
        if (!found)
            m_console->addMessageFmt(errorColor, "Unknown presentation '%s'", qPrintable(id));
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("scenegraph", [this](const QByteArray &args) {
        Q3DSGraphObject *root = m_currentPresentation->scene();
        if (!args.isEmpty())
            root = resolveObj(args);
        if (root)
            m_console->addMessage(printGraph(root), longResponseColor);
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("slidegraph", [this](const QByteArray &args) {
        Q3DSGraphObject *obj = nullptr;
        if (args.isEmpty()) {
            obj = m_currentPresentation->masterSlide();
        } else {
            Q3DSGraphObject *comp = resolveObj(args);
            if (!comp || comp->type() != Q3DSGraphObject::Component) {
                m_console->addMessageFmt(errorColor, "Object '%s' is not a component node", args.constData());
            } else {
                obj = static_cast<Q3DSComponentNode *>(comp)->masterSlide();
            }
        }
        if (obj)
            m_console->addMessage(printGraph(obj), longResponseColor);
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("properties", [this](const QByteArray &args) {
        Q3DSGraphObject *obj = resolveObj(args);
        if (obj) {
            auto names = obj->propertyNames();
            auto values = obj->propertyValues();
            const auto customProperties = obj->dynamicProperties();
            for (int i = 0; i < names.count(); ++i) {
                if (customProperties.contains(QString::fromLatin1(names[i])))
                    continue;
                m_console->addMessageFmt(longResponseColor, "%s = %s",
                                         names[i].constData(),
                                         qPrintable(Q3DS::convertFromVariant(values[i])));
            }

            if (!customProperties.isEmpty()) {
                m_console->addMessageFmt(longResponseColor, "\nCustom properties:");
                for (auto it = customProperties.cbegin(), itEnd = customProperties.cend(); it != itEnd; ++it) {
                    m_console->addMessageFmt(longResponseColor, "%s = %s",
                                             qPrintable(it.key()),
                                             qPrintable(Q3DS::convertFromVariant(it.value())));
                }
            }
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("info", [this](const QByteArray &args) {
        Q3DSGraphObject *obj = resolveObj(args);
        if (obj) {
            Q3DSComponentNode *owningComponent = obj->attached() ? obj->attached()->component : nullptr;
            m_console->addMessageFmt(longResponseColor, "Owner component node: %s (%p)",
                                     owningComponent ? owningComponent->id().constData() : "",
                                     owningComponent);
            if (obj->isNode()) {
                Q3DSNode *node = static_cast<Q3DSNode *>(obj);
                auto d = obj->attached<Q3DSNodeAttached>();
                if (d) {
                    m_console->addMessageFmt(longResponseColor, "Global transform: %s\nGlobal opacity: %f\n"
                                                                "Local logical visibility (eyeball): %d\n"
                                                                "Global logical visibility (inherited eyeball): %d\n"
                                                                "Local effective visibility (eyeball+slide): %d\n"
                                                                "Global effective visibility (inherited eyeball+slide): %d\n",
                                             qPrintable(printMatrix(d->globalTransform)),
                                             d->globalOpacity,
                                             node->flags().testFlag(Q3DSNode::Active),
                                             d->globalLogicalVisibility,
                                             node->flags().testFlag(Q3DSNode::Active) && d->visibilityTag == Q3DSGraphObjectAttached::Visible,
                                             d->globalEffectiveVisibility);
                } else {
                    m_console->addMessageFmt(longResponseColor, "No attached object. (node not in scene?)");
                }
                if (d && d->lightsData) {
                    auto printLightSources = [this](const QVector<Q3DSLightSource> &lightSources) {
                        for (int i = 0; i < lightSources.count(); ++i) {
                            const Q3DSLightSource &lightSource(lightSources.at(i));
                            m_console->addMessageFmt(longResponseColor, "  %d:\n    global position %s\n    global direction %s\n",
                                                     i,
                                                     qPrintable(Q3DS::convertFromVariant(lightSource.positionParam->value())),
                                                     qPrintable(Q3DS::convertFromVariant(lightSource.directionParam->value())));
                        }
                    };
                    m_console->addMessageFmt(longResponseColor, "%d lights for this subtree\nNon-area lights:\n",
                                             d->lightsData->allLights.count());
                    printLightSources(d->lightsData->nonAreaLights);
                    m_console->addMessageFmt(longResponseColor, "Area lights:\n");
                    printLightSources(d->lightsData->areaLights);
                }
            } else if (obj->type() == Q3DSGraphObject::Slide) {
                auto slide = static_cast<Q3DSSlide *>(obj);
                m_console->addMessageFmt(longResponseColor,
                                         "This slide has %d animation tracks and %d property changes.",
                                         slide->animations().count(),
                                         slide->propertyChanges().count());
                m_console->addMessageFmt(longResponseColor, "Objects belonging to this slide:\n");
                for (Q3DSGraphObject *slideObj : slide->objects())
                    m_console->addMessageFmt(longResponseColor, "  %s\n", qPrintable(printObject(slideObj)));
                qint32 startTime = 0, endTime = 0;
                Q3DSSlideUtils::getStartAndEndTime(slide, &startTime, &endTime);
                m_console->addMessageFmt(longResponseColor, "Start time: %d End time: %d\n", startTime, endTime);
                Q3DSSlidePlayer *slidePlayer = slide->parent()
                        ? static_cast<Q3DSSlide *>(slide->parent())->attached<Q3DSSlideAttached>()->slidePlayer
                        : slide->attached<Q3DSSlideAttached>()->slidePlayer;
                if (slidePlayer)
                    m_console->addMessageFmt(longResponseColor, "Play position: %f\n", slidePlayer->position());
            }
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("get", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 2) {
            const QByteArray ref = splitArgs[0];
            const QByteArray name = unquote(splitArgs[1]);
            Q3DSGraphObject *obj = resolveObj(ref);
            if (obj) {
                const QVariant value = obj->property(name);
                if (!value.isNull()) {
                    const QString v = Q3DS::convertFromVariant(value);
                    m_console->addMessageFmt(responseColor, "%s", qPrintable(v));
                }
            }
        } else {
            m_console->addMessageFmt(errorColor, "Invalid arguments, expected 2");
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("set", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 3) {
            const QByteArray ref = splitArgs[0];
            const QByteArray name = unquote(splitArgs[1]);
            const QByteArray value = unquote(splitArgs[2]);
            Q3DSGraphObject *obj = resolveObj(ref);
            if (obj) {
                Q3DSPropertyChangeList cl = { Q3DSPropertyChange(QString::fromUtf8(name), QString::fromUtf8(value)) };
                obj->applyPropertyChanges(cl);
                obj->notifyPropertyChanges(cl);
                m_console->addMessageFmt(responseColor, "%s on %s set to %s",
                                         name.constData(), ref.constData(), value.constData());
            }
        } else {
            m_console->addMessageFmt(errorColor, "Invalid arguments, expected 3");
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("datainput", [this](const QByteArray &) {
        auto diMap = m_currentPresentation->dataInputMap();
        if (diMap) {
            for (auto it = diMap->cbegin(); it != diMap->cend(); ++it) {
                m_console->addMessageFmt(longResponseColor, "%s\n  %s",
                                         qPrintable(it.key()), qPrintable(printObject(it.value())));
                for (const QString &propName : it.value()->dataInputControlledProperties()->values(it.key()))
                    m_console->addMessageFmt(longResponseColor, "    %s", qPrintable(propName));
            }
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("slide", [this](const QByteArray &args) {
        Q3DSGraphObject *ctx = resolveObj(args);
        if (!ctx)
            return;
        Q3DSSlide *slide = m_currentSceneManager->currentSlideForSceneOrComponent(ctx);
        if (slide)
            m_console->addMessageFmt(responseColor, "Current slide is %s", qPrintable(printObject(slide)));
        else
            m_console->addMessageFmt(errorColor, "No slide (invalid context?)");
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("setslide", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 2) {
            Q3DSGraphObject *ctx = resolveObj(splitArgs[0]);
            if (!ctx)
                return;
            QString slideName = QString::fromUtf8(unquote(splitArgs[1]));
            m_currentSceneManager->changeSlideByName(ctx, slideName);
            m_console->addMessageFmt(responseColor, "Changed");
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("kill", [this](const QByteArray &args) {
        Q3DSGraphObject *obj = resolveObj(args);
        if (obj) {
            // Unlink (incl. all children) from the slide.
            Q3DSSlide *slide = m_currentPresentation->masterSlide();
            if (obj->attached() && obj->attached()->component)
                slide = obj->attached()->component->masterSlide();
            Q3DSUipPresentation::forAllObjectsInSubTree(obj, [slide](Q3DSGraphObject *objOrChild) {
                removeFromSlide_helper(objOrChild, slide);
            });
            // Unregister from presentation and unlink from parent. This
            // triggers the scene change notification.
            m_currentPresentation->unlinkObject(obj);
            // Bye bye.
            delete obj;
            m_console->addMessageFmt(responseColor, "Removed");
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("primitive", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 5) {
            const QByteArray id = unquote(splitArgs[0]);
            const QString name = QString::fromUtf8(unquote(splitArgs[1]));
            const QString source = QString::fromUtf8(unquote(splitArgs[2]));
            Q3DSGraphObject *parent = resolveObj(splitArgs[3]);
            Q3DSSlide *slide = static_cast<Q3DSSlide *>(resolveObj(splitArgs[4]));
            Q3DSModelNode *model = m_currentPresentation->newObject<Q3DSModelNode>(id);
            if (!model)
                return;
            model->setName(name);
            model->setMesh(source);
            model->resolveReferences(*m_currentPresentation);
            const QByteArray matId = id + QByteArrayLiteral("_material");
            Q3DSDefaultMaterial *mat = m_currentPresentation->newObject<Q3DSDefaultMaterial>(matId);
            if (!mat)
                return;
            mat->setName(QString::fromUtf8(matId));
            model->appendChildNode(mat);
            if (slide) {
                slide->addObject(model);
                slide->addObject(mat);
            }
            // adding to parent must be the last, since it triggers a scene
            // change notification to the scene manager
            if (parent)
                parent->appendChildNode(model);
            m_console->addMessageFmt(responseColor, "Added");
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("object", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 5) {
            const QByteArray id = unquote(splitArgs[0]);
            const QString name = QString::fromUtf8(unquote(splitArgs[1]));
            const QByteArray type = unquote(splitArgs[2]);
            Q3DSGraphObject *parent = resolveObj(splitArgs[3]);
            Q3DSSlide *slide = static_cast<Q3DSSlide *>(resolveObj(splitArgs[4]));
            Q3DSGraphObject *obj = m_currentPresentation->newObject(type.constData(), id);
            if (obj) {
                obj->setName(name);
                if (slide)
                    slide->addObject(obj);
                // adding to parent must be the last, since it triggers a scene
                // change notification to the scene manager
                if (parent)
                    parent->appendChildNode(obj);
                m_console->addMessageFmt(responseColor, "Added");
            } else {
                m_console->addMessageFmt(errorColor, "Unknown type or duplicate id");
            }
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("setparent", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 2) {
            Q3DSGraphObject *obj = resolveObj(splitArgs[0]);
            Q3DSGraphObject *parent = resolveObj(splitArgs[1]);
            Q3DSGraphObject *insertAfterObj = nullptr;
            bool insertAfterRequested = false;
            if (splitArgs.count() >= 3) {
                insertAfterRequested = true;
                insertAfterObj = resolveObj(splitArgs[2]); // can be null
            }
            if (obj) {
                if (parent) {
                    if (obj->parent())
                        obj->parent()->removeChildNode(obj);
                    // A typical setParent operation only allows appending
                    // to the children list. This is often too restrictive,
                    // so add a way to insert after a specified child
                    // (which may be null, when inserting as first child).
                    if (!insertAfterRequested) {
                        parent->appendChildNode(obj);
                    } else {
                        if (insertAfterObj)
                            parent->insertChildNodeAfter(obj, insertAfterObj);
                        else
                            parent->prependChildNode(obj);
                    }
                    m_console->addMessageFmt(responseColor, "Reparented");
                } else {
                    if (obj->parent()) {
                        obj->parent()->removeChildNode(obj);
                        m_console->addMessageFmt(responseColor, "Unparented");
                    }
                }
            }
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("objslideadd", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 2) {
            Q3DSGraphObject *obj = resolveObj(splitArgs[0]);
            Q3DSSlide *slide = static_cast<Q3DSSlide *>(resolveObj(splitArgs[1]));
            if (obj && slide) {
                slide->addObject(obj);
                m_console->addMessageFmt(responseColor, "Added to slide object list");
            }
        }
    }, Q3DSConsole::CmdRecordable));
    m_console->addCommand(Q3DSConsole::makeCommand("objslideremove", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 2) {
            Q3DSGraphObject *obj = resolveObj(splitArgs[0]);
            Q3DSSlide *slide = static_cast<Q3DSSlide *>(resolveObj(splitArgs[1]));
            if (obj && slide) {
                slide->removeObject(obj);
                m_console->addMessageFmt(responseColor, "Removed from slide object list");
            }
        }
    }, Q3DSConsole::CmdRecordable));

    m_console->setCommandRecorder([this](const QByteArray &cmd) {
        m_program.append(cmd);
    });
    m_console->addCommand(Q3DSConsole::makeCommand("record", [this](const QByteArray &) {
        m_console->setRecording(true);
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("immed", [this](const QByteArray &) {
        m_console->setRecording(false);
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("prgnew", [this](const QByteArray &) {
        m_program.clear();
        m_console->addMessageFmt(responseColor, "Cleared");
        m_console->setRecording(true);
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("prglist", [this](const QByteArray &) {
        for (const QByteArray &line : m_program)
            m_console->addMessageFmt(longResponseColor, "%s\n", line.constData());
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("prgsave", [this](const QByteArray &args) {
        QFile f(QString::fromUtf8(args));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            for (const QByteArray &line : m_program) {
                f.write(line);
                f.write("\n");
            }
            m_console->addMessageFmt(responseColor, "Saved");
        } else {
            m_console->addMessageFmt(errorColor, "Save failed");
        }
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("prgload", [this](const QByteArray &args) {
        if (args == QByteArrayLiteral("$")) { // hehe
            QStringList entries = QDir(QLatin1String(".")).entryList();
            m_program.clear();
            for (const QString &entry : entries)
                m_program.append(entry.toUtf8()); // cannot be run but can be listed
        } else {
            QFile f(QString::fromUtf8(args));
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_program = f.readAll().split('\n');
                auto it = m_program.begin();
                while (it != m_program.end()) {
                    *it = it->trimmed();
                    if (it->isEmpty() || it->startsWith("//"))
                        it = m_program.erase(it);
                    else
                        ++it;
                }
                m_console->addMessageFmt(responseColor, "Loaded");
            } else {
                m_console->addMessageFmt(errorColor, "Load failed");
            }
        }
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("prgrun", [this](const QByteArray &) {
        for (const QByteArray &line : m_program)
            m_console->runRecordedCommand(line);
    }));
#else
    Q_UNUSED(console);
#endif
}

void Q3DSConsoleCommands::setCurrentPresentation(Q3DSSceneManager *sceneManager)
{
#if QT_CONFIG(q3ds_profileui)
    m_currentSceneManager = sceneManager;
    m_currentPresentation = sceneManager->m_presentation;
    m_console->addMessageFmt(responseColor, "Switched to presentation '%s' (%s)",
                             qPrintable(m_currentPresentation->name()),
                             qPrintable(m_currentPresentation->sourceFile()));
#else
    Q_UNUSED(pres);
#endif
}

Q3DSGraphObject *Q3DSConsoleCommands::resolveObj(const QByteArray &ref, bool showErrorWhenNotFound)
{
    Q3DSGraphObject *obj = nullptr;
    const QByteArray r = unquote(ref);

    if (r.startsWith('#'))
        obj = m_currentPresentation->object(r.mid(1));
    else if (r == QByteArrayLiteral("null"))
        return nullptr;
    else
        obj = m_currentPresentation->objectByName(QString::fromUtf8(r));

    if (!obj && showErrorWhenNotFound)
        m_console->addMessageFmt(errorColor, "Object '%s' not found", r.constData());

    return obj;
}

void Q3DSConsoleCommands::runBootScript()
{
#if QT_CONFIG(q3ds_profileui)
    static const bool bootScriptEnabled = qEnvironmentVariableIsSet("Q3DS_BOOTSCRIPT");
    if (!bootScriptEnabled)
        return;

    auto run = [this] {
        QScopedPointer<Q3DSConsole> tempConsole;
        if (!m_console) {
            // Create a temporary instance in case the console view was not yet
            // invoked by the user. An eventual invocation will call setupConsole()
            // again (with m_console being unused in the meantime) so this is safe.
            tempConsole.reset(new Q3DSConsole);
            setupConsole(tempConsole.data());
        }

        QFile f(QLatin1String("autoexec.drgscr"));
        if (f.exists()) {
            qCDebug(lcScene, "Running boot script");
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                while (!f.atEnd()) {
                    QByteArray line = f.readLine().trimmed();
                    if (!line.isEmpty() && !line.startsWith(QByteArrayLiteral("//")))
                        m_console->runRecordedCommand(line);
                }
            }
        }
    };

    static const int bootScriptTimeout = qEnvironmentVariableIntValue("Q3DS_BOOTSCRIPT");
    // 0 = run immediately after building the scene, otherwise seconds to delay
    if (bootScriptTimeout == 0)
        run();
    else
        QTimer::singleShot(bootScriptTimeout * 1000, run);
#endif
}

QT_END_NAMESPACE
