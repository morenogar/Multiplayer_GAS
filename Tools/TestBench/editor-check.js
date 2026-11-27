// Drives the map editor with no browser: captures the canvas listeners, synthesises pointer
// events, and checks that what the tools build is what the pathfinding then sees. The toolbar's
// own rendering is the one thing this cannot speak for.
//
// Usage: node editor-check.js

const fs = require("fs");
const path = require("path");

const listeners = {};

function stub(name) {
  const target = function () { return stub(name + "()"); };
  return new Proxy(target, {
    get(_, prop) {
      if (prop === Symbol.toPrimitive || prop === "toString") return () => "";
      if (prop === "style" || prop === "classList" || prop === "dataset") return stub(name + "." + prop);
      if (prop === "selectedOptions" || prop === "files") return [stub(name + "[0]")];
      if (prop === "value") return "1";
      if (prop === "checked") return true;
      if (prop === "clientWidth") return 1000;
      if (prop === "textContent" || prop === "innerHTML") return "";
      if (prop === "hidden") return false;
      return stub(name + "." + String(prop));
    },
    set() { return true; },
    apply() { return stub(name + "()"); },
  });
}

const ctxStub = new Proxy({}, { get: () => () => {}, set: () => true });

// One object per canvas, kept: the page resizes them on boot, and a rect that disagreed with
// the canvas it describes scales every synthesised click - which is what it did at first.
function makeCanvas(id) {
  const el = {
    width: 1600, height: 1000, style: {}, getContext: () => ctxStub,
    addEventListener: (type, fn) => { if (id === "map") listeners[type] = fn; },
    setPointerCapture: () => {},
    releasePointerCapture: () => {},
    parentElement: { clientWidth: 1000 },
  };
  el.getBoundingClientRect = () => ({ left: 0, top: 0, width: el.width, height: el.height });
  return el;
}
const canvases = { map: makeCanvas("map"), strip: makeCanvas("strip") };

global.document = {
  getElementById: id => canvases[id] || stub(id),
  documentElement: {},
  addEventListener: () => {},
};
global.window = { devicePixelRatio: 1, addEventListener: () => {}, matchMedia: () => ({ matches: false }) };
global.getComputedStyle = () => ({ getPropertyValue: () => "#000" });
global.performance = { now: () => 0 };
global.requestAnimationFrame = () => 0;
global.localStorage = {
  store: {},
  getItem(k) { return this.store[k] || null; },
  setItem(k, v) { this.store[k] = String(v); },
};
global.navigator = { clipboard: { writeText: async () => {}, readText: async () => "{}" } };
global.fetch = async () => ({ ok: false, status: 0 });
global.FileReader = function () {};
global.CanvasRenderingContext2D = function () {};

const html = fs.readFileSync(path.join(__dirname, "..", "FollowLab.html"), "utf8").split("\n");
const src = html.slice(html.findIndex(l => l.trim() === "<script>") + 1,
                       html.findIndex(l => l.trim() === "</script>")).join("\n");
const run = new Function(src + "\n;return { P, comp, leader, view, OBSTACLES: () => OBSTACLES," +
  " PROPS: () => PROPS, ROUTE: () => ROUTE, DEFAULT_MAP, sanitizeMap, mapAsData, applyMap," +
  " pickAt, eraseAt, insideObstacle, furthestClear, setMapMode, setTool(t){ buildTool = t; }," +
  " BODY_RADIUS, resolveObstacles, setDrive(m){ driveMode = m; }," +
  " clickTarget(){ return clickTarget; }," +
  " findPath, lineClearForBody, steerAlongPath, openWayYaw, rot, yawOf, angleDelta, sub," +
  " randomMap, MAZE, mapForUnreal," +
  " setLook(v){ lookAtCompanion = v; }," +
  " mode(){ return mapMode; }, running(){ return running; }, stepLeader, stepCompanion," +
  " updateLeaderAttention, resetSim, dist, V, savedMap(){ return localStorage.getItem('viper-follow-lab-map'); } };");
const lab = run();

let failures = 0;
function check(label, got, want) {
  const ok = JSON.stringify(got) === JSON.stringify(want);
  if (!ok) failures++;
  console.log(`${ok ? "ok  " : "FAIL"} ${label}${ok ? "" : `   got ${JSON.stringify(got)} want ${JSON.stringify(want)}`}`);
}

/** World centimetres to the client pixels the real canvas would have reported. */
function clientOf(wx, wy) {
  return { clientX: wx * lab.view.s + lab.view.ox, clientY: wy * lab.view.s + lab.view.oy, pointerId: 1 };
}

