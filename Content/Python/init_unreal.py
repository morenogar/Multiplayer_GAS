# Viper: registers this project's own MCP toolsets when the editor starts.
#
# The editor's Python plugin runs this file automatically on startup, and the toolsets registered
# here then appear as tools on the MCP server the editor hosts (the one .mcp.json points at). The
# toolsets that ship with the engine cannot create actors at all, which is why this exists.

import unreal

from viper_map_toolset import toolsets

if toolsets.registration.register():
    unreal.log("Viper: toolsets registered (map: build_map, clear_map, map_summary; "
               "statetree: describe/duplicate/conditions/bind/compile).")
else:
    unreal.log_warning(
        "Viper: the toolset registry is unavailable, so the map tools were not registered. "
        "Check that the ModelContextProtocol plugin is enabled."
    )
