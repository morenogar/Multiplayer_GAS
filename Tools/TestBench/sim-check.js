// Runs the browser tuner's simulation with no browser, to answer one question: does the
// companion converge, or does the maths itself send it running off?  The JS was written from
// the same formulas as the C++ but independently, so agreement is evidence about the formula
// and disagreement is evidence about the C++.
//
// Usage: node sim-check.js [seconds] [key=value ...]

const fs = require("fs");
const path = require("path");

/* A stub that absorbs every DOM call the page makes without caring what it is. */
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

global.document = {
  getElementById: id => {
    if (id === "map" || id === "strip") {
      return { width: 1600, height: 1000, style: {}, getContext: () => ctxStub,
               addEventListener: () => {}, parentElement: { clientWidth: 1000 },
               getBoundingClientRect: () => ({ left: 0, top: 0, width: 1000, height: 625 }) };
    }
    return stub(id);
  },
  documentElement: {},
  addEventListener: () => {},
};
global.window = { devicePixelRatio: 1, addEventListener: () => {}, matchMedia: () => ({ matches: false }) };
global.getComputedStyle = () => ({ getPropertyValue: () => "#000" });
global.performance = { now: () => 0 };
global.requestAnimationFrame = () => 0;
global.localStorage = { getItem: () => null, setItem: () => {} };
global.navigator = { clipboard: { writeText: async () => {} } };
global.fetch = async () => ({ ok: false, status: 0 });
global.FileReader = function () {};
global.CanvasRenderingContext2D = function () {};

const html = fs.readFileSync(path.join(__dirname, "..", "FollowLab.html"), "utf8").split("\n");
const src = html.slice(html.findIndex(l => l.trim() === "<script>") + 1,
                       html.findIndex(l => l.trim() === "</script>")).join("\n");
// The script is written for a page; expose what we want to drive from outside.
const run = new Function(src + "\n;return { P, comp, leader, metrics, stepLeader, stepCompanion," +
  " updateLeaderAttention, resetSim, V, len, dist," +
  " setDrive(m){ driveMode = m; }, setLook(v){ lookAtCompanion = v; }, setLookClue(v){ lookAtClue = v; }," +
  " setMouse(x, y){ mouseWorld = (x === null) ? null : V(x, y); }, now(){ return simTime; }," +
  " applyMap, setLeaderSpeed(v){ leaderWalkSpeed = v; } };");
const sim = run();

/**
 * Drives the leader through the three situations the roaming rule is about, and reports which
 * state she chose and when.  Standing still without looking at her is the case the lab could not
 * express before, and it is the whole point of the second area.
 */
const OPEN_MAP = { route: [[300, 1500], [2700, 1500]], obstacles: [], props: [] };
// Walls 60 thick at y 640 and y 980: 220 cm free between them (y 760-980), from x 200 to x 2600.
const CORRIDOR_MAP = { route: [[300, 870], [2500, 870]],
                       obstacles: [[200, 700, 2400, 60], [200, 980, 2400, 60]], props: [] };

