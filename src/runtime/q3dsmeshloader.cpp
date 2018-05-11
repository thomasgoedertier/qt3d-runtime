/****************************************************************************
**
** Copyright (C) 2013 NVIDIA Corporation.
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

#include "q3dsmeshloader_p.h"
#include <Qt3DRender/QGeometry>
#include <QtCore/QMap>
#include <QtCore/QFile>
#include <QtCore/QFlags>
#include <QtCore/QVector>
#include <QtCore/QSharedPointer>
#include <QtCore/QDataStream>
#include <QtCore/QFileInfo>
#include <QtCore/QByteArray>
#include <QtCore/QVector>
#include <QDebug>

#include <Qt3DRender/QBuffer>
#include <Qt3DRender/QAttribute>

#include "q3dsutils_p.h"

QT_BEGIN_NAMESPACE

namespace Q3DSMeshUtils {

template <typename DataType>
struct OffsetDataRef
{
    quint32 m_Offset;
    quint32 m_Size;
    OffsetDataRef()
        : m_Offset(0)
        , m_Size(0)
    {
    }
    DataType *begin(quint8 *inBase) { return reinterpret_cast<DataType *>(inBase + m_Offset); }
    DataType *end(quint8 *inBase) { return begin(inBase) + m_Size; }
    const DataType *begin(const quint8 *inBase) const
    {
        return reinterpret_cast<const DataType *>(inBase + m_Offset);
    }
    const DataType *end(const quint8 *inBase) const { return begin(inBase) + m_Size; }
    quint32 size() const { return m_Size; }
    bool empty() const { return m_Size == 0; }
    DataType &index(quint8 *inBase, quint32 idx)
    {
        Q_ASSERT(idx < m_Size);
        return begin(inBase)[idx];
    }
    const DataType &index(const quint8 *inBase, quint32 idx) const
    {
        Q_ASSERT(idx < m_Size);
        return begin(inBase)[idx];
    }
};

struct RenderComponentTypes {
    enum Enum {
        Unknown = 0,
        UnsignedInt8,
        Int8,
        UnsignedInt16,
        Int16,
        UnsignedInt32,
        Int32,
        UnsignedInt64,
        Int64,
        Float16,
        Float32,
        Float64
    };
    static size_t getSizeOf(RenderComponentTypes::Enum type) {
        size_t size = 4;
        switch (type) {
        case RenderComponentTypes::Unknown:
            break; // use default
        case RenderComponentTypes::UnsignedInt64:
        case RenderComponentTypes::Int64:
        case RenderComponentTypes::Float64:
            size = 8;
            break;
        case RenderComponentTypes::UnsignedInt8:
        case RenderComponentTypes::Int8:
            size = 1;
            break;
        case RenderComponentTypes::UnsignedInt16:
        case RenderComponentTypes::Int16:
        case RenderComponentTypes::Float16:
            size = 2;
            break;
        case RenderComponentTypes::UnsignedInt32:
        case RenderComponentTypes::Int32:
        case RenderComponentTypes::Float32:
            size = 4;
            break;
        }
        return size;
    }
};

Qt3DRender::QAttribute::VertexBaseType convertRenderComponentToVertexBaseType(RenderComponentTypes::Enum type)
{
    Qt3DRender::QAttribute::VertexBaseType qtType = Qt3DRender::QAttribute::UnsignedInt;
    switch (type) {
    case RenderComponentTypes::Unknown:
        break; // use default
    case RenderComponentTypes::UnsignedInt64:
    case RenderComponentTypes::Int64:
    case RenderComponentTypes::Float64:
        qtType = Qt3DRender::QAttribute::Double; // use double for 64bit types
        break;
    case RenderComponentTypes::UnsignedInt8:
        qtType = Qt3DRender::QAttribute::UnsignedByte;
        break;
    case RenderComponentTypes::Int8:
        qtType = Qt3DRender::QAttribute::Byte;
        break;
    case RenderComponentTypes::UnsignedInt16:
        qtType = Qt3DRender::QAttribute::UnsignedShort;
        break;
    case RenderComponentTypes::Int16:
        qtType = Qt3DRender::QAttribute::Short;
        break;
    case RenderComponentTypes::UnsignedInt32:
        qtType = Qt3DRender::QAttribute::UnsignedInt;
        break;
    case RenderComponentTypes::Int32:
        qtType = Qt3DRender::QAttribute::Int;
        break;
    case RenderComponentTypes::Float16:
        qtType = Qt3DRender::QAttribute::HalfFloat;
        break;
    case RenderComponentTypes::Float32:
        qtType = Qt3DRender::QAttribute::Float;
        break;

    default:
        break;
    }
    return qtType;
}

struct RenderDrawMode
{
    enum Enum {
        Unknown = 0,
        Points,
        LineStrip,
        LineLoop,
        Lines,
        TriangleStrip,
        TriangleFan,
        Triangles,
        Patches,
    };
};

Qt3DRender::QGeometryRenderer::PrimitiveType convertRenderDrawModeToPrimitiveType(RenderDrawMode::Enum drawMode)
{
    Qt3DRender::QGeometryRenderer::PrimitiveType qtDrawMode = Qt3DRender::QGeometryRenderer::Triangles;
    switch (drawMode) {
    case RenderDrawMode::Unknown:
        break;
    case RenderDrawMode::Points:
        qtDrawMode = Qt3DRender::QGeometryRenderer::Points;
        break;
    case RenderDrawMode::LineStrip:
        qtDrawMode = Qt3DRender::QGeometryRenderer::LineStrip;
        break;
    case RenderDrawMode::LineLoop:
        qtDrawMode = Qt3DRender::QGeometryRenderer::LineLoop;
        break;
    case RenderDrawMode::Lines:
        qtDrawMode = Qt3DRender::QGeometryRenderer::Lines;
        break;
    case RenderDrawMode::TriangleStrip:
        qtDrawMode = Qt3DRender::QGeometryRenderer::TriangleStrip;
        break;
    case RenderDrawMode::TriangleFan:
        qtDrawMode = Qt3DRender::QGeometryRenderer::TriangleFan;
        break;
    case RenderDrawMode::Triangles:
        qtDrawMode = Qt3DRender::QGeometryRenderer::Triangles;
        break;
    case RenderDrawMode::Patches:
        qtDrawMode = Qt3DRender::QGeometryRenderer::Patches;
        break;
    default:
        break;
    }
    return qtDrawMode;
}

struct RenderWinding
{
    enum Enum {
        Unknown = 0,
        Clockwise,
        CounterClockwise
    };
};

struct MeshVertexBufferEntry
{
    quint32 m_NameOffset;
    /** Datatype of the this entry points to in the buffer */
    RenderComponentTypes::Enum m_ComponentType;
    /** Number of components of each data member. 1,2,3, or 4.  Don't be stupid.*/
    quint32 m_NumComponents;
    /** Offset from the beginning of the buffer of the first item */
    quint32 m_FirstItemOffset;
    MeshVertexBufferEntry()
        : m_NameOffset(0)
        , m_ComponentType(RenderComponentTypes::Float32)
        , m_NumComponents(3)
        , m_FirstItemOffset(0)
    {
    }
};

