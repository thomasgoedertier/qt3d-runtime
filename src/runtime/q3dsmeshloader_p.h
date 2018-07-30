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

#ifndef Q3DSMESHLOADER_P_H
#define Q3DSMESHLOADER_P_H

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

#include "q3dsruntimeglobal_p.h"
#include "q3dsmesh_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
class QBuffer;
}

typedef QVector<Q3DSMesh *> MeshList;

class Q3DSV_PRIVATE_EXPORT Q3DSGeometry
{
public:
    struct Buffer {
        QByteArray data;
    };

    struct Attribute {
        int bufferIndex = 0;
        enum Semantic {
            IndexSemantic,
            PositionSemantic, // attr_pos
            NormalSemantic, // attr_norm
            TexCoordSemantic, // attr_uv0
            TangentSemantic, // attr_textan
            BinormalSemantic // attr_binormal
        };
        Semantic semantic = PositionSemantic;
        enum ComponentType { // matches QAttribute::VertexBaseType
            ByteType = 0,
            UnsignedByteType,
            ShortType,
            UnsignedShortType,
            IntType,
            UnsignedIntType,
            HalfFloatType,
            FloatType,
            DoubleType
        };
        ComponentType componentType = FloatType;
        int componentCount = 4;
        int offset = 0;
        int stride = 0;
    };

    enum PrimitiveType { // matches QGeometryRenderer::PrimitiveType
        Points = 0x0000,
        Lines = 0x0001,
        LineLoop = 0x0002,
        LineStrip = 0x0003,
        Triangles = 0x0004,
        TriangleStrip = 0x0005,
        TriangleFan = 0x0006,
        LinesAdjacency = 0x000A,
        TrianglesAdjacency = 0x000C,
        LineStripAdjacency = 0x000B,
        TriangleStripAdjacency = 0x000D,
        Patches = 0x000E
    };

    enum UsageType {
        StaticMesh,
        DynamicMesh
    };

    int bufferCount() const;
    const Buffer *buffer(int idx) const;
    Buffer *buffer(int idx);
    void addBuffer(Buffer buf);

    int attributeCount() const;
    const Attribute *attribute(int idx) const;
    Attribute *attribute(int idx);
    void addAttribute(Attribute attr);

    int drawCount() const;
    void setDrawCount(int count); // vertex (non-indexed) or element (indexed) count

    PrimitiveType primitiveType() const;
    void setPrimitiveType(PrimitiveType type);

    UsageType usageType() const;
    void setUsageType(UsageType type);

    static const int MAX_BUFFERS = 4;
    static const int MAX_ATTRIBUTES = 8;

private:
    Buffer m_buffers[MAX_BUFFERS];
    int m_bufferCount = 0;
    Attribute m_attributes[MAX_ATTRIBUTES];
    int m_attributeCount = 0;
    int m_drawCount = 0;
    PrimitiveType m_primitiveType = Triangles;
    UsageType m_usageType = StaticMesh;
};

namespace Q3DSMeshLoader {
    Q3DSV_PRIVATE_EXPORT MeshList loadMesh(const QString &meshPath, int partId = 0, bool useQt3DAttributes = false);

    struct MeshMapping
    {
        Qt3DRender::QBuffer *bufferMap[Q3DSGeometry::MAX_BUFFERS];
        Qt3DRender::QAttribute *attributeMap[Q3DSGeometry::MAX_ATTRIBUTES];
        Qt3DRender::QGeometryRenderer *mesh;
    };
    Q3DSV_PRIVATE_EXPORT MeshList loadMesh(const Q3DSGeometry &geom, MeshMapping *mapping);
    Q3DSV_PRIVATE_EXPORT void updateMeshBuffer(const Q3DSGeometry &geom, const MeshMapping &mapping, int bufferIdx);
    Q3DSV_PRIVATE_EXPORT void updateMeshBuffer(const Q3DSGeometry &geom, const MeshMapping &mapping, int bufferIdx, int offset, int size);
}

QT_END_NAMESPACE

#endif // Q3DSMESHLOADER_P_H