function scenario() {
  const DT = 1 / 120;
  const dtOf = DT;
  const phases = [
    { label: "walking the route, not looking", secs: 8, drive: "route", look: false },
    { label: "standing still, NOT looking", secs: 18, drive: "stand", look: false },
    { label: "standing still, looking at her", secs: 8, drive: "stand", look: true },
    { label: "standing still, not looking again", secs: 10, drive: "stand", look: false },
    { label: "she is far off and you keep looking", secs: 14, drive: "stand", look: true,
      before: () => { sim.comp.p.x = sim.leader.p.x + 1100; sim.comp.p.y = sim.leader.p.y + 300; } },

    // Shared attention: you study the nearest clue; she should stop and look at it too.
    { label: "you study a clue", secs: 10, drive: "stand", look: false,
      before: () => sim.setLookClue(true) },
    { label: "...and look away from it", secs: 4, drive: "stand", look: false,
      before: () => sim.setLookClue(false) },

    // The point of the latch: your own steps inside her area are none of her business.
    { label: "standing still again, not looking", secs: 10, drive: "stand", look: false },
    { label: "you walk about INSIDE her area", secs: 10, drive: "mouse", look: false,
      before: () => sim.setMouse(sim.leader.anchor.x + 420, sim.leader.anchor.y + 120) },
    { label: "you walk OUT of her area", secs: 12, drive: "mouse", look: false,
      before: () => sim.setMouse(sim.leader.anchor.x - 1250, sim.leader.anchor.y + 200) },

    // The point of the loitering window: moving the whole time, but about one spot.
    { label: "you pace about one room", secs: 16, drive: "mouse", look: false,
      before: () => { sim.setMouse(sim.leader.p.x + 300, sim.leader.p.y); },
      during: (t, home) => {
        if (Math.floor(t / 2.5) % 2 === 0) sim.setMouse(home.x + 300, home.y + 60);
        else sim.setMouse(home.x - 180, home.y - 220);
      } },
    // And the case it must not catch: actually going somewhere.
    { label: "you walk the long route", secs: 16, drive: "route", look: false },

    // Ask two: your own shuffling about inside the little circle is not her business either.
    { label: "potter, investigating off", secs: 7, drive: "stand", look: false,
      before: () => { sim.P.RoamIdleTime = 999; } },
    { label: "...and keep pottering", secs: 14, drive: "mouse", look: false,
      during: (t) => {
        const a = sim.leader.anchor;
        if (Math.floor(t / 3.5) % 2 === 0) sim.setMouse(a.x + 210, a.y + 60);
        else sim.setMouse(a.x - 140, a.y - 150);
      } },

    // The human touches that need a map of their own: an open field, and a 220 cm corridor.
    // Side by side strolling in the open, single file in the corridor, and stepping aside when
    // you walk straight at her.
    { label: "open field, you walk at 300", secs: 9, drive: "route", look: false,
      before: () => { sim.applyMap(OPEN_MAP); sim.P.RoamIdleTime = 999; sim.setLeaderSpeed(300); } },
    { label: "open field, you stroll at 150", secs: 16, drive: "route", look: false,
      before: () => { sim.applyMap(OPEN_MAP); sim.setLeaderSpeed(150); } },
    { label: "corridor 220 wide, you walk at 300", secs: 8, drive: "route", look: false,
      before: () => { sim.applyMap(CORRIDOR_MAP); sim.setLeaderSpeed(300); } },
    { label: "you walk straight at her", secs: 5, drive: "mouse", look: false, walkAt: true,
      before: () => {
        sim.applyMap(OPEN_MAP);
        sim.leader.p = sim.V(600, 1500); sim.leader.yaw = 0;
        sim.comp.p = sim.V(820, 1530); sim.comp.v = sim.V(0, 0);
        sim.setMouse(2500, 1500);
      } },
  ];

  sim.resetSim(true);
  for (const phase of phases) {
    sim.setDrive(phase.drive);
    sim.setLook(phase.look);
    const home = { x: sim.leader.p.x, y: sim.leader.p.y };
    if (phase.before) phase.before();
    // Baselines for the counters of the five human touches, taken after before(): a map change
    // resets the sim, and the counters with it.
    const base = {
      react: sim.comp.reactLog.length, breaks: sim.comp.gazeBreaks, holds: sim.comp.gazeHolds.length,
      leads: sim.comp.gazeLeads, asides: sim.comp.stepAsides,
      cone: sim.comp.spotRejects.cone, narrow: sim.comp.spotRejects.narrow,
    };
    let besideTime = 0, behindTime = 0, shapeTime = 0;
    const walkAtFrom = { x: sim.comp.p.x, y: sim.comp.p.y };
    let walkAtDone = null;
    const seen = {};
    let moved = 0, minGap = Infinity, maxGap = 0;
    let arrivals = 0, stillTime = 0, align = 0, alignTime = 0;
    let prevDwell = sim.comp.roamDwell;
    let prevSpot = sim.comp.spot, spotPicks = 0;
    const bearings = [];
    let onProp = 0, onGround = 0, prevTarget = null;
    let prev = { x: sim.comp.p.x, y: sim.comp.p.y };
    for (let i = 0; i < Math.round(phase.secs / DT); i++) {
      if (phase.during) phase.during(i * DT, home);
      sim.stepLeader(DT);
      sim.updateLeaderAttention(DT);
      sim.stepCompanion(DT);
      seen[sim.comp.mode] = (seen[sim.comp.mode] || 0) + DT;
      moved += Math.hypot(sim.comp.p.x - prev.x, sim.comp.p.y - prev.y);
      prev = { x: sim.comp.p.x, y: sim.comp.p.y };
      if (sim.comp.roamDwell > prevDwell + 1e-9) arrivals++;
      prevDwell = sim.comp.roamDwell;

      // A fresh roam target: did it come from something examinable, or from open ground?
      if (sim.comp.roamTarget && sim.comp.roamTarget !== prevTarget) {
        if (sim.comp.lastProp) onProp++; else onGround++;
      }
      prevTarget = sim.comp.roamTarget;

      // A fresh planted spot: how far off dead-behind did she choose to stand?
      if (sim.comp.spot && sim.comp.spot !== prevSpot) {
        spotPicks++;
        const d = Math.atan2(sim.comp.spot.y - sim.leader.p.y, sim.comp.spot.x - sim.leader.p.x)
                  * 180 / Math.PI;
        let off = ((d - (sim.leader.yaw + 180)) % 360 + 540) % 360 - 180;
        bearings.push(Math.round(Math.abs(off)));
      }
      prevSpot = sim.comp.spot;
      const cs = sim.len(sim.comp.v), ls = sim.len(sim.leader.v);
      if (cs < 15) stillTime += dtOf;
      // Shadowing shows up as her velocity pointing the same way as yours while she is
      // supposed to be busy: 1 is walking with you, 0 is unrelated.
      if (cs > 40 && ls > 40) {
        align += (sim.comp.v.x * sim.leader.v.x + sim.comp.v.y * sim.leader.v.y) / (cs * ls) * dtOf;
        alignTime += dtOf;
      }
      // Formation shape while you walk and she follows: how far off dead-behind she walks.
      // Beside is over 55 degrees off your back, single file under 20. The first two seconds of
      // a phase are her catching up and are left out.
      if (sim.comp.mode === "follow" && ls > 40 && i * DT > 2) {
        const back = Math.atan2(-sim.leader.v.y, -sim.leader.v.x);
        const her = Math.atan2(sim.comp.p.y - sim.leader.p.y, sim.comp.p.x - sim.leader.p.x);
        const off = Math.abs(((her - back) * 180 / Math.PI % 360 + 540) % 360 - 180);
        if (off > 55) besideTime += DT;
        if (off < 20) behindTime += DT;
        shapeTime += DT;
      }
      // Walking at her: where she went, up to the moment you drew level with her.
      if (phase.walkAt && !walkAtDone && sim.leader.p.x >= sim.comp.p.x) {
        walkAtDone = { side: Math.abs(sim.comp.p.y - walkAtFrom.y), back: sim.comp.p.x - walkAtFrom.x };
      }
      const g = sim.dist(sim.comp.p, sim.leader.p);
      minGap = Math.min(minGap, g);
      maxGap = Math.max(maxGap, g);
    }
    const mix = Object.keys(seen).sort((a, b) => seen[b] - seen[a])
      .map(k => `${k} ${seen[k].toFixed(1)}s`).join(", ");
    const fromAnchor = sim.dist(sim.leader.p, sim.leader.anchor);
    const extra = [];
    if (onProp || onGround) extra.push(`finds ${onProp}/ground ${onGround}`);
    if (bearings.length) extra.push(`planted ${spotPicks}x at ${bearings.join(",")} deg off behind`);
    const reacts = sim.comp.reactLog.slice(base.react);
    if (reacts.length) extra.push(`reacted ${reacts.length}x after ${reacts.map(r => r.toFixed(2)).join("/")}s`);
    const holds = sim.comp.gazeHolds.slice(base.holds);
    if (sim.comp.gazeBreaks > base.breaks) {
      extra.push(`looked away ${sim.comp.gazeBreaks - base.breaks}x` + (holds.length ? `, holds ${holds.map(h => h.toFixed(1)).join("/")}s` : ""));
    }
    if (sim.comp.gazeLeads > base.leads) extra.push(`find->you->find ${sim.comp.gazeLeads - base.leads}x`);
    const cone = sim.comp.spotRejects.cone - base.cone, narrow = sim.comp.spotRejects.narrow - base.narrow;
    if (cone || narrow) extra.push(`spots refused: in your path ${cone}, narrow ${narrow}`);
    if (shapeTime > 1) {
      extra.push(`beside ${(100 * besideTime / shapeTime).toFixed(0)}% / behind ${(100 * behindTime / shapeTime).toFixed(0)}%` +
                 ` (slot ${sim.comp.formAngle.toFixed(0)}deg at ${sim.comp.formDist.toFixed(0)})`);
    }
    if (phase.walkAt) {
      extra.push(`stepped aside ${sim.comp.stepAsides - base.asides}x` + (walkAtDone
        ? `, by the time you drew level: ${walkAtDone.side.toFixed(0)} sideways, ${walkAtDone.back.toFixed(0)} backwards`
        : ", you never drew level"));
      extra.push(`min gap ${minGap.toFixed(0)}`);
    }
    console.log(`${phase.label.padEnd(36)} | ${mix.padEnd(34)} | you ${String(fromAnchor.toFixed(0)).padStart(4)} from anchor` +
      `, she walked ${String(moved.toFixed(0)).padStart(5)}, stops ${arrivals}` +
      `, stood ${stillTime.toFixed(1)}s` + (extra.length ? ", " + extra.join(", ") : ""));
  }
}