struct VertexBuffer
{
    OffsetDataRef<MeshVertexBufferEntry> m_Entries;
    quint32 m_Stride;
    OffsetDataRef<quint8> m_Data;
    VertexBuffer(OffsetDataRef<MeshVertexBufferEntry> entries, quint32 stride,
                 OffsetDataRef<quint8> data)
        : m_Entries(entries)
        , m_Stride(stride)
        , m_Data(data)
    {
    }
    VertexBuffer()
        : m_Stride(0)
    {
    }
};

struct IndexBuffer
{
    // Component types must be either UnsignedInt16 or UnsignedInt8 in order for the
    // graphics hardware to deal with the buffer correctly.
    RenderComponentTypes::Enum m_ComponentType;
    OffsetDataRef<quint8> m_Data;
    // Either quint8 or quint16 component types are allowed by the underlying rendering
    // system, so you would be wise to stick with those.
    IndexBuffer(RenderComponentTypes::Enum compType, OffsetDataRef<quint8> data)
        : m_ComponentType(compType)
        , m_Data(data)
    {
    }
    IndexBuffer()
        : m_ComponentType(RenderComponentTypes::Unknown)
    {
    }
};

template <quint32 TNumBytes>
struct MeshPadding
{
    quint8 m_Padding[TNumBytes];
    MeshPadding() { memZero(m_Padding, TNumBytes); }
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Bounds3 {
    Vec3 minimum;
    Vec3 maximum;
};

struct MeshSubset
{
    // QT3DS_MAX_U32 means use all available items
    quint32 m_Count;
    // Offset is in item size, not bytes.
    quint32 m_Offset;
    // Bounds of this subset.  This is filled in by the builder
    // see AddMeshSubset
    Bounds3 m_Bounds;

