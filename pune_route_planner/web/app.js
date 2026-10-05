/**
 * Pune Tourist Route Planner - Frontend Controller
 * Communicates with C++17 Backend over REST API
 */

// Application State
const state = {
  graph: null,
  placesMap: new Map(),
  roadsMap: new Map(),
  selectedStartId: "SW",
  selectedEndId: "RZ",
  activeMode: "balanced",
  weights: { wd: 0.20, wt: 0.30, ws: 0.20, wc: 0.15, wp: 0.15 },
  selectedInterests: new Set(),
  selectedAvoid: new Set(),
  selectedMustVisit: new Set(),
  currentRoute: null,
  rankedRoutes: [],
  modeRoutes: null,
  selectedRouteIndex: 0,
  selectedRoadId: null,
  
  // Dijkstra Animation State
  settledOrder: [],
  animIndex: 0,
  animTimer: null,
  isAnimating: false,
  
  // Canvas Transform
  canvasBounds: { minX: 1, maxX: 10, minY: 0, maxY: 9 },
  nodePositions: new Map()
};

// DOM Elements
const canvas = document.getElementById("mapCanvas");
const ctx = canvas.getContext("2d");
const canvasContainer = document.getElementById("canvasContainer");
const canvasTooltip = document.getElementById("canvasTooltip");

// Initialize Application
window.addEventListener("DOMContentLoaded", async () => {
  initEventListeners();
  resizeCanvas();
  window.addEventListener("resize", () => {
    resizeCanvas();
    drawGraph();
  });

  await fetchGraphData();
  await fetchStats();
  
  // Initial route calculation
  calculateRoute();
});

function resizeCanvas() {
  const rect = canvasContainer.getBoundingClientRect();
  canvas.width = rect.width * window.devicePixelRatio;
  canvas.height = rect.height * window.devicePixelRatio;
  ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
}

// ==========================================================================
// API Interaction
// ==========================================================================

async function fetchGraphData() {
  try {
    const res = await fetch("/api/graph");
    if (!res.ok) throw new Error("Failed to load graph");
    state.graph = await res.json();

    state.placesMap.clear();
    state.roadsMap.clear();

    let minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    state.graph.places.forEach(p => {
      state.placesMap.set(p.id, p);
      if (p.x < minX) minX = p.x;
      if (p.x > maxX) maxX = p.x;
      if (p.y < minY) minY = p.y;
      if (p.y > maxY) maxY = p.y;
    });

    state.graph.roads.forEach(r => {
      state.roadsMap.set(r.id, r);
    });

    // Add padding to bounds
    const padX = (maxX - minX) * 0.15 || 1.0;
    const padY = (maxY - minY) * 0.15 || 1.0;
    state.canvasBounds = {
      minX: minX - padX,
      maxX: maxX + padX,
      minY: minY - padY,
      maxY: maxY + padY
    };

    populatePlaceSelectors();
    updateTelemetry();
    drawGraph();
  } catch (err) {
    console.error("Error loading graph:", err);
  }
}

async function fetchStats() {
  try {
    const res = await fetch("/api/stats");
    if (res.ok) {
      const stats = await res.json();
      updateTelemetryFromStats(stats);
    }
  } catch (err) {
    console.error("Error fetching stats:", err);
  }
}

async function calculateRoute() {
  const payload = {
    start: state.selectedStartId,
    end: state.selectedEndId,
    mode: state.activeMode,
    weights: state.weights,
    interests: Array.from(state.selectedInterests),
    avoid: Array.from(state.selectedAvoid),
    mustVisit: Array.from(state.selectedMustVisit),
    maxTimeMin: parseFloat(document.getElementById("timeBudgetInput").value) || -1.0,
    maxDistanceKm: parseFloat(document.getElementById("distLimitInput").value) || -1.0,
    k: 5
  };

  const btn = document.getElementById("btnPlanRoute");
  btn.disabled = true;
  btn.innerHTML = `<span class="btn-icon">&#9203;</span> Optimizing Path...`;

  try {
    const res = await fetch("/api/route", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });

    const data = await res.json();
    if (!res.ok) {
      showDiffBanner("Planning Error", data.error || "Route could not be formed");
      document.getElementById("routeSummaryCard").innerHTML = `
        <div class="placeholder-text" style="color: var(--accent-rose); font-style: normal;">
          &#9888; ${data.error || "Infeasible route under current network conditions."}
        </div>
      `;
      state.currentRoute = null;
      state.rankedRoutes = [];
      drawGraph();
      return;
    }

    state.currentRoute = data.best_route;
    state.rankedRoutes = data.ranked_routes || [data.best_route];
    state.modeRoutes = data.mode_routes || null;
    state.selectedRouteIndex = 0;

    // Reset Dijkstra animation with new settled nodes
    state.settledOrder = state.currentRoute.settled_order || [];
    state.animIndex = state.settledOrder.length;
    document.getElementById("animStatus").textContent = `Settled ${state.settledOrder.length} nodes`;

    renderRouteSummary(state.currentRoute);
    renderRankedRoutes(state.rankedRoutes);
    renderComparisonTable();
    renderWhyExplanation(state.currentRoute, payload);
    drawGraph();
  } catch (err) {
    console.error("Route error:", err);
  } finally {
    btn.disabled = false;
    btn.innerHTML = `<span class="btn-icon">&#128640;</span> Calculate Best Route`;
    await fetchStats();
  }
}

