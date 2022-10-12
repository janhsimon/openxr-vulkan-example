#include "Headset.h"
#include "MirrorView.h"
#include "RenderTarget.h"
#include "Renderer.h"

#include <boxer/boxer.h>

namespace
{
inline constexpr size_t mirrorEyeIndex = 1u; // Index of eye to mirror, 0 = left, 1 = right
}

int main()
{
  Headset headset;
  if (headset.getError() == Headset::Error::NoHeadsetDetected)
  {
    boxer::show("No headset detected.\nPlease make sure that your headset connected and running.", "Error",
                boxer::Style::Error);
    return EXIT_FAILURE;
  }
  else if (headset.getError() == Headset::Error::OpenXR)
  {
    boxer::show("Headset encountered generic OpenXR error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }
  else if (headset.getError() == Headset::Error::Vulkan)
  {
    boxer::show("Headset encountered generic Vulkan error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }

  MirrorView mirrorView(&headset);
  if (mirrorView.getError() == MirrorView::Error::GLFW)
  {
    boxer::show("Mirror view encountered generic GLFW error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }
  else if (mirrorView.getError() == MirrorView::Error::Vulkan)
  {
    boxer::show("Mirror view encountered generic Vulkan error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }

  Renderer renderer(&headset);
  if (!renderer.isValid())
  {
    boxer::show("Renderer encountered generic Vulkan error.", "Error", boxer::Style::Error);
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
        headset.beginEye(eyeIndex, swapchainImageIndex);

        if (eyeIndex == mirrorEyeIndex)
        {
          mirrorSwapchainImageIndex = swapchainImageIndex;
        }

        renderer.render(eyeIndex, swapchainImageIndex);

        headset.endEye(eyeIndex);
      }
    }

    headset.endFrame();

    const VkImage mirrorImage = headset.getEyeRenderTarget(mirrorEyeIndex, mirrorSwapchainImageIndex)->getImage();
    mirrorView.render(mirrorImage, headset.getEyeResolution(mirrorEyeIndex));
  }

  headset.sync(); // Sync before destroying so that resources are free
  mirrorView.destroy();
  renderer.destroy();
  headset.destroy();

  return EXIT_SUCCESS;
}