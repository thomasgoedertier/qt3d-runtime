attribute vec4 vertexPosition;
attribute vec2 vertexTexCoord;

varying vec2 texCoord;

uniform mat4 mvp;

void main()
{
    texCoord = vertexTexCoord;
    gl_Position = mvp * vertexPosition;
}
