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

#include "q3dsimagemanager_p.h"
#include "q3dsimageloaders_p.h"
#include "q3dsprofiler_p.h"
#include "q3dslogging_p.h"
#include <qmath.h>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QTextureImageDataGenerator>
#include <Qt3DRender/QTexture>

QT_BEGIN_NAMESPACE

Q3DSImageManager &Q3DSImageManager::instance()
{
    static Q3DSImageManager mgr;
    return mgr;
}

void Q3DSImageManager::invalidate()
{
    m_metadata.clear();
    m_cache.clear();
    m_ioTime = 0;
    m_iblTime = 0;
}

Qt3DRender::QAbstractTexture *Q3DSImageManager::newTextureForImageFile(Qt3DCore::QEntity *parent,
                                                                       ImageFlags flags,
                                                                       Q3DSProfiler *profiler, const char *profDesc, ...)
{
    auto tex = new Qt3DRender::QTexture2D(parent);

    TextureInfo info;
    info.flags = flags;
    m_metadata.insert(tex, info);

    if (profiler && profDesc) {
        va_list ap;
        va_start(ap, profDesc);
        profiler->vtrackNewObject(tex, Q3DSProfiler::Texture2DObject, profDesc, ap);
        va_end(ap);
    }

    return tex;
}

class Q3DSTextureImageDataGen : public Qt3DRender::QTextureImageDataGenerator
{
public:
    Q3DSTextureImageDataGen(const QUrl &source, int mipLevel, const Qt3DRender::QTextureImageDataPtr &data)
        : m_source(source),
          m_mipLevel(mipLevel),
          m_data(data)
    { }

    Qt3DRender::QTextureImageDataPtr operator()() override
    {
        return m_data;
    }

    bool operator==(const Qt3DRender::QTextureImageDataGenerator &other) const override
    {
        const Q3DSTextureImageDataGen *otherFunctor = functor_cast<Q3DSTextureImageDataGen>(&other);
        return otherFunctor && otherFunctor->m_source == m_source && otherFunctor->m_mipLevel == m_mipLevel;
    }

    QT3D_FUNCTOR(Q3DSTextureImageDataGen)

private:
    QUrl m_source;
    int m_mipLevel;
    Qt3DRender::QTextureImageDataPtr m_data;
};

class TextureImage : public Qt3DRender::QAbstractTextureImage
{
public:
    TextureImage(const QUrl &source, int mipLevel, const Qt3DRender::QTextureImageDataPtr &data)
    {
        setMipLevel(mipLevel);
        m_gen = QSharedPointer<Q3DSTextureImageDataGen>::create(source, mipLevel, data);
    }

private:
    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const override { return m_gen; }

    Qt3DRender::QTextureImageDataGeneratorPtr m_gen;
};

