layout(location = 0) in vec3 color;
layout(location = 1) in vec3 position;

layout(location = 0) out vec4 outColor;

bool isOnGrid(float position, float threshold)
{
  return position < threshold || position > 1.0 - threshold;
}

void main()
{
  const float crossThickness = 0.01;
  const float crossLength = 0.05;

  const float x = mod(position.x, 1.0);
  const float z = mod(position.z, 1.0);

  if ((isOnGrid(x, crossThickness) && isOnGrid(z, crossLength)) || (isOnGrid(z, crossThickness) && isOnGrid(x, crossLength)))
  {
    const float fadeLength = 20.0;
    const float fade = clamp(1.0 - (length(position.xz) / fadeLength), 0.0, 1.0);
    outColor = vec4(color, fade);
  }
  else
  {
    const float fadeLength = 10.0;
    const float fade = clamp(1.0 - (length(position.xz) / fadeLength), 0.0, 1.0);

    const vec3 backgroundColor = vec3(0.01, 0.01, 0.01);
    const vec3 floorColor = vec3(0.08, 0.08, 0.08);

    outColor = vec4(mix(backgroundColor, floorColor, fade), 1.0);
  }
}