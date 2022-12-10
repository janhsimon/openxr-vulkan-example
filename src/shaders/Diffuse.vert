#extension GL_EXT_multiview : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 world;
    mat4 viewProjection[2];
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 normal; // In world space
layout(location = 1) out vec3 color;

void main()
{
  gl_Position = ubo.viewProjection[gl_ViewIndex] * ubo.world * vec4(inPosition, 1.0);

  normal = normalize(vec3(ubo.world * vec4(inNormal, 0.0)));
  color = inColor;
}