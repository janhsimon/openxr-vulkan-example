layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 color;

layout(location = 0) out vec4 outColor;

void main()
{
  const vec3 lightDir = vec3(1.0, -1.0, -1.0);
  const float diffuse = clamp(dot(normal, -lightDir), 0.0, 1.0);

  const vec3 ambient = vec3(0.07, 0.05, 0.1);

  outColor = vec4(ambient + color * diffuse, 1.0);
}