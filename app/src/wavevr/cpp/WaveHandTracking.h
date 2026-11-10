/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <wvr/wvr.h>
#include <wvr/wvr_device.h>
#include <wvr/wvr_hand.h>
#include <array>
#include <chrono>
#include <cmath>
#include <vector>

namespace crow {

// Wave natural hand tracking. Hand index 0 is the right hand, 1 the left one.
// Joints are exposed in the WVR_HandJoint order, which matches XrHandJointEXT.
class WaveHandTracking {
public:
  static const int kJointCount = 26;

  ~WaveHandTracking() { Stop(); }

  bool Initialize() {
    supported = (WVR_GetSupportedFeatures() & WVR_SupportedFeature_HandTracking) != 0;
    return supported;
  }

  // Keeps the tracker running while wanted; data is only polled when aPoll is set.
  // Starting and stopping the tracker blocks the caller for about a second, so avoid doing it per frame.
  bool Update(bool wanted, bool aPoll = true) {
    if (!wanted || !supported) {
      Stop();
      return false;
    }
    if (!aPoll) {
      armed.fill(false);
      pressed.fill(false);
      return false;
    }
    if (!running) {
      if (!WVR_IsDeviceConnected(WVR_DeviceType_NaturalHand_Right) &&
          !WVR_IsDeviceConnected(WVR_DeviceType_NaturalHand_Left)) return false;
      const auto now = std::chrono::steady_clock::now();
      if (now < retryAfter) return false;
      retryAfter = now + std::chrono::seconds(1);
      if (!Start()) return false;
    }
    data.right.isValidPose = data.left.isValidPose = false;
    pose = {};
    if (WVR_GetHandTrackingData(WVR_HandTrackerType_Natural, WVR_HandModelType_WithoutController,
                                WVR_PoseOriginModel_OriginOnHead, &data, &pose) != WVR_Success) {
      armed.fill(false);
      pressed.fill(false);
      return false;
    }
    return true;
  }

  void Stop() {
    if (running) WVR_StopHandTracking(WVR_HandTrackerType_Natural);
    running = false;
    armed.fill(false);
    pressed.fill(false);
  }

  bool Valid(int hand) const {
    const auto& h = Hand(hand);
    if (!running || !h.isValidPose) return false;
    for (int joint = 0; joint < kJointCount; ++joint) {
      const int i = indices[joint];
      if (i < 0 || static_cast<uint32_t>(i) >= h.jointCount ||
          !(flags[i] & WVR_HandJointValidFlag_PositionValid)) return false;
      const auto& p = h.joints[i];
      for (float v : p.position.v) if (!std::isfinite(v)) return false;
      if (!std::isfinite(p.rotation.w) || !std::isfinite(p.rotation.x) ||
          !std::isfinite(p.rotation.y) || !std::isfinite(p.rotation.z)) return false;
    }
    return true;
  }

  const WVR_HandJointData_t& Hand(int hand) const { return hand == 0 ? data.right : data.left; }
  const WVR_HandPoseState_t& Pose(int hand) const { return hand == 0 ? pose.right : pose.left; }
  const WVR_Pose_t& Joint(int hand, int joint) const { return Hand(hand).joints[indices[joint]]; }
  bool HasRotation(int joint) const {
    return (flags[indices[joint]] & WVR_HandJointValidFlag_RotationValid) != 0;
  }

  float Strength(int hand) const {
    const auto& p = Pose(hand);
    if (!Valid(hand) || p.base.type != WVR_HandPoseType_Pinch || !std::isfinite(p.pinch.strength)) return 0;
    return std::fmax(0.f, std::fmin(1.f, p.pinch.strength));
  }

  bool Pinched(int hand) {
    if (!Valid(hand)) { armed[hand] = pressed[hand] = false; return false; }
    const auto& p = Pose(hand);
    if (p.base.type != WVR_HandPoseType_Pinch || !std::isfinite(p.pinch.strength)) {
      armed[hand] = pressed[hand] = false;
      return false;
    }
    const float on = info.pinchTHR > 0 && info.pinchTHR <= 1 ? info.pinchTHR : .7f;
    const float off = info.pinchOff > 0 && info.pinchOff < on ? info.pinchOff : on * .7f;
    const float value = Strength(hand);
    // Never synthesize a click from a pinch already held when tracking recovers.
    if (value <= off) armed[hand] = true;
    pressed[hand] = armed[hand] && (pressed[hand] ? value > off : value >= on);
    return pressed[hand];
  }

private:
  bool Start() {
    if (WVR_StartHandTracking(WVR_HandTrackerType_Natural) != WVR_Success) return false;
    running = true;
    uint32_t count = 0;
    if (WVR_GetHandJointCount(WVR_HandTrackerType_Natural, &count) != WVR_Success ||
        count < kJointCount || count > 128) { Stop(); return false; }
    mapping.resize(count);
    flags.resize(count);
    info = {};
    info.jointCount = count;
    info.jointMappingArray = mapping.data();
    info.jointValidFlagArray = flags.data();
    if (WVR_GetHandTrackerInfo(WVR_HandTrackerType_Natural, &info) != WVR_Success ||
        info.jointCount != count) { Stop(); return false; }
    indices.fill(-1);
    for (uint32_t i = 0; i < count; ++i) {
      const int joint = static_cast<int>(mapping[i]);
      if (joint >= 0 && joint < kJointCount) {
        if (indices[joint] != -1) { Stop(); return false; }
        indices[joint] = static_cast<int>(i);
      }
    }
    for (int i : indices) if (i < 0) { Stop(); return false; }
    rightJoints.resize(count);
    leftJoints.resize(count);
    data = {};
    data.right.jointCount = data.left.jointCount = count;
    data.right.joints = rightJoints.data();
    data.left.joints = leftJoints.data();
    return true;
  }

  bool supported = false, running = false;
  std::chrono::steady_clock::time_point retryAfter{};
  WVR_HandTrackerInfo_t info{};
  WVR_HandTrackingData_t data{};
  WVR_HandPoseData_t pose{};
  std::vector<WVR_HandJoint> mapping;
  std::vector<uint64_t> flags;
  std::vector<WVR_Pose_t> rightJoints, leftJoints;
  std::array<int, kJointCount> indices{};
  std::array<bool, 2> armed{}, pressed{};
};

}
