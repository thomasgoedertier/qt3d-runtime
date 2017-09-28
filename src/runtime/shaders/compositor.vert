attribute vec4 vertexPosition;
attribute vec2 vertexTexCoord;

varying vec2 texCoord;

uniform mat4 modelMatrix;

void main()
{
    texCoord = vertexTexCoord;
    gl_Position = modelMatrix * vertexPosition;
}
