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

#include "q3dspresentation_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class Q3DSPresentation
    \inmodule 3dstudioruntime2
    \since Qt 3D Studio 2.0

    \brief Represents a Qt 3D Studio presentation.

    This class provides properties and methods for controlling a
    presentation.

    Qt 3D Studio supports multiple presentations in one project. There
    is always a main presentation and zero or more
    sub-presentations. The sub-presentations are composed into the
    main presentations either as contents of Qt 3D Studio layers or as
    texture maps.

    In the filesystem each presentation corresponds to one \c{.uip}
    file. When present, the \c{.uia} file ties these together by
    specifying a name for each of the (sub-)presentations and
    specifies which one is the main one.

    From the API point of view Q3DSPresentation corresponds to the
    main presentation. The source property can refer either to a
    \c{.uia} or \c{.uip} file. When specifying a file with \c{.uip}
    extension and a \c{.uia} is present with the same name, the
    \c{.uia} is loaded automatically and thus sub-presentation
    information is available regardless.

    \note This class should not be instantiated directly when working with the
    C++ APIs. Q3DSSurfaceViewer and Q3DSWidget create a Q3DSPresentation
    instance implicitly. This can be queried via
    Q3DSSurfaceViewer::presentation() or Q3DSWidget::presentation().
 */

// Unlike in 3DS1, Q3DSPresentation here does not own the engine. This is due
// to the delicate lifetime management needs due to Qt 3D under the hood: for
// instance the Studio3D element has to carefully manage the underlying
// Q3DSEngine in ways that are different from what the widget or surfaceviewer
// APIs need. Therefore the presentation here is just a mere collection of
// data, any actual engine-related behavior is provided by the
// Q3DSPresentationController (for common functionality), or individually by
// Studio3D, Q3DSWidget, or Q3DSSurfaceViewer.

/*!
    Constructs a new Q3DSPresentation with the given \a parent.
 */
Q3DSPresentation::Q3DSPresentation(QObject *parent)
    : QObject(*new Q3DSPresentationPrivate, parent)
{
}

/*!
    \internal
 */
Q3DSPresentation::Q3DSPresentation(Q3DSPresentationPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

/*!
    Destructor.
 */
Q3DSPresentation::~Q3DSPresentation()
{
}

/*!
    \property Q3DSPresentation::source

    Holds the name of the main presentation file (\c{*.uia} or
    \c{*.uip}). This may be either a local file or qrc URL.

    The names of all further assets (image files for texture maps, qml
    behavior scripts, mesh files) will be resolved relative to the
    location of the presentation, unless they use absolute paths. This
    allows bundling all assets next to the presentation in the Qt
    resource system.
*/
QUrl Q3DSPresentation::source() const
{
    Q_D(const Q3DSPresentation);
    return d->source;
}

void Q3DSPresentation::setSource(const QUrl &source)
{
    Q_D(Q3DSPresentation);
    if (d->source == source)
        return;

    d->source = source;
    if (d->controller)
        d->controller->handlePresentationSource(source, d->sourceFlags(), d->inlineQmlSubPresentations);

    emit sourceChanged();
}

bool Q3DSPresentation::isProfilingEnabled() const
{
    Q_D(const Q3DSPresentation);
    return d->profiling;
}

void Q3DSPresentation::setProfilingEnabled(bool enable)
{
    // In this API "profiling" means both the scene manager's EnableProfiling
    // (enables Qt3D QObject tracking; must be set before building the scene)
    // and the ImGui-based profile UI (that can be toggled at any time in the
    // private API - for simplicity there's a single flag for both here, which
    // must be set up front). Defaults to disabled.

    Q_D(Q3DSPresentation);
    if (d->profiling != enable) {
        d->profiling = enable; // no effect until next setSource()
        emit profilingEnabledChanged();
    }
}

bool Q3DSPresentation::isProfileUiVisible() const
{
    Q_D(const Q3DSPresentation);
    return d->profiling ? d->profileUiVisible : false;
}

void Q3DSPresentation::setProfileUiVisible(bool visible)
{
    Q_D(Q3DSPresentation);
    if (d->profiling && d->profileUiVisible != visible) {
        d->profileUiVisible = visible;
        if (d->controller)
            d->controller->handleSetProfileUiVisible(d->profileUiVisible, d->profileUiScale);
        emit profileUiVisibleChanged();
    }
}

float Q3DSPresentation::profileUiScale() const
{
    Q_D(const Q3DSPresentation);
    return d->profileUiScale;
}

void Q3DSPresentation::setProfileUiScale(float scale)
{
    Q_D(Q3DSPresentation);
    if (d->profileUiScale != scale) {
        d->profileUiScale = scale;
        if (d->controller)
            d->controller->handleSetProfileUiVisible(d->profileUiVisible, d->profileUiScale);
        emit profileUiScaleChanged();
    }
}

/*!
    Reloads the presentation.
 */
void Q3DSPresentation::reload()
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationReload();
}

