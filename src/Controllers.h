#pragma once

#include <glm/fwd.hpp>

#include <openxr/openxr.h>

#include <vector>

/*
 * The controllers class handles OpenXR controller support. It represents the controller system as a whole, not an
 * individual controller. This is more convenient due to the OpenXR API. It allows the application to retrieve the
 * current pose of a controller, which is then used to accurately pose the hand models in the scene. It also exposes the
 * current fly speed, which is used to fly the camera in the direction of the controller.
 */
class Controllers final
{
public:
  Controllers(XrInstance instance, XrSession session);
  ~Controllers();

  bool sync(XrSpace space, XrTime time);

  bool isValid() const;

  glm::mat4 getPose(size_t controllerIndex) const;
  float getFlySpeed(size_t controllerIndex) const;

private:
  bool valid = true;

  XrSession session = nullptr;
  std::vector<XrPath> paths;
  std::vector<XrSpace> spaces;

  std::vector<glm::mat4> poses;
  std::vector<float> flySpeeds;

  XrActionSet actionSet = nullptr;
  XrAction poseAction = nullptr, flyAction = nullptr;
};