async function applyRoadUpdate(type, roadId, val) {
  try {
    const res = await fetch("/api/update", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ type, roadId, value: val })
    });
    const data = await res.json();
    if (!res.ok) {
      alert(data.error || "Update failed");
      return;
    }

    state.graph = data.graph;
    state.roadsMap.clear();
    state.graph.roads.forEach(r => state.roadsMap.set(r.id, r));

    if (data.current_route) {
      state.currentRoute = data.current_route;
      renderRouteSummary(state.currentRoute);
    }
    showDiffBanner("Graph Mutation Applied", data.diff.summary);
    updateQuickControls(roadId);
    await fetchStats();
    drawGraph();
  } catch (err) {
    console.error("Update error:", err);
  }
}

async function triggerUndo() {
  try {
    const res = await fetch("/api/undo", { method: "POST" });
    const data = await res.json();
    if (!res.ok) {
      alert(data.error || "Undo failed");
      return;
    }

    state.graph = data.graph;
    state.roadsMap.clear();
    state.graph.roads.forEach(r => state.roadsMap.set(r.id, r));

    if (data.current_route) {
      state.currentRoute = data.current_route;
      renderRouteSummary(state.currentRoute);
    }
    showDiffBanner("Undo Executed (Stack Pop)", data.diff.summary);
    await fetchStats();
    drawGraph();
  } catch (err) {
    console.error("Undo error:", err);
  }
}

async function triggerSimulationEvent() {
  try {
    const res = await fetch("/api/simulate", { method: "POST" });
    const data = await res.json();
    if (!res.ok) {
      alert(data.error || "Simulation failed");
      return;
    }

    state.graph = data.graph;
    state.roadsMap.clear();
    state.graph.roads.forEach(r => state.roadsMap.set(r.id, r));

    if (data.current_route) {
      state.currentRoute = data.current_route;
      renderRouteSummary(state.currentRoute);
    }
    showDiffBanner("Simulated Event: " + data.event_title, data.diff.summary);
    await fetchStats();
    drawGraph();
  } catch (err) {
    console.error("Simulate error:", err);
  }
}

// ==========================================================================
// UI Setup & Event Listeners
// ==========================================================================

