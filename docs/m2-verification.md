# M2 local verification

October 5, 2026. Parent main at start: a0bc425. No field-navigation guarantee.

- Offline import rerun from Geofabrik Western India 2026-10-04 PBF: byte-identical
  bounded Pune JSON, 32,186 vertices / 35,382 segments / 25 node turn rules.
- Importer policy tests: specific motorcar access, private/destination/conditional
  access rejection, directional-tag rejection, reverse one-way acceptance,
  distance calculation and geographical bounds.
- Debug and Release: all four CTest groups pass, including legacy C++/HTTP,
  incoming-arc turn regressions and real-data API/malformed-data rejection.
- ASan/UBSan: all four pass with leak detection enabled and 16 MiB quarantine.
  Default ASan quarantine exhausted this 2 GiB workspace under real graph requests,
  without an ASan/UBSan finding. The smaller quarantine is recorded in CI.
- Installed package: executable + real dataset + web_real only. Runs without
  tests/fixtures, no synthetic fallback. Default installed launch tested.
- Chrome: production route calculation, estimated-fastest, sizes 320/390/768/1440,
  no document overflow or uncaught page errors. Mobile and desktop pixels inspected.
- Legacy Chrome fixture calculate/simulate/undo checks still pass.

Not tested: Windows/macOS, real phones, screen readers, sustained load, field
entrances/parking/access, complete OSM tag semantics or exhaustive adversarial
security. Historical synthetic UI checks are not real-driving evidence.

Nine source-ID-based catalog entries. Zoo omitted. No tours/interest optimization
without verified visit-duration/catalog data, no fabricated conditions or ratings.
One objective route per production request. Complex via-way/conditional turns and
some directional access tags are conservatively excluded. Same-way no-U-turn
rules can exclude legal same-way continuations. These choices can remove valid
routes; missing OSM rules or changing road conditions remain a real limitation.