function press(wx, wy) { listeners.pointerdown(clientOf(wx, wy)); }
function moveTo(wx, wy) { listeners.pointermove(clientOf(wx, wy)); }
function release() { listeners.pointerup({ pointerId: 1 }); }

console.log("--- the tools ---");
check("the canvas has pointer handlers", ["pointerdown", "pointermove", "pointerup"]
  .every(t => typeof listeners[t] === "function"), true);

lab.setMapMode("build");
check("building pauses the run", lab.running(), false);

// An empty map first, so every count below is about what the tools did.
lab.applyMap({ route: [], obstacles: [], props: [] });

lab.setTool("box");
press(1000, 1000);
moveTo(1400, 1300);
release();
check("a drag makes one cube", lab.OBSTACLES().length, 1);
check("the cube is where it was dragged", lab.OBSTACLES()[0], { x: 1000, y: 1000, w: 400, h: 300 });

press(2000, 500);
release();
const dropped = lab.OBSTACLES()[1];
check("a plain click drops a default cube", [dropped.w, dropped.h], [300, 300]);
check("centred on the click", [dropped.x + dropped.w / 2, dropped.y + dropped.h / 2], [2000, 500]);

lab.setTool("prop");
press(1700, 900);
check("a thing to examine goes down", lab.PROPS().length, 1);

lab.setTool("way");
press(300, 300);
press(2600, 1700);
check("route points go down in order", lab.ROUTE().map(p => [p.x, p.y]), [[300, 300], [2600, 1700]]);

console.log("\n--- what the pathfinding sees ---");
check("a point inside the drawn cube is blocked", !!lab.insideObstacle(lab.V(1200, 1150), 0), true);
check("a point outside it is not", !!lab.insideObstacle(lab.V(900, 1150), 0), false);
const through = lab.furthestClear(lab.V(700, 1150), lab.V(1800, 1150));
check("a walk through the cube is not clear", through.clear, false);
check("it stops short of the cube", through.dist < 300, true);
const around = lab.furthestClear(lab.V(700, 1800), lab.V(1800, 1800));
check("a walk clear of it is clear", around.clear, true);

console.log("\n--- erasing ---");
lab.setTool("erase");
press(1200, 1150);
check("the cube under the cursor goes", lab.OBSTACLES().length, 1);
press(1700, 900);
check("so does the thing", lab.PROPS().length, 0);
press(300, 300);
check("and the route point", lab.ROUTE().length, 1);
press(100, 1900);
check("clicking bare ground removes nothing", lab.OBSTACLES().length, 1);

console.log("\n--- saving, pasting, and rubbish ---");
check("every edit is saved", lab.savedMap() !== null, true);
const bad = lab.sanitizeMap({ obstacles: [[1, 2, 0, 5], ["a", 2, 3, 4], [10, 20, 30, 40]], props: [[1, "x"]], route: "no" });
check("zero-sized and non-numeric are dropped", bad, { route: [], obstacles: [[10, 20, 30, 40]], props: [] });
check("nonsense gives an empty map", lab.sanitizeMap(null), { route: [], obstacles: [], props: [] });

lab.applyMap(lab.DEFAULT_MAP);
const round = lab.sanitizeMap(lab.mapAsData());
check("the default map survives a round trip", round, lab.sanitizeMap(lab.DEFAULT_MAP));

console.log("\n--- back to play ---");
lab.setMapMode("play");
check("play resumes the run", lab.running(), true);
check("the mode is play", lab.mode(), "play");

// And she still gets about on a map that came from the editor rather than the source.
lab.applyMap({ route: [[400, 400], [2400, 400]], obstacles: [[1200, 200, 300, 900]], props: [[800, 1200]] });
const DT = 1 / 120;
let worst = 0;
for (let i = 0; i < Math.round(25 / DT); i++) {
  lab.stepLeader(DT);
  lab.updateLeaderAttention(DT);
  lab.stepCompanion(DT);
  worst = Math.max(worst, lab.dist(lab.comp.p, lab.leader.p));
}
console.log(`     worst gap on the built map: ${worst.toFixed(0)} cm`);
check("she keeps up on a map from the editor", worst < 1500, true);

console.log("\n--- moving what you placed ---");
lab.applyMap({ route: [[400, 400]], obstacles: [[1000, 1000, 400, 300]], props: [[1700, 900]] });
lab.setMapMode("build");
lab.setTool("move");

