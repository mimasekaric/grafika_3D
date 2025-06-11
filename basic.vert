#version 330 core

layout(location = 0) in vec3 inPos;

uniform mat4 uM;
uniform mat4 uV;
uniform mat4 uP;

void main()
{
    gl_Position = uP * uV * uM * vec4(inPos, 1.0);
}