for (const arg of process.argv.slice(3)) {
  const [k, v] = arg.split("=");
  if (k in sim.P) sim.P[k] = (v === "true") ? true : (v === "false") ? false : parseFloat(v);
  else { console.log("unknown parameter:", k); process.exit(1); }
}

// Overrides are applied before this, so a scenario run can be compared with one parameter moved -
// which is the only way to tell "she walked to the places she chose" from "she followed me".
if (process.argv[2] === "scenario") {
  scenario();
  process.exit(0);
}

const seconds = parseFloat(process.argv[2] || "40");

sim.resetSim(true);
const DT = 1 / 120;
let worstGap = 0, closest = Infinity, awayTime = 0, walked = 0;
let prev = { x: sim.comp.p.x, y: sim.comp.p.y };
const samples = [];

for (let i = 0; i < Math.round(seconds / DT); i++) {
  sim.stepLeader(DT);
  sim.stepCompanion(DT);
  const gap = sim.dist(sim.comp.p, sim.leader.p);
  worstGap = Math.max(worstGap, gap);
  closest = Math.min(closest, gap);
  if (gap > 1500) awayTime += DT;
  walked += Math.hypot(sim.comp.p.x - prev.x, sim.comp.p.y - prev.y);
  prev = { x: sim.comp.p.x, y: sim.comp.p.y };
  if (i % Math.round(2 / DT) === 0) {
    samples.push(`t=${(i * DT).toFixed(0).padStart(3)}s gap=${gap.toFixed(0).padStart(5)} ` +
                 `cspd=${sim.len(sim.comp.v).toFixed(0).padStart(4)} intent=${sim.comp.intent.toFixed(2)}`);
  }
}

console.log(samples.join("\n"));
console.log("-".repeat(60));
console.log(`gap worst ${worstGap.toFixed(0)}  closest ${closest.toFixed(0)}  ` +
            `beyond 15m for ${awayTime.toFixed(1)}s  walked ${walked.toFixed(0)}`);
console.log(`orbit ${sim.metrics.orbitDeg.toFixed(0)}deg  peak swing ${sim.metrics.headingPeak.toFixed(0)}deg/s  ` +
            `glide ${sim.metrics.glide.toFixed(0)}  crowd ${(100 * sim.metrics.crowdTime / Math.max(0.1, sim.metrics.totalTime)).toFixed(0)}%`);
const runaway = worstGap > 2000;
console.log(runaway ? "VERDICT: runaway - the formula itself diverges" : "VERDICT: converges");
