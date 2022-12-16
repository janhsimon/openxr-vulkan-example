#include "Context.h"
#include "Headset.h"
#include "MeshData.h"
#include "MirrorView.h"
#include "Model.h"
#include "Renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

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

  Model gridModel, carModelLeft, carModelCenter, carModelRight;
  std::vector<Model*> models = { &gridModel, &carModelLeft, &carModelCenter, &carModelRight };

  gridModel.worldMatrix = glm::mat4(1.0f);
  carModelLeft.worldMatrix = glm::translate(glm::mat4(1.0f), { -5.0f, 0.0f, -2.0f });
  carModelRight.worldMatrix = glm::translate(glm::mat4(1.0f), { 5.0f, 0.0f, -2.0f });

  MeshData* meshData = new MeshData;
  if (!meshData->loadModel("models/Grid.obj", MeshData::Color::FromNormals, models, 0u, 1u))
  {
    return EXIT_FAILURE;
  }

  if (!meshData->loadModel("models/Car.obj", MeshData::Color::White, models, 1u, 3u))
  {
    return EXIT_FAILURE;
  }

  Renderer renderer(&context, &headset, meshData, models);
  if (!renderer.isValid())
  {
    return EXIT_FAILURE;
  }

  delete meshData;

  if (!mirrorView.connect(&headset, &renderer))
  {
    return EXIT_FAILURE;
  }

  // Main loop
  std::chrono::high_resolution_clock::time_point previousTime = std::chrono::high_resolution_clock::now();
  while (!headset.isExitRequested() && !mirrorView.isExitRequested())
  {
    // Calculate the delta time in seconds
    const std::chrono::high_resolution_clock::time_point nowTime = std::chrono::high_resolution_clock::now();
    const long long elapsedNanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(nowTime - previousTime).count();
    const float deltaTime = static_cast<float>(elapsedNanoseconds) / 1e9f;
    previousTime = nowTime;

    mirrorView.processWindowEvents();

    uint32_t swapchainImageIndex;
    const Headset::BeginFrameResult frameResult = headset.beginFrame(swapchainImageIndex);
    if (frameResult == Headset::BeginFrameResult::Error)
    {
      return EXIT_FAILURE;
    }
    else if (frameResult == Headset::BeginFrameResult::RenderFully)
    {
      // Update
      static float time = 0.0f;
      time += deltaTime;

      carModelCenter.worldMatrix =
        glm::rotate(glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -3.0f }), time * 0.2f, { 0.0f, 1.0f, 0.0f });

      // Render
      renderer.render(swapchainImageIndex, time);

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