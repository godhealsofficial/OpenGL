//vertex
#version 330 core
layout(location = 0) in vec4 vertex;
layout(location = 1) in vec4 normal;
layout(location = 2) in vec2 texCoords;
out vec2 vTexCoords;
out vec3 vFragPosition;
out vec3 vNormal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
gl_Position = projection * view * model
* vertex;
vTexCoords = texCoords;
vFragPosition = vec3(model * vertex);
vNormal =
mat3(transpose(inverse(model))) *
normal.xyz;
}