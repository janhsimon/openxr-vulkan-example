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
    const Headset::BeginFrameResult frameResult = headset.beginFrame(swapchainImageIndex);
    if (frameResult == Headset::BeginFrameResult::Error)
    {
      return EXIT_FAILURE;
    }
    else if (frameResult == Headset::BeginFrameResult::RenderFully)
    {
      renderer.render(swapchainImageIndex);

      const MirrorView::RenderResult mirrorResult = mirrorView.render(swapchainImageIndex);
      if (mirrorResult == MirrorView::RenderResult::Error)
      {
        return EXIT_FAILURE;
      }

      const bool mirrorViewVisible = (mirrorResult == MirrorView::RenderResult::Visible);
      renderer.submit(mirrorViewVisible);

      if (mirrorViewVisible)
      {
        mirrorView.present();
      }
    }

    if (frameResult == Headset::BeginFrameResult::RenderFully || frameResult == Headset::BeginFrameResult::SkipRender)
    {
      headset.endFrame();
    }
  }

  context.sync(); // Sync before destroying so that resources are free
  return EXIT_SUCCESS;
}