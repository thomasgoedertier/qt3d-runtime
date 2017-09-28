/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include <QString>
#include <QtTest>

#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QAttribute>
#include "q3dsmeshloader.h"

class tst_Q3DSMeshLoader : public QObject
{
    Q_OBJECT

public:
    tst_Q3DSMeshLoader();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testEmpty();
    void loadingPrimitiveMeshes();
    void testConsitencyAfterCleanup();
private:
    void validatePrimitive(MeshList list);
};

tst_Q3DSMeshLoader::tst_Q3DSMeshLoader()
{
}

void tst_Q3DSMeshLoader::initTestCase()
{
}

void tst_Q3DSMeshLoader::cleanupTestCase()
{
}

void tst_Q3DSMeshLoader::testEmpty()
{
    auto geometry = Q3DSMeshLoader::loadMesh("does_not_exist");
    QVERIFY(geometry == nullptr);
}

void tst_Q3DSMeshLoader::loadingPrimitiveMeshes()
{
    auto meshList = Q3DSMeshLoader::loadMesh("#Cube", 1, true);
    validatePrimitive(meshList);

    meshList = Q3DSMeshLoader::loadMesh("#Rectangle", 1, true);
    validatePrimitive(meshList);

    meshList = Q3DSMeshLoader::loadMesh("#Sphere", 1, true);
    validatePrimitive(meshList);

    meshList = Q3DSMeshLoader::loadMesh("#Cone", 1, true);
    validatePrimitive(meshList);

    meshList = Q3DSMeshLoader::loadMesh("#Cylinder", 1, true);
    validatePrimitive(meshList);
}

void tst_Q3DSMeshLoader::testConsitencyAfterCleanup()
{
    auto meshList = Q3DSMeshLoader::loadMesh("#Cube");
    QVERIFY(meshList->count() == 1);
    meshList.reset( new QVector<Q3DSMesh *>);
    QVERIFY(meshList->count() == 0);
    meshList = Q3DSMeshLoader::loadMesh("#Cube");
    QVERIFY(meshList->count() == 1);
    qDeleteAll(*meshList);
}

void tst_Q3DSMeshLoader::validatePrimitive(MeshList list)
{
    // Primitives only have 1 sub-mesh
    QVERIFY(list->count() == 1);
    auto mesh = list->first();
    QVERIFY(mesh != nullptr);
    QVERIFY(mesh->geometry() != nullptr);
    // Qt3D based primitives should have 5 attributes
    QVERIFY(mesh->geometry()->attributes().count() == 5);
}

QTEST_APPLESS_MAIN(tst_Q3DSMeshLoader)

#include "tst_q3dsmeshloader.moc"
