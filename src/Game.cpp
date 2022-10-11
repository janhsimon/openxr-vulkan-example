#include "Headset.h"
#include "MirrorView.h"
#include "RenderTarget.h"
#include "Renderer.h"

namespace
{
inline constexpr size_t mirrorEyeIndex = 1u; // Index of eye to mirror, 0 = left, 1 = right
}

int main()
{
  Headset headset;
  if (!headset.isValid())
  {
    return EXIT_FAILURE;
  }

  MirrorView mirrorView(&headset);
  if (!mirrorView.isValid())
  {
    return EXIT_FAILURE;
  }

  Renderer renderer(&headset);
  if (!renderer.isValid())
  {
    return EXIT_FAILURE;
  }

  // Main loop
  while (!mirrorView.windowShouldClose())
  {
    mirrorView.processWindowEvents();

    uint32_t mirrorSwapchainImageIndex = 0u;
    Headset::BeginFrameResult result = headset.beginFrame();
    if (result == Headset::BeginFrameResult::Error)
    {
      return EXIT_FAILURE;
    }
    else if (result == Headset::BeginFrameResult::SkipImmediately)
    {
      continue;
    }
    else if (result == Headset::BeginFrameResult::RenderFully)
    {
      for (size_t eyeIndex = 0u; eyeIndex < headset.getEyeCount(); ++eyeIndex)
      {
        uint32_t swapchainImageIndex;
        if (!headset.beginEye(eyeIndex, swapchainImageIndex))
        {
          return EXIT_FAILURE;
        }

        if (eyeIndex == mirrorEyeIndex)
        {
          mirrorSwapchainImageIndex = swapchainImageIndex;
        }

        if (!renderer.render(eyeIndex, swapchainImageIndex))
        {
          return EXIT_FAILURE;
        }

        if (!headset.endEye(eyeIndex))
        {
          return EXIT_FAILURE;
        }
      }
    }

    if (!headset.endFrame())
    {
      return EXIT_FAILURE;
    }

    const VkImage mirrorImage = headset.getEyeRenderTarget(mirrorEyeIndex, mirrorSwapchainImageIndex)->getImage();
    if (!mirrorView.render(mirrorImage, headset.getEyeResolution(mirrorEyeIndex)))
    {
      return EXIT_FAILURE;
    }
  }

  headset.sync(); // Sync before destroying so that resources are free
  mirrorView.destroy();
  renderer.destroy();
  headset.destroy();

  return EXIT_SUCCESS;
}