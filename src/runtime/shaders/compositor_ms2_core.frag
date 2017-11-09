#version 330 core

in vec2 texCoord;

uniform sampler2DMS tex;

out vec4 fragColor;

void main()
{
    ivec2 tc = ivec2(floor(textureSize(tex) * texCoord));
    vec4 c = texelFetch(tex, tc, 0) + texelFetch(tex, tc, 1);
    fragColor = c / 2.0;
}
