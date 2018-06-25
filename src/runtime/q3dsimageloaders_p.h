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

#ifndef Q3DSIMAGELOADERS_P_H
#define Q3DSIMAGELOADERS_P_H

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

#include <QIODevice>
#include <Qt3DRender/QTextureImageData>
#include <Qt3DRender/QAbstractTextureImage>
#include <qmath.h>
#include <qendian.h>

QT_BEGIN_NAMESPACE

// These should ideally be shared via QtGui. For now just have them here.

inline Qt3DRender::QTextureImageDataPtr q3ds_loadHdr(QIODevice *source)
{
    Qt3DRender::QTextureImageDataPtr imageData;
    char sig[256];
    source->read(sig, 11);
    if (strncmp(sig, "#?RADIANCE\n", 11))
        return imageData;

    QByteArray buf = source->readAll();
    const char *p = buf.constData();
    const char *pEnd = p + buf.size();

    // Process lines until the empty one.
    QByteArray line;
    while (p < pEnd) {
        char c = *p++;
        if (c == '\n') {
            if (line.isEmpty())
                break;
            if (line.startsWith(QByteArrayLiteral("FORMAT="))) {
                const QByteArray format = line.mid(7).trimmed();
                if (format != QByteArrayLiteral("32-bit_rle_rgbe")) {
                    qWarning("HDR format '%s' is not supported", format.constData());
                    return imageData;
                }
            }
            line.clear();
        } else {
            line.append(c);
        }
    }
    if (p == pEnd) {
        qWarning("Malformed HDR image data at property strings");
        return imageData;
    }

    // Get the resolution string.
    while (p < pEnd) {
        char c = *p++;
        if (c == '\n')
            break;
        line.append(c);
    }
    if (p == pEnd) {
        qWarning("Malformed HDR image data at resolution string");
        return imageData;
    }

    int w = 0, h = 0;
    // We only care about the standard orientation.
    if (!sscanf(line.constData(), "-Y %d +X %d", &h, &w)) {
        qWarning("Unsupported HDR resolution string '%s'", line.constData());
        return imageData;
    }
    if (w <= 0 || h <= 0) {
        qWarning("Invalid HDR resolution");
        return imageData;
    }

    const QOpenGLTexture::TextureFormat textureFormat = QOpenGLTexture::RGBA32F;
    const QOpenGLTexture::PixelFormat pixelFormat = QOpenGLTexture::RGBA;
    const QOpenGLTexture::PixelType pixelType = QOpenGLTexture::Float32;
    const int blockSize = 4 * sizeof(float);
    QByteArray data;
    data.resize(w * h * blockSize);

    typedef unsigned char RGBE[4];
    RGBE *scanline = new RGBE[w];

    for (int y = 0; y < h; ++y) {
        if (pEnd - p < 4) {
            qWarning("Unexpected end of HDR data");
            delete[] scanline;
            return imageData;
        }

        scanline[0][0] = *p++;
        scanline[0][1] = *p++;
        scanline[0][2] = *p++;
        scanline[0][3] = *p++;

        if (scanline[0][0] == 2 && scanline[0][1] == 2 && scanline[0][2] < 128) {
            // new rle, the first pixel was a dummy
            for (int channel = 0; channel < 4; ++channel) {
                for (int x = 0; x < w && p < pEnd; ) {
                    unsigned char c = *p++;
                    if (c > 128) { // run
                        if (p < pEnd) {
                            int repCount = c & 127;
                            c = *p++;
                            while (repCount--)
                                scanline[x++][channel] = c;
                        }
                    } else { // not a run
                        while (c-- && p < pEnd)
                            scanline[x++][channel] = *p++;
                    }
                }
            }
        } else {
            // old rle
            scanline[0][0] = 2;
            int bitshift = 0;
            int x = 1;
            while (x < w && pEnd - p >= 4) {
                scanline[x][0] = *p++;
                scanline[x][1] = *p++;
                scanline[x][2] = *p++;
                scanline[x][3] = *p++;

                if (scanline[x][0] == 1 && scanline[x][1] == 1 && scanline[x][2] == 1) { // run
                    int repCount = scanline[x][3] << bitshift;
                    while (repCount--) {
                        memcpy(scanline[x], scanline[x - 1], 4);
                        ++x;
                    }
                    bitshift += 8;
                } else { // not a run
                    ++x;
                    bitshift = 0;
                }
            }
        }

        // adjust for -Y orientation
        float *fp = reinterpret_cast<float *>(data.data() + (h - 1 - y) * blockSize * w);
        for (int x = 0; x < w; ++x) {
            float d = qPow(2.0f, float(scanline[x][3]) - 128.0f);
            // r, g, b, a
            *fp++ = scanline[x][0] / 256.0f * d;
            *fp++ = scanline[x][1] / 256.0f * d;
            *fp++ = scanline[x][2] / 256.0f * d;
            *fp++ = 1.0f;
        }
    }

    delete[] scanline;

    imageData = Qt3DRender::QTextureImageDataPtr::create();
    imageData->setTarget(QOpenGLTexture::Target2D);
    imageData->setFormat(textureFormat);
    imageData->setWidth(w);
    imageData->setHeight(h);
    imageData->setLayers(1);
    imageData->setDepth(1);
    imageData->setFaces(1);
    imageData->setMipLevels(1);
    imageData->setPixelFormat(pixelFormat);
    imageData->setPixelType(pixelType);
    imageData->setData(data, blockSize, false);

    return imageData;
}

