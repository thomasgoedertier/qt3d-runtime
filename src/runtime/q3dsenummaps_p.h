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

#ifndef Q3DSENUMMAPS_P_H
#define Q3DSENUMMAPS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "q3dspresentation_p.h"

QT_BEGIN_NAMESPACE

struct Q3DSEnumNameMap
{
    int value;
    const char *str;
};

template <typename T>
struct Q3DSEnumParseMap
{
};

template <>
struct Q3DSEnumParseMap<Q3DSPresentation::Rotation>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSNode::RotationOrder>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSNode::Orientation>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSSlide::PlayMode>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSSlide::InitialPlayState>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSSlide::PlayThrough>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSAnimationTrack::AnimationType>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSLayerNode::ProgressiveAA>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSLayerNode::MultisampleAA>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSLayerNode::LayerBackground>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSLayerNode::BlendType>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSLayerNode::HorizontalFields>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSLayerNode::Units>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSLayerNode::VerticalFields>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSImage::MappingMode>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSImage::TilingMode>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSModelNode::Tessellation>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSCameraNode::ScaleMode>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSCameraNode::ScaleAnchor>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSLightNode::LightType>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSDefaultMaterial::ShaderLighting>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSDefaultMaterial::BlendMode>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSDefaultMaterial::SpecularModel>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSTextNode::HorizontalAlignment>
{
    static Q3DSEnumNameMap *get();
};

template <>
struct Q3DSEnumParseMap<Q3DSTextNode::VerticalAlignment>
{
    static Q3DSEnumNameMap *get();
};

class Q3DSEnumMap
{
public:
    template <typename T>
    static bool enumFromStr(const QStringRef &str, T *v) {
        QByteArray ba = str.toUtf8();
        Q3DSEnumNameMap *nameMap = Q3DSEnumParseMap<T>::get();
        for ( ; nameMap->str; ++nameMap) {
            if (!strcmp(nameMap->str, ba.constData())) {
                *v = static_cast<T>(nameMap->value);
                return true;
            }
        }
        return false;
    }
    template <typename T>
    static const char *strFromEnum(T v) {
        Q3DSEnumNameMap *nameMap = Q3DSEnumParseMap<T>::get();
        for ( ; nameMap->str; ++nameMap) {
            if (nameMap->value == v)
                return nameMap->str;
        }
        return nullptr;
    }
};

QT_END_NAMESPACE

#endif // Q3DSENUMMAPS_P_H
