# Estimated metric models (v3 dataset)

Nothing here is live or official. Every value is flagged `estimated` in the dataset.

**Scenic (road segment, 0-10).** Start 0.05. Add, for the nearest feature of each kind to the segment midpoint, weight x (1 - distance/radius), clamped at 0: parks/gardens/forest (radius 60 m, weight 0.45), water/peaks/wood/rivers (90 m, 0.35), heritage/tourism features (120 m, 0.30). Subtract a busy-road penalty (motorway 0.25, trunk 0.20, primary 0.15, secondary 0.08). Clamp to 0-1, scale by 10. Most segments score near 0; that is expected, scenic roads are rare.

**Crowd (attraction, hour, 0-10).** Zero outside opening hours. Inside: base = 0.35 x popularity, rising to popularity inside the peak window and falling linearly over 4 hours either side. Weekend = weekday x 1.3, capped at 10.

**Visit minutes, opening hours, popularity, peak window.** Hand-entered in tools/attractions_meta.json. Verify with venues before relying on them.

**Road crowd (segment, hour, weekday/weekend, 0-10).** Computed at load. Max of: (a) road-class congestion (primary/trunk/motorway 3, secondary 2, tertiary 1, else 0) x a time factor (weekday: 1.0 at 08-11 and 17-20, 0.5 at 11-17 and 20-22, 0.2 otherwise; weekend: 0.8 at 11-21, else 0.2); (b) for each attraction within 400 m of the segment midpoint, its hourly curve value x (1 - distance/400). Departure time comes from the request (`hour` 0-23, `weekend` true/false; default 12, weekday).

**Routing modes (real data).** Shortest = distance. Fastest = estimated free-flow time. Scenic = distance x (1.1 - scenic/10). Least crowded = time x (0.1 + crowd/10). Balanced = weighted mix of distance, time, scenic loss and crowd (weights from the request, normalised). `/api/route` returns the requested mode plus `mode_routes` with all five.