function initEventListeners() {
  // Mode selection buttons
  document.querySelectorAll(".mode-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".mode-btn").forEach(b => b.classList.remove("active"));
      btn.classList.add("active");
      state.activeMode = btn.dataset.mode;
      calculateRoute();
    });
  });

  // Slider events
  const sliders = ["sliderWd", "sliderWt", "sliderWs", "sliderWc", "sliderWp"];
  sliders.forEach(id => {
    document.getElementById(id).addEventListener("input", updateWeightsFromSliders);
  });

  document.getElementById("btnResetWeights").addEventListener("click", () => {
    document.getElementById("sliderWd").value = 20;
    document.getElementById("sliderWt").value = 30;
    document.getElementById("sliderWs").value = 20;
    document.getElementById("sliderWc").value = 15;
    document.getElementById("sliderWp").value = 15;
    updateWeightsFromSliders();
    calculateRoute();
  });

  // Interests chips
  document.getElementById("interestsChips").addEventListener("change", (e) => {
    if (e.target.tagName === "INPUT") {
      if (e.target.checked) state.selectedInterests.add(e.target.value);
      else state.selectedInterests.delete(e.target.value);
      calculateRoute();
    }
  });

  // Avoid chips
  document.getElementById("avoidChips").addEventListener("change", (e) => {
    if (e.target.tagName === "INPUT") {
      if (e.target.checked) state.selectedAvoid.add(e.target.value);
      else state.selectedAvoid.delete(e.target.value);
      calculateRoute();
    }
  });

  // Plan Route Button
  document.getElementById("btnPlanRoute").addEventListener("click", calculateRoute);

  // Undo Button
  document.getElementById("btnUndoAction").addEventListener("click", triggerUndo);

  // Simulate Event Button
  document.getElementById("btnSimulateEvent").addEventListener("click", triggerSimulationEvent);

  // Close Diff Banner
  document.getElementById("btnCloseDiff").addEventListener("click", () => {
    document.getElementById("diffBanner").classList.add("hidden");
  });

  // Stats Modal
  document.getElementById("btnStatsModal").addEventListener("click", () => {
    document.getElementById("statsModal").classList.remove("hidden");
  });
  document.getElementById("btnCloseStatsModal").addEventListener("click", () => {
    document.getElementById("statsModal").classList.add("hidden");
  });

  // Autocomplete bindings
  setupAutocomplete("startInput", "startDropdown", (placeId) => {
    state.selectedStartId = placeId;
    document.getElementById("startSelect").value = placeId;
    calculateRoute();
  });

  setupAutocomplete("endInput", "endDropdown", (placeId) => {
    state.selectedEndId = placeId;
    document.getElementById("endSelect").value = placeId;
    calculateRoute();
  });

  // Select dropdown bindings
  document.getElementById("startSelect").addEventListener("change", (e) => {
    state.selectedStartId = e.target.value;
    document.getElementById("startInput").value = state.placesMap.get(e.target.value)?.name || "";
    calculateRoute();
  });

  document.getElementById("endSelect").addEventListener("change", (e) => {
    state.selectedEndId = e.target.value;
    document.getElementById("endInput").value = state.placesMap.get(e.target.value)?.name || "";
    calculateRoute();
  });

  // Canvas Mouse Interactions
  canvas.addEventListener("mousemove", handleCanvasMouseMove);
  canvas.addEventListener("click", handleCanvasClick);

  // Dijkstra Animation Controls
  document.getElementById("btnAnimPlay").addEventListener("click", playDijkstraAnimation);
  document.getElementById("btnAnimStep").addEventListener("click", stepDijkstraAnimation);
  document.getElementById("btnAnimReset").addEventListener("click", resetDijkstraAnimation);
}

function updateWeightsFromSliders() {
  const wd = parseInt(document.getElementById("sliderWd").value);
  const wt = parseInt(document.getElementById("sliderWt").value);
  const ws = parseInt(document.getElementById("sliderWs").value);
  const wc = parseInt(document.getElementById("sliderWc").value);
  const wp = parseInt(document.getElementById("sliderWp").value);

  const total = wd + wt + ws + wc + wp || 1;
  state.weights = {
    wd: wd / total,
    wt: wt / total,
    ws: ws / total,
    wc: wc / total,
    wp: wp / total
  };

  document.getElementById("wdVal").textContent = Math.round(state.weights.wd * 100) + "%";
  document.getElementById("wtVal").textContent = Math.round(state.weights.wt * 100) + "%";
  document.getElementById("wsVal").textContent = Math.round(state.weights.ws * 100) + "%";
  document.getElementById("wcVal").textContent = Math.round(state.weights.wc * 100) + "%";
  document.getElementById("wpVal").textContent = Math.round(state.weights.wp * 100) + "%";
}

function populatePlaceSelectors() {
  const startSel = document.getElementById("startSelect");
  const endSel = document.getElementById("endSelect");
  const mvContainer = document.getElementById("mustVisitContainer");

  startSel.innerHTML = "";
  endSel.innerHTML = "";
  mvContainer.innerHTML = "";

  state.graph.places.forEach(p => {
    const optStart = document.createElement("option");
    optStart.value = p.id;
    optStart.textContent = `${p.id} - ${p.name}`;
    if (p.id === state.selectedStartId) optStart.selected = true;
    startSel.appendChild(optStart);

    const optEnd = document.createElement("option");
    optEnd.value = p.id;
    optEnd.textContent = `${p.id} - ${p.name}`;
    if (p.id === state.selectedEndId) optEnd.selected = true;
    endSel.appendChild(optEnd);

    // Must-Visit multiselect
    const item = document.createElement("label");
    item.className = "multiselect-item";
    item.innerHTML = `<input type="checkbox" value="${p.id}"> [${p.id}] ${p.name}`;
    item.querySelector("input").addEventListener("change", (e) => {
      if (e.target.checked) state.selectedMustVisit.add(e.target.value);
      else state.selectedMustVisit.delete(e.target.value);
      calculateRoute();
    });
    mvContainer.appendChild(item);
  });

  document.getElementById("startInput").value = state.placesMap.get(state.selectedStartId)?.name || "";
  document.getElementById("endInput").value = state.placesMap.get(state.selectedEndId)?.name || "";
}