QVector<Qt3DRender::QTextureImageDataPtr> Q3DSImageManager::load(const QUrl &source, ImageFlags flags, bool *wasCached)
{
    const QString sourceStr = source.toLocalFile();
    auto it = m_cache.constFind(sourceStr);
    if (it != m_cache.constEnd()) {
        *wasCached = true;
        return *it;
    }

    *wasCached = false;

    QElapsedTimer t;
    t.start();
    qCDebug(lcScene, "Loading image %s", qPrintable(sourceStr));

    Qt3DRender::QTextureImageDataPtr data;

    const QString suffix = QFileInfo(sourceStr).suffix().toLower();
    if (suffix == QStringLiteral("hdr")) {
        QFile f(sourceStr);
        if (f.open(QIODevice::ReadOnly)) {
            data = q3ds_loadHdr(&f);
            f.close();
        }
    } else if (suffix == QStringLiteral("pkm")) {
        QFile f(sourceStr);
        if (f.open(QIODevice::ReadOnly)) {
            data = q3ds_loadPkm(&f);
            f.close();
        }
    } else if (suffix == QStringLiteral("dds")) {
        QFile f(sourceStr);
        if (f.open(QIODevice::ReadOnly)) {
            data = q3ds_loadDds(&f);
            f.close();
        }
    }

    if (!data) {
        QImage image(sourceStr);
        if (!image.isNull()) {
            data = Qt3DRender::QTextureImageDataPtr::create();
            data->setImage(image.mirrored());
        }
    }

    m_ioTime += t.elapsed();

    QVector<Qt3DRender::QTextureImageDataPtr> result;
    if (data) {
        qCDebug(lcPerf, "Image loaded in %lld ms", t.elapsed());
        result << data;

        if (flags.testFlag(GenerateMipMapsForIBL)) {
            // IBL needs special mipmap generation. This could be done
            // asynchronously but the we rely on the previous level in each step so
            // it's not a good fit unfortunately. So do it all here. Also,
            // QTextureGenerator could provide all mipmaps in one go in one blob,
            // but there's no public API for that, have to stick with
            // QTextureImageDataGenerator.
            t.restart();
            int w = data->width();
            int h = data->height();
            const int maxDim = w > h ? w : h;
            const int maxMipLevel = int(qLn(maxDim) / qLn(2.0f));
            // silly QTextureImageData does not expose blockSize
            const int blockSize = blockSizeForFormat(data->format());
            QByteArray prevLevelData = data->data();
            for (int i = 1; i <= maxMipLevel; ++i) {
                int prevW = w;
                int prevH = h;
                w >>= 1;
                h >>= 1;
                w = qMax(1, w);
                h = qMax(1, h);

                auto mipImageData = Qt3DRender::QTextureImageDataPtr::create();
                mipImageData->setTarget(QOpenGLTexture::Target2D);
                mipImageData->setFormat(data->format());
                mipImageData->setWidth(w);
                mipImageData->setHeight(h);
                mipImageData->setLayers(1);
                mipImageData->setDepth(1);
                mipImageData->setFaces(1);
                mipImageData->setMipLevels(1);
                mipImageData->setPixelFormat(data->pixelFormat());
                mipImageData->setPixelType(data->pixelType());

                QByteArray mipData = generateIblMip(w, h, prevW, prevH, mipImageData->format(), blockSize, prevLevelData);
                mipImageData->setData(mipData, blockSize, false);
                result << mipImageData;
                prevLevelData = mipData;
            }
            m_iblTime += t.elapsed();
            qCDebug(lcPerf, "Generated %d IBL mip levels in %lld ms", maxMipLevel, t.elapsed());
        }

        m_cache.insert(sourceStr, result);
    } else {
        qCDebug(lcScene, "Failed to load image");
    }

    return result;
}

void Q3DSImageManager::setSource(Qt3DRender::QAbstractTexture *tex, const QUrl &source)
{
    TextureInfo info;
    auto it = m_metadata.find(tex);
    if (it != m_metadata.end()) {
        if (it->source == source)
            return;
        info = *it;
    }

    info.source = source;

    // yes, it's all synchronous and this is intentional. The generator
    // (invoked from some Qt3D job thread later on) will just return the already
    // loaded data.
    QVector<Qt3DRender::QTextureImageDataPtr> imageData = load(source, info.flags, &info.wasCached);

    if (!imageData.isEmpty()) {
        info.size = QSize(imageData[0]->width(), imageData[0]->height());
        info.format = Qt3DRender::QAbstractTexture::TextureFormat(imageData[0]->format());
        m_metadata.insert(tex, info);

        if (info.flags.testFlag(GenerateMipMapsForIBL)) {
            tex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
            tex->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapLinear);
            tex->setGenerateMipMaps(false);
        }

        for (int i = 0; i < imageData.count(); ++i)
            tex->addTextureImage(new TextureImage(source, i, imageData[i]));
    } else {
        // Provide a dummy image when failing to load since we want to see
        // something that makes it obvious a texture source file was missing.
        info.size = QSize(64, 64);
        info.format = Qt3DRender::QAbstractTexture::RGBA8_UNorm;
        m_metadata.insert(tex, info);

        QImage dummy(info.size, QImage::Format_ARGB32_Premultiplied);
        dummy.fill(Qt::magenta);
        auto dummyData = Qt3DRender::QTextureImageDataPtr::create();
        dummyData->setImage(dummy);

        tex->addTextureImage(new TextureImage(source, 0, dummyData));
        qWarning("Using placeholder texture in place of %s", qPrintable(source.toLocalFile()));
    }
}

// The metadata getters must work also with textures not registered to the
// imagemanager. Just fall back to the texture itself then (which is safe for
// textures created with a size, not sourced from a file; for images loaded
// from a file this is not an option due to QTBUG-65775)

