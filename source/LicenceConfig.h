//------------------------------------------------------------------------
// R2Project licence - THIS BUILD's settings
//
// The per-release knobs from the design doc, in one place:
//
//   kEvaluationDays     days from the first editor open before the
//                       reminder starts (day 0..kEvaluationDays: no nag).
//                       Change the number and rebuild; nothing else.
//   kAcceptedProducts   product codes this build treats as licensed
//                       (Version policy). SimpleSynth v1.x: the paid
//                       bundle and SimpleSynth's free registration.
//   kActivationUrl      where the reminder's "Get activation code" button
//                       goes. Empty: no button.
//   kRevokedLicenceIds  licence IDs refunded or leaked - exported from
//                       `SELECT licence_id FROM licence WHERE status='revoked'`
//                       at release time.
//
// None of these are in the key or the licence file, so recompiling never
// invalidates a licence or restarts an evaluation.
//------------------------------------------------------------------------

#pragma once

#include "LicenceKey.h"

#include <array>
#include <cstdint>

namespace R2Licence {

constexpr int kEvaluationDays = 7;

constexpr std::array<uint16_t, 2> kAcceptedProducts = {kProductBundleV1, kProductSimpleSynthFree};

// TEST SERVER while the activation flow is tried out. Put back
// "https://www.r2-dvd.net/activate.jsp" before any release.
constexpr const char* kActivationUrl = "http://35.204.130.240/activate.jsp";

constexpr std::array<uint32_t, 0> kRevokedLicenceIds = {};

} // namespace R2Licence
