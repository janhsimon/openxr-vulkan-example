#pragma once

#include <glm/fwd.hpp>

#include <openxr/openxr.h>

#include <vector>

class Controllers final
{
public:
  Controllers(XrInstance instance, XrSession session);

  bool sync(XrSpace space, XrTime time);

  bool isValid() const;

  glm::mat4 getTransform(size_t controllerIndex) const;

private:
  bool valid = true;

  XrSession session;
  std::vector<XrPath> paths;
  XrActionSet actionSet;
  XrAction action;
  std::vector<XrSpace> spaces;
  std::vector<glm::mat4> transforms;
};