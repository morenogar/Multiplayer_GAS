"""Builds the browser lab's test map in the open level, as cubes, ready to play.

Why this exists: the toolsets the engine ships could place actors one at a time, but a maze is
twenty-odd of them plus a floor, a boundary and the two characters, and the interesting part is
that all of it has to be derived from a maze whose size changes every time it is generated.

The input is exactly what the lab's "Copy for Unreal" button puts on the clipboard:

    {
      "cubeMesh": "/Engine/BasicShapes/Cube.Cube",
      "wallHeight": 300,
      "walls": [ { "location": [x, y, z], "size": [w, d, h], "scale": [sx, sy, sz] } ],
      "finds": [ [x, y, z] ],
      "route": [ [x, y, z] ]
    }

Centimetres, Z up, location at the centre of each wall, scale for the 100 cm engine cube.

Everything else is worked out from the walls' own extent, so a bigger maze gets a bigger floor:

    floor          one flat cube under the lot, its top surface at z = 0
    outer walls    four of them, a margin out from the maze, so the ground between the maze and
                   the drop is walkable and nobody falls off the edge
    player start   on the first route point, or the middle of the maze when there is no route
    companion      a couple of metres from the player start, so she is there to be recruited

Everything spawned is tagged and foldered, so clear_map can take it all away again and nothing
hand-placed in the level is ever touched.
"""

import json
from typing import List, Optional, Sequence, Tuple

import unreal

import toolset_registry

# Every actor these tools create carries this tag. clear_map deletes by tag and by nothing else,
# which is what keeps it from eating a level someone built by hand.
MAP_TAG = "ViperTestMap"

DEFAULT_CUBE = "/Engine/BasicShapes/Cube.Cube"
DEFAULT_SPHERE = "/Engine/BasicShapes/Sphere.Sphere"

# What a "thing to examine" becomes in the level: a clue, which is what the detective story is
# actually made of, and which the companion can be sent to find. ViperItem is the fallback for a
# project where the clue class is not available.
FIND_CLASS = "/Script/Multiplayer_GAS.ViperClueActor"
FIND_FALLBACK_CLASS = "/Script/Multiplayer_GAS.ViperItem"
COMPANION_ASSET = "/Game/AI/BP_Companion"

FLOOR_THICKNESS = 40.0
OUTER_WALL_THICKNESS = 100.0
SPAWN_HEIGHT = 110.0          # above the floor, so a capsule settles rather than starts buried