    // Subsets have to be named else artists will be unable to use
    // a mesh with multiple subsets as they won't have any idea
    // while part of the model a given mesh actually maps to.
    OffsetDataRef<char16_t> m_Name;

    MeshSubset(quint32 count, quint32 off, const Bounds3 &bounds, OffsetDataRef<char16_t> inName)
        : m_Count(count)
        , m_Offset(off)
        , m_Bounds(bounds)
        , m_Name(inName)
    {
    }
    MeshSubset()
        : m_Count((quint32)-1)
        , m_Offset(0)
        , m_Bounds()
    {
    }
    bool HasCount() const { return m_Count != 4294967295U; } //AKA U_MAX 0xffffffff
};

struct Joint
{
    qint32 m_JointID;
    qint32 m_ParentID;
    float m_invBindPose[16];
    float m_localToGlobalBoneSpace[16];

    Joint(qint32 jointID, qint32 parentID, const float *invBindPose,
          const float *localToGlobalBoneSpace)
        : m_JointID(jointID)
        , m_ParentID(parentID)
    {
        ::memcpy(m_invBindPose, invBindPose, sizeof(float) * 16);
        ::memcpy(m_localToGlobalBoneSpace, localToGlobalBoneSpace, sizeof(float) * 16);
    }
    Joint()
        : m_JointID(-1)
        , m_ParentID(-1)
    {
        ::memset(m_invBindPose, 0, sizeof(float) * 16);
        ::memset(m_localToGlobalBoneSpace, 0, sizeof(float) * 16);
    }
};

struct Mesh
{
    static const char16_t *s_DefaultName;

