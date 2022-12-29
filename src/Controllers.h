#pragma once

#include <glm/fwd.hpp>

#include <openxr/openxr.h>

#include <vector>

/*
 * The controllers class handles OpenXR controller support. It represents the controller system as a whole, not an
 * individual controller. This is more convenient due to the OpenXR API. It allows the application to retrieve the
 * current transform of a controller, which is then used to accuretely pose the hand models in the scene.
 */
class Controllers final
{
public:
  Controllers(XrInstance instance, XrSession session);
  ~Controllers();

  bool sync(XrSpace space, XrTime time);

  bool isValid() const;

  glm::mat4 getTransform(size_t controllerIndex) const;

private:
  bool valid = true;

  XrSession session = nullptr;
  std::vector<XrPath> paths;
  XrActionSet actionSet = nullptr;
  XrAction action = nullptr;
  std::vector<XrSpace> spaces;
  std::vector<glm::mat4> transforms;
};