press(1100, 1100);            // inside the cube, not on its corner
moveTo(1600, 1100);           // 500 to the right
release();
check("the cube moved by the drag", [lab.OBSTACLES()[0].x, lab.OBSTACLES()[0].y], [1500, 1000]);
check("and kept its size", [lab.OBSTACLES()[0].w, lab.OBSTACLES()[0].h], [400, 300]);

press(1700, 900);
moveTo(1900, 1000);
release();
check("a thing to examine moves", [lab.PROPS()[0].x, lab.PROPS()[0].y], [1900, 1000]);

press(400, 400);
moveTo(650, 525);
release();
check("a route point moves", [lab.ROUTE()[0].x, lab.ROUTE()[0].y], [650, 525]);

press(50, 1900);              // bare ground: nothing to grab
moveTo(500, 1900);
release();
check("dragging bare ground moves nothing", [lab.OBSTACLES()[0].x, lab.PROPS()[0].x], [1500, 1900]);

console.log("\n--- collisions ---");
lab.setMapMode("play");
lab.applyMap({ route: [], obstacles: [[1000, 1000, 600, 600]], props: [] });

// Dropped inside a cube, a body comes out of it rather than standing in it.
const body = { p: lab.V(1200, 1200), v: lab.V(0, 0) };
lab.resolveObstacles(body, lab.BODY_RADIUS);
check("a body inside a cube is pushed out", !!lab.insideObstacle(body.p, 0), false);

// Overlapping the left face from OUTSIDE - 980 is 20 into a 45 radius - which is the ordinary
// case. A point inside the cube takes the buried branch instead, and that one stops dead.
const walker = { p: lab.V(980, 1300), v: lab.V(300, 0) };
lab.resolveObstacles(walker, lab.BODY_RADIUS);
check("stopped at the face, not inside it", Math.round(walker.p.x), 1000 - lab.BODY_RADIUS);
check("the velocity into the face is gone", Math.round(walker.v.x), 0);

// Sliding: moving diagonally at a face keeps the along-face part.
const slider = { p: lab.V(980, 1300), v: lab.V(300, 200) };
lab.resolveObstacles(slider, lab.BODY_RADIUS);
check("but it still slides along it", Math.round(slider.v.y), 200);

// And the world's edge holds.
const strayer = { p: lab.V(-5000, 500), v: lab.V(-300, 0) };
lab.resolveObstacles(strayer, lab.BODY_RADIUS);
check("the world edge holds", strayer.p.x > -1000, true);

// The pair, driven through a cube on purpose: neither may end up inside it.
lab.applyMap({ route: [[400, 1300], [2600, 1300]], obstacles: [[1200, 1000, 400, 600]], props: [] });
lab.setDrive("route");
const DT2 = 1 / 120;
let insideCount = 0;
for (let i = 0; i < Math.round(40 / DT2); i++) {
  lab.stepLeader(DT2);
  lab.updateLeaderAttention(DT2);
  lab.stepCompanion(DT2);
  if (lab.insideObstacle(lab.leader.p, 0) || lab.insideObstacle(lab.comp.p, 0)) insideCount++;
}
check("a route straight through a cube never puts them inside it", insideCount, 0);

console.log("\n--- click to move ---");
lab.applyMap({ route: [], obstacles: [], props: [] });
lab.setDrive("click");
lab.setMapMode("play");
press(2200, 1500);
const told = lab.clickTarget();
check("a click gives him somewhere to walk", told ? [Math.round(told.x), Math.round(told.y)] : null, [2200, 1500]);

let reached = false;
for (let i = 0; i < Math.round(30 / DT2) && !reached; i++) {
  lab.stepLeader(DT2);
  lab.updateLeaderAttention(DT2);
  lab.stepCompanion(DT2);
  if (lab.dist(lab.leader.p, lab.V(2200, 1500)) < 90) reached = true;
}
check("he walks there", reached, true);

for (let i = 0; i < Math.round(4 / DT2); i++) { lab.stepLeader(DT2); }
check("and stops, rather than trailing the cursor", lab.clickTarget(), null);
check("standing still once arrived", Math.round(lab.leader.v.x) === 0 && Math.round(lab.leader.v.y) === 0, true);

console.log("\n--- pathfinding ---");

/** Every leg of a path has to be walkable by a body, or it is not a path. */
function legsAreWalkable(from, legs) {
  let at = from;
  for (const leg of legs) {
    if (!lab.lineClearForBody(at, leg)) return false;
    at = leg;
  }
  return true;
}

