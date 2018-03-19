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
#if QT_CONFIG(q3ds_profileui)
#include "profileui/q3dsconsole_p.h"
#endif

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
            .arg(obj->typeAsString()).arg(obj->id().constData()).arg(obj->name());
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
    setCurrentPresentation(m_sceneManager->m_presentation);

    m_console->addCommand(Q3DSConsole::makeCommand("help", [this](const QByteArray &) {
        m_console->addMessageFmt(responseColor, "Available commands:\n");
        m_console->addMessageFmt(longResponseColor,
                               "help - Shows this text.\n"
                               "clear - Clears the console.\n"
                               "presentations - Lists all presentations. (main+sub)\n"
                               "pres(id) - Changes to the given presentation. (id == the id from the .uia or main) Default is main.\n"
                               "scenegraph - Prints the scene graph in the current presentation.\n"
                               "scenegraph(obj) - Prints the scene graph subtree starting from the given object in the current presentation.\n"
                               "slidegraph - Prints the slide graph in the current presentation.\n"
                               "slidegraph(obj) - Prints the slide graph in the given component node.\n"
                               "properties(obj) - Prints the properties for the given object.\n"
                               "info(obj) - Prints additional properties for the given object (node or slide).\n"
                               "get(obj, property) - Prints the property value.\n"
                               "set(obj, property, value) - Applies and notifies a change to the given property.\n"
                               "kill(obj) - Removes a node from the scene graph (and from the slides' object list).\n"
                               "primitive(id, name, source, parentObj, slide) - Adds a model node with one default material. (source == #Cube, #Cone, etc.)\n"
                               );
        m_console->addMessageFmt(responseColor, "\nObject references (obj) are either by id ('#id') or by name ('name')\n");
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("clear", [this](const QByteArray &) {
        m_console->clear();
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("presentations", [this](const QByteArray &) {
        m_console->addMessageFmt(longResponseColor, "%s", qPrintable(m_sceneManager->m_presentation->name()));
        for (const Q3DSSubPresentation &subPres : m_sceneManager->m_subPresentations) {
            if (subPres.sceneManager)
                m_console->addMessageFmt(longResponseColor, "%s", qPrintable(subPres.sceneManager->m_presentation->name()));
        }
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("pres", [this](const QByteArray &args) {
        const QString id = QString::fromUtf8(args);
        bool found = false;
        if (id == m_sceneManager->m_presentation->name()) {
            setCurrentPresentation(m_sceneManager->m_presentation);
            found = true;
        } else {
            for (const Q3DSSubPresentation &subPres : m_sceneManager->m_subPresentations) {
                if (subPres.sceneManager && subPres.id == id) {
                    setCurrentPresentation(subPres.sceneManager->m_presentation);
                    found = true;
                    break;
                }
            }
        }
        if (!found)
            m_console->addMessageFmt(errorColor, "Unknown presentation '%s'", qPrintable(id));
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("scenegraph", [this](const QByteArray &args) {
        Q3DSGraphObject *root = m_currentPresentation->scene();
        if (!args.isEmpty())
            root = resolveObj(args);
        if (root)
            m_console->addMessage(printGraph(root), longResponseColor);
    }));
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
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("properties", [this](const QByteArray &args) {
        Q3DSGraphObject *obj = resolveObj(args);
        if (obj) {
            auto names = obj->propertyNames();
            auto values = obj->propertyValues();
            for (int i = 0; i < names.count(); ++i) {
                m_console->addMessageFmt(longResponseColor, "%s = %s",
                                         qPrintable(names[i]),
                                         qPrintable(Q3DS::convertFromVariant(values[i])));
            }
        }
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("info", [this](const QByteArray &args) {
        Q3DSGraphObject *obj = resolveObj(args);
        if (obj) {
            Q3DSComponentNode *owningComponent = obj->attached()->component;
            m_console->addMessageFmt(longResponseColor, "Owner component node: %s (%p)",
                                     owningComponent ? owningComponent->id().constData() : "",
                                     owningComponent);
            if (obj->isNode()) {
                auto d = obj->attached<Q3DSNodeAttached>();
                m_console->addMessageFmt(longResponseColor, "Global transform: %s\nGlobal opacity: %f\nGlobal visibility: %d\n",
                                         qPrintable(printMatrix(d->globalTransform)),
                                         d->globalOpacity,
                                         d->globalVisibility);
            } else if (obj->type() == Q3DSGraphObject::Slide) {
                auto slide = static_cast<Q3DSSlide *>(obj);
                m_console->addMessageFmt(longResponseColor,
                                         "This slide has %d animation tracks and %d property changes.",
                                         slide->animations().count(),
                                         slide->propertyChanges().count());
                m_console->addMessageFmt(longResponseColor, "Objects belonging to this slide:\n");
                for (Q3DSGraphObject *slideObj : slide->objects())
                    m_console->addMessageFmt(longResponseColor, "  %s\n", qPrintable(printObject(slideObj)));
            }
        }
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("get", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 2) {
            const QByteArray ref = splitArgs[0];
            const QByteArray name = unquote(splitArgs[1]);
            Q3DSGraphObject *obj = resolveObj(ref);
            if (obj) {
                const int idx = obj->propertyNames().indexOf(name);
                if (idx >= 0) {
                    const QString v = Q3DS::convertFromVariant(obj->propertyValues().at(idx));
                    m_console->addMessageFmt(responseColor, "%s", qPrintable(v));
                }
            }
        } else {
            m_console->addMessageFmt(errorColor, "Invalid arguments, expected 2");
        }
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("set", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 3) {
            const QByteArray ref = splitArgs[0];
            const QByteArray name = unquote(splitArgs[1]);
            const QByteArray value = unquote(splitArgs[2]);
            Q3DSGraphObject *obj = resolveObj(ref);
            if (obj) {
                Q3DSPropertyChangeList cl = { Q3DSPropertyChange(name, value) };
                obj->applyPropertyChanges(cl);
                obj->notifyPropertyChanges(cl);
                m_console->addMessageFmt(responseColor, "%s on %s set to %s",
                                         name.constData(), ref.constData(), value.constData());
            }
        } else {
            m_console->addMessageFmt(errorColor, "Invalid arguments, expected 3");
        }
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("kill", [this](const QByteArray &args) {
        Q3DSGraphObject *obj = resolveObj(args);
        if (obj) {
            // Unlink (incl. all children) from the slide.
            Q3DSSlide *slide = m_currentPresentation->masterSlide();
            if (obj->attached()->component)
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
    }));
    m_console->addCommand(Q3DSConsole::makeCommand("primitive", [this](const QByteArray &args) {
        QByteArrayList splitArgs = args.split(',');
        if (splitArgs.count() >= 5) {
            const QByteArray id = unquote(splitArgs[0]);
            const QString name = QString::fromUtf8(unquote(splitArgs[1]));
            const QString source = QString::fromUtf8(unquote(splitArgs[2]));
            Q3DSGraphObject *parent = resolveObj(splitArgs[3]);
            Q3DSSlide *slide = static_cast<Q3DSSlide *>(resolveObj(splitArgs[4]));
            if (parent) {
                Q3DSModelNode *model = m_currentPresentation->newObject<Q3DSModelNode>(id);
                model->setName(name);
                model->setMesh(source, *m_currentPresentation);
                const QByteArray matId = id + QByteArrayLiteral("_material");
                Q3DSDefaultMaterial *mat = m_currentPresentation->newObject<Q3DSDefaultMaterial>(matId);
                mat->setName(QString::fromUtf8(matId));
                model->appendChildNode(mat);
                slide->addObject(model);
                slide->addObject(mat);
                // adding to parent must be the last, since it triggers a scene
                // change notification to the scene manager
                parent->appendChildNode(model);
                m_console->addMessageFmt(responseColor, "Added");
            }
        }
    }));
#else
    Q_UNUSED(console);
#endif
}

void Q3DSConsoleCommands::setCurrentPresentation(Q3DSUipPresentation *pres)
{
#if QT_CONFIG(q3ds_profileui)
    m_currentPresentation = pres;
    m_console->addMessageFmt(responseColor, "Switched to presentation '%s' (%s)",
                             qPrintable(pres->name()), qPrintable(pres->sourceFile()));
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
    else
        obj = m_currentPresentation->objectByName(r);

    if (!obj && showErrorWhenNotFound)
        m_console->addMessageFmt(errorColor, "Object '%s' not found", r.constData());

    return obj;
}

QT_END_NAMESPACE