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

#ifndef Q3DSENGINE_P_H
#define Q3DSENGINE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QObject>
#include <QElapsedTimer>
#include <QSurfaceFormat>
#include <Qt3DCore/QAspectEngine>
#include "q3dsuipdocument_p.h"
#include "q3dsuiadocument_p.h"
#include "q3dsuiaparser_p.h"
#include "q3dsscenemanager_p.h"
#include "private/q3dsbehaviorobject_p.h"

QT_BEGIN_NAMESPACE

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QQmlEngine;
class QQmlComponent;

namespace Qt3DRender {
class QRenderCapture;
class QRenderCaptureReply;
namespace Quick {
class QScene2D;
}
}

struct Q3DSV_PRIVATE_EXPORT Q3DSBehaviorHandle
{
    Q3DSBehaviorInstance *behaviorInstance = nullptr;
    QQmlComponent *component = nullptr;
    Q3DSBehaviorObject *object = nullptr;

    bool initialized = false;
    bool active = false;

    void updateProperties() const;
};

class Q3DSV_PRIVATE_EXPORT Q3DSEngine : public QObject
{
    Q_OBJECT
public:
    Q3DSEngine();
    ~Q3DSEngine();

    enum Flag {
        Force4xMSAA = 0x01,
        EnableProfiling = 0x02,
        WithoutRenderAspect = 0x04
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    static QSurfaceFormat surfaceFormat();
    static void clearLog();

    Flags flags() const { return m_flags; }
    void setFlags(Flags flags);
    void setFlag(Flag flag, bool enabled);

    // Load presentation from a uip/uia file.
    bool setSource(const QString &uipOrUiaFileName, QString *error = nullptr);
    QString source() const;

    // Load presentation from a uip document object.
    bool setDocument(const Q3DSUipDocument &uipDocument, QString *error = nullptr);

    // Load presentation from a uia document object.
    bool setDocument(const Q3DSUiaDocument &uiaDocument, QString *error = nullptr);

    // Provide pre-constructed presentation(s).
    bool setPresentations(const QVector<Q3DSUipPresentation *> &presentations);

    qint64 behaviorLoadTimeMsecs() const;
    qint64 totalLoadTimeMsecs() const;

    int presentationCount() const;
    QString uipFileName(int index = 0) const;
    Q3DSUipDocument *uipDocument(int index = 0) const;
    Q3DSUipPresentation *presentation(int index = 0) const;
    Q3DSUipPresentation *presentationByName(const QString &name) const;
    Q3DSSceneManager *sceneManager(int index = 0) const;

    Qt3DCore::QAspectEngine *aspectEngine() const;
    Qt3DCore::QEntity *rootEntity() const;

    void setOnDemandRendering(bool enabled);

    QSize implicitSize() const;

    void setSurface(QObject *surface);
    QObject *surface() const;

    bool start();

    void resize(const QSize &size, qreal dpr = qreal(1.0), bool forceSynchronous = false);
    void resize(int w, int h, qreal dpr) { resize(QSize(w, h), dpr); }
    void setAutoStart(bool autoStart);

    void setDataInputValue(const QString &name, const QVariant &value);

    void handleKeyPressEvent(QKeyEvent *e);
    void handleKeyReleaseEvent(QKeyEvent *e);
    void handleMousePressEvent(QMouseEvent *e);
    void handleMouseMoveEvent(QMouseEvent *e);
    void handleMouseReleaseEvent(QMouseEvent *e);
    void handleMouseDoubleClickEvent(QMouseEvent *e);
#if QT_CONFIG(wheelevent)
    void handleWheelEvent(QWheelEvent *e);
#endif

    void setProfileUiEnabled(bool enabled) { m_profileUiEnabled = enabled; }

    typedef QHash<Q3DSBehaviorInstance *, Q3DSBehaviorHandle> BehaviorMap;

    typedef std::function<void(Q3DSBehaviorInstance *, const QString &)> BehaviorLoadedCallback;
    void loadBehaviorInstance(Q3DSBehaviorInstance *behaviorInstance,
                              Q3DSUipPresentation *pres,
                              BehaviorLoadedCallback callback);
    void unloadBehaviorInstance(Q3DSBehaviorInstance *behaviorInstance);
    const BehaviorMap &behaviorHandles() const { return m_behaviorHandles; }

public Q_SLOTS:
    void requestGrab();

Q_SIGNALS:
    void presentationLoaded();
    void nextFrameStarting();
    void grabReady(const QImage &image);

private:
    Q_DISABLE_COPY(Q3DSEngine)

    struct Presentation {
        Q3DSSubPresentation subPres;
    };

    struct UipPresentation : Presentation {
        Q3DSUipDocument *uipDocument = nullptr;
        Q3DSSceneManager::Scene q3dscene;
        Q3DSSceneManager *sceneManager = nullptr;
        Q3DSUipPresentation *presentation = nullptr;
    };

    struct QmlPresentation : Presentation {
        Q3DSQmlDocument *qmlDocument = nullptr;
        Qt3DRender::Quick::QScene2D *scene2d = nullptr;
    };

    void createAspectEngine();

    bool loadPresentations();
    void finalizePresentations();
    bool loadUipPresentation(UipPresentation *pres);
    bool buildUipPresentationScene(UipPresentation *pres);
    bool loadSubUipPresentation(UipPresentation *pres);
    bool buildSubUipPresentationScene(UipPresentation *pres);
    bool loadSubQmlPresentation(QmlPresentation *pres);

    bool parseUipDocument(UipPresentation *pres);
    bool parseUiaDocument(Q3DSUiaParser::Uia &uiaDoc, const QString &sourcePrefix);

    void destroy();
    void prepareForReload();

    void loadBehaviors();
    void destroyBehaviorHandle(const Q3DSBehaviorHandle &h);
    void behaviorFrameUpdate();

    QObject *m_surface = nullptr;
    QSize m_implicitSize;
    QSize m_size;
    qreal m_dpr = 1;
    Flags m_flags;
    QString m_source; // uip or uia file
    QVector<UipPresentation> m_uipPresentations;
    QVector<QmlPresentation> m_qmlPresentations;
    Q3DSDataInputEntry::Map m_dataInputEntries;

    QScopedPointer<QQmlEngine> m_qmlEngine;
    QScopedPointer<Qt3DCore::QAspectEngine> m_aspectEngine;

    qint64 m_loadTime = 0;
    qint64 m_behaviorLoadTime = 0;

    QElapsedTimer m_profilerActivateTimer;
    QElapsedTimer m_sourceLoadTimer;

    Qt3DRender::QRenderCapture *m_capture = nullptr;
    QHash<Qt3DRender::QRenderCaptureReply*, QMetaObject::Connection> m_captureConnections;

    QObject m_profileUiEventSource;
    bool m_autoStart = true;
    float m_profileUiScale = 1;
    bool m_profileUiEnabled = true;

    QQmlEngine *m_behaviorQmlEngine = nullptr;
    BehaviorMap m_behaviorHandles;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSEngine::Flags)

QT_END_NAMESPACE

#endif // Q3DSENGINE_P_H
