"""Runs the editor toolset with a fake `unreal` module, on the lab's real export.

What this can check: that the JSON contract between the two halves actually holds, that every
wall, find and route point becomes a spawn at the right location and scale, that tagging and
foldering happen (so clear_map can undo it and only it), and that bad input is refused with a
message that says what to do instead. What it cannot check: anything the editor itself does - the
spawn really landing, the mesh assigning, the navmesh rebuilding.

Usage: py -I toolset-check.py
"""

import json
import os
import subprocess
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
PY_ROOT = "D:/Multiplayer_GAS/Multiplayer_GAS/Content/Python"

failures = []


def check(label, got, want):
    ok = got == want
    print(("ok   " if ok else "FAIL ") + label + ("" if ok else "   got %r want %r" % (got, want)))
    if not ok:
        failures.append(label)


# --------------------------------------------------------------- the fake editor
class FakeActor:
    def __init__(self, cls, location):
        self.cls = cls
        self.location = (location.x, location.y, location.z)
        self.label = None
        self.folder = None
        self.tags = []
        self.scale = (1.0, 1.0, 1.0)
        self.mesh = None
        self.mobility = None
        self.static_mesh_component = self

    def set_actor_label(self, label):
        self.label = label

    def set_folder_path(self, folder):
        self.folder = str(folder)

    def set_actor_scale3d(self, scale):
        self.scale = (scale.x, scale.y, scale.z)

    def set_editor_property(self, name, value):
        if name == "static_mesh":
            self.mesh = value

    def set_mobility(self, mobility):
        self.mobility = mobility

    def actor_has_tag(self, tag):
        return str(tag) in [str(t) for t in self.tags]

    def get_actor_label(self):
        return self.label or ""


class FakeActorSubsystem:
    def __init__(self):
        self.actors = []

    def spawn_actor_from_class(self, cls, location, rotation):
        actor = FakeActor(cls, location)
        self.actors.append(actor)
        return actor

    def destroy_actor(self, actor):
        self.actors.remove(actor)

    def get_all_level_actors(self):
        return list(self.actors)


def make_unreal():
    """Just enough of the editor's Python API for these tools to run."""
    unreal = types.ModuleType("unreal")
    subsystem = FakeActorSubsystem()

    class Vector:
        def __init__(self, x=0.0, y=0.0, z=0.0):
            self.x, self.y, self.z = float(x), float(y), float(z)

    class Rotator:
        def __init__(self, *args):
            self.args = args

    class Name(str):
        pass

    class ScopedEditorTransaction:
        def __init__(self, title):
            self.title = title

        def __enter__(self):
            return self

        def __exit__(self, *exc):
            return False

    class Mobility:
        STATIC = "static"

    unreal.Vector = Vector
    unreal.Rotator = Rotator
    unreal.Name = Name
    unreal.ScopedEditorTransaction = ScopedEditorTransaction
    unreal.ComponentMobility = Mobility
    unreal.uclass = lambda: (lambda cls: cls)
    unreal.ToolsetDefinition = object
    unreal.StaticMeshActor = "StaticMeshActor"
    unreal.TargetPoint = "TargetPoint"

    class PlayerStart:
        pass

    unreal.PlayerStart = PlayerStart
    # The toolset loads the blueprint through EditorAssetLibrary.load_asset and asks it for its
    # generated class; the module-level load_asset is a different function and stubbing only that
    # one left the companion silently unplaced.
    unreal.EditorAssetLibrary = type("EditorAssetLibrary", (), {
        "load_asset": staticmethod(lambda path: unreal.load_asset(path)),
        "load_blueprint_class": staticmethod(lambda path: ("bp", path)),
    })
    unreal.EditorActorSubsystem = "EditorActorSubsystem"
    unreal.get_editor_subsystem = lambda _cls: subsystem
    # An asset stands in for both a mesh and a blueprint: the toolset asks a blueprint for its
    # generated_class, and a stub that could not answer that made the companion step look broken
    # when only the fake editor was.
    class FakeAsset:
        def __init__(self, path):
            self.path = path

        def generated_class(self):
            return ("bp", self.path)

    unreal.load_asset = lambda path: FakeAsset(path) if path else None
    # The project is not compiled into this fake editor, so the interactable cannot load: the
    # sphere fallback is therefore the path under test here, and the warning that goes with it.
    unreal.load_class = lambda outer, path: None
    unreal.log = print
    unreal.log_warning = print

    # A package, not a plain module: importing the toolset runs the package __init__, which does
    # `from toolset_registry.registration import Registration`.
    registry = types.ModuleType("toolset_registry")
    registry.__path__ = []
    registry.tool_call = lambda func: func

    registration_module = types.ModuleType("toolset_registry.registration")

    class Registration:
        def __init__(self, toolset_classes):
            self.toolset_classes = tuple(toolset_classes)
            self.registered = False

        def register(self):
            self.registered = True
            return True

        def unregister(self):
            self.registered = False

    registration_module.Registration = Registration
    registry.registration = registration_module
    sys.modules["toolset_registry.registration"] = registration_module
    return unreal, registry, subsystem


