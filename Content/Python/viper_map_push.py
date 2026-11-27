# Builds whatever map was last written to Saved/ViperMapPush.json.
#
# The browser lab cannot call the Python toolset directly: Remote Control, which is the one door
# into the editor a web page is allowed through, calls C++ functions. So the C++ side writes the
# JSON next to the project and runs this, and this does the real work by calling the same toolset
# a person would call by hand. One builder, two doors.

import io
import os

import unreal

from viper_map_toolset.toolsets.viper_map import ViperMapTools

_PUSH_FILE = os.path.join(unreal.Paths.project_saved_dir(), "ViperMapPush.json")


def run():
    path = os.path.abspath(_PUSH_FILE)
    if not os.path.isfile(path):
        unreal.log_warning("Viper: nothing to push, %s is not there." % path)
        return

    with io.open(path, encoding="utf-8") as handle:
        text = handle.read()

    unreal.log("Viper: " + ViperMapTools.build_map(text))


run()
