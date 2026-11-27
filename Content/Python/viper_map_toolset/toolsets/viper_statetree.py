"""Edits the structure of a StateTree asset over MCP: states, enter conditions and bindings.

Why this exists: the engine's ObjectTools can change values inside a StateTree but cannot see its
bindings, and a condition that is not bound compares two constants. The work is done in C++ by
UViperStateTreeEditLibrary (module Multiplayer_GASEditor); this only puts it on the MCP server.

States are addressed by name path, e.g. "Root/Follow/EyeContact". Every tool returns a report; a
failure starts with "ERROR:" and changes nothing. Nothing is saved: run compile_state_tree, then
AssetTools.save_assets. Each edit is an editor transaction, so Ctrl+Z undoes it.

Typical use, adding a state that behaves like an existing one but reads other facts:

    duplicate_state(tree, "Root/Follow/WanderOff", "RoamArea", "WanderOff")
    clear_enter_conditions(tree, "Root/Follow/RoamArea")
    add_bool_enter_condition(tree, "Root/Follow/RoamArea", "bLeaderSettled", True)
    bind_to_evaluator(tree, "Root/Follow/RoamArea", "Task", 0, "LeashCentre", "LeaderAnchor")
    compile_state_tree(tree)
"""

import unreal

import toolset_registry

DEFAULT_TREE = "/Game/AI/ST_Companion"


def _library():
    library = getattr(unreal, "ViperStateTreeEditLibrary", None)
    if library is None:
        raise RuntimeError(
            "UViperStateTreeEditLibrary is not loaded: build Multiplayer_GASEditor and restart the editor."
        )
    return library


def _tree(asset_path: str):
    tree = unreal.load_asset(asset_path or DEFAULT_TREE)
    if tree is None or not isinstance(tree, unreal.StateTree):
        raise ValueError(f"'{asset_path}' is not a StateTree asset.")
    return tree


