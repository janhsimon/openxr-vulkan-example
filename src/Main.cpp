#include "Context.h"
#include "Headset.h"
#include "MirrorView.h"
#include "Renderer.h"

#include <boxer/boxer.h>

int main()
{
  Context context;
  if (context.getError() == Context::Error::NoHeadsetDetected)
  {
    boxer::show("No headset detected.\nPlease make sure that your headset is connected and running.", "Error",
                boxer::Style::Error);
    return EXIT_FAILURE;
  }
  else if (context.getError() == Context::Error::GLFW)
  {
    boxer::show("Context encountered generic GLFW error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }
  else if (context.getError() == Context::Error::OpenXR)
  {
    boxer::show("Context encountered generic OpenXR error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }
  else if (context.getError() == Context::Error::Vulkan)
  {
    boxer::show("Context encountered generic Vulkan error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }

  MirrorView mirrorView(&context);
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

  Headset headset(&context, mirrorView.getSurface());
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

  if (!mirrorView.mirrorHeadset(&headset))
  {
    boxer::show("Mirror view encountered generic Vulkan error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }

  Renderer renderer(&headset, context.getVkPhysicalDevice());
  if (!renderer.isValid())
  {
    boxer::show("Renderer encountered generic Vulkan error.", "Error", boxer::Style::Error);
    return EXIT_FAILURE;
  }

  // Main loop
  while (!mirrorView.windowShouldClose())
  {
    mirrorView.processWindowEvents();

    uint32_t swapchainImageIndex;
    Headset::BeginFrameResult result = headset.beginFrame(swapchainImageIndex);
    if (result == Headset::BeginFrameResult::Error)
    {
      return EXIT_FAILURE;
    }
    else if (result == Headset::BeginFrameResult::RenderFully)
    {
      renderer.render(swapchainImageIndex);
      const bool presentMirrorView = mirrorView.render(swapchainImageIndex);
      headset.submit();

      if (presentMirrorView)
      {
        mirrorView.present();
      }
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
  context.destroy();
  return EXIT_SUCCESS;
}