inline Qt3DRender::QTextureImageDataPtr q3ds_loadPkm(QIODevice *source)
{
    struct PkmHeader
    {
        char magic[4];
        char version[2];
        quint16 textureType;
        quint16 paddedWidth;
        quint16 paddedHeight;
        quint16 width;
        quint16 height;
    };

    Qt3DRender::QTextureImageDataPtr imageData;

    PkmHeader header;
    if ((source->read(reinterpret_cast<char *>(&header), sizeof header) != sizeof header)
            || (qstrncmp(header.magic, "PKM ", 4) != 0))
        return imageData;

    QOpenGLTexture::TextureFormat format = QOpenGLTexture::NoFormat;
    int blockSize = 0;

    if (header.version[0] == '2' && header.version[1] == '0') {
        switch (qFromBigEndian(header.textureType)) {
        case 0:
            format = QOpenGLTexture::RGB8_ETC1;
            blockSize = 8;
            break;

        case 1:
            format = QOpenGLTexture::RGB8_ETC2;
            blockSize = 8;
            break;

        case 3:
            format = QOpenGLTexture::RGBA8_ETC2_EAC;
            blockSize = 16;
            break;

        case 4:
            format = QOpenGLTexture::RGB8_PunchThrough_Alpha1_ETC2;
            blockSize = 8;
            break;

        case 5:
            format = QOpenGLTexture::R11_EAC_UNorm;
            blockSize = 8;
            break;

        case 6:
            format = QOpenGLTexture::RG11_EAC_UNorm;
            blockSize = 16;
            break;

        case 7:
            format = QOpenGLTexture::R11_EAC_SNorm;
            blockSize = 8;
            break;

        case 8:
            format = QOpenGLTexture::RG11_EAC_SNorm;
            blockSize = 16;
            break;
        }
    } else {
        format = QOpenGLTexture::RGB8_ETC1;
        blockSize = 8;
    }

    if (format == QOpenGLTexture::NoFormat) {
        qWarning() << "Unrecognized compression format in" << source;
        return imageData;
    }

    // get the extended (multiple of 4) width and height
    const int width = qFromBigEndian(header.paddedWidth);
    const int height = qFromBigEndian(header.paddedHeight);

    const QByteArray data = source->readAll();
    if (data.size() != (width / 4) * (height / 4) * blockSize) {
        qWarning() << "Unexpected data size in" << source;
        return imageData;
    }

    imageData = Qt3DRender::QTextureImageDataPtr::create();
    imageData->setTarget(QOpenGLTexture::Target2D);
    imageData->setFormat(format);
    imageData->setWidth(width);
    imageData->setHeight(height);
    imageData->setLayers(1);
    imageData->setDepth(1);
    imageData->setFaces(1);
    imageData->setMipLevels(1);
    imageData->setPixelFormat(QOpenGLTexture::NoSourceFormat);
    imageData->setPixelType(QOpenGLTexture::NoPixelType);
    imageData->setData(data, blockSize, true);

    return imageData;
}

template <int a, int b, int c, int d>
struct DdsFourCC
{
    static const quint32 value = a | (b << 8) | (c << 16) | (d << 24);
};

