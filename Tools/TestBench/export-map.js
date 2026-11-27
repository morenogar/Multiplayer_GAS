const fs = require("fs");
const path = require("path");
const h = fs.readFileSync(process.argv[2] || path.join(__dirname, "..", "FollowLab.html"), "utf8").split("\n");
const src = h.slice(h.findIndex(l => l.trim() === "<script>") + 1, h.findIndex(l => l.trim() === "</script>")).join("\n");
global.document = { getElementById: () => ({ width: 1600, height: 1000, style: {},
  getContext: () => new Proxy({}, { get: () => () => {}, set: () => true }),
  addEventListener: () => {}, parentElement: { clientWidth: 1000 },
  getBoundingClientRect: () => ({ left: 0, top: 0, width: 1600, height: 1000 }) }),
  documentElement: {}, addEventListener: () => {} };
global.window = { devicePixelRatio: 1, addEventListener: () => {}, matchMedia: () => ({ matches: false }) };
global.getComputedStyle = () => ({ getPropertyValue: () => "#000" });
global.performance = { now: () => 0 };
global.requestAnimationFrame = () => 0;
global.localStorage = { getItem: () => null, setItem: () => {} };
global.navigator = { clipboard: { writeText: async () => {} } };
global.fetch = async () => ({ ok: false });
global.FileReader = function () {};
global.CanvasRenderingContext2D = function () {};
const lab = new Function(src + ";return { randomMap, applyMap, mapForUnreal, OBSTACLES: () => OBSTACLES };")();
lab.applyMap(lab.randomMap());
process.stdout.write(JSON.stringify(lab.mapForUnreal(), null, 2));