function setupAutocomplete(inputId, dropdownId, onSelect) {
  const input = document.getElementById(inputId);
  const dropdown = document.getElementById(dropdownId);

  input.addEventListener("input", async () => {
    const q = input.value.trim();
    if (!q) {
      dropdown.classList.add("hidden");
      return;
    }

    try {
      const res = await fetch(`/api/autocomplete?q=${encodeURIComponent(q)}`);
      if (!res.ok) return;
      const matches = await res.json();
      dropdown.innerHTML = "";
      if (matches.length === 0) {
        dropdown.classList.add("hidden");
        return;
      }

      matches.forEach(m => {
        const item = document.createElement("div");
        item.className = "autocomplete-item";
        item.textContent = `[${m.place_id}] ${m.full_name}`;
        item.addEventListener("click", () => {
          input.value = m.full_name;
          dropdown.classList.add("hidden");
          onSelect(m.place_id);
        });
        dropdown.appendChild(item);
      });
      dropdown.classList.remove("hidden");
    } catch (e) {
      console.error(e);
    }
  });

  document.addEventListener("click", (e) => {
    if (!input.contains(e.target) && !dropdown.contains(e.target)) {
      dropdown.classList.add("hidden");
    }
  });
}

function showDiffBanner(title, content) {
  const banner = document.getElementById("diffBanner");
  document.getElementById("diffTitle").textContent = title;
  document.getElementById("diffContent").textContent = content;
  banner.classList.remove("hidden");
}

function updateTelemetryFromStats(stats) {
  document.getElementById("telNodes").textContent = stats.num_places || 10;
  document.getElementById("telRoads").textContent = stats.num_roads || 22;

  const compBadge = document.getElementById("telComponents");
  const comps = stats.connected_components || 1;
  compBadge.textContent = `${comps} ${comps === 1 ? '(Fully Connected)' : '(Disconnected!)'}`;
  if (comps > 1) {
    compBadge.classList.add("warning");
  } else {
    compBadge.classList.remove("warning");
  }

  document.getElementById("telHashLoad").textContent = (stats.hash_load_factor || 0).toFixed(2);
  document.getElementById("telUndoDepth").textContent = stats.undo_stack_depth || 0;
  document.getElementById("telEventQueue").textContent = stats.pending_events_count || 0;

  document.getElementById("undoCount").textContent = stats.undo_stack_depth || 0;
  document.getElementById("btnUndoAction").disabled = (stats.undo_stack_depth || 0) === 0;

  // Modal stats
  document.getElementById("statRoadsCount").textContent = stats.num_roads || 22;
  document.getElementById("statHashLf").textContent = (stats.hash_load_factor || 0).toFixed(2);
  document.getElementById("statHashBuckets").textContent = stats.hash_bucket_count || 16;
  document.getElementById("statUndoDepth").textContent = stats.undo_stack_depth || 0;
  document.getElementById("statEventQueue").textContent = stats.pending_events_count || 0;
  document.getElementById("statDsuComp").textContent = comps;
}

function updateTelemetry() {
  if (!state.graph) return;
  document.getElementById("telNodes").textContent = state.graph.places.length;
  document.getElementById("telRoads").textContent = state.graph.roads.length;
}

// ==========================================================================
// Canvas Drawing & Map Rendering
// ==========================================================================

function toScreenCoords(x, y) {
  const rect = canvasContainer.getBoundingClientRect();
  const b = state.canvasBounds;
  // Flip Y so schematic Cartesian (0,0 bottom-left) renders intuitively
  const screenX = ((x - b.minX) / (b.maxX - b.minX)) * rect.width;
  const screenY = rect.height - (((y - b.minY) / (b.maxY - b.minY)) * rect.height);
  return { x: screenX, y: screenY };
}

