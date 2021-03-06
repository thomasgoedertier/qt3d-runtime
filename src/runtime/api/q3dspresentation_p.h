/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
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

#ifndef Q3DSPRESENTATION_P_H
#define Q3DSPRESENTATION_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtStudio3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/q3dsruntimeglobal_p.h>
#include "q3dspresentation.h"
#include <private/qobject_p.h>
#include <QVariant>

QT_BEGIN_NAMESPACE

class Q3DSEngine;
class Q3DSInlineQmlSubPresentation;

class Q3DSV_PRIVATE_EXPORT Q3DSPresentationController
{
public:
    virtual ~Q3DSPresentationController() { }

    void initializePresentationController(Q3DSEngine *engine, Q3DSPresentation *presentation);

    enum SourceFlag {
        Profiling = 0x01
    };
    Q_DECLARE_FLAGS(SourceFlags, SourceFlag)

    virtual void handlePresentationSource(const QUrl &source,
                                          SourceFlags flags,
                                          const QVector<Q3DSInlineQmlSubPresentation *> &inlineQmlSubPresentations) = 0;
    virtual void handlePresentationReload() = 0;

    virtual void handlePresentationKeyPressEvent(QKeyEvent *e);
    virtual void handlePresentationKeyReleaseEvent(QKeyEvent *e);
    virtual void handlePresentationMousePressEvent(QMouseEvent *e);
    virtual void handlePresentationMouseMoveEvent(QMouseEvent *e);
    virtual void handlePresentationMouseReleaseEvent(QMouseEvent *e);
    virtual void handlePresentationMouseDoubleClickEvent(QMouseEvent *e);
#if QT_CONFIG(wheelevent)
    virtual void handlePresentationWheelEvent(QWheelEvent *e);
#endif
    virtual void handlePresentationTouchEvent(QTouchEvent *e);
#if QT_CONFIG(tabletevent)
    virtual void handlePresentationTabletEvent(QTabletEvent *e);
#endif

    virtual void handleDataInputValue(const QString &name, const QVariant &value);
    virtual void handleFireEvent(const QString &elementPath, const QString &eventName);
    virtual void handleGoToTime(const QString &elementPath, float timeSeconds);
    virtual void handleGoToSlideByName(const QString &elementPath, const QString &name);
    virtual void handleGoToSlideByIndex(const QString &elementPath, int index);
    virtual void handleGoToSlideByDirection(const QString &elementPath, bool next, bool wrap);
    virtual QVariant handleGetAttribute(const QString &elementPath, const QString &attribute);
    virtual void handleSetAttribute(const QString &elementPath, const QString &attributeName, const QVariant &value);
    virtual void handleSetProfileUiVisible(bool visible, float scale);

    bool compareElementPath(const QString &a, const QString &b) const;

protected:
    Q3DSEngine *m_pcEngine = nullptr; // don't want clashes with commonly used m_engine members
    QVector<QPair<QString, QVariant> > m_pendingDataInputSets;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSPresentationController::SourceFlags)

class Q3DSV_PRIVATE_EXPORT Q3DSPresentationPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(Q3DSPresentation)

public:
    static Q3DSPresentationPrivate *get(Q3DSPresentation *p) { return p->d_func(); }

    void setController(Q3DSPresentationController *c);
    Q3DSPresentationController::SourceFlags sourceFlags() const;
    bool compareElementPath(const QString &a, const QString &b) const;
    void registerInlineQmlSubPresentations(const QVector<Q3DSInlineQmlSubPresentation *> &list);

    QUrl source;
    Q3DSPresentationController *controller = nullptr;
    bool profiling = false; // unlike the viewer, the public API defaults to profile off
    bool profileUiVisible = false;
    float profileUiScale = 1.0f;
    QVector<Q3DSInlineQmlSubPresentation *> inlineQmlSubPresentations;
};

QT_END_NAMESPACE

#endif // Q3DSPRESENTATION_P_H
