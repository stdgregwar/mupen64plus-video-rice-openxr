#ifndef OXRCONTEXT_H
#define OXRCONTEXT_H

#include <vector>
#include <variant>
#include <array>

#include "OGLExtensions.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#undef uchar
#undef uint32
#undef uint16
#undef uint64
#include <fmt/format.h>
#include <glm/glm.hpp>
#define uchar  unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
#define uint64 unsigned long long

#include "typedefs.h"

[[noreturn]] inline void ThrowXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    throw std::runtime_error(fmt::format("XrResult failure [{}] {} ({})", std::to_string(res), originator, sourceLocation));
}

inline XrResult CheckXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    if (XR_FAILED(res)) {
        ThrowXrResult(res, originator, sourceLocation);
    }

    return res;
}

#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)
#define CHECK_XR(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE)

class OXRContext
{
  public:
    static constexpr float gnear = 0.1f;
    static constexpr float gfar = 100.f;

    static constexpr float frustumScale = 1.f / 6000.f;

    OXRContext();
    void initXr();
    void frameStart();
    void frameEnd();
    std::array<Matrix, 2> getVPMatrices(const Matrix& projectMatrix) const;
    void SetViewportRender();
    ~OXRContext();
  private:


    std::vector<XrCompositionLayerDepthInfoKHR> projectionDepths;
    std::vector<XrCompositionLayerProjectionView> projectionViews;
    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    XrCompositionLayerBaseHeader* layers;
    XrFrameState xrFrameState{XR_TYPE_FRAME_STATE};
    XrFrameEndInfo visibleFrameEndInfos{XR_TYPE_FRAME_END_INFO};
    XrFrameEndInfo frameEndInfos{XR_TYPE_FRAME_END_INFO};


    struct StateIdle{
        void frameStart(OXRContext&);
        void frameEnd(OXRContext&);
    };

    struct StateSynchronised{
        void frameStart(OXRContext&);
        void frameEnd(OXRContext&);
    };

    struct StateVisible{
        void frameStart(OXRContext&);
        void frameEnd(OXRContext&);
    };


    using State = std::variant<StateIdle, StateSynchronised, StateVisible>;

    XrInstance xrInstance = XR_NULL_HANDLE;
    XrSystemId xrSystemId = XR_NULL_SYSTEM_ID;
    XrSession xrSession = XR_NULL_HANDLE;
    XrSpace referenceSpace = XR_NULL_HANDLE;
    XrEnvironmentBlendMode blendMode = XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
    XrSwapchain depthSwapchain = XR_NULL_HANDLE;
    XrSwapchain colorSwapchain = XR_NULL_HANDLE;

    std::vector<GLuint> colorImages;
    std::vector<GLuint> depthImages;

    XrViewConfigurationType configType;
    std::vector<XrViewConfigurationView> viewConfigurations;
    std::vector<XrView> viewsStorage;

    glm::ivec2 xrExtent;
    GLuint xrFramebuffer;
    State state = StateIdle();
    std::array<glm::mat4, 2> vpMatrices;
};

#endif // OXRCONTEXT_H
