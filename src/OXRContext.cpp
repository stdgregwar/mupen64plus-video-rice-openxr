#include "OXRContext.h"

#include <array>
#include "XrGlm.h"
#include "Video.h"

OXRContext::OXRContext()
{
  initXr();
}

void OXRContext::initXr() {
  auto extensions = std::array{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME};
  XrInstanceCreateInfo create_info{
    .type = XR_TYPE_INSTANCE_CREATE_INFO,
    .createFlags = {},
    .applicationInfo = XrApplicationInfo{
        //.applicationName = "XRBoy",
        .applicationVersion = XR_MAKE_VERSION(0,0,0),
        //.engineName = "None",
        .engineVersion = XR_MAKE_VERSION(0,0,0),
        .apiVersion = XR_CURRENT_API_VERSION
    },
    .enabledApiLayerCount = 0,
    .enabledApiLayerNames = nullptr,
    .enabledExtensionCount = extensions.size(),
    .enabledExtensionNames = extensions.data()
  };
  strcpy(create_info.applicationInfo.applicationName, "Mupen64PlusRiceOpenxr");
  strcpy(create_info.applicationInfo.engineName, "None");

  CHECK_XR(xrCreateInstance(&create_info, &xrInstance));

  XrSystemGetInfo sys_get_info{
    .type = XR_TYPE_SYSTEM_GET_INFO,
    .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY
  };

  // Pick system
  CHECK_XR(xrGetSystem(xrInstance, &sys_get_info, &xrSystemId));

  // Pick default view config
  uint32_t view_config_count;
  CHECK_XR(xrEnumerateViewConfigurations(xrInstance, xrSystemId, 0, &view_config_count, nullptr));
  std::vector<XrViewConfigurationType> view_config_types(view_config_count);
  CHECK_XR(xrEnumerateViewConfigurations(xrInstance, xrSystemId, view_config_types.size(), &view_config_count, view_config_types.data()));
  configType = view_config_types.front();

  // Get view configurations
  CHECK_XR(xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, configType, 0, &view_config_count, nullptr));
  viewConfigurations.resize(view_config_count, XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW});
  CHECK_XR(xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, configType, viewConfigurations.size(), &view_config_count, viewConfigurations.data()));

  viewsStorage.resize(viewConfigurations.size(), {XR_TYPE_VIEW});

  uint32_t blend_modes_count;
  CHECK_XR(xrEnumerateEnvironmentBlendModes(xrInstance, xrSystemId, configType, 0, &blend_modes_count, nullptr));
  std::vector<XrEnvironmentBlendMode> blend_modes(blend_modes_count);
  CHECK_XR(xrEnumerateEnvironmentBlendModes(xrInstance, xrSystemId, configType, blend_modes.size(), &blend_modes_count, blend_modes.data()));

  blendMode = blend_modes.front();

  auto graphics_binding = XrGraphicsBindingOpenGLWin32KHR{
    .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
    .hDC = NULL, // Lets hope it does not explode
    .hGLRC = NULL,
  };

  // Create session
  XrSessionCreateInfo session_create_info{
    .type = XR_TYPE_SESSION_CREATE_INFO,
    .next = &graphics_binding,
    .createFlags = {},
    .systemId = xrSystemId
  };

  CHECK_XR(xrCreateSession(xrInstance, &session_create_info, &xrSession));

  // Setup reference space
  XrReferenceSpaceCreateInfo ref_create_info{
    .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
    .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
    .poseInReferenceSpace = XrPosef{.orientation={.w=1}} //Identity pose
  };

  CHECK_XR(xrCreateReferenceSpace(xrSession, &ref_create_info, &referenceSpace));

  // Create swapchains and retrieve images
  auto masterView = viewConfigurations.front();

  xrExtent = glm::ivec2(masterView.recommendedImageRectWidth, masterView.recommendedImageRectHeight);

  auto make_swapchain = [&](bool depth) {
    auto format = depth ? GL_DEPTH_COMPONENT16 : GL_SRGB8_ALPHA8;
    auto usage = depth ? XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    XrSwapchainCreateInfo swap_create{
      .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
      .createFlags = {},
      .usageFlags = usage,
      .format = (int64_t)format,
      .sampleCount = 1,
      .width = (uint32_t)xrExtent.x,
      .height = (uint32_t)xrExtent.y,
      .faceCount = 1,
      .arraySize = (uint32_t)viewConfigurations.size(),
      .mipCount = 1
    };

    XrSwapchain swap;
    CHECK_XR(xrCreateSwapchain(xrSession, &swap_create, &swap));
    return swap;
  };

  colorSwapchain = make_swapchain(false);
  depthSwapchain = make_swapchain(true);

  auto get_swapchain_images = [&](XrSwapchain swap) {
    uint32_t image_count = 0;
    CHECK_XR(xrEnumerateSwapchainImages(swap, 0, &image_count, nullptr));
    std::vector<XrSwapchainImageOpenGLKHR> images(image_count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
    CHECK_XR(xrEnumerateSwapchainImages(swap, image_count, &image_count, (XrSwapchainImageBaseHeader*)images.data()));

    std::vector<GLuint> gl_images; gl_images.reserve(images.size());
    for(const auto& img : images){ gl_images.push_back(img.image);}
    return gl_images;
  };

  colorImages = get_swapchain_images(colorSwapchain);
  depthImages = get_swapchain_images(depthSwapchain);

  // Init frame end infos
  projectionDepths.resize(viewConfigurations.size());
  projectionViews.resize(viewConfigurations.size());

  const auto& vconfig = viewConfigurations.front();
  for(uint32_t i = 0; i < viewConfigurations.size(); i++) {
    auto rect = XrRect2Di{{0,0},{xrExtent.x, xrExtent.y}};
    projectionDepths.at(i) = XrCompositionLayerDepthInfoKHR{
      .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
        .subImage = XrSwapchainSubImage{
          .swapchain = depthSwapchain,
          .imageRect = rect,
          .imageArrayIndex = i
        },
        .minDepth = 0.f,
        .maxDepth = 1.f,
        .nearZ = gnear,
        .farZ = gfar,
    };
    projectionViews.at(i) = XrCompositionLayerProjectionView{
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
        .next = &projectionDepths.at(i),
        .subImage = XrSwapchainSubImage{
          .swapchain= colorSwapchain,
          .imageRect= rect,
          .imageArrayIndex = i
        },
    };
  }

  projectionLayer = XrCompositionLayerProjection{
    .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
    .layerFlags = {},
    .space = referenceSpace,
    .viewCount = (uint32_t)projectionViews.size(),
    .views = projectionViews.data()
  };

  layers = (XrCompositionLayerBaseHeader*)&projectionLayer;

  visibleFrameEndInfos = XrFrameEndInfo{
    .type = XR_TYPE_FRAME_END_INFO,
    .environmentBlendMode = blendMode,
    .layerCount = 1,
    .layers = &layers
  };

  frameEndInfos = XrFrameEndInfo{
    .type = XR_TYPE_FRAME_END_INFO,
    .environmentBlendMode = blendMode,
    .layerCount = 0,
    .layers = nullptr
  };

  // Init the xr framebuffer
  glGenFramebuffers(1, &xrFramebuffer);
}

