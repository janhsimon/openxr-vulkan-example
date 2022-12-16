#extension GL_EXT_multiview : enable

layout(binding = 0) uniform World
{
    mat4 matrix;
} world;

layout(binding = 1) uniform ViewProjection
{
    mat4 matrices[2];
} viewProjection;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 normal; // In world space
layout(location = 1) out vec3 color;

void main()
{
  gl_Position = viewProjection.matrices[gl_ViewIndex] * world.matrix * vec4(inPosition, 1.0);

  normal = normalize(vec3(world.matrix * vec4(inNormal, 0.0)));
  color = inColor;
}