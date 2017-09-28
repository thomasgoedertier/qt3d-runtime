#version 330 core

in vec4 vertexPosition;
in vec2 vertexTexCoord;

out vec2 texCoord;

uniform mat4 mvp;

void main()
{
    texCoord = vertexTexCoord;
    gl_Position = mvp * vertexPosition;
}
