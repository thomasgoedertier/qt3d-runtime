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
#include <QFileInfo>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <Qt3DRender/QTextureImageDataGenerator>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcScene)
Q_DECLARE_LOGGING_CATEGORY(lcPerf)

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
    Q3DSTextureImageDataGen(const QUrl &source, const Qt3DRender::QTextureImageDataPtr &data)
        : m_source(source),
          m_metadata(data)
    { }

    Qt3DRender::QTextureImageDataPtr operator()() override
    {
        return m_metadata;
    }

    bool operator==(const Qt3DRender::QTextureImageDataGenerator &other) const override
    {
        const Q3DSTextureImageDataGen *otherFunctor = functor_cast<Q3DSTextureImageDataGen>(&other);
        return otherFunctor && otherFunctor->m_source == m_source;
    }

    QT3D_FUNCTOR(Q3DSTextureImageDataGen)

private:
    QUrl m_source;
    Qt3DRender::QTextureImageDataPtr m_metadata;
};

class TextureImage : public Qt3DRender::QAbstractTextureImage
{
public:
    TextureImage(const QUrl &source, const Qt3DRender::QTextureImageDataPtr &data)
    {
        m_gen = QSharedPointer<Q3DSTextureImageDataGen>::create(source, data);
    }

private:
    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const override { return m_gen; }

    Qt3DRender::QTextureImageDataGeneratorPtr m_gen;
};

Qt3DRender::QTextureImageDataPtr Q3DSImageManager::load(const QUrl &source, bool *wasCached)
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

    if (data) {
        m_cache.insert(sourceStr, data);
        qCDebug(lcPerf, "Image loaded in %lld ms", t.elapsed());
        return data;
    } else {
        qCDebug(lcScene, "Failed to load image");
    }

    return Qt3DRender::QTextureImageDataPtr();
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

    Qt3DRender::QTextureImageDataPtr imageData = load(source, &info.wasCached);

    info.size = QSize(imageData->width(), imageData->height());
    info.format = Qt3DRender::QAbstractTexture::TextureFormat(imageData->format());
    m_metadata.insert(tex, info);

    if (imageData)
        tex->addTextureImage(new TextureImage(source, imageData));

    if (info.flags.testFlag(GenerateMipMapsForIBL)) {
        // ### to be replaced with manual mipmap generation
        tex->setGenerateMipMaps(true);
        tex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        tex->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapLinear);
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

QT_END_NAMESPACE
