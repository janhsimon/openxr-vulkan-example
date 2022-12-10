#extension GL_EXT_multiview : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 world;
    mat4 viewProjection[2];
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 position; // In world space
layout(location = 1) out vec3 color;

void main()
{
  vec4 pos = ubo.world * vec4(inPosition, 1.0);
  gl_Position = ubo.viewProjection[gl_ViewIndex] * pos;
  position = pos.xyz;

  color = inColor;
}