function drawGraph() {
  if (!state.graph) return;
  const rect = canvasContainer.getBoundingClientRect();
  ctx.clearRect(0, 0, rect.width, rect.height);

  // Pre-calculate node screen positions
  state.nodePositions.clear();
  state.graph.places.forEach(p => {
    state.nodePositions.set(p.id, toScreenCoords(p.x, p.y));
  });

  const activeRoads = new Set(state.currentRoute?.roads || []);

  // 1. Draw Roads
  state.graph.roads.forEach(road => {
    const p1 = state.nodePositions.get(road.u);
    const p2 = state.nodePositions.get(road.v);
    if (!p1 || !p2) return;

    const isActive = activeRoads.has(road.id);

    ctx.save();
    ctx.beginPath();
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);

    if (road.blocked) {
      // Blocked Road Style
      ctx.strokeStyle = "#f43f5e";
      ctx.lineWidth = 3;
      ctx.setLineDash([6, 6]);
      ctx.stroke();

      // Draw X marker at midpoint
      const midX = (p1.x + p2.x) / 2;
      const midY = (p1.y + p2.y) / 2;
      ctx.fillStyle = "#f43f5e";
      ctx.font = "bold 14px sans-serif";
      ctx.fillText("✕", midX - 5, midY + 5);
    } else {
      // Crowd color palette: Low (Emerald) -> Mid (Amber) -> High (Rose)
      let roadColor = "rgba(100, 116, 139, 0.4)";
      if (road.crowd <= 4) roadColor = "rgba(16, 185, 129, 0.4)";
      else if (road.crowd <= 7) roadColor = "rgba(245, 158, 11, 0.45)";
      else roadColor = "rgba(244, 63, 94, 0.5)";

      ctx.strokeStyle = roadColor;
      ctx.lineWidth = 2.5 + (road.traffic / 10) * 2;
      ctx.stroke();

      // Road ID label
      const midX = (p1.x + p2.x) / 2;
      const midY = (p1.y + p2.y) / 2;
      ctx.fillStyle = "rgba(148, 163, 184, 0.7)";
      ctx.font = "9px monospace";
      ctx.fillText(road.id, midX - 8, midY - 4);
    }
    ctx.restore();
  });

  // 2. Draw Highlighted Active Route (Glowing Path)
  if (state.currentRoute && state.currentRoute.legs) {
    ctx.save();
    state.currentRoute.legs.forEach(leg => {
      const p1 = state.nodePositions.get(leg.from_id);
      const p2 = state.nodePositions.get(leg.to_id);
      if (!p1 || !p2) return;

      // Outer Glow
      ctx.beginPath();
      ctx.moveTo(p1.x, p1.y);
      ctx.lineTo(p2.x, p2.y);
      ctx.strokeStyle = "rgba(6, 182, 212, 0.4)";
      ctx.lineWidth = 9;
      ctx.stroke();

      // Inner Core
      ctx.beginPath();
      ctx.moveTo(p1.x, p1.y);
      ctx.lineTo(p2.x, p2.y);
      ctx.strokeStyle = "#06b6d4";
      ctx.lineWidth = 3.5;
      ctx.stroke();
    });
    ctx.restore();
  }

  // 3. Draw Nodes (Attractions)
  const settledSet = new Set();
  for (let i = 0; i < state.animIndex && i < state.settledOrder.length; ++i) {
    settledSet.add(state.settledOrder[i]);
  }

  state.graph.places.forEach(place => {
    const pos = state.nodePositions.get(place.id);
    if (!pos) return;

    const isStart = place.id === state.selectedStartId;
    const isEnd = place.id === state.selectedEndId;
    const isMust = state.selectedMustVisit.has(place.id);
    const isSettled = settledSet.has(place.index);

    ctx.save();

    // Settled Node Animation Glow
    if (isSettled) {
      ctx.beginPath();
      ctx.arc(pos.x, pos.y, 16, 0, Math.PI * 2);
      ctx.fillStyle = "rgba(139, 92, 246, 0.35)";
      ctx.fill();
    }

    // Node Base Circle
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, 10, 0, Math.PI * 2);

    if (isStart) {
      ctx.fillStyle = "#10b981"; // Green
    } else if (isEnd) {
      ctx.fillStyle = "#f43f5e"; // Rose
    } else if (isMust) {
      ctx.fillStyle = "#f59e0b"; // Amber
    } else {
      ctx.fillStyle = "#334155"; // Slate
    }
    ctx.fill();

    ctx.strokeStyle = isSettled ? "#c4b5fd" : "#ffffff";
    ctx.lineWidth = 2;
    ctx.stroke();

    // Node Code Label inside circle
    ctx.fillStyle = "#ffffff";
    ctx.font = "bold 8px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(place.id, pos.x, pos.y);

    // Place Name Label
    ctx.font = "600 11px sans-serif";
    ctx.fillStyle = "#f8fafc";
    ctx.fillText(place.name, pos.x, pos.y + 19);

    ctx.restore();
  });
}