/*!
    Sets the \a value of a data input element \a name in the presentation.

    Data input provides a higher level, designer-driven alternative to
    Q3DSElement and setAttribute(). Instead of exposing a large set of
    properties with their intenal engine names, data input allows designers to
    decide which properties should be writable by the application, and can
    assign custom names to these data input entries, thus forming a
    well-defined contract between the designer and the developer.

    In addition, data input also allows controlling the time line and the
    current slide for time context objects (Scene or Component). Therefore it
    is also an alternative to the goToSlide() and goToTime() family of APIs and
    to Q3DSSceneElement.
 */
void Q3DSPresentation::setDataInputValue(const QString &name, const QVariant &value)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleDataInputValue(name, value);
}

/*!
    Dispatches an event with \a eventName on a specific element found in \a
    elementPath. Appropriate actions created in Qt 3D Studio or callbacks
    registered using the registerForEvent() method in attached (behavior)
    scripts will be executed in response to the event.

    See setAttribute() for a description of \a elementPath.
 */
void Q3DSPresentation::fireEvent(const QString &elementPath, const QString &eventName)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleFireEvent(elementPath, eventName);
}

/*!
    Moves the timeline for a time context (a Scene or a Component element) to a
    specific position. The position is given in seconds in \a timeSeconds.

    If \a elementPath points to a time context, that element is
    controlled. For all other element types the time context owning
    that element is controlled instead.  You can target the command to
    a specific sub-presentation by adding "SubPresentationId:" in
    front of the element path, for example
    \c{"SubPresentationOne:Scene"}.

    The behavior when specifying a time before 0 or after the end time
    for the current slide depends on the play mode of the slide:

    \list
    \li \c{Stop at End} - values outside the valid time range instead clamp to the boundaries.
    For example, going to time -5 is the same as going to time 0.
    \li \c{Looping} - values outside the valid time range mod into the valid range. For example,
    going to time -4 on a 10 second slide is the same as going to time 6.
    \li \c{Ping Pong} - values outside the valid time range bounce off the ends. For example,
    going to time -4 is the same as going to time 4 (assuming the time context is at least 4 seconds
    long), while going to time 12 on a 10 second slide is the same as going to time 8.
    \li \c{Ping} - values less than 0 are treated as time 0, while values greater than the endtime
    bounce off the end (eventually hitting 0.)
    \endlist
 */
void Q3DSPresentation::goToTime(const QString &elementPath, float timeSeconds)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleGoToTime(elementPath, timeSeconds);
}

/*!
    Requests a time context (a Scene or a Component object) to change
    to a specific slide by \a name. If the context is already on that
    slide, playback will start over.

    If \a elementPath points to a time context, that element is
    controlled. For all other element types the time context owning
    that element is controlled instead.  You can target the command to
    a specific sub-presentation by adding "SubPresentationId:" in
    front of the element path, for example \c{"SubPresentationOne:Scene"}.
 */
void Q3DSPresentation::goToSlide(const QString &elementPath, const QString &name)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleGoToSlideByName(elementPath, name);
}

/*!
    Requests a time context (a Scene or a Component object) to change
    to a specific slide by \a index. If the context is already on that
    slide, playback will start over.

    If \a elementPath points to a time context, that element is
    controlled. For all other element types the time context owning
    that element is controlled instead.  You can target the command to
    a specific sub-presentation by adding "SubPresentationId:" in
    front of the element path, for example \c{"SubPresentationOne:Scene"}.
 */
void Q3DSPresentation::goToSlide(const QString &elementPath, int index)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleGoToSlideByIndex(elementPath, index);
}