    VertexBuffer m_VertexBuffer;
    IndexBuffer m_IndexBuffer;
    OffsetDataRef<MeshSubset> m_Subsets;
    OffsetDataRef<Joint> m_Joints;
    RenderDrawMode::Enum m_DrawMode;
    RenderWinding::Enum m_Winding;
};

static const char *getPositionAttrName() { return "attr_pos"; }
static const char *getNormalAttrName() { return "attr_norm"; }
static const char *getUVAttrName() { return "attr_uv0"; }
//static const char *getUV2AttrName() { return "attr_uv1"; }
static const char *getTexTanAttrName() { return "attr_textan"; }
//static const char *getTexBinormalAttrName() { return "attr_binormal"; }
//static const char *getWeightAttrName() { return "attr_weight"; }
//static const char *getBoneIndexAttrName() { return "attr_boneid"; }
static const char *getColorAttrName() { return "attr_color"; }

struct MeshDataHeader
{
    static quint32 GetFileId() { return (quint32)-929005747; }
    static quint16 GetCurrentFileVersion() { return 3; }
    quint32 m_FileId;
    quint16 m_FileVersion;
    quint16 m_HeaderFlags;
    quint32 m_SizeInBytes;
    MeshDataHeader(quint32 size = 0)
        : m_FileId(GetFileId())
        , m_FileVersion(GetCurrentFileVersion())
        , m_SizeInBytes(size)
    {
    }
};

// Tells us what offset a mesh with this ID starts.
struct MeshMultiEntry
{
    quint64 m_MeshOffset;
    quint32 m_MeshId;
    quint32 m_Padding;
    MeshMultiEntry()
        : m_MeshOffset(0)
        , m_MeshId(0)
        , m_Padding(0)
    {
    }
    MeshMultiEntry(quint64 mo, quint32 meshId)
        : m_MeshOffset(mo)
        , m_MeshId(meshId)
        , m_Padding(0)
    {
    }
};

// The multi headers are actually saved at the end of the file.
// Thus when you append to the file we overwrite the last header
// then write out a new header structure.
// The last 8 bytes of the file contain the multi header.
// The previous N*8 bytes contain the mesh entries.
struct MeshMultiHeader
{
    quint32 m_FileId;
    quint32 m_Version;
    quint32 offset;
    quint32 size;
    static quint32 GetMultiStaticFileId() { return 555777497; }
    static quint32 GetMultiStaticVersion() { return 1; }

    MeshMultiHeader()
        : m_FileId(GetMultiStaticFileId())
        , m_Version(GetMultiStaticVersion())
    {
    }
};

// Localize the knowledge required to read/write a mesh into one function
// written in such a way that you can both read and write by passing
// in one serializer type or another.
// This function needs to be careful to request alignment after every write of a
// buffer that may leave us unaligned.  The easiest way to be correct is to request
// alignment a lot.  The hardest way is to use knowledge of the datatypes and
// only request alignment when necessary.
template <typename TSerializer>
void Serialize(TSerializer &serializer, Mesh &mesh)
{
    quint8 *baseAddr = reinterpret_cast<quint8 *>(&mesh);
    serializer.streamify(mesh.m_VertexBuffer.m_Entries);
    serializer.align();

    for (quint32 entry = 0, numItems = mesh.m_VertexBuffer.m_Entries.size(); entry < numItems; ++entry)
    {
        MeshVertexBufferEntry &entryData = mesh.m_VertexBuffer.m_Entries.index(baseAddr, entry);
        serializer.streamifyCharPointerOffset(entryData.m_NameOffset);
        serializer.align();
    }
    serializer.streamify(mesh.m_VertexBuffer.m_Data);
    serializer.align();
    serializer.streamify(mesh.m_IndexBuffer.m_Data);
    serializer.align();
    serializer.streamify(mesh.m_Subsets);
    serializer.align();

    for (quint32 entry = 0, numItems = mesh.m_Subsets.size(); entry < numItems; ++entry)
    {
        MeshSubset &theSubset = const_cast<MeshSubset &>(mesh.m_Subsets.index(baseAddr, entry));
        serializer.streamify(theSubset.m_Name);
        serializer.align();
    }
    serializer.streamify(mesh.m_Joints);
    serializer.align();
}

struct TotallingSerializer
{
    quint32 m_NumBytes;
    quint8 *m_BaseAddress;
    TotallingSerializer(quint8 *inBaseAddr)
        : m_NumBytes(0)
        , m_BaseAddress(inBaseAddr)
    {
    }
    template <typename TDataType>
    void streamify(const OffsetDataRef<TDataType> &data)
    {
        m_NumBytes += data.size() * sizeof(TDataType);
    }
    void streamify(const char *data)
    {
        if (data == NULL)
            data = "";
        quint32 len = (quint32)strlen(data) + 1;
        m_NumBytes += 4;
        m_NumBytes += len;
    }
    void streamifyCharPointerOffset(quint32 inOffset)
    {
        if (inOffset) {
            const char *dataPtr = (const char *)(inOffset + m_BaseAddress);
            streamify(dataPtr);
        } else
            streamify("");
    }
    bool needsAlignment() const { return getAlignmentAmount() > 0; }
    quint32 getAlignmentAmount() const { return 4 - (m_NumBytes % 4); }
    void align()
    {
        if (needsAlignment())
            m_NumBytes += getAlignmentAmount();
    }
};

struct MemoryAssigningSerializer
{
    quint8 *m_Memory;
    quint8 *m_BaseAddress;
    quint32 m_Size;
    TotallingSerializer m_ByteCounter;
    bool m_Failure;
    MemoryAssigningSerializer(quint8 *data, quint32 size, quint32 startOffset)
        : m_Memory(data + startOffset)
        , m_BaseAddress(data)
        , m_Size(size)
        , m_ByteCounter(data)
        , m_Failure(false)
    {
        // We expect 4 byte aligned memory to begin with
        Q_ASSERT((((size_t)m_Memory) % 4) == 0);
    }