// A long wall with one way round it. The straight line is blocked, so a path is required.
lab.applyMap({ route: [], props: [], obstacles: [[1400, -200, 200, 1900]] });
const roundWall = lab.findPath(lab.V(800, 800), lab.V(2200, 800));
check("a path exists round a wall", Array.isArray(roundWall) && roundWall.length > 0, true);
check("every leg of it is walkable", legsAreWalkable(lab.V(800, 800), roundWall), true);
check("it ends at the goal", roundWall && [Math.round(roundWall[roundWall.length - 1].x), Math.round(roundWall[roundWall.length - 1].y)], [2200, 800]);
check("and it goes round the open end", roundWall && roundWall.some(p => p.y > 1700), true);
check("the straight line really was blocked", lab.lineClearForBody(lab.V(800, 800), lab.V(2200, 800)), false);

// Corners pulled out: a handful of legs, not a staircase of fifty cells.
check("the corners are pulled out of it", roundWall.length <= 6, true);

// A goal with no way to it at all.
lab.applyMap({ route: [], props: [], obstacles: [
  [1000, 600, 800, 100], [1000, 1400, 800, 100], [1000, 600, 100, 900], [1700, 600, 100, 900]] });
check("a sealed box has no path into it", lab.findPath(lab.V(400, 1000), lab.V(1400, 1000)), null);

console.log("\n--- the player, clicking past a wall ---");
lab.applyMap({ route: [], props: [], obstacles: [[1400, -200, 200, 1900]] });
lab.resetSim(true);
lab.leader.p = lab.V(800, 800);
lab.comp.p = lab.V(600, 900);
lab.setDrive("click");
lab.setMapMode("play");
press(2200, 800);

const DT3 = 1 / 120;
let arrived = false, pressedIn = 0;
for (let i = 0; i < Math.round(60 / DT3) && !arrived; i++) {
  lab.stepLeader(DT3);
  lab.updateLeaderAttention(DT3);
  lab.stepCompanion(DT3);
  if (lab.insideObstacle(lab.leader.p, 0) || lab.insideObstacle(lab.comp.p, 0)) pressedIn++;
  if (lab.dist(lab.leader.p, lab.V(2200, 800)) < 120) arrived = true;
}
check("he walks round the wall to the click", arrived, true);
check("neither of them ends up inside it", pressedIn, 0);
check("she is not left on the far side", lab.dist(lab.comp.p, lab.leader.p) < 900, true);

console.log("\n--- she gets out of a U ---");
// Her leader walks off while she is boxed into a U with one way out.
lab.applyMap({ route: [], props: [], obstacles: [
  [900, 400, 100, 800], [1500, 400, 100, 800], [900, 400, 700, 100]] });
lab.resetSim(true);
lab.comp.p = lab.V(1250, 900);      // inside the U
lab.leader.p = lab.V(2400, 1700);   // well outside it
lab.setDrive("stand");
let worstAfter = 0;
for (let i = 0; i < Math.round(30 / DT3); i++) {
  lab.stepLeader(DT3);
  lab.updateLeaderAttention(DT3);
  lab.stepCompanion(DT3);
  if (i > Math.round(20 / DT3)) worstAfter = Math.max(worstAfter, lab.dist(lab.comp.p, lab.leader.p));
}
console.log(`     gap after she has had 20s to get out: ${worstAfter.toFixed(0)} cm`);
check("she finds her way out of the U", worstAfter < 700, true);

console.log("");
console.log("--- where she looks when she stops ---");

/** How far the ground runs on in the direction she is facing. 0 when she has chosen nothing. */
function reachAlongGaze() {
  if (lab.comp.standFace === null) return 0;
  const dir = lab.rot(lab.V(1, 0), lab.comp.standFace);
  let reach = 0;
  for (let d = 60; d <= 1200; d += 40) {
    if (!lab.lineClearForBody(lab.comp.p, lab.V(lab.comp.p.x + dir.x * d, lab.comp.p.y + dir.y * d))) break;
    reach = d;
  }
  return reach;
}

function settle(seconds) {
  for (let i = 0; i < Math.round(seconds / DT2); i++) {
    lab.stepLeader(DT2);
    lab.updateLeaderAttention(DT2);
    lab.stepCompanion(DT2);
  }
}

/**
 * Steps until she is standing still and has chosen a gaze. Waiting a fixed twelve seconds made
 * the check flaky: the planted bearing is random, so sometimes she was still walking at the bell
 * and had quite correctly chosen nothing.
 */
