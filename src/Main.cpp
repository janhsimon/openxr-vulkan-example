#include "Context.h"
#include "Controllers.h"
#include "Headset.h"
#include "MeshData.h"
#include "MirrorView.h"
#include "Model.h"
#include "Renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

namespace
{
constexpr float flySpeedMultiplier = 2.5f;
}

int main()
{
  glm::mat4 cameraMatrix = glm::mat4(1.0f); // Transform from world to stage space

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

  Controllers controllers(context.getXrInstance(), headset.getXrSession());
  if (!controllers.isValid())
  {
    return EXIT_FAILURE;
  }

  Model gridModel, ruinsModel, carModelLeft, carModelRight, beetleModel, bikeModel, handModelLeft, handModelRight,
    logoModel;
  std::vector<Model*> models = { &gridModel, &ruinsModel,    &carModelLeft,   &carModelRight, &beetleModel,
                                 &bikeModel, &handModelLeft, &handModelRight, &logoModel };

  gridModel.worldMatrix = ruinsModel.worldMatrix = glm::mat4(1.0f);
  carModelLeft.worldMatrix =
    glm::rotate(glm::translate(glm::mat4(1.0f), { -3.5f, 0.0f, -7.0f }), glm::radians(75.0f), { 0.0f, 1.0f, 0.0f });
  carModelRight.worldMatrix =
    glm::rotate(glm::translate(glm::mat4(1.0f), { 8.0f, 0.0f, -15.0f }), glm::radians(-15.0f), { 0.0f, 1.0f, 0.0f });
  beetleModel.worldMatrix =
    glm::rotate(glm::translate(glm::mat4(1.0f), { -3.5f, 0.0f, -0.5f }), glm::radians(-125.0f), { 0.0f, 1.0f, 0.0f });
  logoModel.worldMatrix = glm::translate(glm::mat4(1.0f), { 0.0f, 3.0f, -10.0f });

  MeshData* meshData = new MeshData;
  if (!meshData->loadModel("models/Grid.obj", MeshData::Color::FromNormals, models, 0u, 1u))
  {
    return EXIT_FAILURE;
  }

  if (!meshData->loadModel("models/Ruins.obj", MeshData::Color::White, models, 1u, 1u))
  {
    return EXIT_FAILURE;
  }

  if (!meshData->loadModel("models/Car.obj", MeshData::Color::White, models, 2u, 2u))
  {
    return EXIT_FAILURE;
  }

  if (!meshData->loadModel("models/Beetle.obj", MeshData::Color::White, models, 4u, 1u))
  {
    return EXIT_FAILURE;
  }

  if (!meshData->loadModel("models/Bike.obj", MeshData::Color::White, models, 5u, 1u))
  {
    return EXIT_FAILURE;
  }

  if (!meshData->loadModel("models/Hand.obj", MeshData::Color::White, models, 6u, 2u))
  {
    return EXIT_FAILURE;
  }

  if (!meshData->loadModel("models/Logo.obj", MeshData::Color::White, models, 8u, 1u))
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
      if (!controllers.sync(headset.getXrSpace(), headset.getXrFrameState().predictedDisplayTime))
      {
        return EXIT_FAILURE;
      }

      static float time = 0.0f;
      time += deltaTime;

      // Update
      for (size_t controllerIndex = 0u; controllerIndex < 2u; ++controllerIndex)
      {
        const float flySpeed = controllers.getFlySpeed(controllerIndex);
        if (flySpeed > 0.0f)
        {
          const glm::vec3 forward = glm::normalize(controllers.getPose(controllerIndex)[2]);
          cameraMatrix = glm::translate(cameraMatrix, forward * flySpeed * flySpeedMultiplier * deltaTime);
        }
      }

      const glm::mat4 inverseCameraMatrix = glm::inverse(cameraMatrix);
      handModelLeft.worldMatrix = inverseCameraMatrix * controllers.getPose(0u);
      handModelRight.worldMatrix = inverseCameraMatrix * controllers.getPose(1u);
      handModelRight.worldMatrix = glm::scale(handModelRight.worldMatrix, { -1.0f, 1.0f, 1.0f });

      bikeModel.worldMatrix =
        glm::rotate(glm::translate(glm::mat4(1.0f), { 0.5f, 0.0f, -4.5f }), time * 0.2f, { 0.0f, 1.0f, 0.0f });

      // Render
      renderer.render(cameraMatrix, swapchainImageIndex, time);

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