    template <typename TDataType>
    void streamify(const OffsetDataRef<TDataType> &_data)
    {
        OffsetDataRef<TDataType> &data = const_cast<OffsetDataRef<TDataType> &>(_data);
        if (m_Failure) {
            data.m_Size = 0;
            data.m_Offset = 0;
            return;
        }
        quint32 current = m_ByteCounter.m_NumBytes;
        m_ByteCounter.streamify(_data);
        if (m_ByteCounter.m_NumBytes > m_Size) {
            data.m_Size = 0;
            data.m_Offset = 0;
            m_Failure = true;
            return;
        }
        quint32 numBytes = m_ByteCounter.m_NumBytes - current;
        if (numBytes) {
            data.m_Offset = (quint32)(m_Memory - m_BaseAddress);
            updateMemoryBuffer(numBytes);
        } else {
            data.m_Offset = 0;
            data.m_Size = 0;
        }
    }
    void streamify(const char *&_data)
    {
        quint32 len;
        m_ByteCounter.m_NumBytes += 4;
        if (m_ByteCounter.m_NumBytes > m_Size) {
            _data = "";
            m_Failure = true;
            return;
        }
        memcpy(&len, m_Memory, 4);
        updateMemoryBuffer(4);
        m_ByteCounter.m_NumBytes += len;
        if (m_ByteCounter.m_NumBytes > m_Size) {
            _data = "";
            m_Failure = true;
            return;
        }
        _data = (const char *)m_Memory;
        updateMemoryBuffer(len);
    }
    void streamifyCharPointerOffset(quint32 &inOffset)
    {
        const char *dataPtr;
        streamify(dataPtr);
        inOffset = (quint32)(dataPtr - (const char *)m_BaseAddress);
    }
    void align()
    {
        if (m_ByteCounter.needsAlignment()) {
            quint32 numBytes = m_ByteCounter.getAlignmentAmount();
            m_ByteCounter.align();
            updateMemoryBuffer(numBytes);
        }
    }
    void updateMemoryBuffer(quint32 numBytes) { m_Memory += numBytes; }
};

}

using namespace Q3DSMeshUtils;