inline Qt3DRender::QTextureImageDataPtr q3ds_loadDds(QIODevice *source)
{
    struct DdsPixelFormat
    {
        quint32 size;
        quint32 flags;
        quint32 fourCC;
        quint32 rgbBitCount;
        quint32 redMask;
        quint32 greenMask;
        quint32 blueMask;
        quint32 alphaMask;
    };

    struct DdsHeader
    {
        char magic[4];
        quint32 size;
        quint32 flags;
        quint32 height;
        quint32 width;
        quint32 pitchOrLinearSize;
        quint32 depth;
        quint32 mipmapCount;
        quint32 reserved[11];
        DdsPixelFormat pixelFormat;
        quint32 caps;
        quint32 caps2;
        quint32 caps3;
        quint32 caps4;
        quint32 reserved2;
    };

    struct DdsDX10Header
    {
        quint32 format;
        quint32 dimension;
        quint32 miscFlag;
        quint32 arraySize;
        quint32 miscFlags2;
    };

    enum DdsFlags
    {
        MipmapCountFlag             = 0x20000,
    };

    enum PixelFormatFlag
    {
        AlphaFlag                   = 0x1,
        FourCCFlag                  = 0x4,
        RGBFlag                     = 0x40,
        RGBAFlag                    = RGBFlag | AlphaFlag,
        YUVFlag                     = 0x200,
        LuminanceFlag               = 0x20000
    };

    enum Caps2Flags
    {
        CubemapFlag                 = 0x200,
        CubemapPositiveXFlag        = 0x400,
        CubemapNegativeXFlag        = 0x800,
        CubemapPositiveYFlag        = 0x1000,
        CubemapNegativeYFlag        = 0x2000,
        CubemapPositiveZFlag        = 0x4000,
        CubemapNegativeZFlag        = 0x8000,
        AllCubemapFaceFlags         = CubemapPositiveXFlag |
        CubemapNegativeXFlag |
        CubemapPositiveYFlag |
        CubemapNegativeYFlag |
        CubemapPositiveZFlag |
        CubemapNegativeZFlag,
        VolumeFlag                  = 0x200000
    };

    enum DXGIFormat
    {
        DXGI_FORMAT_UNKNOWN                     = 0,
        DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
        DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
        DXGI_FORMAT_R32G32B32A32_UINT           = 3,
        DXGI_FORMAT_R32G32B32A32_SINT           = 4,
        DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
        DXGI_FORMAT_R32G32B32_FLOAT             = 6,
        DXGI_FORMAT_R32G32B32_UINT              = 7,
        DXGI_FORMAT_R32G32B32_SINT              = 8,
        DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
        DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
        DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
        DXGI_FORMAT_R16G16B16A16_UINT           = 12,
        DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
        DXGI_FORMAT_R16G16B16A16_SINT           = 14,
        DXGI_FORMAT_R32G32_TYPELESS             = 15,
        DXGI_FORMAT_R32G32_FLOAT                = 16,
        DXGI_FORMAT_R32G32_UINT                 = 17,
        DXGI_FORMAT_R32G32_SINT                 = 18,
        DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
        DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
        DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
        DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
        DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
        DXGI_FORMAT_R10G10B10A2_UINT            = 25,
        DXGI_FORMAT_R11G11B10_FLOAT             = 26,
        DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
        DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
        DXGI_FORMAT_R8G8B8A8_UINT               = 30,
        DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
        DXGI_FORMAT_R8G8B8A8_SINT               = 32,
        DXGI_FORMAT_R16G16_TYPELESS             = 33,
        DXGI_FORMAT_R16G16_FLOAT                = 34,
        DXGI_FORMAT_R16G16_UNORM                = 35,
        DXGI_FORMAT_R16G16_UINT                 = 36,
        DXGI_FORMAT_R16G16_SNORM                = 37,
        DXGI_FORMAT_R16G16_SINT                 = 38,
        DXGI_FORMAT_R32_TYPELESS                = 39,
        DXGI_FORMAT_D32_FLOAT                   = 40,
        DXGI_FORMAT_R32_FLOAT                   = 41,
        DXGI_FORMAT_R32_UINT                    = 42,
        DXGI_FORMAT_R32_SINT                    = 43,
        DXGI_FORMAT_R24G8_TYPELESS              = 44,
        DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
        DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
        DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
        DXGI_FORMAT_R8G8_TYPELESS               = 48,
        DXGI_FORMAT_R8G8_UNORM                  = 49,
        DXGI_FORMAT_R8G8_UINT                   = 50,
        DXGI_FORMAT_R8G8_SNORM                  = 51,
        DXGI_FORMAT_R8G8_SINT                   = 52,
        DXGI_FORMAT_R16_TYPELESS                = 53,
        DXGI_FORMAT_R16_FLOAT                   = 54,
        DXGI_FORMAT_D16_UNORM                   = 55,
        DXGI_FORMAT_R16_UNORM                   = 56,
        DXGI_FORMAT_R16_UINT                    = 57,
        DXGI_FORMAT_R16_SNORM                   = 58,
        DXGI_FORMAT_R16_SINT                    = 59,
        DXGI_FORMAT_R8_TYPELESS                 = 60,
        DXGI_FORMAT_R8_UNORM                    = 61,
        DXGI_FORMAT_R8_UINT                     = 62,
        DXGI_FORMAT_R8_SNORM                    = 63,
        DXGI_FORMAT_R8_SINT                     = 64,
        DXGI_FORMAT_A8_UNORM                    = 65,
        DXGI_FORMAT_R1_UNORM                    = 66,
        DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
        DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
        DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,
        DXGI_FORMAT_BC1_TYPELESS                = 70,
        DXGI_FORMAT_BC1_UNORM                   = 71,
        DXGI_FORMAT_BC1_UNORM_SRGB              = 72,
        DXGI_FORMAT_BC2_TYPELESS                = 73,
        DXGI_FORMAT_BC2_UNORM                   = 74,
        DXGI_FORMAT_BC2_UNORM_SRGB              = 75,
        DXGI_FORMAT_BC3_TYPELESS                = 76,
        DXGI_FORMAT_BC3_UNORM                   = 77,
        DXGI_FORMAT_BC3_UNORM_SRGB              = 78,
        DXGI_FORMAT_BC4_TYPELESS                = 79,
        DXGI_FORMAT_BC4_UNORM                   = 80,
        DXGI_FORMAT_BC4_SNORM                   = 81,
        DXGI_FORMAT_BC5_TYPELESS                = 82,
        DXGI_FORMAT_BC5_UNORM                   = 83,
        DXGI_FORMAT_BC5_SNORM                   = 84,
        DXGI_FORMAT_B5G6R5_UNORM                = 85,
        DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
        DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
        DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
        DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
        DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
        DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
        DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
        DXGI_FORMAT_BC6H_TYPELESS               = 94,
        DXGI_FORMAT_BC6H_UF16                   = 95,
        DXGI_FORMAT_BC6H_SF16                   = 96,
        DXGI_FORMAT_BC7_TYPELESS                = 97,
        DXGI_FORMAT_BC7_UNORM                   = 98,
        DXGI_FORMAT_BC7_UNORM_SRGB              = 99,
        DXGI_FORMAT_AYUV                        = 100,
        DXGI_FORMAT_Y410                        = 101,
        DXGI_FORMAT_Y416                        = 102,
        DXGI_FORMAT_NV12                        = 103,
        DXGI_FORMAT_P010                        = 104,
        DXGI_FORMAT_P016                        = 105,
        DXGI_FORMAT_420_OPAQUE                  = 106,
        DXGI_FORMAT_YUY2                        = 107,
        DXGI_FORMAT_Y210                        = 108,
        DXGI_FORMAT_Y216                        = 109,
        DXGI_FORMAT_NV11                        = 110,
        DXGI_FORMAT_AI44                        = 111,
        DXGI_FORMAT_IA44                        = 112,
        DXGI_FORMAT_P8                          = 113,
        DXGI_FORMAT_A8P8                        = 114,
        DXGI_FORMAT_B4G4R4A4_UNORM              = 115,
        DXGI_FORMAT_P208                        = 130,
        DXGI_FORMAT_V208                        = 131,
        DXGI_FORMAT_V408                        = 132,
    };

    struct FormatInfo
    {
        QOpenGLTexture::PixelFormat pixelFormat;
        QOpenGLTexture::TextureFormat textureFormat;
        QOpenGLTexture::PixelType pixelType;
        int components;
        bool compressed;
    };

    const struct RGBAFormat
    {
        quint32 redMask;
        quint32 greenMask;
        quint32 blueMask;
        quint32 alphaMask;
        FormatInfo formatInfo;

    } rgbaFormats[] = {
        // unorm formats
    { 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000,   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA8_UNorm,    QOpenGLTexture::UInt8,           4, false } },
    { 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000,   { QOpenGLTexture::BGRA,             QOpenGLTexture::RGBA8_UNorm,    QOpenGLTexture::UInt8,           4, false } },
    { 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000,   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA8_UNorm,    QOpenGLTexture::UInt8,           4, false } },
    { 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000,   { QOpenGLTexture::BGRA,             QOpenGLTexture::RGBA8_UNorm,    QOpenGLTexture::UInt8,           4, false } },
    { 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000,   { QOpenGLTexture::RGB,              QOpenGLTexture::RGB8_UNorm,     QOpenGLTexture::UInt8,           3, false } },
    { 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000,   { QOpenGLTexture::BGR,              QOpenGLTexture::RGB8_UNorm,     QOpenGLTexture::UInt8,           3, false } },

    // packed formats
    { 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000,   { QOpenGLTexture::RGB,              QOpenGLTexture::R5G6B5,         QOpenGLTexture::UInt16_R5G6B5,   2, false } },
    { 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000,   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGB5A1,         QOpenGLTexture::UInt16_RGB5A1,   2, false } },
    { 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000,   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA4,          QOpenGLTexture::UInt16_RGBA4,    2, false } },
    { 0x000000e0, 0x0000001c, 0x00000003, 0x00000000,   { QOpenGLTexture::RGB,              QOpenGLTexture::RG3B2,          QOpenGLTexture::UInt8_RG3B2,     1, false } },
    { 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000,   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGB10A2,        QOpenGLTexture::UInt32_RGB10A2,  4, false } },

    // luminance alpha
    { 0x000000ff, 0x000000ff, 0x000000ff, 0x00000000,   { QOpenGLTexture::Red,              QOpenGLTexture::R8_UNorm,       QOpenGLTexture::UInt8,           1, false } },
    { 0x000000ff, 0x00000000, 0x00000000, 0x00000000,   { QOpenGLTexture::Red,              QOpenGLTexture::R8_UNorm,       QOpenGLTexture::UInt8,           1, false } },
    { 0x000000ff, 0x000000ff, 0x000000ff, 0x0000ff00,   { QOpenGLTexture::RG,               QOpenGLTexture::RG8_UNorm,      QOpenGLTexture::UInt8,           2, false } },
    { 0x000000ff, 0x00000000, 0x00000000, 0x0000ff00,   { QOpenGLTexture::RG,               QOpenGLTexture::RG8_UNorm,      QOpenGLTexture::UInt8,           2, false } },
    };

    const struct FourCCFormat
    {
        quint32 fourCC;
        FormatInfo formatInfo;
    } fourCCFormats[] = {
    { DdsFourCC<'D','X','T','1'>::value,                { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGBA_DXT1,      QOpenGLTexture::NoPixelType,     8, true } },
    { DdsFourCC<'D','X','T','3'>::value,                { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGBA_DXT3,      QOpenGLTexture::NoPixelType,    16, true } },
    { DdsFourCC<'D','X','T','5'>::value,                { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGBA_DXT5,      QOpenGLTexture::NoPixelType,    16, true } },
    { DdsFourCC<'A','T','I','1'>::value,                { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::R_ATI1N_UNorm,  QOpenGLTexture::NoPixelType,     8, true } },
    { DdsFourCC<'A','T','I','2'>::value,                { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RG_ATI2N_UNorm, QOpenGLTexture::NoPixelType,    16, true } },
    };

    const struct DX10Format
    {
        DXGIFormat dxgiFormat;
        FormatInfo formatInfo;
    } dx10Formats[] = {
        // unorm formats
    { DXGI_FORMAT_R8_UNORM,                             { QOpenGLTexture::Red,              QOpenGLTexture::R8_UNorm,       QOpenGLTexture::UInt8,           1, false } },
    { DXGI_FORMAT_R8G8_UNORM,                           { QOpenGLTexture::RG,               QOpenGLTexture::RG8_UNorm,      QOpenGLTexture::UInt8,           2, false } },
    { DXGI_FORMAT_R8G8B8A8_UNORM,                       { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA8_UNorm,    QOpenGLTexture::UInt8,           4, false } },

    { DXGI_FORMAT_R16_UNORM,                            { QOpenGLTexture::Red,              QOpenGLTexture::R16_UNorm,      QOpenGLTexture::UInt16,          2, false } },
    { DXGI_FORMAT_R16G16_UNORM,                         { QOpenGLTexture::RG,               QOpenGLTexture::RG16_UNorm,     QOpenGLTexture::UInt16,          4, false } },
    { DXGI_FORMAT_R16G16B16A16_UNORM,                   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA16_UNorm,   QOpenGLTexture::UInt16,          8, false } },

    // snorm formats
    { DXGI_FORMAT_R8_SNORM,                             { QOpenGLTexture::Red,              QOpenGLTexture::R8_SNorm,       QOpenGLTexture::Int8,            1, false } },
    { DXGI_FORMAT_R8G8_SNORM,                           { QOpenGLTexture::RG,               QOpenGLTexture::RG8_SNorm,      QOpenGLTexture::Int8,            2, false } },
    { DXGI_FORMAT_R8G8B8A8_SNORM,                       { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA8_SNorm,    QOpenGLTexture::Int8,            4, false } },

    { DXGI_FORMAT_R16_SNORM,                            { QOpenGLTexture::Red,              QOpenGLTexture::R16_SNorm,      QOpenGLTexture::Int16,           2, false } },
    { DXGI_FORMAT_R16G16_SNORM,                         { QOpenGLTexture::RG,               QOpenGLTexture::RG16_SNorm,     QOpenGLTexture::Int16,           4, false } },
    { DXGI_FORMAT_R16G16B16A16_SNORM,                   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA16_SNorm,   QOpenGLTexture::Int16,           8, false } },

    // unsigned integer formats
    { DXGI_FORMAT_R8_UINT,                              { QOpenGLTexture::Red_Integer,      QOpenGLTexture::R8U,            QOpenGLTexture::UInt8,           1, false } },
    { DXGI_FORMAT_R8G8_UINT,                            { QOpenGLTexture::RG_Integer,       QOpenGLTexture::RG8U,           QOpenGLTexture::UInt8,           2, false } },
    { DXGI_FORMAT_R8G8B8A8_UINT,                        { QOpenGLTexture::RGBA_Integer,     QOpenGLTexture::RGBA8U,         QOpenGLTexture::UInt8,           4, false } },

    { DXGI_FORMAT_R16_UINT,                             { QOpenGLTexture::Red_Integer,      QOpenGLTexture::R16U,           QOpenGLTexture::UInt16,          2, false } },
    { DXGI_FORMAT_R16G16_UINT,                          { QOpenGLTexture::RG_Integer,       QOpenGLTexture::RG16U,          QOpenGLTexture::UInt16,          4, false } },
    { DXGI_FORMAT_R16G16B16A16_UINT,                    { QOpenGLTexture::RGBA_Integer,     QOpenGLTexture::RGBA16U,        QOpenGLTexture::UInt16,          8, false } },

    { DXGI_FORMAT_R32_UINT,                             { QOpenGLTexture::Red_Integer,      QOpenGLTexture::R32U,           QOpenGLTexture::UInt32,          4, false } },
    { DXGI_FORMAT_R32G32_UINT,                          { QOpenGLTexture::RG_Integer,       QOpenGLTexture::RG32U,          QOpenGLTexture::UInt32,          8, false } },
    { DXGI_FORMAT_R32G32B32_UINT,                       { QOpenGLTexture::RGB_Integer,      QOpenGLTexture::RGB32U,         QOpenGLTexture::UInt32,         12, false } },
    { DXGI_FORMAT_R32G32B32A32_UINT,                    { QOpenGLTexture::RGBA_Integer,     QOpenGLTexture::RGBA32U,        QOpenGLTexture::UInt32,         16, false } },

    // signed integer formats
    { DXGI_FORMAT_R8_SINT,                              { QOpenGLTexture::Red_Integer,      QOpenGLTexture::R8I,            QOpenGLTexture::Int8,            1, false } },
    { DXGI_FORMAT_R8G8_SINT,                            { QOpenGLTexture::RG_Integer,       QOpenGLTexture::RG8I,           QOpenGLTexture::Int8,            2, false } },
    { DXGI_FORMAT_R8G8B8A8_SINT,                        { QOpenGLTexture::RGBA_Integer,     QOpenGLTexture::RGBA8I,         QOpenGLTexture::Int8,            4, false } },

    { DXGI_FORMAT_R16_SINT,                             { QOpenGLTexture::Red_Integer,      QOpenGLTexture::R16I,           QOpenGLTexture::Int16,           2, false } },
    { DXGI_FORMAT_R16G16_SINT,                          { QOpenGLTexture::RG_Integer,       QOpenGLTexture::RG16I,          QOpenGLTexture::Int16,           4, false } },
    { DXGI_FORMAT_R16G16B16A16_SINT,                    { QOpenGLTexture::RGBA_Integer,     QOpenGLTexture::RGBA16I,        QOpenGLTexture::Int16,           8, false } },

    { DXGI_FORMAT_R32_SINT,                             { QOpenGLTexture::Red_Integer,      QOpenGLTexture::R32I,           QOpenGLTexture::Int32,           4, false } },
    { DXGI_FORMAT_R32G32_SINT,                          { QOpenGLTexture::RG_Integer,       QOpenGLTexture::RG32I,          QOpenGLTexture::Int32,           8, false } },
    { DXGI_FORMAT_R32G32B32_SINT,                       { QOpenGLTexture::RGB_Integer,      QOpenGLTexture::RGB32I,         QOpenGLTexture::Int32,          12, false } },
    { DXGI_FORMAT_R32G32B32A32_SINT,                    { QOpenGLTexture::RGBA_Integer,     QOpenGLTexture::RGBA32I,        QOpenGLTexture::Int32,          16, false } },

    // floating formats
    { DXGI_FORMAT_R16_FLOAT,                            { QOpenGLTexture::Red,              QOpenGLTexture::R16F,           QOpenGLTexture::Float16,         2, false } },
    { DXGI_FORMAT_R16G16_FLOAT,                         { QOpenGLTexture::RG,               QOpenGLTexture::RG16F,          QOpenGLTexture::Float16,         4, false } },
    { DXGI_FORMAT_R16G16B16A16_FLOAT,                   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA16F,        QOpenGLTexture::Float16,         8, false } },

    { DXGI_FORMAT_R32_FLOAT,                            { QOpenGLTexture::Red,              QOpenGLTexture::R32F,           QOpenGLTexture::Float32,         4, false } },
    { DXGI_FORMAT_R32G32_FLOAT,                         { QOpenGLTexture::RG,               QOpenGLTexture::RG32F,          QOpenGLTexture::Float32,         8, false } },
    { DXGI_FORMAT_R32G32B32_FLOAT,                      { QOpenGLTexture::RGB,              QOpenGLTexture::RGB32F,         QOpenGLTexture::Float32,        12, false } },
    { DXGI_FORMAT_R32G32B32A32_FLOAT,                   { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA32F,        QOpenGLTexture::Float32,        16, false } },

    // sRGB formats
    { DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,                  { QOpenGLTexture::RGB,              QOpenGLTexture::SRGB8,          QOpenGLTexture::UInt8,           4, false } },
    { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,                  { QOpenGLTexture::RGBA,             QOpenGLTexture::SRGB8_Alpha8,   QOpenGLTexture::UInt8,           4, false } },

    // packed formats
    // { DXGI_FORMAT_R10G10B10A2_UNORM,                 { QOpenGLTexture::RGB10A2_UNORM, QOpenGLTexture::RGBA, QOpenGLTexture::UInt32_RGB10A2, 4, false } },
    { DXGI_FORMAT_R10G10B10A2_UINT,                     { QOpenGLTexture::RGBA_Integer,     QOpenGLTexture::RGB10A2,        QOpenGLTexture::UInt32_RGB10A2,  4, false } },
    { DXGI_FORMAT_R9G9B9E5_SHAREDEXP,                   { QOpenGLTexture::RGB,              QOpenGLTexture::RGB9E5,         QOpenGLTexture::UInt32_RGB9_E5,  4, false } },
    { DXGI_FORMAT_R11G11B10_FLOAT,                      { QOpenGLTexture::RGB,              QOpenGLTexture::RG11B10F,       QOpenGLTexture::UInt32_RG11B10F, 4, false } },
    { DXGI_FORMAT_B5G6R5_UNORM,                         { QOpenGLTexture::RGB,              QOpenGLTexture::R5G6B5,         QOpenGLTexture::UInt16_R5G6B5,   2, false } },
    { DXGI_FORMAT_B5G5R5A1_UNORM,                       { QOpenGLTexture::RGBA,             QOpenGLTexture::RGB5A1,         QOpenGLTexture::UInt16_RGB5A1,   2, false } },
    { DXGI_FORMAT_B4G4R4A4_UNORM,                       { QOpenGLTexture::RGBA,             QOpenGLTexture::RGBA4,          QOpenGLTexture::UInt16_RGBA4,    2, false } },

    // swizzle formats
    { DXGI_FORMAT_B8G8R8X8_UNORM,                       { QOpenGLTexture::BGRA,             QOpenGLTexture::RGB8_UNorm,     QOpenGLTexture::UInt8,           4, false } },
    { DXGI_FORMAT_B8G8R8A8_UNORM,                       { QOpenGLTexture::BGRA,             QOpenGLTexture::RGBA8_UNorm,    QOpenGLTexture::UInt8,           4, false } },
    { DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,                  { QOpenGLTexture::BGRA,             QOpenGLTexture::SRGB8,          QOpenGLTexture::UInt8,           4, false } },
    { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,                  { QOpenGLTexture::BGRA,             QOpenGLTexture::SRGB8_Alpha8,   QOpenGLTexture::UInt8,           4, false } },

    // luminance alpha formats
    { DXGI_FORMAT_R8_UNORM,                             { QOpenGLTexture::Red,              QOpenGLTexture::R8_UNorm,       QOpenGLTexture::UInt8,           1, false } },
    { DXGI_FORMAT_R8G8_UNORM,                           { QOpenGLTexture::RG,               QOpenGLTexture::RG8_UNorm,      QOpenGLTexture::UInt8,           2, false } },
    { DXGI_FORMAT_R16_UNORM,                            { QOpenGLTexture::Red,              QOpenGLTexture::R16_UNorm,      QOpenGLTexture::UInt16,          2, false } },
    { DXGI_FORMAT_R16G16_UNORM,                         { QOpenGLTexture::RG,               QOpenGLTexture::RG16_UNorm,     QOpenGLTexture::UInt16,          4, false } },

    // depth formats
    { DXGI_FORMAT_D16_UNORM,                            { QOpenGLTexture::Depth,            QOpenGLTexture::D16,            QOpenGLTexture::NoPixelType,     2, false } },
    { DXGI_FORMAT_D24_UNORM_S8_UINT,                    { QOpenGLTexture::DepthStencil,     QOpenGLTexture::D24S8,          QOpenGLTexture::NoPixelType,     4, false } },
    { DXGI_FORMAT_D32_FLOAT,                            { QOpenGLTexture::Depth,            QOpenGLTexture::D32F,           QOpenGLTexture::NoPixelType,     4, false } },
    { DXGI_FORMAT_D32_FLOAT_S8X24_UINT,                 { QOpenGLTexture::DepthStencil,     QOpenGLTexture::D32FS8X24,      QOpenGLTexture::NoPixelType,     8, false } },

    // compressed formats
    { DXGI_FORMAT_BC1_UNORM,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGBA_DXT1,      QOpenGLTexture::NoPixelType,     8, true } },
    { DXGI_FORMAT_BC2_UNORM,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGBA_DXT3,      QOpenGLTexture::NoPixelType,    16, true } },
    { DXGI_FORMAT_BC3_UNORM,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGBA_DXT5,      QOpenGLTexture::NoPixelType,    16, true } },
    { DXGI_FORMAT_BC4_UNORM,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::R_ATI1N_UNorm,  QOpenGLTexture::NoPixelType,     8, true } },
    { DXGI_FORMAT_BC4_SNORM,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::R_ATI1N_SNorm,  QOpenGLTexture::NoPixelType,     8, true } },
    { DXGI_FORMAT_BC5_UNORM,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RG_ATI2N_UNorm, QOpenGLTexture::NoPixelType,    16, true } },
    { DXGI_FORMAT_BC5_SNORM,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RG_ATI2N_SNorm, QOpenGLTexture::NoPixelType,    16, true } },
    { DXGI_FORMAT_BC6H_UF16,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGB_BP_UNSIGNED_FLOAT, QOpenGLTexture::NoPixelType, 16, true } },
    { DXGI_FORMAT_BC6H_SF16,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGB_BP_SIGNED_FLOAT, QOpenGLTexture::NoPixelType, 16, true } },
    { DXGI_FORMAT_BC7_UNORM,                            { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::RGB_BP_UNorm, QOpenGLTexture::NoPixelType,      16, true } },

    // compressed sRGB formats
    { DXGI_FORMAT_BC1_UNORM_SRGB,                       { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::SRGB_DXT1,      QOpenGLTexture::NoPixelType,     8, true } },
    { DXGI_FORMAT_BC1_UNORM_SRGB,                       { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::SRGB_Alpha_DXT1, QOpenGLTexture::NoPixelType,    8, true } },
    { DXGI_FORMAT_BC2_UNORM_SRGB,                       { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::SRGB_Alpha_DXT3, QOpenGLTexture::NoPixelType,   16, true } },
    { DXGI_FORMAT_BC3_UNORM_SRGB,                       { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::SRGB_Alpha_DXT5, QOpenGLTexture::NoPixelType,   16, true } },
    { DXGI_FORMAT_BC7_UNORM_SRGB,                       { QOpenGLTexture::NoSourceFormat,   QOpenGLTexture::SRGB_BP_UNorm,  QOpenGLTexture::NoPixelType,    16, true } },
    };

    Qt3DRender::QTextureImageDataPtr imageData;

    DdsHeader header;
    if ((source->read(reinterpret_cast<char *>(&header), sizeof header) != sizeof header)
            || (qstrncmp(header.magic, "DDS ", 4) != 0))
        return imageData;

    int layers = 1;
    const quint32 pixelFlags = qFromLittleEndian(header.pixelFormat.flags);
    const quint32 fourCC = qFromLittleEndian(header.pixelFormat.fourCC);
    const FormatInfo *formatInfo = nullptr;

    if ((pixelFlags & FourCCFlag) == FourCCFlag) {
        if (fourCC == DdsFourCC<'D', 'X', '1', '0'>::value) {
            // DX10 texture
            DdsDX10Header dx10Header;
            if (source->read(reinterpret_cast<char *>(&dx10Header), sizeof dx10Header) != sizeof dx10Header)
                return imageData;

            layers = qFromLittleEndian(dx10Header.arraySize);
            DXGIFormat format = static_cast<DXGIFormat>(qFromLittleEndian(dx10Header.format));

            for (const auto &info : dx10Formats) {
                if (info.dxgiFormat == format) {
                    formatInfo = &info.formatInfo;
                    break;
                }
            }
        } else {
            // compressed, FourCC texture
            for (const auto &info : fourCCFormats) {
                if (info.fourCC == fourCC) {
                    formatInfo = &info.formatInfo;
                    break;
                }
            }
        }
    } else {
        // uncompressed texture
        const quint32 rgbBitCount   = qFromLittleEndian(header.pixelFormat.rgbBitCount);
        const quint32 redMask       = qFromLittleEndian(header.pixelFormat.redMask);
        const quint32 greenMask     = qFromLittleEndian(header.pixelFormat.greenMask);
        const quint32 blueMask      = qFromLittleEndian(header.pixelFormat.blueMask);
        const quint32 alphaMask     = qFromLittleEndian(header.pixelFormat.alphaMask);

        for (const auto &info : rgbaFormats) {
            if (info.formatInfo.components * 8u == rgbBitCount &&
                    info.redMask == redMask && info.greenMask == greenMask &&
                    info.blueMask == blueMask && info.alphaMask == alphaMask) {
                formatInfo = &info.formatInfo;
                break;
            }
        }
    }

    if (formatInfo == nullptr) {
        qWarning() << "Unrecognized pixel format in" << source;
        return imageData;
    }

    // target
    // XXX should worry about Target1D?
    QOpenGLTexture::Target target;
    const int width = qFromLittleEndian(header.width);
    const int height = qFromLittleEndian(header.height);
    const quint32 caps2Flags = qFromLittleEndian(header.caps2);
    const int blockSize = formatInfo->components;
    const bool isCompressed = formatInfo->compressed;
    const int mipLevelCount = ((qFromLittleEndian(header.flags) & MipmapCountFlag) == MipmapCountFlag) ? qFromLittleEndian(header.mipmapCount) : 1;
    int depth;
    int faces;

    if ((caps2Flags & VolumeFlag) == VolumeFlag) {
        target = QOpenGLTexture::Target3D;
        depth = qFromLittleEndian(header.depth);
        faces = 1;
    } else if ((caps2Flags & CubemapFlag) == CubemapFlag) {
        target = layers > 1 ? QOpenGLTexture::TargetCubeMapArray : QOpenGLTexture::TargetCubeMap;
        depth = 1;
        faces = qPopulationCount(caps2Flags & AllCubemapFaceFlags);
    } else {
        target = layers > 1 ? QOpenGLTexture::Target2DArray : QOpenGLTexture::Target2D;
        depth = 1;
        faces = 1;
    }

    int layerSize = 0;
    int tmpSize = 0;

    auto computeMipMapLevelSize = [&] (int level) {
        const int w = qMax(width >> level, 1);
        const int h = qMax(height >> level, 1);
        const int d = qMax(depth >> level, 1);

        if (isCompressed)
            return ((w + 3) / 4) * ((h + 3) / 4) * blockSize * d;
        else
            return w * h * blockSize * d;
    };

    for (auto i = 0; i < mipLevelCount; ++i)
        tmpSize += computeMipMapLevelSize(i);

    layerSize = faces * tmpSize;

    // data
    const int dataSize = layers * layerSize;

    const QByteArray data = source->read(dataSize);
    if (data.size() < dataSize) {
        qWarning() << "Unexpected end of data in" << source;
        return imageData;
    }

    if (!source->atEnd())
        qWarning() << "Unrecognized data in" << source;

    imageData = Qt3DRender::QTextureImageDataPtr::create();
    imageData->setData(data,blockSize, isCompressed);

    // target
    imageData->setTarget(target);

    // mip levels
    // ### this is broken, mip levels > 0 will be ignored, see Q3DSImageManager::load()
    // should be refactored to return multiple QTextureImageDataPtrs, one per mip level.
    imageData->setMipLevels(mipLevelCount);

    // texture format
    imageData->setFormat(formatInfo->textureFormat);
    imageData->setPixelType(formatInfo->pixelType);
    imageData->setPixelFormat(formatInfo->pixelFormat);

    // dimensions
    imageData->setLayers(layers);
    imageData->setDepth(depth);
    imageData->setWidth(width);
    imageData->setHeight(height);
    imageData->setFaces(faces);

    return imageData;
}

