/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest>

#include <QtCore/qvector.h>

#include <private/q3dsuippresentation_p.h>
#include <private/q3dsslideplayer_p.h>
#include <private/q3dsuipparser_p.h>

class tst_slidedeck : public QObject
{
    Q_OBJECT

public:
    tst_slidedeck();
    ~tst_slidedeck();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void masterSlide();
    void nextSlide();
    void previousSlide();
    void invalid();

private:
    QScopedPointer<Q3DSUipPresentation> m_pres;
    Q3DSSlide *m_masterSlide = nullptr;
};

tst_slidedeck::tst_slidedeck()
{

}

tst_slidedeck::~tst_slidedeck()
{

}

void tst_slidedeck::initTestCase()
{
    Q3DSUipParser parser;
    m_pres.reset(parser.parse(QLatin1String(":/simple.uip")));
    QVERIFY(!m_pres.isNull());

    m_masterSlide = m_pres->masterSlide();
    QVERIFY(m_masterSlide);
}

void tst_slidedeck::cleanupTestCase()
{

}

void tst_slidedeck::masterSlide()
{
    const int slideCount = m_masterSlide->childCount();
    QVERIFY(slideCount > 0);

    Q3DSSlideDeck slideDeck(m_masterSlide);
    QVERIFY(slideDeck.slideCount() == slideCount);
    QVERIFY(!slideDeck.isEmpty());
}

void tst_slidedeck::nextSlide()
{
    Q3DSSlideDeck slideDeck(m_masterSlide);
    QVERIFY(!slideDeck.isEmpty());

    Q3DSGraphObject *ns = m_masterSlide->firstChild();
    while (ns) {
        QVERIFY(slideDeck.currentSlide() == ns);
        QVERIFY(slideDeck.nextSlide() == ns->nextSibling());
        ns = ns->nextSibling();
    }
}

void tst_slidedeck::previousSlide()
{
    Q3DSSlideDeck slideDeck(m_masterSlide);
    QVERIFY(!slideDeck.isEmpty());
    slideDeck.setCurrentSlide(slideDeck.slideCount() - 1);

    Q3DSGraphObject *ns = m_masterSlide->lastChild();
    while (ns) {
        QVERIFY(slideDeck.currentSlide() == ns);
        QVERIFY(slideDeck.previousSlide() == ns->previousSibling());
        ns = ns->previousSibling();
    }
}

void tst_slidedeck::invalid()
{
    Q3DSSlideDeck slideDeck(m_masterSlide);
    QVERIFY(!slideDeck.isEmpty());

    slideDeck.setCurrentSlide(0);
    QVERIFY(slideDeck.currentSlide() == m_masterSlide->firstChild());
    slideDeck.setCurrentSlide(-1); // Invalid, should not change the current slide
    QVERIFY(slideDeck.currentSlide() == m_masterSlide->firstChild());
}

QTEST_APPLESS_MAIN(tst_slidedeck)

#include "tst_slidedeck.moc"