namespace {

MeshList loadMeshData(const QByteArray &meshData, quint32 flags, bool useQt3DAttributes)
{
    Q_UNUSED(flags)

    quint8 *dataStart = (quint8*)meshData.data();
    quint32 amountLeft = meshData.size() - sizeof(Mesh);
    MemoryAssigningSerializer serializer(dataStart, amountLeft, sizeof(Mesh));
    Mesh *mesh = (Mesh *)dataStart;
    Serialize(serializer, *mesh);

    if (serializer.m_Failure)
        return MeshList();

    // Vertex Buffer
    auto vertexBuffer = new Qt3DRender::QBuffer;
    vertexBuffer->setData(QByteArray((char *)mesh->m_VertexBuffer.m_Data.begin(dataStart), mesh->m_VertexBuffer.m_Data.size()));
    vertexBuffer->setUsage(Qt3DRender::QBuffer::StaticDraw);

    // Iterate through the Vertex Buffer Entries and create QAttributes to associate with the buffer
    QMap<QString, MeshVertexBufferEntry> entryBufferMap;
    for (quint32 index = 0, entryEnd = mesh->m_VertexBuffer.m_Entries.size(); index < entryEnd; ++index) {
        auto entry = mesh->m_VertexBuffer.m_Entries.index(dataStart, index);
        const char *nameBuffer = "";
        if (entry.m_NameOffset)
            nameBuffer = reinterpret_cast<const char *>(dataStart + entry.m_NameOffset);
        entryBufferMap.insert(QString::fromLocal8Bit(nameBuffer), entry);
    }

    quint32 stride = mesh->m_VertexBuffer.m_Stride;
    quint32 vertexCount = mesh->m_VertexBuffer.m_Data.size() / stride;
    QVector<Qt3DRender::QAttribute*> attributes;
    if (!useQt3DAttributes) {
        //Load all attributes + names as is for 3D Studio Runtime
        for (auto attributeName : entryBufferMap.keys()) {
            auto entry = entryBufferMap[attributeName];
            auto attribute = new Qt3DRender::QAttribute(vertexBuffer,
                                                        attributeName,
                                                        convertRenderComponentToVertexBaseType(entry.m_ComponentType),
                                                        entry.m_NumComponents,
                                                        vertexCount,
                                                        entry.m_FirstItemOffset,
                                                        stride);
            attributes.append(attribute);
        }
    } else {
        // Only load the attributes we know how to use for testing with Qt3D default materials
        // Vertex Position
        if (entryBufferMap.contains(QString::fromLocal8Bit(getPositionAttrName()))) {
            auto vertexPostionEntry = entryBufferMap[QString::fromLocal8Bit(getPositionAttrName())];
            auto positionAttribute = new Qt3DRender::QAttribute(vertexBuffer,
                                                                Qt3DRender::QAttribute::defaultPositionAttributeName(),
                                                                convertRenderComponentToVertexBaseType(vertexPostionEntry.m_ComponentType),
                                                                vertexPostionEntry.m_NumComponents,
                                                                vertexCount,
                                                                vertexPostionEntry.m_FirstItemOffset,
                                                                stride);
            attributes.append(positionAttribute);
        }

        // Vertex Normal
        if (entryBufferMap.contains(QString::fromLocal8Bit(getNormalAttrName()))) {
            auto vertexNormalEntry = entryBufferMap[QString::fromLocal8Bit(getNormalAttrName())];
            auto normalAttribute = new Qt3DRender::QAttribute(vertexBuffer,
                                                              Qt3DRender::QAttribute::defaultNormalAttributeName(),
                                                              convertRenderComponentToVertexBaseType(vertexNormalEntry.m_ComponentType),
                                                              vertexNormalEntry.m_NumComponents,
                                                              vertexCount,
                                                              vertexNormalEntry.m_FirstItemOffset,
                                                              stride);
            attributes.append(normalAttribute);
        }

        // Vertex Texture Coordinate
        if (entryBufferMap.contains(QString::fromLocal8Bit(getUVAttrName()))) {
            auto vertexTextureUVEntry = entryBufferMap[QString::fromLocal8Bit(getUVAttrName())];
            auto texutreUVAttribute = new Qt3DRender::QAttribute(vertexBuffer,
                                                                 Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName(),
                                                                 convertRenderComponentToVertexBaseType(vertexTextureUVEntry.m_ComponentType),
                                                                 vertexTextureUVEntry.m_NumComponents,
                                                                 vertexCount,
                                                                 vertexTextureUVEntry.m_FirstItemOffset,
                                                                 stride);
            attributes.append(texutreUVAttribute);
        }

        // Vertex Tangent
        if (entryBufferMap.contains(QString::fromLocal8Bit(getTexTanAttrName()))) {
            auto tangentEntry = entryBufferMap[QString::fromLocal8Bit(getTexTanAttrName())];
            auto tangentsAttribute = new Qt3DRender::QAttribute(vertexBuffer,
                                                                Qt3DRender::QAttribute::defaultTangentAttributeName(),
                                                                convertRenderComponentToVertexBaseType(tangentEntry.m_ComponentType),
                                                                tangentEntry.m_NumComponents,
                                                                vertexCount,
                                                                tangentEntry.m_FirstItemOffset,
                                                                stride);
            attributes.append(tangentsAttribute);
        }

        // Vertex Color
        if (entryBufferMap.contains(QString::fromLocal8Bit(getColorAttrName()))) {
            auto vertexColorEntry = entryBufferMap[QString::fromLocal8Bit(getColorAttrName())];
            auto colorAttribute = new Qt3DRender::QAttribute(vertexBuffer,
                                                             Qt3DRender::QAttribute::defaultColorAttributeName(),
                                                             convertRenderComponentToVertexBaseType(vertexColorEntry.m_ComponentType),
                                                             vertexColorEntry.m_NumComponents,
                                                             vertexCount,
                                                             vertexColorEntry.m_FirstItemOffset,
                                                             stride);
            attributes.append(colorAttribute);
        }
    }
    // Index Buffer
    auto indexBuffer = new Qt3DRender::QBuffer;
    indexBuffer->setData(QByteArray((char *)mesh->m_IndexBuffer.m_Data.begin(dataStart), mesh->m_IndexBuffer.m_Data.size()));
    indexBuffer->setUsage(Qt3DRender::QBuffer::StaticDraw);

    // Mesh Sub-sets
    MeshList subsets;
    for (quint32 subsetId = 0, subSetEnd = mesh->m_Subsets.size(); subsetId < subSetEnd; ++subsetId) {
        MeshSubset &source(mesh->m_Subsets.index(dataStart, subsetId));
        QString subsetName = QString::fromUtf16((const char16_t *)(dataStart + source.m_Name.m_Offset));
        auto subMesh = new Q3DSMesh();
        // Setup Geometry
        auto geometry = new Qt3DRender::QGeometry();
        for (auto attribute : attributes) {
            geometry->addAttribute(attribute);
            if (attribute->name() == QString::fromLocal8Bit(getPositionAttrName()))
                geometry->setBoundingVolumePositionAttribute(attribute);
        }
        // Index Buffer (with offset)
        Qt3DRender::QAttribute::VertexBaseType type = convertRenderComponentToVertexBaseType(mesh->m_IndexBuffer.m_ComponentType);
        auto indexAttribute = new Qt3DRender::QAttribute(indexBuffer, type, 1, source.m_Count, source.m_Offset * static_cast<uint>(RenderComponentTypes::getSizeOf(mesh->m_IndexBuffer.m_ComponentType)));
        indexAttribute->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
        geometry->addAttribute(indexAttribute);
        subMesh->setGeometry(geometry);
        subMesh->setObjectName(subsetName);
        subMesh->setVertexCount(source.m_Count);
        subMesh->setPrimitiveType(convertRenderDrawModeToPrimitiveType(mesh->m_DrawMode));
        subsets.append(subMesh);
    }

    return subsets;
}

MeshList loadMeshDataFromMulti(const QString &path, int id, bool useQt3DAttributes)
{
    // This method takes a *.mesh file generated by Qt3D Studio
    // Load mesh file
    QFile meshFile(path);
    if (!meshFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open mesh file at: " << path;
        return MeshList();
    }

    QDataStream meshFileStream(&meshFile);
    meshFileStream.setByteOrder(QDataStream::LittleEndian);

    quint64 multiHeaderStart = meshFile.size() - sizeof(MeshMultiHeader);
    meshFile.seek(multiHeaderStart);

    // Read mesh data header (which is actually at the end of the file...)
    MeshMultiHeader header;
    meshFileStream >> header.m_FileId;
    meshFileStream >> header.m_Version;
    if (header.m_FileId != MeshMultiHeader::GetMultiStaticFileId() || header.m_Version != MeshMultiHeader::GetMultiStaticVersion()) {
        qWarning() << "Mesh file does not contain valid mesh data: " << path;
        meshFile.close();
        return MeshList();
    }
    quint32 offset;
    quint32 size;
    meshFileStream >> offset;
    meshFileStream >> size;

    // Read MeshMultiEntries
    QMap<quint32, MeshMultiEntry> entries;
    quint64 multiOffset = multiHeaderStart - size * sizeof(MeshMultiEntry);
    meshFile.seek(multiOffset);
    for (quint32 i = 0; i < size; ++i) {
        MeshMultiEntry entry;
        meshFileStream >> entry.m_MeshOffset;
        meshFileStream >> entry.m_MeshId;
        meshFileStream >> entry.m_Padding;
        entries.insert(entry.m_MeshId, entry);
    }

    // Load mesh data
    MeshList meshList;
    if (entries.contains(id)) {
        meshFile.seek(entries[id].m_MeshOffset);
        // Read MeshDataHeader
        MeshDataHeader meshDataHeader;
        meshFileStream >> meshDataHeader.m_FileId;
        meshFileStream >> meshDataHeader.m_FileVersion;
        meshFileStream >> meshDataHeader.m_HeaderFlags;
        meshFileStream >> meshDataHeader.m_SizeInBytes;

        // Validate
        Q_ASSERT(meshDataHeader.m_FileId == MeshDataHeader::GetFileId());
        bool isValidMesh = true;
        if (meshDataHeader.m_FileId != MeshDataHeader::GetFileId())
            isValidMesh = false;
        if (meshDataHeader.m_FileVersion < 3 || meshDataHeader.m_FileVersion > MeshDataHeader::GetCurrentFileVersion())
            isValidMesh = false;

        if (isValidMesh) {
            QByteArray meshData;
            meshData.resize(meshDataHeader.m_SizeInBytes);
            int amountRead = meshFileStream.readRawData(meshData.data(), meshDataHeader.m_SizeInBytes);
            if (quint32(amountRead) == meshDataHeader.m_SizeInBytes) {
                // We only support Meshes version 3 and higher
                meshList = loadMeshData(meshData, meshDataHeader.m_HeaderFlags, useQt3DAttributes);
            }
        }
    }

    meshFile.close();
    return meshList;
}

} // end anonymous namespace

MeshList Q3DSMeshLoader::loadMesh(const QString &meshPath, int partId, bool useQt3DAttributes)
{
    static QMap<QString, QString> primitiveMap = {{"#Rectangle", "res/primitives/Rectangle.mesh"},
                                                  {"#Sphere", "res/primitives/Sphere.mesh"},
                                                  {"#Cube", "res/primitives/Cube.mesh"},
                                                  {"#Cone", "res/primitives/Cone.mesh"},
                                                  {"#Cylinder", "res/primitives/Cylinder.mesh"}};
    QString resolvedPath = meshPath;
    int id = partId;

    // Check if the path refers to a primitive type (built-ins)
    QString primitivePath = primitiveMap.value(meshPath, QString());
    if (!primitivePath.isEmpty()) {
        resolvedPath = primitivePath;
        // Primitives always use id 1
        id = 1;
        resolvedPath.prepend(Q3DSUtils::resourcePrefix());
    }

    return loadMeshDataFromMulti(resolvedPath, id, useQt3DAttributes);
}

QT_END_NAMESPACE