void OXRContext::frameStart(){
  // XR event pump
  XrEventDataBuffer event_buffer{XR_TYPE_EVENT_DATA_BUFFER};
  while(CHECK_XR(xrPollEvent(xrInstance, &event_buffer)) != XR_EVENT_UNAVAILABLE) {
    switch(event_buffer.type){
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(&event_buffer);
        switch(sessionStateChangedEvent.state){
          case XR_SESSION_STATE_IDLE:
            break;
          case XR_SESSION_STATE_READY: {
            XrSessionBeginInfo binfo{
              .type = XR_TYPE_SESSION_BEGIN_INFO,
              .primaryViewConfigurationType = configType,
            };
            CHECK_XR(xrBeginSession(xrSession, &binfo));
            state = StateIdle();
            break;
          }
          case XR_SESSION_STATE_SYNCHRONIZED:
            state = StateSynchronised(); break;
          case XR_SESSION_STATE_VISIBLE:
          case XR_SESSION_STATE_FOCUSED:
            state = StateVisible(); break;
          case XR_SESSION_STATE_STOPPING:
            CHECK_XR(xrEndSession(xrSession));
            state = StateIdle(); break;
          case XR_SESSION_STATE_EXITING:
            CHECK_XR(xrDestroySession(xrSession));
            xrSession = XR_NULL_HANDLE;
            //keep_on = false;
            break; //TODO exit
          default:
            break;
        }
      }
      default:
        break;
    }
  }

  std::visit([this](auto& s){s.frameStart(*this);}, state); //Execute current state
}

void OXRContext::frameEnd(){
  std::visit([this](auto& s){s.frameEnd(*this);}, state);
}


void OXRContext::StateIdle::frameStart(OXRContext&) {

}

void OXRContext::StateIdle::frameEnd(OXRContext&) {

}

void OXRContext::StateSynchronised::frameStart(OXRContext&) {

}

void OXRContext::StateSynchronised::frameEnd(OXRContext&) {

}