@unreal.uclass()
class ViperStateTreeTools(unreal.ToolsetDefinition):
    """Edit StateTree structure: duplicate states, set enter conditions, bind inputs to the evaluator."""

    @toolset_registry.tool_call
    @staticmethod
    def describe_state_tree(asset_path: str = DEFAULT_TREE) -> str:
        """Dumps the StateTree as JSON, including what every condition and task input is bound to.

        Args:
            asset_path: The StateTree asset, e.g. /Game/AI/ST_Companion.

        Returns:
            JSON with evaluators and every state (path, conditions with values and bindings,
            tasks with bindings, transitions).
        """
        return _library().describe_state_tree(_tree(asset_path))

    @toolset_registry.tool_call
    @staticmethod
    def duplicate_state(asset_path: str, source_state_path: str, new_name: str, insert_before_name: str = "") -> str:
        """Copies a state (conditions, tasks, transitions, children and their bindings) under the same parent.

        Args:
            asset_path: The StateTree asset.
            source_state_path: State to copy, e.g. Root/Follow/WanderOff.
            new_name: Name of the copy.
            insert_before_name: Sibling to place the copy in front of (its priority). Empty = right after the source.

        Returns:
            What was copied, or an ERROR line.
        """
        return _library().duplicate_state(_tree(asset_path), source_state_path, new_name, insert_before_name)

    @toolset_registry.tool_call
    @staticmethod
    def clear_enter_conditions(asset_path: str, state_path: str) -> str:
        """Removes every enter condition of a state, and their bindings.

        Args:
            asset_path: The StateTree asset.
            state_path: e.g. Root/Follow/RoamArea.

        Returns:
            How many were removed, or an ERROR line.
        """
        return _library().clear_enter_conditions(_tree(asset_path), state_path)

    @toolset_registry.tool_call
    @staticmethod
    def add_bool_enter_condition(asset_path: str, state_path: str, evaluator_property: str, expected: bool) -> str:
        """Adds 'evaluator.<evaluator_property> == expected' as an enter condition (AND with the others).

        Args:
            asset_path: The StateTree asset.
            state_path: e.g. Root/Follow/RoamArea.
            evaluator_property: A bool output of the evaluator, e.g. bLeaderNoticesMe.
            expected: The value it must have.

        Returns:
            The new condition and its binding, or an ERROR line.
        """
        return _library().add_bool_enter_condition(_tree(asset_path), state_path, evaluator_property, expected)

    @toolset_registry.tool_call
    @staticmethod
    def add_float_enter_condition(asset_path: str, state_path: str, evaluator_property: str, operator: str, value: float) -> str:
        """Adds 'evaluator.<evaluator_property> <operator> value' as an enter condition (AND with the others).

        Args:
            asset_path: The StateTree asset.
            state_path: e.g. Root/Follow/ComeOver.
            evaluator_property: A float output of the evaluator, e.g. TimeLookedAt.
            operator: Less, LessOrEqual, Equal, NotEqual, GreaterOrEqual or Greater.
            value: The constant to compare with.

        Returns:
            The new condition and its binding, or an ERROR line.
        """
        return _library().add_float_enter_condition(_tree(asset_path), state_path, evaluator_property, operator, value)

    @toolset_registry.tool_call
    @staticmethod
    def bind_to_evaluator(asset_path: str, state_path: str, node_kind: str, node_index: int, target_property: str, evaluator_property: str) -> str:
        """Binds one input of a condition or task to an evaluator output, replacing any existing binding.

        Args:
            asset_path: The StateTree asset.
            state_path: e.g. Root/Follow/EyeContact.
            node_kind: Condition, Task, or Transition[N] for the conditions of the N-th transition.
            node_index: Index in that list, as describe_state_tree prints it.
            target_property: Input on the node, e.g. bLeft, Left or LeashCentre.
            evaluator_property: Evaluator output, e.g. bLeaderNoticesMe or LeaderAnchor.

        Returns:
            The new binding, or an ERROR line.
        """
        return _library().bind_to_evaluator(_tree(asset_path), state_path, node_kind, node_index, target_property, evaluator_property)

    @toolset_registry.tool_call
    @staticmethod
    def clear_transitions(asset_path: str, state_path: str) -> str:
        """Removes every transition of a state, and their conditions' bindings.

        Args:
            asset_path: The StateTree asset.
            state_path: e.g. Root/Follow/RoamArea.

        Returns:
            How many were removed, or an ERROR line.
        """
        return _library().clear_transitions(_tree(asset_path), state_path)

    @toolset_registry.tool_call
    @staticmethod
    def add_transition(asset_path: str, state_path: str, trigger: str, target_state_path: str) -> str:
        """Adds a 'go to state' transition with no conditions (add them with add_*_transition_condition).

        Args:
            asset_path: The StateTree asset.
            state_path: The state that owns the transition.
            trigger: OnStateCompleted, OnStateSucceeded, OnStateFailed or OnTick.
            target_state_path: e.g. Root/Follow (going to the parent re-selects among its children).

        Returns:
            The new transition's index, or an ERROR line.
        """
        return _library().add_transition(_tree(asset_path), state_path, trigger, target_state_path)

    @toolset_registry.tool_call
    @staticmethod
    def add_bool_transition_condition(asset_path: str, state_path: str, transition_index: int, evaluator_property: str, expected: bool) -> str:
        """Adds 'evaluator.<evaluator_property> == expected' to a transition (AND with its other conditions).

        Args:
            asset_path: The StateTree asset.
            state_path: The state that owns the transition.
            transition_index: Index of the transition, as describe_state_tree prints it.
            evaluator_property: A bool output of the evaluator.
            expected: The value it must have.

        Returns:
            The new condition and its binding, or an ERROR line.
        """
        return _library().add_bool_transition_condition(_tree(asset_path), state_path, transition_index, evaluator_property, expected)

    @toolset_registry.tool_call
    @staticmethod
    def add_float_transition_condition(asset_path: str, state_path: str, transition_index: int, evaluator_property: str, operator: str, value: float) -> str:
        """Adds 'evaluator.<evaluator_property> <operator> value' to a transition (AND with its other conditions).

        Args:
            asset_path: The StateTree asset.
            state_path: The state that owns the transition.
            transition_index: Index of the transition, as describe_state_tree prints it.
            evaluator_property: A float output of the evaluator.
            operator: Less, LessOrEqual, Equal, NotEqual, GreaterOrEqual or Greater.
            value: The constant to compare with.

        Returns:
            The new condition and its binding, or an ERROR line.
        """
        return _library().add_float_transition_condition(_tree(asset_path), state_path, transition_index, evaluator_property, operator, value)

    @toolset_registry.tool_call
    @staticmethod
    def compile_state_tree(asset_path: str = DEFAULT_TREE) -> str:
        """Compiles the StateTree and returns the compiler's messages. Does not save.

        Args:
            asset_path: The StateTree asset.

        Returns:
            'compiled OK' or 'ERROR: compile failed', followed by the compiler messages.
        """
        return _library().compile_state_tree(_tree(asset_path))
