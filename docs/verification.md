# Historical M0-M1 verification (superseded runtime)

The checks below describe the synthetic test fixture only. M2 production uses the real OSM graph and web_real UI documented in README.md. Do not use these historical numbers as driving data.

# M0-M1 fix verification

Baseline: 9af5a9e9c73b13627d08aea4d70e5d3413923242.
Scope: test truth, routing/API correctness, responsive fixes. Not real-data ingestion.

Root CTest now discovers the legacy C++ suite and real HTTP regressions.
Legacy assertions remain enabled in Release. Added 500 endpoint/objective comparisons,
tour limit and zero-edge regressions, JSON grammar/Unicode checks, and request validation
cases. The HTTP suite starts an isolated loopback process and tests update/undo limits.

Fixed root causes:
- Alternative selection ranked everything by balanced score: now ranks the selected objective.
- Tour feasibility used fastest legs then stitched selected-mode legs: now measures the same legs throughout.
- Tour distance ignored: check mandatory, optional insertion and final metrics.
- Round-trip start visit was counted inconsistently: endpoint visits are zero throughout.
- 2-opt ignored directed interior reversals: now measures the entire proposed directed sequence.
- API errors interpolated IDs into JSON: one serializer-backed error helper.
- Wrong types/enums silently fell back; JSON accepted trailing/invalid numbers: reject explicitly.
- Dynamic recalculation bypassed hard limits and hid infeasible routes: shared solve and explicit result.
- Fixed desktop columns forced mobile overflow: responsive one-column layout and contained table scroll.
- Explanations incorrectly said every route used balanced weights: short objective-specific text.

Verified locally on Linux:
- Debug and Release CMake builds; root CTest both pass.
- AddressSanitizer + UndefinedBehaviorSanitizer build and both CTest entries pass.
- Actual headless Chrome: calculate fastest SW-SB gives 22.8 minutes; simulate and undo work.
- Page width equals viewport width at 320, 390, 768 and 1440 pixels; no uncaught page errors.
- Inspected desktop/mobile screenshots; schematic labels can overlap in the dense center
  (this is not a real geographic map). No real-device or screen-reader testing.

Added CI is configuration only until GitHub executes it. Windows/macOS, sustained load,
full security audit, ThreadSanitizer and a production driving dataset are not validated.
The fixture and simulation remain clearly labeled test/demo behavior pending M2.
