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

#include "q3dsenummaps_p.h"

QT_BEGIN_NAMESPACE

// When mapping NoXxxx to both "None" and "", "None" should come first to help
// strFromEnum give more readable results.

static Q3DSEnumNameMap g_presentationRotationMap[] = {
    { Q3DSUipPresentation::NoRotation, "None" },
    { Q3DSUipPresentation::NoRotation, "" },
    { Q3DSUipPresentation::NoRotation, "NoRotation" },
    { Q3DSUipPresentation::Clockwise90, "90" },
    { Q3DSUipPresentation::Clockwise180, "180" },
    { Q3DSUipPresentation::Clockwise270, "270" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSUipPresentation::Rotation>::get()
{
    return g_presentationRotationMap;
}

static Q3DSEnumNameMap g_nodeRotationOrderMap[] = {
    { Q3DSNode::XYZ, "XYZ" },
    { Q3DSNode::YZX, "YZX" },
    { Q3DSNode::ZXY, "ZXY" },
    { Q3DSNode::XZY, "XZY" },
    { Q3DSNode::YXZ, "YXZ" },
    { Q3DSNode::ZYX, "ZYX" },
    { Q3DSNode::XYZr, "XYZr" },
    { Q3DSNode::YZXr, "YZXr" },
    { Q3DSNode::ZXYr, "ZXYr" },
    { Q3DSNode::XZYr, "XZYr" },
    { Q3DSNode::YXZr, "YXZr" },
    { Q3DSNode::ZYXr, "ZYXr" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSNode::RotationOrder>::get()
{
    return g_nodeRotationOrderMap;
}

static Q3DSEnumNameMap g_nodeOrientationMap[] = {
    { Q3DSNode::LeftHanded, "Left Handed" },
    { Q3DSNode::RightHanded, "Right Handed" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSNode::Orientation>::get()
{
    return g_nodeOrientationMap;
}

static Q3DSEnumNameMap g_slidePlayModeMap[] = {
    { Q3DSSlide::StopAtEnd, "Stop at end" },
    { Q3DSSlide::Looping, "Looping" },
    { Q3DSSlide::PingPong, "PingPong" },
    { Q3DSSlide::Ping, "Ping" },
    { Q3DSSlide::PlayThroughTo, "Play Through To..." },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSSlide::PlayMode>::get()
{
    return g_slidePlayModeMap;
}

static Q3DSEnumNameMap g_slideInitialPlayStateMap[] = {
    { Q3DSSlide::Play, "Play" },
    { Q3DSSlide::Pause, "Pause" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSSlide::InitialPlayState>::get()
{
    return g_slideInitialPlayStateMap;
}

static Q3DSEnumNameMap g_slidePlayThroughMap[] = {
    { Q3DSSlide::Next, "Next" },
    { Q3DSSlide::Previous, "Previous" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSSlide::PlayThrough>::get()
{
    return g_slidePlayThroughMap;
}

static Q3DSEnumNameMap g_animationTrackAnimationType[] = {
    { Q3DSAnimationTrack::NoAnimation, "None" },
    { Q3DSAnimationTrack::NoAnimation, "" },
    { Q3DSAnimationTrack::NoAnimation, "NoAnimation" },
    { Q3DSAnimationTrack::Linear, "Linear" },
    { Q3DSAnimationTrack::EaseInOut, "EaseInOut" },
    { Q3DSAnimationTrack::Bezier, "Bezier" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSAnimationTrack::AnimationType>::get()
{
    return g_animationTrackAnimationType;
}

static Q3DSEnumNameMap g_layerNodeProgressiveAA[] = {
    { Q3DSLayerNode::NoPAA, "None" },
    { Q3DSLayerNode::NoPAA, "" },
    { Q3DSLayerNode::PAA2x, "2x" },
    { Q3DSLayerNode::PAA4x, "4x" },
    { Q3DSLayerNode::PAA8x, "8x" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSLayerNode::ProgressiveAA>::get()
{
    return g_layerNodeProgressiveAA;
}

static Q3DSEnumNameMap g_layerNodeMultisampleAA[] = {
    { Q3DSLayerNode::NoMSAA, "None" },
    { Q3DSLayerNode::NoMSAA, "" },
    { Q3DSLayerNode::MSAA2x, "2x" },
    { Q3DSLayerNode::MSAA4x, "4x" },
    { Q3DSLayerNode::SSAA, "SSAA" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSLayerNode::MultisampleAA>::get()
{
    return g_layerNodeMultisampleAA;
}

static Q3DSEnumNameMap g_layerNodeLayerBackground[] = {
    { Q3DSLayerNode::Transparent, "Transparent" },
    { Q3DSLayerNode::SolidColor, "SolidColor" },
    { Q3DSLayerNode::Unspecified, "Unspecified" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSLayerNode::LayerBackground>::get()
{
    return g_layerNodeLayerBackground;
}

static Q3DSEnumNameMap g_layerNodeBlendType[] = {
    { Q3DSLayerNode::Normal, "Normal" },
    { Q3DSLayerNode::Screen, "Screen" },
    { Q3DSLayerNode::Multiply, "Multiply" },
    { Q3DSLayerNode::Add, "Add" },
    { Q3DSLayerNode::Subtract, "Subtract" },
    { Q3DSLayerNode::Overlay, "*Overlay" },
    { Q3DSLayerNode::ColorBurn, "*ColorBurn" },
    { Q3DSLayerNode::ColorDodge, "*ColorDodge" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSLayerNode::BlendType>::get()
{
    return g_layerNodeBlendType;
}

static Q3DSEnumNameMap g_LayerNodehorzfields[] = {
    { Q3DSLayerNode::LeftWidth, "Left/Width" },
    { Q3DSLayerNode::LeftRight, "Left/Right" },
    { Q3DSLayerNode::WidthRight, "Width/Right" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSLayerNode::HorizontalFields>::get()
{
    return g_LayerNodehorzfields;
}

static Q3DSEnumNameMap g_LayerNodeUnits[] = {
    { Q3DSLayerNode::Percent, "percent" },
    { Q3DSLayerNode::Pixels, "pixels" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSLayerNode::Units>::get()
{
    return g_LayerNodeUnits;
}

static Q3DSEnumNameMap g_LayerNodevertfields[] = {
    { Q3DSLayerNode::TopHeight, "Top/Height" },
    { Q3DSLayerNode::TopBottom, "Top/Bottom" },
    { Q3DSLayerNode::HeightBottom, "Height/Bottom" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSLayerNode::VerticalFields>::get()
{
    return g_LayerNodevertfields;
}

static Q3DSEnumNameMap g_Imagemappingmode[] = {
    { Q3DSImage::UVMapping, "UV Mapping" },
    { Q3DSImage::EnvironmentalMapping, "Environmental Mapping" },
    { Q3DSImage::LightProbe, "Light Probe" },
    { Q3DSImage::IBLOverride, "IBL Override" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSImage::MappingMode>::get()
{
    return g_Imagemappingmode;
}

static Q3DSEnumNameMap g_Imagetilingmode[] = {
    { Q3DSImage::Tiled, "Tiled" },
    { Q3DSImage::Mirrored, "Mirrored" },
    { Q3DSImage::NoTiling, "No Tiling" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSImage::TilingMode>::get()
{
    return g_Imagetilingmode;
}

static Q3DSEnumNameMap g_ModelNodetessellation[] = {
    { Q3DSModelNode::None, "None" },
    { Q3DSModelNode::Linear, "Linear" },
    { Q3DSModelNode::Phong, "Phong" },
    { Q3DSModelNode::NPatch, "NPatch" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSModelNode::Tessellation>::get()
{
    return g_ModelNodetessellation;
}

static Q3DSEnumNameMap g_CameraNodescalemode[] = {
    { Q3DSCameraNode::SameSize, "Same Size" },
    { Q3DSCameraNode::Fit, "Fit" },
    { Q3DSCameraNode::FitHorizontal, "Fit Horizontal" },
    { Q3DSCameraNode::FitVertical, "Fit Vertical" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSCameraNode::ScaleMode>::get()
{
    return g_CameraNodescalemode;
}

static Q3DSEnumNameMap g_CameraNodescaleanchor[] = {
    { Q3DSCameraNode::Center, "Center" },
    { Q3DSCameraNode::N, "N" },
    { Q3DSCameraNode::NE, "NE" },
    { Q3DSCameraNode::E, "E" },
    { Q3DSCameraNode::SE, "SE" },
    { Q3DSCameraNode::S, "S" },
    { Q3DSCameraNode::SW, "SW" },
    { Q3DSCameraNode::W, "W" },
    { Q3DSCameraNode::NW, "NW" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSCameraNode::ScaleAnchor>::get()
{
    return g_CameraNodescaleanchor;
}

static Q3DSEnumNameMap g_LightNodelighttype[] = {
    { Q3DSLightNode::Directional, "Directional" },
    { Q3DSLightNode::Point, "Point" },
    { Q3DSLightNode::Area, "Area" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSLightNode::LightType>::get()
{
    return g_LightNodelighttype;
}

static Q3DSEnumNameMap g_MaterialNodeshaderlighting[] = {
    { Q3DSDefaultMaterial::PixelShaderLighting, "Pixel" },
    { Q3DSDefaultMaterial::NoShaderLighting, "None" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSDefaultMaterial::ShaderLighting>::get()
{
    return g_MaterialNodeshaderlighting;
}

static Q3DSEnumNameMap g_MaterialNodeblendmode[] = {
    { Q3DSDefaultMaterial::Normal, "Normal" },
    { Q3DSDefaultMaterial::Screen, "Screen" },
    { Q3DSDefaultMaterial::Multiply, "Multiply" },
    { Q3DSDefaultMaterial::Overlay, "*Overlay" },
    { Q3DSDefaultMaterial::ColorBurn, "*ColorBurn" },
    { Q3DSDefaultMaterial::ColorDodge, "*ColorDodge" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSDefaultMaterial::BlendMode>::get()
{
    return g_MaterialNodeblendmode;
}

static Q3DSEnumNameMap g_MaterialNodespecularmodel[] = {
    { Q3DSDefaultMaterial::DefaultSpecularModel, "Default" },
    { Q3DSDefaultMaterial::KGGX, "KGGX" },
    { Q3DSDefaultMaterial::KWard, "KWard" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSDefaultMaterial::SpecularModel>::get()
{
    return g_MaterialNodespecularmodel;
}

static Q3DSEnumNameMap g_TextNodehorzalign[] = {
    { Q3DSTextNode::Left, "Left" },
    { Q3DSTextNode::Center, "Center" },
    { Q3DSTextNode::Right, "Right" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSTextNode::HorizontalAlignment>::get()
{
    return g_TextNodehorzalign;
}

static Q3DSEnumNameMap g_TextNodevertalign[] = {
    { Q3DSTextNode::Top, "Top" },
    { Q3DSTextNode::Middle, "Middle" },
    { Q3DSTextNode::Bottom, "Bottom" },
    { 0, nullptr }
};

Q3DSEnumNameMap *Q3DSEnumParseMap<Q3DSTextNode::VerticalAlignment>::get()
{
    return g_TextNodevertalign;
}

QT_END_NAMESPACE