def load_toolset():
    unreal, registry, subsystem = make_unreal()
    sys.modules["unreal"] = unreal
    sys.modules["toolset_registry"] = registry
    sys.path.insert(0, PY_ROOT)
    for stale in [m for m in list(sys.modules) if m.startswith("viper_map_toolset")]:
        del sys.modules[stale]
    from viper_map_toolset.toolsets.viper_map import ViperMapTools
    return ViperMapTools, subsystem, unreal


def lab_export():
    """The map the lab would put on the clipboard, produced by the lab's own code."""
    script = r"""
const fs=require('fs');const h=fs.readFileSync(process.argv[2],'utf8').split('\n');const src=h.slice(h.findIndex(l=>l.trim()==='<script>')+1,h.findIndex(l=>l.trim()==='</script>')).join('\n');
global.document={getElementById:()=>({width:1600,height:1000,style:{},getContext:()=>new Proxy({},{get:()=>()=>{},set:()=>true}),addEventListener:()=>{},parentElement:{clientWidth:1000},getBoundingClientRect:()=>({left:0,top:0,width:1600,height:1000})}),documentElement:{},addEventListener:()=>{}};
global.window={devicePixelRatio:1,addEventListener:()=>{},matchMedia:()=>({matches:false})};
global.getComputedStyle=()=>({getPropertyValue:()=>'#000'});global.performance={now:()=>0};
global.requestAnimationFrame=()=>0;global.localStorage={getItem:()=>null,setItem:()=>{}};
global.navigator={clipboard:{writeText:async()=>{}}};global.fetch=async()=>({ok:false});
global.FileReader=function(){};global.CanvasRenderingContext2D=function(){};
const lab=new Function(src+';return { randomMap, applyMap, mapForUnreal };')();
lab.applyMap(lab.randomMap());
process.stdout.write(JSON.stringify(lab.mapForUnreal()));
"""
    runner = os.path.join(HERE, "_export_once.js")
    with open(runner, "w", encoding="utf-8") as handle:
        handle.write(script)
    try:
        out = subprocess.run(["node", runner, os.path.join(HERE, "..", "FollowLab.html")],
                             capture_output=True, text=True, check=True)
    finally:
        os.remove(runner)
    return out.stdout


print("--- the contract between the lab and the editor ---")
Tools, level, unreal = load_toolset()
exported = lab_export()
data = json.loads(exported)
check("the lab exports walls", len(data["walls"]) > 0, True)

summary = Tools.build_map(exported, folder="ViperTestMap")
walls = [a for a in level.actors if a.label.startswith("Wall_")]
finds = [a for a in level.actors if a.label.startswith("Find_")]
route = [a for a in level.actors if a.label.startswith("Route_")]

check("every wall became an actor", len(walls), len(data["walls"]))
check("every find became an actor", len(finds), len(data["finds"]))
check("every route point became an actor", len(route), len(data["route"]))
check("all of it is tagged for clearing",
      all(a.actor_has_tag("ViperTestMap") for a in level.actors), True)
check("all of it is in the folder", {a.folder for a in level.actors}, {"ViperTestMap"})
check("walls are static mesh actors", {a.cls for a in walls}, {"StaticMeshActor"})
check("walls are static mobility", {a.mobility for a in walls}, {"static"})
check("walls got the cube mesh", {a.mesh.path for a in walls}, {data["cubeMesh"]})

first = data["walls"][0]
check("the first wall is where the lab put it", walls[0].location, tuple(first["location"]))
check("and scaled as the lab said", walls[0].scale, tuple(first["scale"]))
check("it warns that the navmesh has to rebuild", "navmesh" in summary.lower(), True)
check("and says the interactable could not load", "ViperItem" in summary, True)

print("")
print("--- building twice does not pile up ---")
before = len(level.actors)
again = Tools.build_map(exported, folder="ViperTestMap")
check("the second build replaces the first", len(level.actors), before)
check("and says what it removed", "Removed" in again, True)

print("")
print("--- clearing takes only what it built ---")
hand_placed = level.spawn_actor_from_class("StaticMeshActor", unreal.Vector(0, 0, 0), None)
hand_placed.set_actor_label("SomethingSomeoneBuilt")
removed = Tools.clear_map("ViperTestMap")
check("it removed the built map", removed, before)
check("and left the hand-placed actor alone",
      [a.label for a in level.actors], ["SomethingSomeoneBuilt"])
