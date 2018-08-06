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

#ifndef Q3DSIMAGEMANAGERER_P_H
#define Q3DSIMAGEMANAGERER_P_H

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
#include <QHash>
#include <QUrl>
#include <QImage>
#include <Qt3DRender/QAbstractTexture>
#include <Qt3DRender/QTextureImageData>

QT_BEGIN_NAMESPACE

class Q3DSProfiler;

namespace Qt3DCore {
class QEntity;
}

class Q3DSImageManager
{
public:
    enum ImageFlag {
        GenerateMipMapsForIBL = 0x01
    };
    Q_DECLARE_FLAGS(ImageFlags, ImageFlag)

    static Q3DSImageManager &instance();
    void invalidate();

    Qt3DRender::QAbstractTexture *newTextureForImage(Qt3DCore::QEntity *parent,
                                                     ImageFlags flags,
                                                     Q3DSProfiler *profiler = nullptr, const char *profName = nullptr, ...);
    void setSource(Qt3DRender::QAbstractTexture *tex, const QUrl &source);
    void setSource(Qt3DRender::QAbstractTexture *tex, const QImage &image);

    QSize size(Qt3DRender::QAbstractTexture *tex) const;
    Qt3DRender::QAbstractTexture::TextureFormat format(Qt3DRender::QAbstractTexture *tex) const;
    bool wasCached(Qt3DRender::QAbstractTexture *tex) const;

    qint64 ioTimeMsecs() const { return m_ioTime; }
    qint64 iblTimeMsecs() const { return m_iblTime; }

private:
    QVector<Qt3DRender::QTextureImageDataPtr> load(const QUrl &source, ImageFlags flags, bool *wasCached);
    int blockSizeForFormat(QOpenGLTexture::TextureFormat format);
    QByteArray generateIblMip(int w, int h, int prevW, int prevH,
                              QOpenGLTexture::TextureFormat format,
                              int blockSize, const QByteArray &prevLevelData);

    struct TextureInfo {
        ImageFlags flags;
        QUrl source;
        QSize size;
        Qt3DRender::QAbstractTexture::TextureFormat format = Qt3DRender::QAbstractTexture::NoFormat;
        bool wasCached = false;
    };

    QHash<Qt3DRender::QAbstractTexture *, TextureInfo> m_metadata;
    QHash<QString, QVector<Qt3DRender::QTextureImageDataPtr> > m_cache;
    qint64 m_ioTime = 0;
    qint64 m_iblTime = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSImageManager::ImageFlags)

QT_END_NAMESPACE

#endif // Q3DSIMAGEMANAGERER_P_H
