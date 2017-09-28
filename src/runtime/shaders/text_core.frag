#version 330 core

in vec2 texCoord;

uniform sampler2D tex;
uniform float opacity;
uniform vec3 color;

out vec4 fragColor;

void main()
{
    vec4 c = texture(tex, vec2(texCoord.x, 1.0 - texCoord.y));
    fragColor = vec4(c.rgb * color, c.a) * opacity;
}