check("the summary then says there is no map", Tools.map_summary(), "No test map in this level.")

def footprint(actor):
    """World-space x/y extent of a spawned cube, from its location and scale."""
    w = actor.scale[0] * 100.0
    d = actor.scale[1] * 100.0
    return (actor.location[0] - w / 2, actor.location[1] - d / 2,
            actor.location[0] + w / 2, actor.location[1] + d / 2)


def contains(outer, inner, slack=0.01):
    return (outer[0] <= inner[0] + slack and outer[1] <= inner[1] + slack
            and outer[2] >= inner[2] - slack and outer[3] >= inner[3] - slack)


print("")
print("--- the floor, the pen, and the cast ---")
Tools, level, unreal = load_toolset()
Tools.build_map(exported, folder="ViperTestMap", outer_margin=700.0)

named = lambda prefix: [a for a in level.actors if a.label.startswith(prefix)]
floors, outers = named("Floor"), named("Outer_")
starts, companions = named("PlayerStart_"), named("Companion_")

check("one floor", len(floors), 1)
check("four outer walls", len(outers), 4)
check("one player start", len(starts), 1)
check("one companion", len(companions), 1)

mazeWalls = [a for a in level.actors if a.label.startswith("Wall_")]
mazeBox = (min(footprint(a)[0] for a in mazeWalls), min(footprint(a)[1] for a in mazeWalls),
           max(footprint(a)[2] for a in mazeWalls), max(footprint(a)[3] for a in mazeWalls))
check("the floor covers the whole maze", contains(footprint(floors[0]), mazeBox), True)
check("its top surface is at zero",
      round(floors[0].location[2] + floors[0].scale[2] * 50.0, 3), 0.0)

pen = (min(footprint(a)[0] for a in outers), min(footprint(a)[1] for a in outers),
       max(footprint(a)[2] for a in outers), max(footprint(a)[3] for a in outers))
check("the outer walls enclose the maze", contains(pen, mazeBox), True)
check("the floor reaches past them", contains(footprint(floors[0]), pen), True)
gapW = round(mazeBox[0] - (pen[0] + 100.0))
gapE = round((pen[2] - 100.0) - mazeBox[2])
print("     walkable gap: %d cm west, %d cm east" % (gapW, gapE))
check("there is the margin asked for, all round", (gapW, gapE), (700, 700))

route0 = data["route"][0]
check("the player starts on the first route point",
      [round(starts[0].location[0]), round(starts[0].location[1])],
      [round(route0[0]), round(route0[1])])
farFromStart = round(((companions[0].location[0] - starts[0].location[0]) ** 2
                      + (companions[0].location[1] - starts[0].location[1]) ** 2) ** 0.5)
print("     she waits %d cm from the player start" % farFromStart)
check("by default she is somewhere to be found, not beside him", farFromStart > 500, True)
check("the companion is the project blueprint", companions[0].cls, ("bp", "/Game/AI/BP_Companion"))
builtCount = len(level.actors)
check("and all of it clears with the rest", Tools.clear_map(), builtCount)
check("leaving the level empty", len(level.actors), 0)

print("")
print("--- a maze of any size gets a floor to match ---")
for label, half in [("tiny", 300.0), ("huge", 6000.0)]:
    Tools2, level2, _ = load_toolset()
    tiny = {"cubeMesh": "/Engine/BasicShapes/Cube.Cube", "wallHeight": 300, "walls": [
        {"location": [-half, 0, 150], "size": [60, half * 2, 300], "scale": [0.6, half / 50, 3]},
        {"location": [half, 0, 150], "size": [60, half * 2, 300], "scale": [0.6, half / 50, 3]}],
        "finds": [], "route": [[0, 0, 0]]}
    Tools2.build_map(json.dumps(tiny), outer_margin=700.0)
    floor = [a for a in level2.actors if a.label.startswith("Floor")][0]
    width = round(footprint(floor)[2] - footprint(floor)[0])
    want = round(half * 2 + 60 + 700 * 2 + 100 * 2)
    print("     %s maze %d cm wide -> floor %d cm" % (label, round(half * 2 + 60), width))
    check("the %s maze gets a floor that fits" % label, width, want)

print("")
print("--- refusing bad input ---")
for bad, expect in [("not json at all", "valid JSON"),
                    ('{"route": []}', "no walls"),
                    ('{"walls": [{"location": [1, 2], "scale": [1, 1, 1]}]}', "three numbers")]:
    try:
        Tools.build_map(bad)
        check("refuses %r" % bad[:26], "no error raised", expect)
    except ValueError as error:
        check("refuses %r" % bad[:26], expect in str(error), True)

print("")
print("%d FAILED" % len(failures) if failures else "all good")
sys.exit(1 if failures else 0)
