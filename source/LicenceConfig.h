//------------------------------------------------------------------------
// R2Project licence - THIS BUILD's settings
//
// The per-release knobs from the design doc, in one place:
//
//   kEvaluationDays     days from the first editor open before the
//                       reminder starts (day 0..kEvaluationDays: no nag).
//                       Change the number and rebuild; nothing else.
//   kAcceptedProducts   product codes this build treats as licensed
//                       (Version policy). R2Synth v1.x: the paid
//                       bundle and R2Synth's free registration.
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

constexpr std::array<uint16_t, 2> kAcceptedProducts = {kProductBundleV1, kProductR2SynthFree};

// THE REAL SITE (6 Oct 2026). It pointed at the test server,
// http://34.7.233.43/activate.jsp, while the activation flow was tried
// out (1-4 Oct). HTTPS, so the result page's Copy button works.
constexpr const char* kActivationUrl = "https://www.r2-dvd.net/activate.jsp";

constexpr std::array<uint32_t, 0> kRevokedLicenceIds = {};

} // namespace R2Licence
