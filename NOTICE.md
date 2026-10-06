# SimpleSynth — licence notice

Copyright (C) 2026 A. E. Cobley

SimpleSynth is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. The full text is in [`LICENSE`](LICENSE).

SimpleSynth is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

SPDX-License-Identifier: GPL-3.0-or-later

## Additional term under section 7 of the GPL

Under section 7(e) of the GNU General Public License version 3, this licence
does **not** grant any right to use the name "SimpleSynth", the "R2Project"
name, or any associated logo or trade dress, to identify a modified version.
If you distribute a modified version, give it a different name, so that
nobody mistakes it for the official build or asks the author to support it.
You may still say truthfully that your version is based on SimpleSynth.

## The official build and activation codes

The signed installer published by the author includes a 7-day evaluation,
after which the plug-in shows a reminder until an activation code is entered.
Activation codes are issued, currently free of charge, at
<https://www.r2-dvd.net/activate.jsp>.

- The GPL gives you the freedom to build SimpleSynth from this source
  yourself, with or without the reminder. The activation code is a way of
  supporting the official, signed and notarised build; it is not a
  restriction on the software.
- **An activation code is personal to the person it was issued to.** Please
  do not publish or share it. This is a condition of the registration
  service, not of this licence.
- Activation codes are signed with a private key that is **not** part of
  this source. The public key in `source/LicencePublicKey.h` only verifies
  codes; it cannot make them.

## Third-party components

These are not covered by the copyright above and keep their own licences,
all of which are compatible with GPL version 3:

| Component | Where | Licence |
|---|---|---|
| Steinberg VST 3 SDK | fetched into `external/vst3sdk` | MIT |
| VSTGUI | inside the VST 3 SDK | BSD 3-Clause |
| Apple AudioUnitSDK | fetched into `external/AudioUnitSDK` | Apache 2.0 |
| Monocypher 4.0.2 | `third_party/monocypher` | BSD 2-Clause or CC0 (see its `LICENCE.md`) |

VST is a registered trademark of Steinberg Media Technologies GmbH. Audio
Units is a trademark of Apple Inc.

## Credits

- The Moog ladder filter follows A. Huovilainen, "Non-linear digital
  implementation of the Moog ladder filter", DAFx 2004.
- The oscillators' anti-aliasing is the standard two-sample polyBLEP; see the
  credit above `polyBlep()` in `source/SimpleSynthDsp.cpp`.