/*!
    Requests a time context (a Scene or a Component object) to change to the
    next or previous slide, depending on the value of \a next. If the context
    is already at the last or first slide, \a wrap defines if wrapping over to
    the first or last slide, respectively, occurs.

    If \a elementPath points to a time context, that element is controlled. For
    all other element types the time context owning that element is controlled
    instead. You can target the command to a specific sub-presentation by
    adding "SubPresentationId:" in front of the element path, for example
    \c{"SubPresentationOne:Scene"}.
 */
void Q3DSPresentation::goToSlide(const QString &elementPath, bool next, bool wrap)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleGoToSlideByDirection(elementPath, next, wrap);
}

/*!
    Returns the value of an attribute (property) on the object specified by \a
    elementPath. The \a attributeName is the \l{Attribute Names}{scripting
    name} of the attribute.

    See setAttribute() for a description of \a elementPath.

    \sa setAttribute
 */
QVariant Q3DSPresentation::getAttribute(const QString &elementPath, const QString &attributeName)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        return d->controller->handleGetAttribute(elementPath, attributeName);

    return QVariant();
}

/*!
    Sets the \a value of an attribute (property) on the object specified by
    \a elementPath. The \a attributeName is the \l{Attribute Names}{scripting
    name} of the attribute.

    An element path refers to an object in the scene either by name or id. The
    latter is rarely used in application code since the unique IDs are not
    exposed in the Qt 3D Studio application. To refer to an object by id,
    prepend \c{#} to the name. Applications will typically refer to objects by
    name.

    Names are not necessarily unique, however. To access an object with a
    non-unique name, the path can be specified, for example,
    \c{Scene.Layer.Camera}. Here the right camera object gets chosen even if
    the scene contains other layers with the default camera names (for instance
    \c{Scene.Layer2.Camera}).

    If the object is renamed to a unique name in the Qt 3D Studio application's
    Timeline view, the path can be omitted. For example, if the camera in
    question was renamed to \c MyCamera, applications can then simply pass \c
    MyCamera as the element path.

    To access an object in a sub-presentation, prepend the name of the
    sub-presentation followed by a colon, for example,
    \c{SubPresentationOne:Scene.Layer.Camera}.

    \sa getAttribute
 */
void Q3DSPresentation::setAttribute(const QString &elementPath, const QString &attributeName, const QVariant &value)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleSetAttribute(elementPath, attributeName, value);
}

// These event forwarders are not stricly needed, Studio3D et al are fine
// without them. However, they are there in 3DS1 and can become handy to feed
// arbitrary, application-generated events into the engine.

void Q3DSPresentation::keyPressEvent(QKeyEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationKeyPressEvent(e);
}

void Q3DSPresentation::keyReleaseEvent(QKeyEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationKeyReleaseEvent(e);
}

void Q3DSPresentation::mousePressEvent(QMouseEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationMousePressEvent(e);
}

void Q3DSPresentation::mouseMoveEvent(QMouseEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationMouseMoveEvent(e);
}

void Q3DSPresentation::mouseReleaseEvent(QMouseEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationMouseReleaseEvent(e);
}

void Q3DSPresentation::mouseDoubleClickEvent(QMouseEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationMouseDoubleClickEvent(e);
}

#if QT_CONFIG(wheelevent)
void Q3DSPresentation::wheelEvent(QWheelEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationWheelEvent(e);
}
#endif

void Q3DSPresentation::touchEvent(QTouchEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationTouchEvent(e);
}

#if QT_CONFIG(tabletevent)
void Q3DSPresentation::tabletEvent(QTabletEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationTabletEvent(e);
}
#endif

void Q3DSPresentationPrivate::setController(Q3DSPresentationController *c)
{
    if (controller == c)
        return;

    controller = c;
    controller->handlePresentationSource(source, sourceFlags(), inlineQmlSubPresentations);
}

Q3DSPresentationController::SourceFlags Q3DSPresentationPrivate::sourceFlags() const
{
    Q3DSPresentationController::SourceFlags flags = 0;
    if (profiling)
        flags |= Q3DSPresentationController::Profiling;

    return flags;
}

bool Q3DSPresentationPrivate::compareElementPath(const QString &a, const QString &b) const
{
    return controller ? controller->compareElementPath(a, b) : false;
}

void Q3DSPresentationPrivate::registerInlineQmlSubPresentations(const QVector<Q3DSInlineQmlSubPresentation *> &list)
{
    inlineQmlSubPresentations += list;
}