function handleCanvasMouseMove(e) {
  const rect = canvas.getBoundingClientRect();
  const mouseX = e.clientX - rect.left;
  const mouseY = e.clientY - rect.top;

  let hoveredPlace = null;
  let hoveredRoad = null;

  // Check Node hover
  for (const [id, pos] of state.nodePositions.entries()) {
    const dist = Math.hypot(mouseX - pos.x, mouseY - pos.y);
    if (dist <= 14) {
      hoveredPlace = state.placesMap.get(id);
      break;
    }
  }

  // Check Road hover if not hovering node
  if (!hoveredPlace && state.graph) {
    for (const road of state.graph.roads) {
      const p1 = state.nodePositions.get(road.u);
      const p2 = state.nodePositions.get(road.v);
      if (!p1 || !p2) continue;

      const d = distToSegment({ x: mouseX, y: mouseY }, p1, p2);
      if (d <= 8) {
        hoveredRoad = road;
        break;
      }
    }
  }

  if (hoveredPlace) {
    canvasTooltip.classList.remove("hidden");
    canvasTooltip.style.left = `${mouseX + 15}px`;
    canvasTooltip.style.top = `${mouseY + 15}px`;
    canvasTooltip.innerHTML = `
      <strong>${hoveredPlace.name} (${hoveredPlace.id})</strong><br>
      Categories: ${hoveredPlace.categories.join(", ")}<br>
      Typical Visit: ${hoveredPlace.visit_minutes} min<br>
      Crowd Level: ${hoveredPlace.crowd}/10
    `;
    canvas.style.cursor = "pointer";
  } else if (hoveredRoad) {
    canvasTooltip.classList.remove("hidden");
    canvasTooltip.style.left = `${mouseX + 15}px`;
    canvasTooltip.style.top = `${mouseY + 15}px`;
    canvasTooltip.innerHTML = `
      <strong>Corridor ${hoveredRoad.id} (${hoveredRoad.u} &harr; ${hoveredRoad.v})</strong><br>
      Distance: ${hoveredRoad.distance_km} km | Time: ${hoveredRoad.effective_time_min.toFixed(1)} min<br>
      Traffic: ${hoveredRoad.traffic}/10 | Crowd: ${hoveredRoad.crowd}/10 | Scenic: ${hoveredRoad.scenic}/10<br>
      Status: ${hoveredRoad.blocked ? '<span style="color:#f43f5e">BLOCKED</span>' : '<span style="color:#10b981">OPEN</span>'} (Click to toggle)
    `;
    canvas.style.cursor = "pointer";
  } else {
    canvasTooltip.classList.add("hidden");
    canvas.style.cursor = "default";
  }
}

function handleCanvasClick(e) {
  const rect = canvas.getBoundingClientRect();
  const mouseX = e.clientX - rect.left;
  const mouseY = e.clientY - rect.top;

  // Check Road click
  if (state.graph) {
    for (const road of state.graph.roads) {
      const p1 = state.nodePositions.get(road.u);
      const p2 = state.nodePositions.get(road.v);
      if (!p1 || !p2) continue;

      if (distToSegment({ x: mouseX, y: mouseY }, p1, p2) <= 8) {
        state.selectedRoadId = road.id;
        updateQuickControls(road.id);
        return;
      }
    }
  }

  // Check Node click to set start/end
  for (const [id, pos] of state.nodePositions.entries()) {
    if (Math.hypot(mouseX - pos.x, mouseY - pos.y) <= 14) {
      if (id !== state.selectedStartId) {
        state.selectedEndId = id;
        document.getElementById("endSelect").value = id;
        document.getElementById("endInput").value = state.placesMap.get(id)?.name || "";
      } else {
        state.selectedStartId = id;
      }
      calculateRoute();
      return;
    }
  }
}

function distToSegment(p, v, w) {
  const l2 = (v.x - w.x) ** 2 + (v.y - w.y) ** 2;
  if (l2 === 0) return Math.hypot(p.x - v.x, p.y - v.y);
  let t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
  t = Math.max(0, Math.min(1, t));
  return Math.hypot(p.x - (v.x + t * (w.x - v.x)), p.y - (v.y + t * (w.y - v.y)));
}

function updateQuickControls(roadId) {
  const road = state.roadsMap.get(roadId);
  const container = document.getElementById("quickControlsArea");
  if (!road) return;

  container.innerHTML = `
    <span>Corridor <strong>${road.id} (${road.u} &harr; ${road.v})</strong>:</span>
    <button class="btn btn-xs ${road.blocked ? 'btn-primary' : 'btn-secondary'}" id="btnToggleBlock">
      ${road.blocked ? 'Unblock Road' : 'Block Road'}
    </button>
    <label style="font-size: 0.72rem; color: var(--text-muted)">Traffic:</label>
    <input type="range" id="quickTrafficSlider" min="0" max="10" value="${road.traffic}" style="width: 80px">
    <span id="quickTrafficVal" style="font-size: 0.72rem">${road.traffic}</span>
    <button class="btn btn-xs btn-outline" id="btnCrowdSpike">&#128293; Crowd Spike</button>
  `;

  document.getElementById("btnToggleBlock").addEventListener("click", () => {
    applyRoadUpdate(road.blocked ? "unblock" : "block", road.id, road.blocked ? 0 : 1);
  });

  const slider = document.getElementById("quickTrafficSlider");
  slider.addEventListener("input", (e) => {
    document.getElementById("quickTrafficVal").textContent = e.target.value;
  });
  slider.addEventListener("change", (e) => {
    applyRoadUpdate("traffic", road.id, parseFloat(e.target.value));
  });

  document.getElementById("btnCrowdSpike").addEventListener("click", () => {
    applyRoadUpdate("road_crowd", road.id, 9.0);
  });
}

