#include "Controllers.h"

#include "Context.h"
#include "Util.h"

#include <glm/mat4x4.hpp>

#include <array>
#include <cstring>

namespace
{
constexpr size_t controllerCount = 2u;

const std::string actionSetName = "actionset";
const std::string localizedActionSetName = "Actions";
} // namespace

Controllers::Controllers(XrInstance instance, XrSession session) : session(session)
{
  // Create an action set
  XrActionSetCreateInfo actionSetCreateInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };

  memcpy(actionSetCreateInfo.actionSetName, actionSetName.data(), actionSetName.length() + 1u);
  memcpy(actionSetCreateInfo.localizedActionSetName, localizedActionSetName.data(),
         localizedActionSetName.length() + 1u);

  XrResult result = xrCreateActionSet(instance, &actionSetCreateInfo, &actionSet);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  // Create paths
  paths.resize(controllerCount);
  paths.at(0u) = util::stringToPath(instance, "/user/hand/left");
  paths.at(1u) = util::stringToPath(instance, "/user/hand/right");

  // Create actions
  if (!util::createAction(actionSet, paths, "handpose", "Hand Pose", XR_ACTION_TYPE_POSE_INPUT, poseAction))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  if (!util::createAction(actionSet, paths, "fly", "Fly", XR_ACTION_TYPE_FLOAT_INPUT, flyAction))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  // Create spaces
  spaces.resize(controllerCount);
  for (size_t controllerIndex = 0u; controllerIndex < controllerCount; ++controllerIndex)
  {
    const XrPath& path = paths.at(controllerIndex);

    XrActionSpaceCreateInfo actionSpaceCreateInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
    actionSpaceCreateInfo.action = poseAction;
    actionSpaceCreateInfo.poseInActionSpace = util::makeIdentity();
    actionSpaceCreateInfo.subactionPath = path;

    result = xrCreateActionSpace(session, &actionSpaceCreateInfo, &spaces.at(controllerIndex));
    if (XR_FAILED(result))
    {
      util::error(Error::GenericOpenXR);
      valid = false;
      return;
    }
  }

  // Suggest simple controller binding (generic)
  const std::array<XrActionSuggestedBinding, 4u> bindings = {
    { { poseAction, util::stringToPath(instance, "/user/hand/left/input/aim/pose") },
      { poseAction, util::stringToPath(instance, "/user/hand/right/input/aim/pose") },
      { flyAction, util::stringToPath(instance, "/user/hand/left/input/select/click") },
      { flyAction, util::stringToPath(instance, "/user/hand/right/input/select/click") } }
  };

  XrInteractionProfileSuggestedBinding interactionProfileSuggestedBinding{
    XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING
  };
  interactionProfileSuggestedBinding.interactionProfile =
    util::stringToPath(instance, "/interaction_profiles/khr/simple_controller");
  interactionProfileSuggestedBinding.suggestedBindings = bindings.data();
  interactionProfileSuggestedBinding.countSuggestedBindings = static_cast<uint32_t>(bindings.size());

  result = xrSuggestInteractionProfileBindings(instance, &interactionProfileSuggestedBinding);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  // Attach the controller action set
  XrSessionActionSetsAttachInfo sessionActionSetsAttachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
  sessionActionSetsAttachInfo.countActionSets = 1u;
  sessionActionSetsAttachInfo.actionSets = &actionSet;

  result = xrAttachSessionActionSets(session, &sessionActionSetsAttachInfo);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    valid = false;
    return;
  }

  poses.resize(controllerCount);
  flySpeeds.resize(controllerCount);
}

Controllers::~Controllers()
{
  for (const XrSpace& space : spaces)
  {
    xrDestroySpace(space);
  }

  if (flyAction)
  {
    xrDestroyAction(flyAction);
  }

  if (poseAction)
  {
    xrDestroyAction(poseAction);
  }

  if (actionSet)
  {
    xrDestroyActionSet(actionSet);
  }
}

bool Controllers::sync(XrSpace space, XrTime time)
{
  // Sync the actions
  XrActiveActionSet activeActionSet;
  activeActionSet.actionSet = actionSet;
  activeActionSet.subactionPath = XR_NULL_PATH; // Wildcard for all

  XrActionsSyncInfo actionsSyncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
  actionsSyncInfo.countActiveActionSets = 1u;
  actionsSyncInfo.activeActionSets = &activeActionSet;

  XrResult result = xrSyncActions(session, &actionsSyncInfo);
  if (XR_FAILED(result))
  {
    util::error(Error::GenericOpenXR);
    return false;
  }

  // Update the actions
  for (size_t controllerIndex = 0u; controllerIndex < controllerCount; ++controllerIndex)
  {
    const XrPath& path = paths.at(controllerIndex);

    // Pose
    XrActionStatePose poseState{ XR_TYPE_ACTION_STATE_POSE };
    if (!util::updateActionStatePose(session, poseAction, path, poseState))
    {
      util::error(Error::GenericOpenXR);
      return false;
    }

    if (poseState.isActive)
    {
      XrSpaceLocation spaceLocation{ XR_TYPE_SPACE_LOCATION };
      result = xrLocateSpace(spaces.at(controllerIndex), space, time, &spaceLocation);
      if (XR_FAILED(result))
      {
        util::error(Error::GenericOpenXR);
        return false;
      }

      // Check that the position and orientation are valid and tracked
      constexpr XrSpaceLocationFlags checkFlags =
        XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT |
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
      if ((spaceLocation.locationFlags & checkFlags) == checkFlags)
      {
        poses.at(controllerIndex) = util::poseToMatrix(spaceLocation.pose);
      }
    }

    // Fly speed
    XrActionStateFloat flySpeedState{ XR_TYPE_ACTION_STATE_FLOAT };
    if (!util::updateActionStateFloat(session, flyAction, path, flySpeedState))
    {
      util::error(Error::GenericOpenXR);
      return false;
    }

    if (flySpeedState.isActive)
    {
      flySpeeds.at(controllerIndex) = flySpeedState.currentState;
    }
  }

  return true;
}

bool Controllers::isValid() const
{
  return valid;
}

glm::mat4 Controllers::getPose(size_t controllerIndex) const
{
  return poses.at(controllerIndex);
}

float Controllers::getFlySpeed(size_t controllerIndex) const
{
  return flySpeeds.at(controllerIndex);
}