/*!
    \qmltype Presentation
    \instantiates Q3DSPresentation
    \inqmlmodule QtStudio3D
    \ingroup 3dstudioruntime2

    \brief Represents a Qt 3D Studio presentation.

    This class provides properties and methods for controlling a
    presentation.

    Qt 3D Studio supports multiple presentations in one project. There is
    always a main presentation and zero or more sub-presentations. The
    sub-presentations are composed into the main presentations either as
    contents of Qt 3D Studio layers or as texture maps.

    In the filesystem each presentation corresponds to one \c{.uip}
    file. When present, the \c{.uia} file ties these together by
    specifying a name for each of the (sub-)presentations and
    specifies which one is the main one.

    From the API point of view Presentation corresponds to the main
    presentation. The source property can refer either to a \c{.uia} or
    \c{.uip} file. When specifying a file with \c{.uip} extension and a
    \c{.uia} is present with the same name, the \c{.uia} is loaded
    automatically and thus sub-presentation information is available
    regardless.

    The Presentation type handles child objects of the types \l Element, \l
    SceneElement, \l DataInput, and \l SubPresentationSettings specially. These
    will get automatically associated with the presentation and can control
    certain aspects of it from that point on.

    \section2 Example usage

    \qml
    Studio3D {
        Presentation {
            id: presentation

            source: "qrc:/presentation/barrel.uip"
            profilingEnabled: true

            onSlideEntered: console.log("Entered slide " + name + "(index " + index + ") on " + elementPath)
            onSlideExited: console.log("Exited slide " + name + "(index " + index + ") on " + elementPath)
            onCustomSignalEmitted: console.log("Got custom signal " + name)

            DataInput {
                name: "di_text"
                value: "hello world"
            }

            SceneElement {
                elementPath: "SomeComponentNode"
                onCurrentSlideIndexChanged: console.log("Current slide index for component: " + currentSlideIndex)
                onCurrentSlideNameChanged: console.log("Current slide name for component: " + currentSlideName)
            }

            SubPresentationSettings {
                qmlStreams: [
                    QmlStream {
                        presentationId: "sub-presentation-id"
                        Rectangle {
                            width: 1024
                            height: 1024
                            color: "red"
                        }
                    }
                ]
            }
        }
    }
    Button {
        onClicked: presentation.setAttribute("SomeMaterial", "diffuse", "0 1 0");
    }
    \endqml

    \sa Studio3D
*/

/*!
    \qmlproperty url Presentation::source

    Holds the main presentation source (\c{*.uia} or \c{*.uip}) file location.
    May be either a file URL or a qrc URL.
*/

/*!
    \qmlmethod void Presentation::goToSlide(string elementPath, string name)

    Requests a time context (a Scene or a Component node) to change to a specific slide
    by \a name. If the context is already on that slide playback will start over.

    If \a elementPath points to a time context, that element is controlled. For
    all other element types the time context owning that element is controlled instead.
    You can target the command to a specific sub-presentation by adding "SubPresentationId:" in
    front of the element path, for example \c{"SubPresentationOne:Scene"}.
*/

/*!
    \qmlmethod void Presentation::goToSlide(string elementPath, int index)

    Requests a time context (a Scene or a Component node) to change to a specific slide by
    index \a index. If the context is already on that slide playback will start over.

    If \a elementPath points to a time context, that element is controlled. For
    all other element types the time context owning that element is controlled instead.
    You can target the command to a specific sub-presentation by adding "SubPresentationId:" in
    front of the element path, for example \c{"SubPresentationOne:Scene"}.
*/

/*!
    \qmlmethod void Presentation::goToSlide(string elementPath, bool next, bool wrap)

    Requests a time context (a Scene or a Component node) to change to the next or the
    previous slide, depending on the value of \a next. If the context is already at the
    last or first slide, \a wrap defines if change occurs to the opposite end.

    If \a elementPath points to a time context, that element is controlled. For
    all other element types the time context owning that element is controlled instead.
    You can target the command to a specific sub-presentation by adding "SubPresentationId:" in
    front of the element path, for example \c{"SubPresentationOne:Scene"}.
*/

