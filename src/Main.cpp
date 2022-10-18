#include "Headset.h"
#include "MirrorView.h"
#include "RenderTarget.h"
#include "Renderer.h"

#include <boxer/boxer.h>

int main()
{
  Headset headset;
  if (headset.getError() == Headset::Error::NoHeadsetDetected)
  {
    boxer::show("No headset detected.\nPlease make sure that your headset connected and running.", "Error",
                boxer::Style::Error);
    return EXIT_FAILURE;
  }
  else if (headset.getError() == Headset::Error::GLFW)
  {
    boxer::show("Headset encountered generic GLFW error.", "Error", boxer::Style::Error);
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

    uint32_t imageIndex;
    Headset::BeginFrameResult result = headset.beginFrame(imageIndex);
    if (result == Headset::BeginFrameResult::Error)
    {
      return EXIT_FAILURE;
    }
    else if (result == Headset::BeginFrameResult::RenderFully)
    {
      renderer.render(imageIndex);

      const VkImage mirrorImage = headset.getRenderTarget(imageIndex)->getImage();
      mirrorView.render(mirrorImage, headset.getEyeResolution(0u));
    }

    if (result == Headset::BeginFrameResult::RenderFully || result == Headset::BeginFrameResult::SkipRender)
    {
      headset.endFrame();
    }
  }

  headset.sync(); // Sync before destroying so that resources are free
  mirrorView.destroy();
  renderer.destroy();
  headset.destroy();
  return EXIT_SUCCESS;
}