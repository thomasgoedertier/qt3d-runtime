precision highp float;

varying vec2 texCoord;

uniform sampler2D tex;

void main()
{
    vec4 c = texture2D(tex, texCoord);
    // This discard, while not necessarily ideal for some GPUs, is necessary to
    // get correct results with certain layer blend modes for example.
    if (c.a == 0.0)
        discard;
    gl_FragColor = c;
}
