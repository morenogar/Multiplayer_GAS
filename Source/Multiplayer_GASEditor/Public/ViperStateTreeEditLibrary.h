// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ViperStateTreeEditLibrary.generated.h"

class UStateTree;

/**
 * Edits the structure of a StateTree asset from outside the StateTree editor: Python, the MCP
 * toolset (Content/Python/viper_map_toolset/toolsets/viper_statetree.py) or Blueprint.
 *
 * Why it exists: the engine's generic tools (ObjectTools.get/set_properties) can change the
 * values inside a StateTree, but not its bindings - EditorBindings is not readable that way - and
 * a condition that is not bound to anything compares two constants. Adding a state that reads the
 * companion evaluator therefore needs the editor-data API, which only C++ can reach.
 *
 * States are addressed by name path from the root, e.g. "Root/Follow/EyeContact".
 * Every function returns a report; a failure starts with "ERROR:" and changes nothing.
 * Nothing here saves the asset: compile with CompileStateTree, then save it (AssetTools.save_assets
 * over MCP, or Ctrl+S). Every edit is a transaction, so Ctrl+Z in the editor undoes it.
 *
 * Editor-only. Authority/networking: not applicable, this never runs in a game.
 */
UCLASS()
class MULTIPLAYER_GASEDITOR_API UViperStateTreeEditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Dumps the tree as JSON: every state with its path, enter conditions (values and what each
	 * input is bound to), tasks (with their bindings) and transitions, plus the evaluators.
	 * This is the way to see what a condition actually reads.
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString DescribeStateTree(UStateTree* StateTree);

	/**
	 * Copies a state - its conditions, tasks, transitions, children and the bindings of all of
	 * them - under the same parent, with fresh IDs.
	 *
	 * @param SourceStatePath  State to copy, e.g. "Root/Follow/WanderOff".
	 * @param NewName          Name of the copy. Must not clash with a sibling.
	 * @param InsertBeforeName Sibling to insert the copy in front of (children are selected in
	 *                         order, so this is its priority). Empty = right after the source.
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString DuplicateState(UStateTree* StateTree, const FString& SourceStatePath,
		const FString& NewName, const FString& InsertBeforeName);

	/** Removes every enter condition of a state (and their bindings). */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString ClearEnterConditions(UStateTree* StateTree, const FString& StatePath);

	/**
	 * Adds "evaluator.<EvaluatorProperty> == bExpected" as an enter condition (AND with the rest).
	 * The property is looked up on the tree's evaluators' instance data, e.g. "bLeaderNoticesMe".
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString AddBoolEnterCondition(UStateTree* StateTree, const FString& StatePath,
		const FString& EvaluatorProperty, bool bExpected);

	/**
	 * Adds "evaluator.<EvaluatorProperty> <Operator> Value" as an enter condition (AND).
	 * Operator: Less, LessOrEqual, Equal, NotEqual, GreaterOrEqual, Greater.
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString AddFloatEnterCondition(UStateTree* StateTree, const FString& StatePath,
		const FString& EvaluatorProperty, const FString& Operator, float Value);

	/**
	 * Binds one input of a node in a state to an evaluator output, replacing any binding it had.
	 *
	 * @param NodeKind          "Condition" (enter conditions), "Task", or "Transition[N]" for the
	 *                          conditions of the state's N-th transition.
	 * @param NodeIndex         Index in that list, as DescribeStateTree prints it.
	 * @param TargetProperty    Property on the node's instance data, e.g. "bLeft" or "LeashCentre".
	 * @param EvaluatorProperty Evaluator output to read, e.g. "bLeaderNoticesMe" or "LeaderAnchor".
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString BindToEvaluator(UStateTree* StateTree, const FString& StatePath,
		const FString& NodeKind, int32 NodeIndex, const FString& TargetProperty,
		const FString& EvaluatorProperty);

	/**
	 * Removes every transition of a state (and their conditions' bindings).
	 * A running state only gives way through its transitions, so a state copied from another one
	 * usually needs its own instead of the copied ones.
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString ClearTransitions(UStateTree* StateTree, const FString& StatePath);

	/**
	 * Adds a "go to state" transition with no conditions; add them with Add*TransitionCondition.
	 *
	 * @param Trigger         OnStateCompleted, OnStateSucceeded, OnStateFailed or OnTick.
	 * @param TargetStatePath State to go to, e.g. "Root/Follow" (going to a parent re-selects
	 *                        among its children, which is how this tree re-evaluates).
	 */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString AddTransition(UStateTree* StateTree, const FString& StatePath, const FString& Trigger,
		const FString& TargetStatePath);

	/** Adds "evaluator.<EvaluatorProperty> == bExpected" to the state's TransitionIndex-th transition (AND). */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString AddBoolTransitionCondition(UStateTree* StateTree, const FString& StatePath,
		int32 TransitionIndex, const FString& EvaluatorProperty, bool bExpected);

	/** Adds "evaluator.<EvaluatorProperty> <Operator> Value" to the state's TransitionIndex-th transition (AND). */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString AddFloatTransitionCondition(UStateTree* StateTree, const FString& StatePath,
		int32 TransitionIndex, const FString& EvaluatorProperty, const FString& Operator, float Value);

	/** Compiles the tree (what the editor does on save) and returns the compiler's messages. */
	UFUNCTION(BlueprintCallable, Category = "Viper|StateTree")
	static FString CompileStateTree(UStateTree* StateTree);
};
