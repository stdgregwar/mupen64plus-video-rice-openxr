#pragma once

#include <openxr/openxr.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace xr2glm {
  inline XrFovf toTanFovf(const XrFovf& fov) {
      return { tanf(fov.angleLeft), tanf(fov.angleRight), tanf(fov.angleUp), tanf(fov.angleDown) };
  }

  inline glm::mat4 fov(const XrFovf& fov, float anear = 0.01f, float afar = 10000.0f) {
      /*auto tanFov = toTanFovf(fov);
      const auto& tanAngleRight = tanFov.angleRight;
      const auto& tanAngleLeft = tanFov.angleLeft;
      const auto& tanAngleUp = tanFov.angleDown;
      const auto& tanAngleDown = tanFov.angleUp;

      const float tanAngleWidth = tanAngleRight - tanAngleLeft;
      const float tanAngleHeight = (tanAngleDown - tanAngleUp);
      const float offsetZ = 0;

      glm::mat4 resultm{};
      float* result = &resultm[0][0];
      // normal projection
      result[0] = 2 / tanAngleWidth;
      result[4] = 0;
      result[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
      result[12] = 0;

      result[1] = 0;
      result[5] = 2 / tanAngleHeight;
      result[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
      result[13] = 0;

      result[2] = 0;
      result[6] = 0;
      result[10] = -(farZ + offsetZ) / (farZ - nearZ);
      result[14] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

      result[3] = 0;
      result[7] = 0;
      result[11] = -1;
      result[15] = 0;

      return resultm;*/
      auto [left, right, top, bottom] = fov;
      float B = anear * afar;
      float A = anear + afar;
      auto tl = std::tan(left);
      auto tr = std::tan(right);
      auto tt = std::tan(top);
      auto tb = std::tan(bottom);

      auto tw = tr - tl;
      auto th = tt - tb;

      auto x0 = (tl + tr) / tw;
      auto y0 = (tt + tb) / th;

      auto fx = 2.f / tw;
      auto fy = 2.f / th;

      return
          glm::ortho<float>(-1, 1, -1, 1, afar, anear)
             * glm::mat4(
                         glm::vec4(fx,  0, 0,   0),
                         glm::vec4(0,  fy, 0,   0),
                         glm::vec4(x0,  y0,  A,  -1),
                         glm::vec4(0,   0,  B,   0)
                       );
  }

  inline glm::quat quat(const XrQuaternionf& q) {
      return glm::make_quat(&q.x);
  }

  inline glm::vec3 position(const XrVector3f& v) {
      return glm::make_vec3(&v.x);
  }

  inline glm::mat4 pose(const XrPosef& p) {
      glm::mat4 orientation = glm::mat4_cast(quat(p.orientation));
      glm::mat4 translation = glm::translate(glm::mat4{ 1 }, position(p.position));
      return translation * orientation;
  }
}