function settleUntilLooking(maxSeconds) {
  for (let i = 0; i < Math.round(maxSeconds / DT2); i++) {
    lab.stepLeader(DT2);
    lab.updateLeaderAttention(DT2);
    lab.stepCompanion(DT2);
    if (lab.comp.standFace !== null && Math.hypot(lab.comp.v.x, lab.comp.v.y) <= 15) return true;
  }
  return false;
}

const CORRIDOR = [[700, 300, 1400, 200], [700, 1200, 1400, 200], [700, 300, 200, 1100]];

// A dead end with one way out, and nothing to examine: she must look down the way out, not at a wall.
lab.P.RoamIdleTime = 999;          // keep her in follow, so this is about the standing gaze alone
lab.applyMap({ route: [], props: [], obstacles: CORRIDOR });
lab.setDrive("stand");
lab.setLook(false);
lab.resetSim(true);
lab.leader.p = lab.V(1100, 800);
lab.comp.p = lab.V(1400, 800);
check("she stops and chooses somewhere to look", settleUntilLooking(25), true);
const gazeReach = reachAlongGaze();
console.log("     the ground runs on " + gazeReach + " cm the way she is looking");
check("and it is not a wall a metre away", gazeReach >= 400, true);
const atLeader = lab.yawOf(lab.sub(lab.leader.p, lab.comp.p));
check("she is not staring at the leader", Math.abs(lab.angleDelta(lab.comp.standFace, atLeader)) > 35, true);

// The same spot, with something worth examining beside her.
lab.applyMap({ route: [], props: [[1600, 1000]], obstacles: CORRIDOR });
lab.resetSim(true);
lab.leader.p = lab.V(1100, 800);
lab.comp.p = lab.V(1400, 800);
settleUntilLooking(25);
const atFind = lab.yawOf(lab.sub(lab.V(1600, 1000), lab.comp.p));
console.log("     gaze " + Math.round(lab.comp.standFace) + " deg, the find is at " + Math.round(atFind) + " deg");
check("a find beats bare ground", Math.abs(lab.angleDelta(lab.comp.standFace, atFind)) < 30, true);

// And she still looks back when she is being looked at.
lab.setLook(true);
settle(6);
const backAtLeader = lab.yawOf(lab.sub(lab.leader.p, lab.comp.p));
check("she looks back when you look at her", Math.abs(lab.angleDelta(lab.comp.yaw, backAtLeader)) < 30, true);

// Shut in a small room, there is no way out - but there is still the room itself, and looking
// across it beats looking at the wall thirty centimetres away. A 3x4 m interior has three or four
// metres of walking in it, so a direction is the right answer here, not null.
lab.setLook(false);
lab.applyMap({ route: [], props: [], obstacles: [
  [1000, 700, 600, 100], [1000, 1300, 600, 100], [1000, 700, 100, 700], [1500, 700, 100, 700]] });
lab.resetSim(true);
lab.comp.p = lab.V(1150, 900);        // in a corner of the room, facing into it
lab.leader.p = lab.V(1150, 900);
lab.comp.standFace = lab.openWayYaw(lab.comp.p);
check("shut in a room, she looks across it", lab.comp.standFace !== null, true);
console.log("     and the room runs on " + reachAlongGaze() + " cm that way");
check("not at the wall beside her", reachAlongGaze() >= 200, true);

console.log("");
console.log("--- she keeps looking the way she came ---");

// A corridor open at both ends, nothing to examine: whichever way she walked in is the way she
// should keep looking. Set deterministically rather than hoping a random bearing produces it.
lab.applyMap({ route: [], props: [], obstacles: [[0, 600, 3000, 200], [0, 1400, 3000, 200]] });
const middle = lab.V(1500, 1100);

const cameEast = lab.openWayYaw(middle, 0);      // walked in heading +X
const cameWest = lab.openWayYaw(middle, 180);    // walked in heading -X
console.log("     arriving east she looks " + Math.round(cameEast)
            + " deg, arriving west " + Math.round(cameWest) + " deg");
check("arriving eastwards she looks east", Math.abs(lab.angleDelta(cameEast, 0)) < 60, true);
check("arriving westwards she looks west", Math.abs(lab.angleDelta(cameWest, 180)) < 60, true);