QSize Q3DSImageManager::size(Qt3DRender::QAbstractTexture *tex) const
{
    auto it = m_metadata.constFind(tex);
    if (it != m_metadata.cend())
        return it->size;

    return QSize(tex->width(), tex->height());
}

Qt3DRender::QAbstractTexture::TextureFormat Q3DSImageManager::format(Qt3DRender::QAbstractTexture *tex) const
{
    auto it = m_metadata.constFind(tex);
    if (it != m_metadata.cend())
        return it->format;

    return tex->format();
}

bool Q3DSImageManager::wasCached(Qt3DRender::QAbstractTexture *tex) const
{
    auto it = m_metadata.constFind(tex);
    if (it != m_metadata.cend())
        return it->wasCached;

    return false;
}

// IBL mipmap generation (BSDF prefiltering), adapted from 3DS1

int Q3DSImageManager::blockSizeForFormat(QOpenGLTexture::TextureFormat format)
{
    switch (format) {
    case QOpenGLTexture::R8_UNorm:
        return 1;
    case QOpenGLTexture::R16F:
        return 2;
    case QOpenGLTexture::R16_UNorm:
        return 2;
    case QOpenGLTexture::R32I:
        return 4;
    case QOpenGLTexture::R32F:
        return 4;
    case QOpenGLTexture::RGBA8_UNorm:
        return 4;
    case QOpenGLTexture::RGB8_UNorm:
        return 3;
    case QOpenGLTexture::R5G6B5:
        return 2;
    case QOpenGLTexture::RGB5A1:
        return 2;
    case QOpenGLTexture::AlphaFormat:
        return 1;
    case QOpenGLTexture::LuminanceFormat:
        return 1;
    case QOpenGLTexture::LuminanceAlphaFormat:
        return 1;
    case QOpenGLTexture::D16:
        return 2;
    case QOpenGLTexture::D24:
        return 3;
    case QOpenGLTexture::D32:
        return 4;
    case QOpenGLTexture::D24S8:
        return 4;
    case QOpenGLTexture::RGB9E5:
        return 4;
    case QOpenGLTexture::SRGB8:
        return 3;
    case QOpenGLTexture::SRGB8_Alpha8:
        return 4;
    case QOpenGLTexture::RGBA16F:
        return 8;
    case QOpenGLTexture::RG16F:
        return 4;
    case QOpenGLTexture::RG32F:
        return 8;
    case QOpenGLTexture::RGBA32F:
        return 16;
    case QOpenGLTexture::RGB32F:
        return 12;
    case QOpenGLTexture::RG11B10F:
        return 4;
    default:
        break;
    }
    Q_UNREACHABLE();
    return 0;
}

static inline int wrapMod(int a, int base)
{
    return (a >= 0) ? a % base : (a % base) + base;
}

static inline void getWrappedCoords(int &sX, int &sY, int width, int height)
{
    if (sY < 0) {
        sX -= width >> 1;
        sY = -sY;
    }
    if (sY >= height) {
        sX += width >> 1;
        sY = height - sY;
    }
    sX = wrapMod(sX, width);
}