@unreal.uclass()
class ViperMapTools(unreal.ToolsetDefinition):
    """Build and clear the companion test map in the current level."""

    @toolset_registry.tool_call
    @staticmethod
    def build_map(
        map_json: str,
        folder: str = "ViperTestMap",
        clear_first: bool = True,
        spawn_finds: bool = True,
        spawn_route: bool = True,
        build_floor: bool = True,
        outer_margin: float = 700.0,
        outer_wall_height: float = 500.0,
        spawn_player_start: bool = True,
        spawn_companion: bool = True,
    ) -> str:
        """Builds a playable test map of cubes in the level that is currently open.

        Args:
            map_json: The map, as the browser lab's "Copy for Unreal" button produces it.
            folder: World Outliner folder to put everything in.
            clear_first: Remove anything a previous build left before building.
            spawn_finds: Also place an interactable at each find, for her to examine.
            spawn_route: Also place a TargetPoint at each route point.
            build_floor: Also lay a floor under the whole thing, sized to the maze.
            outer_margin: Walkable ground to leave between the maze and the outer wall, in cm.
            outer_wall_height: Height of the four outer walls, in cm. 0 leaves them out.
            spawn_player_start: Also place a PlayerStart in the maze.
            spawn_companion: Also place the companion near the player start.

        Returns:
            A line saying what was built, and anything it could not do.
        """
        try:
            data = json.loads(map_json)
        except json.JSONDecodeError as error:
            raise ValueError("map_json is not valid JSON: %s" % error) from error

        walls = data.get("walls") or []
        finds = data.get("finds") or []
        route = data.get("route") or []
        if not walls and not finds and not route:
            raise ValueError(
                "That JSON holds no walls, finds or route. Use the lab's Copy for Unreal button, "
                "not Copy map, which is the lab's own format."
            )

        cube = unreal.load_asset(data.get("cubeMesh") or DEFAULT_CUBE)
        if cube is None:
            raise ValueError("Could not load the cube mesh %r." % data.get("cubeMesh"))

        wall_height = float(data.get("wallHeight") or 300.0)
        removed = ViperMapTools.clear_map(folder) if clear_first else 0
        notes: List[str] = []
        built: List[str] = []

        with unreal.ScopedEditorTransaction("Build the Viper test map"):
            for index, wall in enumerate(walls):
                location = ViperMapTools._vector(wall.get("location"), "wall %d location" % index)
                scale = ViperMapTools._vector(wall.get("scale"), "wall %d scale" % index)
                if ViperMapTools._spawn_mesh(cube, location, scale, "Wall_%02d" % index, folder) is None:
                    notes.append("wall %d could not be spawned" % index)
            built.append("%d walls" % len(walls))

            # The ground and its boundary, both worked out from how big this maze happens to be.
            bounds = ViperMapTools._maze_bounds(walls, finds, route)
            if bounds is None:
                notes.append("the maze has no extent, so no floor or outer walls were built")
            else:
                min_x, min_y, max_x, max_y = bounds
                outer = (min_x - outer_margin, min_y - outer_margin,
                         max_x + outer_margin, max_y + outer_margin)

                if build_floor:
                    ViperMapTools._build_floor(cube, outer, folder)
                    built.append("a %.0f x %.0f m floor"
                                 % ((outer[2] - outer[0]) / 100.0, (outer[3] - outer[1]) / 100.0))

                if outer_wall_height > 0.0:
                    ViperMapTools._build_outer_walls(cube, outer, outer_wall_height, folder)
                    built.append("4 outer walls")

            if spawn_finds:
                notes.extend(ViperMapTools._build_finds(finds, folder))
                built.append("%d finds" % len(finds))

            if spawn_route:
                for index, point in enumerate(route):
                    location = ViperMapTools._vector(point, "route point %d" % index)
                    ViperMapTools._spawn(unreal.TargetPoint, location, "Route_%02d" % index, folder)
                built.append("%d route points" % len(route))

            # Where play starts: the first route point, or the middle of the maze.
            start = ViperMapTools._start_point(route, walls, finds)
            if spawn_player_start and start is not None:
                ViperMapTools._spawn(unreal.PlayerStart, start, "PlayerStart_Viper", folder)
                built.append("a player start")
                notes.extend(ViperMapTools._other_player_starts())

            if spawn_companion and start is not None:
                # In a try: a blueprint that will not load is a note, not a dead tool call. The
                # first version called EditorAssetLibrary.load_blueprint_class inside the
                # transaction and the whole HTTP connection died, taking the build with it - the
                # editor stayed up and the level stayed empty, which reads like nothing happened.
                try:
                    blueprint = unreal.EditorAssetLibrary.load_asset(COMPANION_ASSET)
                    companion_class = blueprint.generated_class() if blueprint else None
                except Exception as error:                       # noqa: BLE001 - any load failure
                    companion_class = None
                    notes.append("loading %s raised %s" % (COMPANION_ASSET, error))

                if companion_class is None:
                    notes.append("%s could not be loaded, so no companion was placed" % COMPANION_ASSET)
                else:
                    # Where she waits says what the test is. Beside you, you are testing what
                    # happens after recruiting; off in the maze, you are testing the finding and
                    # the recruiting, which is the harder and more interesting half.
                    play = data.get("play") or {}
                    with_you = str(play.get("companion", "recruit")) == "withYou"

                    if with_you:
                        where = unreal.Vector(start.x + 220.0, start.y + 160.0, start.z)
                    else:
                        where = ViperMapTools._far_from_start(start, route, finds)

                    ViperMapTools._spawn(companion_class, where, "Companion_Viper", folder)
                    away = ((where.x - start.x) ** 2 + (where.y - start.y) ** 2) ** 0.5
                    built.append("the companion %s" % (
                        "beside you" if with_you else "waiting %.0f m away" % (away / 100.0)))

        summary = "Built " + ", ".join(built) + " in '%s'." % folder
        if removed:
            summary += " Removed %d actors from the previous build first." % removed

        # Said every time, because a map whose navmesh has not caught up looks like a pathfinding
        # bug and is not one.
        summary += (
            " The navmesh only covers this once it has rebuilt: the level needs a"
            " NavMeshBoundsVolume over the area, and with static generation you have to rebuild it"
            " (Build > Build Paths)."
        )
        if notes:
            summary += " Note: " + "; ".join(notes) + "."
        return summary

    @toolset_registry.tool_call
    @staticmethod
    def build_map_from_clipboard(
        folder: str = "ViperTestMap",
        clear_first: bool = True,
    ) -> str:
        """Builds the map that is on the clipboard, straight from the lab's Copy for Unreal button.

        The page cannot call the editor itself - it is served over https and the editor listens on
        plain http, which the browser refuses to mix - so the clipboard is the shortest honest
        bridge: press the button there, call this here, and nothing has to be pasted anywhere.

        Args:
            folder: World Outliner folder to put everything in.
            clear_first: Remove anything a previous build left before building.

        Returns:
            The same line build_map returns.
        """
        text = ViperMapTools._clipboard_text()
        if not text:
            raise ValueError(
                "The clipboard is empty, or holds something that is not text. Press 'Copy for "
                "Unreal' in the lab first - not 'Copy map', which is the lab's own format."
            )
        return ViperMapTools.build_map(text, folder=folder, clear_first=clear_first)

    @toolset_registry.tool_call
    @staticmethod
    def clear_map(folder: str = "ViperTestMap") -> int:
        """Deletes everything a previous build_map left in the level.

        Args:
            folder: The folder it was built into. Kept for symmetry with build_map; the tag is
                what actually decides, so nothing outside a build is ever touched.

        Returns:
            How many actors were deleted.
        """
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if subsystem is None:
            raise RuntimeError("No EditorActorSubsystem: is a level open?")

        doomed = [
            actor for actor in subsystem.get_all_level_actors()
            if actor is not None and actor.actor_has_tag(unreal.Name(MAP_TAG))
        ]

        with unreal.ScopedEditorTransaction("Clear the Viper test map"):
            for actor in doomed:
                subsystem.destroy_actor(actor)

        return len(doomed)

    @toolset_registry.tool_call
    @staticmethod
    def map_summary() -> str:
        """Says what of a built test map is currently in the level.

        Returns:
            A line counting what is there.
        """
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if subsystem is None:
            raise RuntimeError("No EditorActorSubsystem: is a level open?")

        counts = {"walls": 0, "finds": 0, "route points": 0, "outer walls": 0,
                  "floors": 0, "player starts": 0, "companions": 0, "other": 0}
        for actor in subsystem.get_all_level_actors():
            if actor is None or not actor.actor_has_tag(unreal.Name(MAP_TAG)):
                continue
            label = actor.get_actor_label()
            if label.startswith("Wall_"):
                counts["walls"] += 1
            elif label.startswith("Find_"):
                counts["finds"] += 1
            elif label.startswith("Route_"):
                counts["route points"] += 1
            elif label.startswith("Outer_"):
                counts["outer walls"] += 1
            elif label.startswith("Floor"):
                counts["floors"] += 1
            elif label.startswith("PlayerStart_"):
                counts["player starts"] += 1
            elif label.startswith("Companion_"):
                counts["companions"] += 1
            else:
                counts["other"] += 1

        if not any(counts.values()):
            return "No test map in this level."
        return "In this level: " + ", ".join(
            "%d %s" % (count, name) for name, count in counts.items() if count)

    # ------------------------------------------------------------------ the ground

    @staticmethod
    def _maze_bounds(walls, finds, route) -> Optional[Tuple[float, float, float, float]]:
        """The footprint everything has to sit on, from the walls themselves.

        A maze is a different size every time it is generated, so the floor and the boundary are
        measured off it rather than fixed. Finds and route points count too: one of them outside
        the walls would otherwise end up over the edge.
        """
        min_x = min_y = float("inf")
        max_x = max_y = float("-inf")

        for wall in walls:
            location = wall.get("location") or []
            size = wall.get("size")
            if not size:
                # Older exports carried only the scale; the cube it scales is 100 cm.
                scale = wall.get("scale") or [1, 1, 1]
                size = [float(scale[0]) * 100.0, float(scale[1]) * 100.0, 0.0]
            if len(location) < 2 or len(size) < 2:
                continue
            min_x = min(min_x, float(location[0]) - float(size[0]) / 2.0)
            max_x = max(max_x, float(location[0]) + float(size[0]) / 2.0)
            min_y = min(min_y, float(location[1]) - float(size[1]) / 2.0)
            max_y = max(max_y, float(location[1]) + float(size[1]) / 2.0)

        for point in list(finds) + list(route):
            if len(point) < 2:
                continue
            min_x = min(min_x, float(point[0]))
            max_x = max(max_x, float(point[0]))
            min_y = min(min_y, float(point[1]))
            max_y = max(max_y, float(point[1]))

        if min_x > max_x or min_y > max_y:
            return None
        return (min_x, min_y, max_x, max_y)

    @staticmethod
    def _build_floor(cube, outer: Tuple[float, float, float, float], folder: str) -> None:
        """One flat cube under the lot, its top surface at z = 0 so the maze sits on it."""
        # Out to the middle of the outer walls, so there is no seam for anyone to fall through.
        half = OUTER_WALL_THICKNESS
        width = (outer[2] - outer[0]) + half * 2.0
        depth = (outer[3] - outer[1]) + half * 2.0
        centre = unreal.Vector((outer[0] + outer[2]) / 2.0, (outer[1] + outer[3]) / 2.0,
                               -FLOOR_THICKNESS / 2.0)
        scale = unreal.Vector(width / 100.0, depth / 100.0, FLOOR_THICKNESS / 100.0)
        ViperMapTools._spawn_mesh(cube, centre, scale, "Floor", folder)

    @staticmethod
    def _build_outer_walls(cube, outer: Tuple[float, float, float, float],
                           height: float, folder: str) -> None:
        """Four walls a margin out from the maze, so the ground around it is a pen, not a ledge.

        The gap between them and the maze is deliberate: it is somewhere to walk, and it is where
        a companion sent round the outside of the maze actually goes.
        """
        min_x, min_y, max_x, max_y = outer
        thickness = OUTER_WALL_THICKNESS
        mid_x = (min_x + max_x) / 2.0
        mid_y = (min_y + max_y) / 2.0
        span_x = (max_x - min_x) + thickness * 2.0
        span_y = (max_y - min_y)
        z = height / 2.0

        sides = [
            ("Outer_North", unreal.Vector(mid_x, min_y - thickness / 2.0, z),
             unreal.Vector(span_x / 100.0, thickness / 100.0, height / 100.0)),
            ("Outer_South", unreal.Vector(mid_x, max_y + thickness / 2.0, z),
             unreal.Vector(span_x / 100.0, thickness / 100.0, height / 100.0)),
            ("Outer_West", unreal.Vector(min_x - thickness / 2.0, mid_y, z),
             unreal.Vector(thickness / 100.0, span_y / 100.0, height / 100.0)),
            ("Outer_East", unreal.Vector(max_x + thickness / 2.0, mid_y, z),
             unreal.Vector(thickness / 100.0, span_y / 100.0, height / 100.0)),
        ]
        for label, location, scale in sides:
            ViperMapTools._spawn_mesh(cube, location, scale, label, folder)

    # ------------------------------------------------------------------ the cast

    @staticmethod
    def _start_point(route, walls, finds) -> Optional[unreal.Vector]:
        """Where play begins: the first route point, or the middle of the maze."""
        if route:
            point = route[0]
            if len(point) >= 2:
                return unreal.Vector(float(point[0]), float(point[1]), SPAWN_HEIGHT)

        bounds = ViperMapTools._maze_bounds(walls, finds, route)
        if bounds is None:
            return None
        return unreal.Vector((bounds[0] + bounds[2]) / 2.0, (bounds[1] + bounds[3]) / 2.0,
                             SPAWN_HEIGHT)

    @staticmethod
    def _far_from_start(start: unreal.Vector, route, finds) -> unreal.Vector:
        """The furthest route point or find from where play begins.

        Somewhere she has to be looked for, and somewhere known to be reachable: every one of
        these came from the maze generator, which only offers points inside the maze.
        """
        best = unreal.Vector(start.x, start.y, start.z)
        best_distance = -1.0

        for point in list(route) + list(finds):
            if len(point) < 2:
                continue
            distance = ((float(point[0]) - start.x) ** 2 + (float(point[1]) - start.y) ** 2) ** 0.5
            if distance > best_distance:
                best_distance = distance
                best = unreal.Vector(float(point[0]), float(point[1]), SPAWN_HEIGHT)

        return best

    @staticmethod
    def _other_player_starts() -> List[str]:
        """Warns about player starts that were already in the level.

        Not deleted and not moved: they are the level's, not ours. But two of them means the game
        picks one, and a test that starts somewhere different each run is worse than no test.
        """
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if subsystem is None:
            return []

        strangers = [
            actor for actor in subsystem.get_all_level_actors()
            if actor is not None
            and isinstance(actor, unreal.PlayerStart)
            and not actor.actor_has_tag(unreal.Name(MAP_TAG))
        ]
        if not strangers:
            return []
        return ["the level already had %d other PlayerStart(s), so play may begin at one of those "
                "instead - delete or move them, or pass spawn_player_start=False" % len(strangers)]

    @staticmethod
    def _build_finds(finds, folder: str) -> List[str]:
        """An interactable at each find when the class loads, and a marker when it does not."""
        notes: List[str] = []
        find_class = unreal.load_class(None, FIND_CLASS)
        if find_class is None:
            find_class = unreal.load_class(None, FIND_FALLBACK_CLASS)
            if find_class is not None:
                notes.append("%s would not load, so the finds are plain interactables rather than "
                             "clues" % FIND_CLASS)

        sphere = unreal.load_asset(DEFAULT_SPHERE) if find_class is None else None
        if find_class is None:
            notes.append("neither %s nor %s would load, so the finds are plain spheres and she "
                         "will not examine them" % (FIND_CLASS, FIND_FALLBACK_CLASS))

        for index, point in enumerate(finds):
            location = ViperMapTools._vector(point, "find %d" % index)
            if find_class is not None:
                actor = ViperMapTools._spawn(find_class, location, "Find_%02d" % index, folder)
            else:
                actor = ViperMapTools._spawn_mesh(sphere, location,
                                                  unreal.Vector(0.5, 0.5, 0.5),
                                                  "Find_%02d" % index, folder)
            if actor is None:
                notes.append("find %d could not be spawned" % index)
        return notes

    # ------------------------------------------------------------------ helpers

    @staticmethod
    def _clipboard_text() -> str:
        """Whatever text is on the Windows clipboard, or an empty string.

        Through ctypes rather than a library: the editor's Python is a full CPython, and this
        needs no dependency that a fresh checkout would not have.
        """
        try:
            import ctypes
            from ctypes import wintypes
        except Exception:                                    # noqa: BLE001 - not Windows
            return ""

        CF_UNICODETEXT = 13
        user32 = ctypes.windll.user32
        kernel32 = ctypes.windll.kernel32

        # Every one of these needs its types spelled out. Without them ctypes assumes a 32-bit
        # int, and a 64-bit clipboard handle comes back as "OverflowError: int too long to
        # convert" - which it did, on GlobalUnlock, the one call easy to forget.
        user32.OpenClipboard.argtypes = [wintypes.HWND]
        user32.OpenClipboard.restype = wintypes.BOOL
        user32.GetClipboardData.argtypes = [wintypes.UINT]
        user32.GetClipboardData.restype = wintypes.HANDLE
        user32.CloseClipboard.argtypes = []
        user32.CloseClipboard.restype = wintypes.BOOL
        kernel32.GlobalLock.argtypes = [wintypes.HANDLE]
        kernel32.GlobalLock.restype = ctypes.c_void_p
        kernel32.GlobalUnlock.argtypes = [wintypes.HANDLE]
        kernel32.GlobalUnlock.restype = wintypes.BOOL

        if not user32.OpenClipboard(None):
            return ""
        try:
            handle = user32.GetClipboardData(CF_UNICODETEXT)
            if not handle:
                return ""
            pointer = kernel32.GlobalLock(handle)
            if not pointer:
                return ""
            try:
                return ctypes.c_wchar_p(pointer).value or ""
            finally:
                kernel32.GlobalUnlock(handle)
        finally:
            user32.CloseClipboard()

    @staticmethod
    def _vector(value, what: str) -> unreal.Vector:
        """A three-number list from the JSON, as a vector. Raises if it is not one."""
        if not isinstance(value, (list, tuple)) or len(value) != 3:
            raise ValueError("%s should be three numbers, got %r" % (what, value))
        try:
            return unreal.Vector(float(value[0]), float(value[1]), float(value[2]))
        except (TypeError, ValueError) as error:
            raise ValueError("%s is not numeric: %r" % (what, value)) from error

    @staticmethod
    def _spawn(actor_class, location: unreal.Vector, label: str, folder: str):
        """Spawns one actor, labelled, foldered and tagged so clear_map can find it again."""
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if subsystem is None:
            raise RuntimeError("No EditorActorSubsystem: is a level open?")

        actor = subsystem.spawn_actor_from_class(actor_class, location, unreal.Rotator(0, 0, 0))
        if actor is None:
            return None

        actor.set_actor_label(label)
        actor.set_folder_path(unreal.Name(folder))
        actor.tags = [unreal.Name(MAP_TAG)]
        return actor

    @staticmethod
    def _spawn_mesh(mesh, location: unreal.Vector, scale: unreal.Vector, label: str, folder: str):
        """A StaticMeshActor with that mesh and scale. Static mobility, so it blocks and navs."""
        actor = ViperMapTools._spawn(unreal.StaticMeshActor, location, label, folder)
        if actor is None:
            return None

        component = actor.static_mesh_component
        component.set_editor_property("static_mesh", mesh)
        component.set_mobility(unreal.ComponentMobility.STATIC)
        actor.set_actor_scale3d(scale)
        return actor