// With the bias off, the two answers are the same - which is the old behaviour, and the reason
// she sometimes turned round on arrival.
const keptBias = lab.P.LookForwardBias;
lab.P.LookForwardBias = 0;
const flatEast = lab.openWayYaw(middle, 0);
const flatWest = lab.openWayYaw(middle, 180);
check("with the bias off the arrival heading is ignored",
  Math.abs(lab.angleDelta(flatEast, flatWest)) < 1, true);
lab.P.LookForwardBias = keptBias;

// A dead end: the only way out is behind her, and there the right answer IS to turn round.
lab.applyMap({ route: [], props: [], obstacles: [
  [1800, 600, 200, 1000], [0, 600, 2000, 200], [0, 1400, 2000, 200]] });
const inDeadEnd = lab.openWayYaw(lab.V(1600, 1100), 0);   // facing into the dead end
console.log("     in a dead end facing the wall she looks " + Math.round(inDeadEnd) + " deg");
check("in a dead end she does turn round", Math.abs(lab.angleDelta(inDeadEnd, 180)) < 60, true);

console.log("");
console.log("--- random mazes ---");

// Twenty of them, because the interesting failures of a generator are the rare layouts.
let worstWalls = 0, badRoutes = 0, unreachable = 0, noLoops = 0;
for (let n = 0; n < 20; n++) {
  const m = lab.randomMap();
  lab.applyMap(m);
  worstWalls = Math.max(worstWalls, lab.OBSTACLES().length);

  // Every route point and every find must be somewhere a body can stand...
  for (const p of lab.ROUTE().concat(lab.PROPS())) {
    if (lab.insideObstacle(p, lab.BODY_RADIUS)) badRoutes++;
  }
  // ...and the whole tour must be walkable, or the leader would be stuck in a cell.
  const r = lab.ROUTE();
  for (let i = 0; i < r.length; i++) {
    const from = r[i], to = r[(i + 1) % r.length];
    if (!lab.lineClearForBody(from, to) && !lab.findPath(from, to)) unreachable++;
  }
  // More than one way round is the point of knocking extra walls out: with a plain spanning tree
  // the number of walls is fixed, so a count well under the tree's tells us loops were made.
  if (lab.OBSTACLES().length > 44) noLoops++;
}

console.log("     at most " + worstWalls + " wall cubes per maze");
check("every route point and find is standable", badRoutes, 0);
check("the whole route is walkable in every maze", unreachable, 0);
check("the cube count stays workable", worstWalls <= 44, true);
check("and they are not plain spanning trees", noLoops, 0);

// Rooms: the open spaces have to be genuinely open, so a 2x2 block has room to stand about in.
lab.applyMap(lab.randomMap());
let roomy = 0;
for (const prop of lab.PROPS()) {
  if (lab.openWayYaw(prop, null) !== null) roomy++;
}
check("each room has somewhere to walk from it", roomy, lab.PROPS().length);

console.log("");
console.log("--- the maze sliders ---");
lab.P.MazeCols = 4; lab.P.MazeRows = 3; lab.P.MazeCell = 300; lab.P.MazeRooms = 0; lab.P.MazeLoops = 0;
const small = lab.randomMap();
lab.P.MazeCols = 10; lab.P.MazeRows = 8; lab.P.MazeCell = 500;
const big = lab.randomMap();

function span(m) {
  const xs = m.obstacles.map(b => b[0] + b[2]);
  const ys = m.obstacles.map(b => b[1] + b[3]);
  return [Math.round(Math.max.apply(null, xs)), Math.round(Math.max.apply(null, ys))];
}
console.log("     4x3 of 300 reaches " + span(small) + ", 10x8 of 500 reaches " + span(big));
check("a smaller maze is smaller", span(small)[0] < span(big)[0] && span(small)[1] < span(big)[1], true);
check("both are still walkable throughout", [small, big].every(m => {
  lab.applyMap(m);
  const r = lab.ROUTE();
  return r.every((from, i) => {
    const to = r[(i + 1) % r.length];
    return lab.lineClearForBody(from, to) || !!lab.findPath(from, to);
  });
}), true);

lab.P.MazeCols = 7; lab.P.MazeRows = 5; lab.P.MazeCell = 400; lab.P.MazeRooms = 3; lab.P.MazeLoops = 0.18;
lab.applyMap(lab.randomMap());
const forUnreal = lab.mapForUnreal();
check("the export says how it is played", !!forUnreal.play, true);
check("and defaults to you driving and recruiting her",
  [forUnreal.play.mode, forUnreal.play.companion], ["player", "recruit"]);

console.log(failures ? `\n${failures} FAILED` : "\nall good");
process.exit(failures ? 1 : 0);