static inline quint32 q3ds_alignedOffset(quint32 offset, quint32 byteAlign)
{
    return (offset + byteAlign - 1) & ~(byteAlign - 1);
}

static inline quint32 q3ds_blockSizeForTextureFormat(QOpenGLTexture::TextureFormat format)
{
    switch (format) {
    case QOpenGLTexture::RGB8_ETC1:
    case QOpenGLTexture::RGB8_ETC2:
    case QOpenGLTexture::SRGB8_ETC2:
    case QOpenGLTexture::RGB8_PunchThrough_Alpha1_ETC2:
    case QOpenGLTexture::SRGB8_PunchThrough_Alpha1_ETC2:
    case QOpenGLTexture::R11_EAC_UNorm:
    case QOpenGLTexture::R11_EAC_SNorm:
        return 8;

    default:
        return 16;
    }
}

inline QVector<Qt3DRender::QTextureImageDataPtr> q3ds_loadKtx(QIODevice *source)
{
    static const int KTX_IDENTIFIER_LENGTH = 12;
    static const char ktxIdentifier[KTX_IDENTIFIER_LENGTH] = { '\xAB', 'K', 'T', 'X', ' ', '1', '1', '\xBB', '\r', '\n', '\x1A', '\n' };
    static const quint32 platformEndianIdentifier = 0x04030201;
    static const quint32 inversePlatformEndianIdentifier = 0x01020304;

    struct KTXHeader {
        quint8 identifier[KTX_IDENTIFIER_LENGTH];
        quint32 endianness;
        quint32 glType;
        quint32 glTypeSize;
        quint32 glFormat;
        quint32 glInternalFormat;
        quint32 glBaseInternalFormat;
        quint32 pixelWidth;
        quint32 pixelHeight;
        quint32 pixelDepth;
        quint32 numberOfArrayElements;
        quint32 numberOfFaces;
        quint32 numberOfMipmapLevels;
        quint32 bytesOfKeyValueData;
    };

    KTXHeader header;
    QVector<Qt3DRender::QTextureImageDataPtr> result;
    if (source->read(reinterpret_cast<char *>(&header), sizeof(header)) != sizeof(header)
            || qstrncmp(reinterpret_cast<char *>(header.identifier), ktxIdentifier, KTX_IDENTIFIER_LENGTH) != 0
            || (header.endianness != platformEndianIdentifier && header.endianness != inversePlatformEndianIdentifier))
    {
        return result;
    }

    const bool isInverseEndian = (header.endianness == inversePlatformEndianIdentifier);
    auto decode = [isInverseEndian](quint32 val) {
        return isInverseEndian ? qbswap<quint32>(val) : val;
    };

    const bool isCompressed = decode(header.glType) == 0 && decode(header.glFormat) == 0 && decode(header.glTypeSize) == 1;
    if (!isCompressed) {
        qWarning("Uncompressed ktx texture data is not supported");
        return result;
    }

    if (decode(header.numberOfArrayElements) != 0) {
        qWarning("Array ktx textures not supported");
        return result;
    }

    if (decode(header.pixelDepth) != 0) {
        qWarning("Only 2D and cube ktx textures are supported");
        return result;
    }

    const int bytesToSkip = decode(header.bytesOfKeyValueData);
    if (source->read(bytesToSkip).count() != bytesToSkip) {
        qWarning("Unexpected end of ktx data");
        return result;
    }

    // now for each mipmap level we have (arrays and 3d textures not supported here)
    // uint32 imageSize
    // for each array element
    //   for each face
    //     for each z slice
    //       compressed data
    //     padding so that each face data starts at an offset that is a multiple of 4
    // padding so that each imageSize starts at an offset that is a multiple of 4

    QByteArray rawData = source->readAll();
    if (rawData.count() < 4) {
        qWarning("Unexpected end of ktx data");
        return result;
    }

    const char *basep = rawData.constData();
    const char *p = basep;
    const int level0Width = decode(header.pixelWidth);
    const int level0Height = decode(header.pixelHeight);
    int faceCount = decode(header.numberOfFaces);
    const int mipMapLevels = decode(header.numberOfMipmapLevels);
    auto createImageData = [=](const QByteArray &compressedData) {
        Qt3DRender::QTextureImageDataPtr imageData = Qt3DRender::QTextureImageDataPtr::create();
        imageData->setTarget(faceCount == 6 ? QOpenGLTexture::TargetCubeMap : QOpenGLTexture::Target2D);
        imageData->setFormat(QOpenGLTexture::TextureFormat(decode(header.glInternalFormat)));
        imageData->setWidth(level0Width);
        imageData->setHeight(level0Height);
        imageData->setLayers(1);
        imageData->setDepth(1);
        // separate textureimage per face and per mipmap
        imageData->setFaces(1);
        imageData->setMipLevels(1);
        imageData->setPixelFormat(QOpenGLTexture::NoSourceFormat);
        imageData->setPixelType(QOpenGLTexture::NoPixelType);
        imageData->setData(compressedData, q3ds_blockSizeForTextureFormat(imageData->format()), true);
        return imageData;
    };

    // ### proper support for cubemaps should be implemented at some point
    if (faceCount > 1) {
        qWarning("Multiple faces (cubemaps) not currently supported in ktx");
        faceCount = 1;
    }

    for (int mip = 0; mip < mipMapLevels; ++mip) {
        int imageSize = *reinterpret_cast<const quint32 *>(p);
        p += 4;
        for (int face = 0; face < faceCount; ++face) {
            result << createImageData(QByteArray(p, imageSize));
            p = basep + q3ds_alignedOffset(p + imageSize - basep, 4);
        }
    }

    return result;
}

QT_END_NAMESPACE

#endif // Q3DSIMAGELOADERS_P_H