// ==========================================================================
// Dijkstra Step-Through Animation Controller
// ==========================================================================

function playDijkstraAnimation() {
  if (state.isAnimating) return;
  state.isAnimating = true;
  state.animIndex = 0;
  document.getElementById("animStatus").textContent = "Exploring graph...";

  clearInterval(state.animTimer);
  state.animTimer = setInterval(() => {
    if (state.animIndex < state.settledOrder.length) {
      state.animIndex++;
      const nodeId = state.settledOrder[state.animIndex - 1];
      const place = state.graph.places[nodeId];
      document.getElementById("animStatus").textContent = `Settled: ${place ? place.name : nodeId}`;
      drawGraph();
    } else {
      clearInterval(state.animTimer);
      state.isAnimating = false;
      document.getElementById("animStatus").textContent = "Shortest path settled!";
    }
  }, 400);
}

function stepDijkstraAnimation() {
  clearInterval(state.animTimer);
  state.isAnimating = false;
  if (state.animIndex < state.settledOrder.length) {
    state.animIndex++;
    const nodeId = state.settledOrder[state.animIndex - 1];
    const place = state.graph.places[nodeId];
    document.getElementById("animStatus").textContent = `Settled: ${place ? place.name : nodeId}`;
    drawGraph();
  }
}

function resetDijkstraAnimation() {
  clearInterval(state.animTimer);
  state.isAnimating = false;
  state.animIndex = state.settledOrder.length;
  document.getElementById("animStatus").textContent = `Settled ${state.settledOrder.length} nodes`;
  drawGraph();
}

// ==========================================================================
// Right Panel Renderers
// ==========================================================================

function renderRouteSummary(route) {
  const container = document.getElementById("routeSummaryCard");
  document.getElementById("routeModeBadge").textContent = route.mode_label.toUpperCase();

  let stopsHtml = "";
  if (route.stops && route.stops.length > 0) {
    stopsHtml = `
      <div style="margin-top: 10px; font-size: 0.74rem;">
        <strong>Tour Itinerary (${route.stops.length} stops):</strong>
        <div style="display: flex; flex-direction: column; gap: 4px; margin-top: 6px;">
          ${route.stops.map((st, i) => `
            <div style="display: flex; justify-content: space-between; padding: 4px 8px; background: var(--bg-primary); border-radius: 4px;">
              <span>${i + 1}. [${st.place_id}] ${st.name} ${st.is_must_visit ? '<span class="badge" style="background: rgba(245, 158, 11, 0.2); color: #fbbf24;">Mandatory</span>' : ''}</span>
              <span style="color: var(--text-dim)">${st.visit_minutes > 0 ? st.visit_minutes + ' min visit' : 'Transit'}</span>
            </div>
          `).join("")}
        </div>
      </div>
    `;
  }

  container.innerHTML = `
    <div class="metrics-grid">
      <div class="metric-box">
        <div class="metric-label">Total Distance</div>
        <div class="metric-val">${route.total_distance_km.toFixed(1)} <span class="metric-unit">km</span></div>
      </div>
      <div class="metric-box">
        <div class="metric-label">Travel Time</div>
        <div class="metric-val">${route.total_travel_time_min.toFixed(0)} <span class="metric-unit">min</span></div>
      </div>
      <div class="metric-box">
        <div class="metric-label">Scenic Rating</div>
        <div class="metric-val">${route.avg_scenic.toFixed(1)} <span class="metric-unit">/10</span></div>
      </div>
      <div class="metric-box">
        <div class="metric-label">Crowd Exposure</div>
        <div class="metric-val">${route.avg_crowd.toFixed(1)} <span class="metric-unit">/10</span></div>
      </div>
    </div>

    <div style="display: flex; justify-content: space-between; font-size: 0.74rem; color: var(--text-muted); background: var(--bg-primary); padding: 8px 10px; border-radius: 6px;">
      <span>Balanced Cost: <strong>${route.balanced_cost.toFixed(3)}</strong></span>
      <span>Demo Index: <strong style="color: var(--accent-cyan)">${route.demo_index.toFixed(1)} / 100</strong></span>
    </div>

    <div style="font-size: 0.74rem; color: var(--text-dim); line-height: 1.4;">
      Corridors: <code>${route.roads ? route.roads.join(" &rarr; ") : "None"}</code>
    </div>

    ${stopsHtml}
  `;
}

