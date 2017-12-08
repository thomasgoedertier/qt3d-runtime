#version 330 core

in vec2 texCoord;

uniform sampler2D tex;

out vec4 fragColor;

void main()
{
    vec4 c = texture(tex, texCoord);
    // This discard, while not necessarily ideal for some GPUs, is necessary to
    // get correct results with certain layer blend modes for example.
    if (c.a == 0.0)
        discard;
    fragColor = c;
}
