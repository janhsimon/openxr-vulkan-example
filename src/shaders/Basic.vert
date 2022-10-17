#extension GL_EXT_multiview : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 world[2];
    mat4 view[2];
    mat4 projection[2];
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 color;
layout(location = 1) out vec3 position; // In world space

void main()
{
  vec4 pos = ubo.world[gl_ViewIndex] * vec4(inPosition, 1.0);
  gl_Position = ubo.projection[gl_ViewIndex] * ubo.view[gl_ViewIndex] * pos;
  color = inColor;
  position = pos.xyz;
}