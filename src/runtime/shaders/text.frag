precision highp float;

varying vec2 texCoord;

uniform sampler2D tex;
uniform float opacity;
uniform vec3 color;

void main()
{
    vec4 c = texture2D(tex, vec2(texCoord.x, 1.0 - texCoord.y));
    gl_FragColor = vec4(c.aaa * color, c.a) * opacity;
}
