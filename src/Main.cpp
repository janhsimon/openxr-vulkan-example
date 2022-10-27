#include "Context.h"
#include "Headset.h"
#include "MirrorView.h"
#include "Renderer.h"

int main()
{
  Context context;
  if (!context.isValid())
  {
    return EXIT_FAILURE;
  }

  MirrorView mirrorView(&context);
  if (!mirrorView.isValid())
  {
    return EXIT_FAILURE;
  }

  if (!context.createDevice(mirrorView.getSurface()))
  {
    return EXIT_FAILURE;
  }

  Headset headset(&context);
  if (!headset.isValid())
  {
    return EXIT_FAILURE;
  }

  Renderer renderer(&context, &headset);
  if (!renderer.isValid())
  {
    return EXIT_FAILURE;
  }

  if (!mirrorView.connect(&headset, &renderer))
  {
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
      renderer.submit();

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

  context.sync(); // Sync before destroying so that resources are free
  renderer.destroy();
  headset.destroy();
  mirrorView.destroy();
  context.destroy();
  return EXIT_SUCCESS;
}