function renderRankedRoutes(routes) {
  const container = document.getElementById("routesList");
  document.getElementById("altCountBadge").textContent = `${routes.length} options`;
  container.innerHTML = "";

  routes.forEach((r, idx) => {
    const item = document.createElement("div");
    item.className = `route-item ${idx === state.selectedRouteIndex ? 'selected' : ''}`;
    item.innerHTML = `
      <div class="route-item-header">
        <span>Option #${idx + 1} (${r.mode_label})</span>
        <span style="color: var(--accent-cyan); font-weight: bold;">${r.demo_index.toFixed(1)} idx</span>
      </div>
      <div class="route-item-stats">
        <span>${r.total_distance_km.toFixed(1)} km</span>
        <span>&bull;</span>
        <span>${r.total_travel_time_min.toFixed(0)} min</span>
        <span>&bull;</span>
        <span>Scenic: ${r.avg_scenic.toFixed(1)}</span>
        <span>&bull;</span>
        <span>Cost: ${r.balanced_cost.toFixed(2)}</span>
      </div>
    `;

    item.addEventListener("click", () => {
      document.querySelectorAll(".route-item").forEach(el => el.classList.remove("selected"));
      item.classList.add("selected");
      state.selectedRouteIndex = idx;
      state.currentRoute = routes[idx];
      renderRouteSummary(routes[idx]);
      drawGraph();
    });

    container.appendChild(item);
  });
}

function renderComparisonTable() {
  const tbody = document.querySelector("#comparisonTable tbody");
  if (!state.modeRoutes) {
    tbody.innerHTML = `<tr><td colspan="6" class="placeholder-text text-center">Standard modes comparison unavailable for multi-stop tour</td></tr>`;
    return;
  }

  tbody.innerHTML = "";
  const modes = [
    { key: "balanced", label: "Balanced" },
    { key: "fastest", label: "Fastest" },
    { key: "shortest", label: "Shortest" },
    { key: "scenic", label: "Scenic" },
    { key: "least_crowded", label: "Least Crowded" }
  ];

  modes.forEach(m => {
    const r = state.modeRoutes[m.key];
    if (!r || !r.found) return;

    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td><strong>${m.label}</strong></td>
      <td>${r.total_distance_km.toFixed(1)}</td>
      <td>${r.total_travel_time_min.toFixed(1)}</td>
      <td>${r.avg_scenic.toFixed(1)}</td>
      <td>${r.avg_crowd.toFixed(1)}</td>
      <td><span style="color: var(--accent-cyan); font-weight: 600;">${r.demo_index.toFixed(0)}</span></td>
    `;
    tbody.appendChild(tr);
  });
}

function renderWhyExplanation(route, query) {
  const container = document.getElementById("explanationContent");
  if (!route || !route.found) {
    container.innerHTML = `<p class="placeholder-text">No route selected.</p>`;
    return;
  }

  const interestsList = query.interests.length > 0 ? query.interests.join(", ") : "general exploration";
  let explanation = `This corridor was selected by optimizing the 5-criterion balanced objective with weights: `;
  explanation += `Distance ${(query.weights.wd * 100).toFixed(0)}%, Time ${(query.weights.wt * 100).toFixed(0)}%, `;
  explanation += `Scenic ${(query.weights.ws * 100).toFixed(0)}%, Crowd ${(query.weights.wc * 100).toFixed(0)}%, and Interest Match ${(query.weights.wp * 100).toFixed(0)}%.<br><br>`;

  if (state.modeRoutes && state.modeRoutes.shortest) {
    const shortest = state.modeRoutes.shortest;
    const timeDiff = shortest.total_travel_time_min - route.total_travel_time_min;
    if (timeDiff > 1.0) {
      explanation += `&#9889; Saves <strong>${timeDiff.toFixed(1)} min</strong> over the shortest-distance route by bypassing congested street bottlenecks.<br>`;
    }
  }

  explanation += `&#127963; Prioritizes corridors matching your interest in <strong>${interestsList}</strong> while avoiding highly congested temple and commercial stretches during peak hours.`;
  container.innerHTML = explanation;
}
