# Viper: the toolsets this project adds to the editor's MCP server.

from toolset_registry.registration import Registration

from viper_map_toolset.toolsets.viper_map import ViperMapTools
from viper_map_toolset.toolsets.viper_statetree import ViperStateTreeTools

registration = Registration([ViperMapTools, ViperStateTreeTools])