static inline void decodeToFloat(const void *inPtr, int byteOfs, float *outPtr,
                                 QOpenGLTexture::TextureFormat format, int blockSize)
{
    outPtr[0] = 0.0f;
    outPtr[1] = 0.0f;
    outPtr[2] = 0.0f;
    outPtr[3] = 0.0f;
    const uchar *src = reinterpret_cast<const uchar *>(inPtr);
    // float divisor; // If we want to support RGBD?
    switch (format) {
    case QOpenGLTexture::AlphaFormat:
        outPtr[0] = ((float)src[byteOfs]) / 255.0f;
        break;

    case QOpenGLTexture::LuminanceFormat:
    case QOpenGLTexture::LuminanceAlphaFormat:
    case QOpenGLTexture::R8_UNorm:
    case QOpenGLTexture::RG8_UNorm:
    case QOpenGLTexture::RGB8_UNorm:
    case QOpenGLTexture::RGBA8_UNorm:
    case QOpenGLTexture::SRGB8:
    case QOpenGLTexture::SRGB8_Alpha8:
        // NOTE : RGBD Hack here for reference.  Not meant for installation.
        // divisor = (blockSize == 4) ? ((float)src[byteOfs+3]) / 255.0f : 1.0f;
        for (int i = 0; i < blockSize; ++i) {
            float val = ((float)src[byteOfs + i]) / 255.0f;
            outPtr[i] = (i < 3) ? qPow(val, 0.4545454545f) : val;
            // Assuming RGBA8 actually means RGBD (which is stupid, I know)
            // if ( blockSize == 4 ) { outPtr[i] /= divisor; }
        }
        // outPtr[3] = divisor;
        break;

    case QOpenGLTexture::R32F:
        outPtr[0] = reinterpret_cast<const float *>(src + byteOfs)[0];
        break;
    case QOpenGLTexture::RG32F:
        outPtr[0] = reinterpret_cast<const float *>(src + byteOfs)[0];
        outPtr[1] = reinterpret_cast<const float *>(src + byteOfs)[1];
        break;
    case QOpenGLTexture::RGBA32F:
        outPtr[0] = reinterpret_cast<const float *>(src + byteOfs)[0];
        outPtr[1] = reinterpret_cast<const float *>(src + byteOfs)[1];
        outPtr[2] = reinterpret_cast<const float *>(src + byteOfs)[2];
        outPtr[3] = reinterpret_cast<const float *>(src + byteOfs)[3];
        break;
    case QOpenGLTexture::RGB32F:
        outPtr[0] = reinterpret_cast<const float *>(src + byteOfs)[0];
        outPtr[1] = reinterpret_cast<const float *>(src + byteOfs)[1];
        outPtr[2] = reinterpret_cast<const float *>(src + byteOfs)[2];
        break;

    case QOpenGLTexture::R16F:
    case QOpenGLTexture::RG16F:
    case QOpenGLTexture::RGBA16F:
        for (int i = 0; i < (blockSize >> 1); ++i) {
            // NOTE : This only works on the assumption that we don't have any denormals,
            // Infs or NaNs.
            // Every pixel in our source image should be "regular"
            quint16 h = reinterpret_cast<const quint16 *>(src + byteOfs)[i];
            quint32 sign = (h & 0x8000) << 16;
            quint32 exponent = (((((h & 0x7c00) >> 10) - 15) + 127) << 23);
            quint32 mantissa = ((h & 0x3ff) << 13);
            quint32 result = sign | exponent | mantissa;
            // Special case for zero and negative zero
            if (h == 0 || h == 0x8000)
                result = 0;
            memcpy(reinterpret_cast<quint32 *>(outPtr) + i, &result, 4);
        }
        break;

    case QOpenGLTexture::RG11B10F:
        // place holder
        Q_UNREACHABLE();
        break;

    default:
        outPtr[0] = 0.0f;
        outPtr[1] = 0.0f;
        outPtr[2] = 0.0f;
        outPtr[3] = 0.0f;
        break;
    }
}