void OXRContext::StateVisible::frameStart(OXRContext& ctx) {
  XrFrameWaitInfo frame_wait_info{XR_TYPE_FRAME_WAIT_INFO};
  CHECK_XR(xrWaitFrame(ctx.xrSession, &frame_wait_info, &ctx.xrFrameState));

  XrFrameBeginInfo frame_binfos{XR_TYPE_FRAME_BEGIN_INFO};
  CHECK_XR(xrBeginFrame(ctx.xrSession, &frame_binfos));

  XrViewLocateInfo locate_info{
    .type = XR_TYPE_VIEW_LOCATE_INFO,
    .viewConfigurationType = ctx.configType,
    .displayTime = ctx.xrFrameState.predictedDisplayTime,
    .space = ctx.referenceSpace,
  };

  //Locate views
  XrViewState view_state{XR_TYPE_VIEW_STATE};
  uint32_t viewCountOutput;
  CHECK_XR(xrLocateViews(ctx.xrSession, &locate_info, &view_state, ctx.viewsStorage.size(), &viewCountOutput, ctx.viewsStorage.data()));

  // Transform views to view proj matrices
  for(size_t i = 0; i < ctx.viewsStorage.size(); i++) {
    auto& xrview = ctx.viewsStorage.at(i);
    auto proj = xr2glm::fov(xrview.fov, gnear, gfar);
    auto view = glm::inverse(xr2glm::pose(xrview.pose));
    auto viewproj = proj * view;
    ctx.vpMatrices[i] = viewproj;
  }

  // Aquire images from the swapchains
  XrSwapchainImageAcquireInfo ainfos{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  uint32_t color_index;
  CHECK_XR(xrAcquireSwapchainImage(ctx.colorSwapchain, &ainfos, &color_index));
  uint32_t depth_index;
  CHECK_XR(xrAcquireSwapchainImage(ctx.depthSwapchain, &ainfos, &depth_index));

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx.xrFramebuffer);
  glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ctx.colorImages.at(color_index), 0);
  glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, ctx.depthImages.at(depth_index), 0);
  GLint fbstatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
  if(fbstatus != GL_FRAMEBUFFER_COMPLETE) {
    throw std::runtime_error(fmt::format("Framebuffer is not complete :  {}", fbstatus));
  }

  glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
  glViewport(0,0, ctx.xrExtent.x, ctx.xrExtent.y);
  glScissor(0,0, ctx.xrExtent.x, ctx.xrExtent.y);
  glClearDepth(0.f);
  glClearColor(0.f,0.f,0.f,1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OXRContext::SetViewportRender() {
  glViewport(0,0, xrExtent.x, xrExtent.y);
  glScissor(0,0, xrExtent.x, xrExtent.y);
}

void OXRContext::StateVisible::frameEnd(OXRContext& ctx) {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.xrFramebuffer);
  glViewport(0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight);
  glScissor(0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight);
  glBlitFramebuffer(
        0,0, ctx.xrExtent.x, ctx.xrExtent.y,
        0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight,
        GL_COLOR_BUFFER_BIT,
        GL_LINEAR);

  for(int i = 0; i < ctx.viewsStorage.size(); i++) {
    auto& pview = ctx.projectionViews.at(i);
    auto& view = ctx.viewsStorage.at(i);
    pview.pose = view.pose;
    pview.fov = view.fov;
  }

  XrSwapchainImageReleaseInfo rinfos{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  CHECK_XR(xrReleaseSwapchainImage(ctx.colorSwapchain, &rinfos));
  CHECK_XR(xrReleaseSwapchainImage(ctx.depthSwapchain, &rinfos));

  ctx.visibleFrameEndInfos.displayTime = ctx.xrFrameState.predictedDisplayTime;
  CHECK_XR(xrEndFrame(ctx.xrSession, &ctx.visibleFrameEndInfos));
}

std::array<Matrix, 2> OXRContext::getVPMatrices(const Matrix& projectMatrix) const {
  std::array<Matrix, 2> matrices;
  auto glmProj = *reinterpret_cast<const glm::mat4*>(&projectMatrix);

  auto glmiProj = glm::inverse(glmProj);

  auto scale = glm::scale(glm::mat4(1.f), glm::vec3(frustumScale));

  auto glmVp = vpMatrices.at(0) * scale * glmiProj;
  memcpy(matrices[0], glm::value_ptr(glmVp), sizeof(glm::mat4));
  glmVp = vpMatrices.at(1) * scale * glmiProj;
  memcpy(matrices[1], glm::value_ptr(glmVp), sizeof(glm::mat4));
  return matrices;
}

OXRContext::~OXRContext(){
  CHECK_XR(xrDestroySwapchain(colorSwapchain));
  CHECK_XR(xrDestroySwapchain(depthSwapchain));
  CHECK_XR(xrDestroySession(xrSession));
  CHECK_XR(xrDestroyInstance(xrInstance));
}
