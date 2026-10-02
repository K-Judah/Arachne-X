#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arachne {
enum class Leg : std::uint8_t { LF, LM, LR, RF, RM, RR };
enum class JointKind : std::uint8_t { Coxa, Femur, Tibia };
enum class JointId : std::uint8_t {
    LF_Coxa, LF_Femur, LF_Tibia,
    LM_Coxa, LM_Femur, LM_Tibia,
    LR_Coxa, LR_Femur, LR_Tibia,
    RF_Coxa, RF_Femur, RF_Tibia,
    RM_Coxa, RM_Femur, RM_Tibia,
    RR_Coxa, RR_Femur, RR_Tibia,
    Unknown = 255 // Sentinel, not a nineteenth logical joint.
};
inline constexpr std::size_t kLegCount = 6;
inline constexpr std::size_t kJointsPerLeg = 3;
inline constexpr std::size_t kJointCount = kLegCount * kJointsPerLeg;
inline constexpr std::array<JointId, kJointCount> kAllJoints{
    JointId::LF_Coxa, JointId::LF_Femur, JointId::LF_Tibia,
    JointId::LM_Coxa, JointId::LM_Femur, JointId::LM_Tibia,
    JointId::LR_Coxa, JointId::LR_Femur, JointId::LR_Tibia,
    JointId::RF_Coxa, JointId::RF_Femur, JointId::RF_Tibia,
    JointId::RM_Coxa, JointId::RM_Femur, JointId::RM_Tibia,
    JointId::RR_Coxa, JointId::RR_Femur, JointId::RR_Tibia
};
constexpr bool valid(JointId id) { return static_cast<std::size_t>(id) < kJointCount; }
constexpr std::optional<JointId> joint_id(Leg leg, JointKind kind) {
    const auto l = static_cast<std::size_t>(leg);
    const auto j = static_cast<std::size_t>(kind);
    if (l >= kLegCount || j >= kJointsPerLeg) return std::nullopt;
    return static_cast<JointId>(l * kJointsPerLeg + j);
}
constexpr std::optional<Leg> leg_of(JointId id) {
    if (!valid(id)) return std::nullopt;
    return static_cast<Leg>(static_cast<std::size_t>(id) / kJointsPerLeg);
}
constexpr std::optional<JointKind> kind_of(JointId id) {
    if (!valid(id)) return std::nullopt;
    return static_cast<JointKind>(static_cast<std::size_t>(id) % kJointsPerLeg);
}
constexpr const char* joint_name(JointId id) {
    constexpr std::array<const char*, kJointCount> names{
        "LF.coxa", "LF.femur", "LF.tibia", "LM.coxa", "LM.femur", "LM.tibia",
        "LR.coxa", "LR.femur", "LR.tibia", "RF.coxa", "RF.femur", "RF.tibia",
        "RM.coxa", "RM.femur", "RM.tibia", "RR.coxa", "RR.femur", "RR.tibia"
    };
    return valid(id) ? names[static_cast<std::size_t>(id)] : "invalid joint";
}
} // namespace arachne