/*!
    \qmlmethod void Presentation::goToTime(string elementPath, real time)

    Sets a time context (a Scene or a Component node) to a specific playback \a time in seconds.

    If \a elementPath points to a time context, that element is controlled. For
    all other element types the time context owning that element is controlled instead.
    You can target the command to a specific sub-presentation by adding "SubPresentationId:" in
    front of the element path, for example \c{"SubPresentationOne:Scene"}.

    The behavior when specifying a time before 0 or after the end time for the current slide depends
    on the play mode of the slide:
    \list
    \li \c{Stop at End} - values outside the valid time range instead clamp to the boundaries.
    For example, going to time -5 is the same as going to time 0.
    \li \c{Looping} - values outside the valid time range mod into the valid range. For example,
    going to time -4 on a 10 second slide is the same as going to time 6.
    \li \c{Ping Pong} - values outside the valid time range ‘bounce’ off the ends. For example,
    going to time -4 is the same as going to time 4 (assuming the time context is at least 4 seconds
    long), while going to time 12 on a 10 second slide is the same as going to time 8.
    \li \c{Ping} - values less than 0 are treated as time 0, while values greater than the endtime
    bounce off the end (eventually hitting 0.)
    \endlist
*/

/*!
    \qmlmethod variant Presentation::getAttribute(string elementPath, string attributeName)

    Returns the value of an attribute (property) on the object specified by \a
    elementPath. The \a attributeName is the \l{Attribute Names}{scripting
    name} of the attribute.
*/

/*!
    \qmlmethod void Presentation::setAttribute(string elementPath, string attributeName,
                                               variant value)

    Sets the \a value of an attribute (property) on the Qt 3D Studio scene
    object specified by \a elementPath. The \a attributeName is the
    \l{Attribute Names}{scripting name} of the attribute.

    An element path refers to an object in the scene either by name or id. The
    latter is rarely used in application code since the unique IDs are not
    exposed in the Qt 3D Studio application. To refer to an object by id,
    prepend \c{#} to the name. Applications will typically refer to objects by
    name.

    Names are not necessarily unique, however. To access an object with a
    non-unique name, the path can be specified, for example,
    \c{Scene.Layer.Camera}. Here the right camera object gets chosen even if
    the scene contains other layers with the default camera names (for instance
    \c{Scene.Layer2.Camera}).

    If the object is renamed to a unique name in the Qt 3D Studio application's
    Timeline view, the path can be omitted. For example, if the camera in
    question was renamed to \c MyCamera, applications can then simply pass \c
    MyCamera as the element path.

    To access an object in a sub-presentation, prepend the name of the
    sub-presentation followed by a colon, for example,
    \c{SubPresentationOne:Scene.Layer.Camera}.
*/

/*!
    \qmlmethod void Presentation::fireEvent(string elementPath, string eventName)

    Dispatches an event with \a eventName on a specific element found in \a
    elementPath. Appropriate actions created in Qt 3D Studio or callbacks
    registered using the registerForEvent() method in attached \c{behavior
    scripts} will be executed in response to the event.
*/

/*!
    \qmlmethod void Presentation::setDataInputValue(string name, variant value)

    Sets the \a value of a data input element \a name in the presentation.

    Data input provides a higher level, designer-driven alternative to
    setAttribute or Element. Instead of exposing a large set of properties with
    their intenal engine names, data input allows designers to decide which
    properties should be writable by the application, and can assign custom
    names to these data input entries, thus forming a well-defined contract
    between the designer and the developer.

    In addition, data input also allows controlling the time line and the
    current slide for time context objects (Scene or Component). Therefore it
    is also an alternative to the goToSlide, goToTime, and SceneElement.

    As an alternative to this method, the \l DataInput type can be used. That
    approach has the advantage of being able to use QML property bindings for
    the value, instead of having to resort to JavaScript function calls for
    every value change.
*/

/*!
    \qmlsignal Presentation::slideEntered(string elementPath, int index, string name)

    This signal is emitted when a slide is entered in the presentation. The \a
    elementPath specifies the time context (a Scene or a Component element)
    owning the entered slide. The \a index and \a name contain the index and
    the name of the entered slide.
*/

/*!
    \qmlsignal Presentation::slideExited(string elementPath, int index, string name)

    This signal is emitted when a slide is exited in the presentation. The \a
    elementPath specifies the time context (a Scene or a Component element)
    owning the exited slide. The \a index and \a name contain the index and the
    name of the exited slide.
*/

QT_END_NAMESPACE