static inline void encodeToPixel(float *inPtr, void *outPtr, int byteOfs,
                                 QOpenGLTexture::TextureFormat format, int blockSize)
{
    uchar *dest = reinterpret_cast<uchar *>(outPtr);
    switch (format) {
    case QOpenGLTexture::AlphaFormat:
        dest[byteOfs] = uchar(inPtr[0] * 255.0f);
        break;

    case QOpenGLTexture::LuminanceFormat:
    case QOpenGLTexture::LuminanceAlphaFormat:
    case QOpenGLTexture::R8_UNorm:
    case QOpenGLTexture::RG8_UNorm:
    case QOpenGLTexture::RGB8_UNorm:
    case QOpenGLTexture::RGBA8_UNorm:
    case QOpenGLTexture::SRGB8:
    case QOpenGLTexture::SRGB8_Alpha8:
        for (int i = 0; i < blockSize; ++i) {
            inPtr[i] = (inPtr[i] > 1.0f) ? 1.0f : inPtr[i];
            if (i < 3)
                dest[byteOfs + i] = uchar(qPow(inPtr[i], 2.2f) * 255.0f);
            else
                dest[byteOfs + i] = uchar(inPtr[i] * 255.0f);
        }
        break;

    case QOpenGLTexture::R32F:
        reinterpret_cast<float *>(dest + byteOfs)[0] = inPtr[0];
        break;
    case QOpenGLTexture::RG32F:
        reinterpret_cast<float *>(dest + byteOfs)[0] = inPtr[0];
        reinterpret_cast<float *>(dest + byteOfs)[1] = inPtr[1];
        break;
    case QOpenGLTexture::RGBA32F:
        reinterpret_cast<float *>(dest + byteOfs)[0] = inPtr[0];
        reinterpret_cast<float *>(dest + byteOfs)[1] = inPtr[1];
        reinterpret_cast<float *>(dest + byteOfs)[2] = inPtr[2];
        reinterpret_cast<float *>(dest + byteOfs)[3] = inPtr[3];
        break;
    case QOpenGLTexture::RGB32F:
        reinterpret_cast<float *>(dest + byteOfs)[0] = inPtr[0];
        reinterpret_cast<float *>(dest + byteOfs)[1] = inPtr[1];
        reinterpret_cast<float *>(dest + byteOfs)[2] = inPtr[2];
        break;

    case QOpenGLTexture::R16F:
    case QOpenGLTexture::RG16F:
    case QOpenGLTexture::RGBA16F:
        for (int i = 0; i < (blockSize >> 1); ++i) {
            // NOTE : This also has the limitation of not handling  infs, NaNs and
            // denormals, but it should be
            // sufficient for our purposes.
            if (inPtr[i] > 65519.0f)
                inPtr[i] = 65519.0f;
            if (qAbs(inPtr[i]) < 6.10352E-5f)
                inPtr[i] = 0.0f;
            quint32 f = reinterpret_cast<quint32 *>(inPtr)[i];
            quint32 sign = (f & 0x80000000) >> 16;
            qint32 exponent = (f & 0x7f800000) >> 23;
            quint32 mantissa = (f >> 13) & 0x3ff;
            exponent = exponent - 112;
            if (exponent > 31)
                exponent = 31;
            if (exponent < 0)
                exponent = 0;
            exponent = exponent << 10;
            reinterpret_cast<quint16 *>(dest + byteOfs)[i] = quint16(sign | exponent | mantissa);
        }
        break;

    case QOpenGLTexture::RG11B10F:
        // place holder
        Q_UNREACHABLE();
        break;

    default:
        dest[byteOfs] = 0;
        dest[byteOfs + 1] = 0;
        dest[byteOfs + 2] = 0;
        dest[byteOfs + 3] = 0;
        break;
    }
}

QByteArray Q3DSImageManager::generateIblMip(int w, int h, int prevW, int prevH,
                                            QOpenGLTexture::TextureFormat format,
                                            int blockSize, const QByteArray &prevLevelData)
{
    QByteArray data;
    data.resize(w * h * blockSize);
    char *p = data.data();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float accumVal[4];
            accumVal[0] = 0;
            accumVal[1] = 0;
            accumVal[2] = 0;
            accumVal[3] = 0;
            for (int sy = -2; sy <= 2; ++sy) {
                for (int sx = -2; sx <= 2; ++sx) {
                    int sampleX = sx + (x << 1);
                    int sampleY = sy + (y << 1);
                    getWrappedCoords(sampleX, sampleY, prevW, prevH);

                    // Cauchy filter (this is simply because it's the easiest to evaluate, and
                    // requires no complex
                    // functions).
                    float filterPdf = 1.f / (1.f + float(sx * sx + sy * sy) * 2.f);
                    // With FP HDR formats, we're not worried about intensity loss so much as
                    // unnecessary energy gain,
                    // whereas with LDR formats, the fear with a continuous normalization factor is
                    // that we'd lose
                    // intensity and saturation as well.
                    filterPdf /= (blockSize >= 8)
                            ? 4.71238898f
                            : 4.5403446f;
                    // filterPdf /= 4.5403446f; // Discrete normalization factor
                    // filterPdf /= 4.71238898f; // Continuous normalization factor
                    float curPix[4];
                    int byteOffset = (sampleY * prevW + sampleX) * blockSize;
                    if (byteOffset < 0) {
                        sampleY = prevH + sampleY;
                        byteOffset = (sampleY * prevW + sampleX) * blockSize;
                    }

                    decodeToFloat(prevLevelData.constData(), byteOffset, curPix, format, blockSize);

                    accumVal[0] += filterPdf * curPix[0];
                    accumVal[1] += filterPdf * curPix[1];
                    accumVal[2] += filterPdf * curPix[2];
                    accumVal[3] += filterPdf * curPix[3];
                }
            }

            int newIdx = (y * w + x) * blockSize;
            encodeToPixel(accumVal, p, newIdx, format, blockSize);
        }
    }

    return data;
}

QT_END_NAMESPACE
