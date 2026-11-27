// Fill out your copyright notice in the Description page of Project Settings.

#include "ViperStateTreeEditLibrary.h"

#include "Conditions/StateTreeCommonConditions.h"
#include "Dom/JsonObject.h"
#include "Logging/TokenizedMessage.h"
#include "PropertyBindingBinding.h"
#include "PropertyBindingPath.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorNode.h"
#include "StateTreeEditorPropertyBindings.h"
#include "StateTreeState.h"
#include "StateTreeTypes.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "ViperStateTreeEditLibrary"

namespace ViperStateTreeEdit
{
	static FString ReportError(const FString& Message)
	{
		return TEXT("ERROR: ") + Message;
	}

	static UStateTreeEditorData* GetEditorData(UStateTree* StateTree, FString& OutError)
	{
		if (!StateTree)
		{
			OutError = ReportError(TEXT("no StateTree asset given."));
			return nullptr;
		}
		UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
		if (!EditorData)
		{
			OutError = ReportError(FString::Printf(TEXT("%s has no editor data."), *StateTree->GetPathName()));
		}
		return EditorData;
	}

	static FString GetStatePath(const UStateTreeState* State)
	{
		TArray<FString> Names;
		for (const UStateTreeState* It = State; It; It = It->Parent)
		{
			Names.Insert(It->Name.ToString(), 0);
		}
		return FString::Join(Names, TEXT("/"));
	}

	/** "Root/Follow/EyeContact" -> the state. Names compare case-insensitively. */
	static UStateTreeState* FindState(UStateTreeEditorData* EditorData, const FString& Path)
	{
		TArray<FString> Parts;
		Path.ParseIntoArray(Parts, TEXT("/"), /*InCullEmpty*/ true);
		if (Parts.IsEmpty())
		{
			return nullptr;
		}

		const TArray<TObjectPtr<UStateTreeState>>* Candidates = &EditorData->SubTrees;
		UStateTreeState* Found = nullptr;
		for (const FString& Part : Parts)
		{
			Found = nullptr;
			for (UStateTreeState* Candidate : *Candidates)
			{
				if (Candidate && Candidate->Name.ToString().Equals(Part, ESearchCase::IgnoreCase))
				{
					Found = Candidate;
					break;
				}
			}
			if (!Found)
			{
				return nullptr;
			}
			Candidates = &Found->Children;
		}
		return Found;
	}

	static const UStruct* GetInstanceStruct(const FStateTreeEditorNode& Node)
	{
		return Node.GetInstance().GetStruct();
	}

	static const UStruct* GetNodeStruct(const FStateTreeEditorNode& Node)
	{
		return Node.Node.GetScriptStruct();
	}

	/** The evaluator whose instance data has this property, so callers never deal in IDs. */
	static const FStateTreeEditorNode* FindEvaluatorWithProperty(const UStateTreeEditorData* EditorData,
		const FName PropertyName)
	{
		for (const FStateTreeEditorNode& Evaluator : EditorData->Evaluators)
		{
			const UStruct* InstanceStruct = GetInstanceStruct(Evaluator);
			if (InstanceStruct && InstanceStruct->FindPropertyByName(PropertyName))
			{
				return &Evaluator;
			}
		}
		return nullptr;
	}

	static TArray<FStateTreeEditorNode>* GetNodeList(UStateTreeState* State, const FString& NodeKind)
	{
		if (NodeKind.Equals(TEXT("Condition"), ESearchCase::IgnoreCase))
		{
			return &State->EnterConditions;
		}
		if (NodeKind.Equals(TEXT("Task"), ESearchCase::IgnoreCase))
		{
			return &State->Tasks;
		}
		// "Transition[2]" -> the conditions of the state's third transition.
		if (NodeKind.StartsWith(TEXT("Transition["), ESearchCase::IgnoreCase) && NodeKind.EndsWith(TEXT("]"))
			&& NodeKind.Mid(11, NodeKind.Len() - 12).IsNumeric())
		{
			const int32 TransitionIndex = FCString::Atoi(*NodeKind.Mid(11, NodeKind.Len() - 12));
			if (State->Transitions.IsValidIndex(TransitionIndex))
			{
				return &State->Transitions[TransitionIndex].Conditions;
			}
		}
		return nullptr;
	}

	static bool ParseOperator(const FString& Operator, UE::StateTree::EComparisonOperator& OutOperator)
	{
		const int64 Value = StaticEnum<UE::StateTree::EComparisonOperator>()->GetValueByNameString(Operator);
		if (Value == INDEX_NONE)
		{
			return false;
		}
		OutOperator = static_cast<UE::StateTree::EComparisonOperator>(Value);
		return true;
	}

	/** Removes whatever feeds TargetProperty on the node, then binds it to the evaluator output. */
	static FString Bind(UStateTreeEditorData* EditorData, const FStateTreeEditorNode& Target,
		const FString& TargetProperty, const FString& EvaluatorProperty)
	{
		const UStruct* TargetStruct = GetInstanceStruct(Target);
		if (!TargetStruct || !TargetStruct->FindPropertyByName(FName(*TargetProperty)))
		{
			return ReportError(FString::Printf(TEXT("node '%s' has no input called '%s'."),
				*Target.GetName().ToString(), *TargetProperty));
		}

		const FStateTreeEditorNode* Evaluator = FindEvaluatorWithProperty(EditorData, FName(*EvaluatorProperty));
		if (!Evaluator)
		{
			return ReportError(FString::Printf(TEXT("no evaluator has a property called '%s'."), *EvaluatorProperty));
		}

		FPropertyBindingPath TargetPath;
		TargetPath.SetStructID(Target.ID);
		if (!TargetPath.FromString(TargetProperty))
		{
			return ReportError(FString::Printf(TEXT("'%s' is not a valid property path."), *TargetProperty));
		}
		EditorData->RemovePropertyBinding(TargetPath);

		if (!EditorData->AddPropertyBinding(*Evaluator, EvaluatorProperty, Target, TargetProperty))
		{
			return ReportError(FString::Printf(TEXT("could not bind %s.%s <- %s."),
				*Target.GetName().ToString(), *TargetProperty, *EvaluatorProperty));
		}
		return FString::Printf(TEXT("bound %s.%s <- %s.%s"), *Target.GetName().ToString(), *TargetProperty,
			*Evaluator->GetName().ToString(), *EvaluatorProperty);
	}

	/** Every property of a struct as exported text: what a condition compares against. */
	static TSharedRef<FJsonObject> ExportValues(const UStruct* Struct, const void* Memory)
	{
		TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
		if (!Struct || !Memory)
		{
			return Values;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
			{
				continue;
			}
			FString Text;
			Property->ExportTextItem_Direct(Text, Property->ContainerPtrToValuePtr<void>(Memory), nullptr,
				nullptr, PPF_None);
			Values->SetStringField(Property->GetName(), Text);
		}
		return Values;
	}

	struct FBindingInfo
	{
		FString Target;
		FString Source;
	};

	/**
	 * Gives every state a new ID and every node a new ID, copying the bindings across so the copy
	 * reads the same things the original does. Old state IDs are recorded so transitions inside
	 * the copy that pointed at the original (e.g. "go to self") can be repointed at the copy.
	 */
	static void ReassignIDs(UStateTreeState& State, FStateTreeEditorPropertyBindings& Bindings,
		TMap<FGuid, FGuid>& OldToNewStateIDs)
	{
		auto ReassignNode = [&Bindings](FStateTreeEditorNode& Node)
		{
			if (!Node.ID.IsValid())
			{
				return;
			}
			const FGuid OldID = Node.ID;
			Node.ID = FGuid::NewGuid();
			Bindings.CopyBindings(OldID, Node.ID);
		};

		const FGuid OldStateID = State.ID;
		State.ID = FGuid::NewGuid();
		OldToNewStateIDs.Add(OldStateID, State.ID);

		if (State.Parameters.ID.IsValid())
		{
			const FGuid OldParametersID = State.Parameters.ID;
			State.Parameters.ID = FGuid::NewGuid();
			Bindings.CopyBindings(OldParametersID, State.Parameters.ID);
		}

		for (FStateTreeEditorNode& Node : State.EnterConditions) { ReassignNode(Node); }
		for (FStateTreeEditorNode& Node : State.Tasks) { ReassignNode(Node); }
		for (FStateTreeEditorNode& Node : State.Considerations) { ReassignNode(Node); }
		ReassignNode(State.SingleTask);

		for (FStateTreeTransition& Transition : State.Transitions)
		{
			Transition.ID = FGuid::NewGuid();
			for (FStateTreeEditorNode& Node : Transition.Conditions) { ReassignNode(Node); }
		}

		for (UStateTreeState* Child : State.Children)
		{
			if (Child)
			{
				ReassignIDs(*Child, Bindings, OldToNewStateIDs);
			}
		}
	}

	static void RepointInternalTransitions(UStateTreeState& State, const TMap<FGuid, FGuid>& OldToNewStateIDs)
	{
		for (FStateTreeTransition& Transition : State.Transitions)
		{
			if (const FGuid* NewID = OldToNewStateIDs.Find(Transition.State.ID))
			{
				Transition.State.ID = *NewID;
			}
		}
		for (UStateTreeState* Child : State.Children)
		{
			if (Child)
			{
				RepointInternalTransitions(*Child, OldToNewStateIDs);
			}
		}
	}

	static void MarkDirty(UStateTree* StateTree)
	{
		StateTree->MarkPackageDirty();
		UStateTreeEditingSubsystem::MarkAsModified(StateTree);
	}
}

FString UViperStateTreeEditLibrary::DescribeStateTree(UStateTree* StateTree)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}

	// Who is who: struct ID -> a readable label, so a binding prints as a name, not a GUID.
	TMap<FGuid, FString> Labels;
	for (int32 Index = 0; Index < EditorData->Evaluators.Num(); ++Index)
	{
		const FStateTreeEditorNode& Node = EditorData->Evaluators[Index];
		Labels.Add(Node.ID, FString::Printf(TEXT("Evaluator[%d] %s"), Index, *Node.GetName().ToString()));
	}
	for (int32 Index = 0; Index < EditorData->GlobalTasks.Num(); ++Index)
	{
		const FStateTreeEditorNode& Node = EditorData->GlobalTasks[Index];
		Labels.Add(Node.ID, FString::Printf(TEXT("GlobalTask[%d] %s"), Index, *Node.GetName().ToString()));
	}
	EditorData->VisitHierarchy([&Labels](UStateTreeState& State, UStateTreeState*)
	{
		const FString Path = GetStatePath(&State);
		for (int32 Index = 0; Index < State.EnterConditions.Num(); ++Index)
		{
			Labels.Add(State.EnterConditions[Index].ID, FString::Printf(TEXT("%s Condition[%d]"), *Path, Index));
		}
		for (int32 Index = 0; Index < State.Tasks.Num(); ++Index)
		{
			Labels.Add(State.Tasks[Index].ID, FString::Printf(TEXT("%s Task[%d]"), *Path, Index));
		}
		for (int32 T = 0; T < State.Transitions.Num(); ++T)
		{
			for (int32 Index = 0; Index < State.Transitions[T].Conditions.Num(); ++Index)
			{
				Labels.Add(State.Transitions[T].Conditions[Index].ID,
					FString::Printf(TEXT("%s Transition[%d] Condition[%d]"), *Path, T, Index));
			}
		}
		Labels.Add(State.Parameters.ID, Path + TEXT(" Parameters"));
		return EStateTreeVisitor::Continue;
	});

	// Target struct ID -> what feeds each of its properties.
	TMultiMap<FGuid, FBindingInfo> BindingsByTarget;
	EditorData->GetPropertyEditorBindings()->ForEachBinding([&Labels, &BindingsByTarget](const FPropertyBindingBinding& Binding)
	{
		const FPropertyBindingPath& Source = Binding.GetSourcePath();
		const FString* SourceLabel = Labels.Find(Source.GetStructID());
		FBindingInfo Info;
		Info.Target = Binding.GetTargetPath().ToString();
		Info.Source = FString::Printf(TEXT("%s.%s"),
			SourceLabel ? **SourceLabel : *FString::Printf(TEXT("{%s}"), *Source.GetStructID().ToString()),
			*Source.ToString());
		BindingsByTarget.Add(Binding.GetTargetPath().GetStructID(), Info);
	});

	auto DescribeNode = [&BindingsByTarget](const FStateTreeEditorNode& Node, const int32 Index, const bool bWithValues)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("index"), Index);
		Json->SetStringField(TEXT("type"), GetNodeStruct(Node) ? GetNodeStruct(Node)->GetName() : TEXT("None"));
		Json->SetStringField(TEXT("name"), Node.GetName().ToString());
		Json->SetStringField(TEXT("id"), Node.ID.ToString());
		Json->SetStringField(TEXT("operand"), StaticEnum<EStateTreeExpressionOperand>()->GetNameStringByValue(
			static_cast<int64>(Node.ExpressionOperand)));
		Json->SetNumberField(TEXT("indent"), Node.ExpressionIndent);
		if (bWithValues)
		{
			Json->SetObjectField(TEXT("node"), ExportValues(GetNodeStruct(Node), Node.Node.GetMemory()));
			const FStateTreeDataView Instance = Node.GetInstance();
			Json->SetObjectField(TEXT("values"), ExportValues(Instance.GetStruct(), Instance.GetMemory()));
		}
		TArray<TSharedPtr<FJsonValue>> Bound;
		TArray<FBindingInfo> Infos;
		BindingsByTarget.MultiFind(Node.ID, Infos);
		for (const FBindingInfo& Info : Infos)
		{
			Bound.Add(MakeShared<FJsonValueString>(Info.Target + TEXT(" <- ") + Info.Source));
		}
		Json->SetArrayField(TEXT("bindings"), Bound);
		return Json;
	};

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset"), StateTree->GetPathName());

	TArray<TSharedPtr<FJsonValue>> Evaluators;
	for (int32 Index = 0; Index < EditorData->Evaluators.Num(); ++Index)
	{
		Evaluators.Add(MakeShared<FJsonValueObject>(DescribeNode(EditorData->Evaluators[Index], Index, false)));
	}
	Root->SetArrayField(TEXT("evaluators"), Evaluators);

	TArray<TSharedPtr<FJsonValue>> States;
	EditorData->VisitHierarchy([&States, &DescribeNode](UStateTreeState& State, UStateTreeState*)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("path"), GetStatePath(&State));
		Json->SetStringField(TEXT("id"), State.ID.ToString());
		Json->SetBoolField(TEXT("enabled"), State.bEnabled);

		TArray<TSharedPtr<FJsonValue>> Conditions;
		for (int32 Index = 0; Index < State.EnterConditions.Num(); ++Index)
		{
			Conditions.Add(MakeShared<FJsonValueObject>(DescribeNode(State.EnterConditions[Index], Index, true)));
		}
		Json->SetArrayField(TEXT("conditions"), Conditions);

		TArray<TSharedPtr<FJsonValue>> Tasks;
		for (int32 Index = 0; Index < State.Tasks.Num(); ++Index)
		{
			Tasks.Add(MakeShared<FJsonValueObject>(DescribeNode(State.Tasks[Index], Index, false)));
		}
		Json->SetArrayField(TEXT("tasks"), Tasks);

		TArray<TSharedPtr<FJsonValue>> Transitions;
		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
			T->SetStringField(TEXT("trigger"), StaticEnum<EStateTreeTransitionTrigger>()->GetNameStringByValue(
				static_cast<int64>(Transition.Trigger)));
			T->SetStringField(TEXT("link"), StaticEnum<EStateTreeTransitionType>()->GetNameStringByValue(
				static_cast<int64>(Transition.State.LinkType)));
			T->SetStringField(TEXT("to"), Transition.State.Name.ToString());
			T->SetStringField(TEXT("event"), Transition.RequiredEvent.Tag.ToString());
			TArray<TSharedPtr<FJsonValue>> TransitionConditions;
			for (int32 Index = 0; Index < Transition.Conditions.Num(); ++Index)
			{
				TransitionConditions.Add(MakeShared<FJsonValueObject>(DescribeNode(Transition.Conditions[Index], Index, true)));
			}
			T->SetArrayField(TEXT("conditions"), TransitionConditions);
			T->SetBoolField(TEXT("enabled"), Transition.bTransitionEnabled);
			Transitions.Add(MakeShared<FJsonValueObject>(T));
		}
		Json->SetArrayField(TEXT("transitions"), Transitions);

		States.Add(MakeShared<FJsonValueObject>(Json));
		return EStateTreeVisitor::Continue;
	});
	Root->SetArrayField(TEXT("states"), States);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

FString UViperStateTreeEditLibrary::DuplicateState(UStateTree* StateTree, const FString& SourceStatePath,
	const FString& NewName, const FString& InsertBeforeName)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* Source = FindState(EditorData, SourceStatePath);
	if (!Source)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *SourceStatePath));
	}
	UStateTreeState* Parent = Source->Parent;
	if (!Parent)
	{
		return ReportError(TEXT("cannot duplicate a root or subtree state."));
	}
	if (NewName.IsEmpty())
	{
		return ReportError(TEXT("the copy needs a name."));
	}
	for (const UStateTreeState* Sibling : Parent->Children)
	{
		if (Sibling && Sibling->Name.ToString().Equals(NewName, ESearchCase::IgnoreCase))
		{
			return ReportError(FString::Printf(TEXT("'%s' already has a child called '%s'."), *GetStatePath(Parent), *NewName));
		}
	}

	int32 InsertIndex = Parent->Children.IndexOfByKey(Source) + 1;
	if (!InsertBeforeName.IsEmpty())
	{
		InsertIndex = Parent->Children.IndexOfByPredicate([&InsertBeforeName](const UStateTreeState* Sibling)
		{
			return Sibling && Sibling->Name.ToString().Equals(InsertBeforeName, ESearchCase::IgnoreCase);
		});
		if (InsertIndex == INDEX_NONE)
		{
			return ReportError(FString::Printf(TEXT("'%s' has no child called '%s' to insert before."),
				*GetStatePath(Parent), *InsertBeforeName));
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("DuplicateState", "Viper: Duplicate StateTree State"));
	StateTree->Modify();
	EditorData->Modify();
	Parent->Modify();

	UStateTreeState* Copy = DuplicateObject<UStateTreeState>(Source, Parent);
	Copy->SetFlags(RF_Transactional);
	Copy->Name = FName(*NewName);

	TMap<FGuid, FGuid> OldToNewStateIDs;
	ReassignIDs(*Copy, *EditorData->GetPropertyEditorBindings(), OldToNewStateIDs);
	RepointInternalTransitions(*Copy, OldToNewStateIDs);

	Parent->Children.Insert(Copy, InsertIndex);
	EditorData->ReparentStates();
	MarkDirty(StateTree);

	return FString::Printf(TEXT("copied %s to %s (position %d of %d under %s), %d conditions, %d tasks, %d transitions"),
		*SourceStatePath, *GetStatePath(Copy), InsertIndex, Parent->Children.Num(), *GetStatePath(Parent),
		Copy->EnterConditions.Num(), Copy->Tasks.Num(), Copy->Transitions.Num());
}

FString UViperStateTreeEditLibrary::ClearEnterConditions(UStateTree* StateTree, const FString& StatePath)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* State = FindState(EditorData, StatePath);
	if (!State)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *StatePath));
	}

	const FScopedTransaction Transaction(LOCTEXT("ClearConditions", "Viper: Clear Enter Conditions"));
	EditorData->Modify();
	State->Modify();

	const int32 Removed = State->EnterConditions.Num();
	for (const FStateTreeEditorNode& Condition : State->EnterConditions)
	{
		FPropertyBindingPath AllOfIt;
		AllOfIt.SetStructID(Condition.ID);
		EditorData->RemovePropertyBinding(AllOfIt);
	}
	State->EnterConditions.Reset();
	MarkDirty(StateTree);

	return FString::Printf(TEXT("removed %d enter conditions from %s"), Removed, *StatePath);
}

FString UViperStateTreeEditLibrary::AddBoolEnterCondition(UStateTree* StateTree, const FString& StatePath,
	const FString& EvaluatorProperty, const bool bExpected)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* State = FindState(EditorData, StatePath);
	if (!State)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *StatePath));
	}
	if (!FindEvaluatorWithProperty(EditorData, FName(*EvaluatorProperty)))
	{
		return ReportError(FString::Printf(TEXT("no evaluator has a property called '%s'."), *EvaluatorProperty));
	}

	const FScopedTransaction Transaction(LOCTEXT("AddBoolCondition", "Viper: Add Bool Enter Condition"));
	EditorData->Modify();
	State->Modify();

	TStateTreeEditorNode<FStateTreeCompareBoolCondition>& Condition =
		State->AddEnterCondition<FStateTreeCompareBoolCondition>();
	Condition.GetInstanceData().bRight = bExpected;
	const FString BindReport = Bind(EditorData, Condition, TEXT("bLeft"), EvaluatorProperty);
	MarkDirty(StateTree);

	return FString::Printf(TEXT("%s: Condition[%d] %s == %s; %s"), *StatePath, State->EnterConditions.Num() - 1,
		*EvaluatorProperty, bExpected ? TEXT("true") : TEXT("false"), *BindReport);
}

FString UViperStateTreeEditLibrary::AddFloatEnterCondition(UStateTree* StateTree, const FString& StatePath,
	const FString& EvaluatorProperty, const FString& Operator, const float Value)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* State = FindState(EditorData, StatePath);
	if (!State)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *StatePath));
	}
	if (!FindEvaluatorWithProperty(EditorData, FName(*EvaluatorProperty)))
	{
		return ReportError(FString::Printf(TEXT("no evaluator has a property called '%s'."), *EvaluatorProperty));
	}

	const UEnum* OperatorEnum = StaticEnum<UE::StateTree::EComparisonOperator>();
	const int64 OperatorValue = OperatorEnum->GetValueByNameString(Operator);
	if (OperatorValue == INDEX_NONE)
	{
		return ReportError(FString::Printf(TEXT("unknown operator '%s' (Less, LessOrEqual, Equal, NotEqual, GreaterOrEqual, Greater)."),
			*Operator));
	}

	const FScopedTransaction Transaction(LOCTEXT("AddFloatCondition", "Viper: Add Float Enter Condition"));
	EditorData->Modify();
	State->Modify();

	TStateTreeEditorNode<FStateTreeCompareFloatCondition>& Condition =
		State->AddEnterCondition<FStateTreeCompareFloatCondition>(
			static_cast<UE::StateTree::EComparisonOperator>(OperatorValue));
	Condition.GetInstanceData().Right = Value;
	const FString BindReport = Bind(EditorData, Condition, TEXT("Left"), EvaluatorProperty);
	MarkDirty(StateTree);

	return FString::Printf(TEXT("%s: Condition[%d] %s %s %g; %s"), *StatePath, State->EnterConditions.Num() - 1,
		*EvaluatorProperty, *Operator, Value, *BindReport);
}

FString UViperStateTreeEditLibrary::BindToEvaluator(UStateTree* StateTree, const FString& StatePath,
	const FString& NodeKind, const int32 NodeIndex, const FString& TargetProperty, const FString& EvaluatorProperty)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* State = FindState(EditorData, StatePath);
	if (!State)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *StatePath));
	}
	TArray<FStateTreeEditorNode>* Nodes = GetNodeList(State, NodeKind);
	if (!Nodes)
	{
		return ReportError(FString::Printf(TEXT("NodeKind must be 'Condition' or 'Task', not '%s'."), *NodeKind));
	}
	if (!Nodes->IsValidIndex(NodeIndex))
	{
		return ReportError(FString::Printf(TEXT("%s has no %s[%d] (it has %d)."), *StatePath, *NodeKind, NodeIndex, Nodes->Num()));
	}

	const FScopedTransaction Transaction(LOCTEXT("BindToEvaluator", "Viper: Bind StateTree Property"));
	EditorData->Modify();
	State->Modify();

	const FString Report = Bind(EditorData, (*Nodes)[NodeIndex], TargetProperty, EvaluatorProperty);
	if (!Report.StartsWith(TEXT("ERROR")))
	{
		MarkDirty(StateTree);
	}
	return FString::Printf(TEXT("%s %s[%d]: %s"), *StatePath, *NodeKind, NodeIndex, *Report);
}

FString UViperStateTreeEditLibrary::ClearTransitions(UStateTree* StateTree, const FString& StatePath)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* State = FindState(EditorData, StatePath);
	if (!State)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *StatePath));
	}

	const FScopedTransaction Transaction(LOCTEXT("ClearTransitions", "Viper: Clear Transitions"));
	EditorData->Modify();
	State->Modify();

	const int32 Removed = State->Transitions.Num();
	for (const FStateTreeTransition& Transition : State->Transitions)
	{
		for (const FStateTreeEditorNode& Condition : Transition.Conditions)
		{
			FPropertyBindingPath AllOfIt;
			AllOfIt.SetStructID(Condition.ID);
			EditorData->RemovePropertyBinding(AllOfIt);
		}
	}
	State->Transitions.Reset();
	MarkDirty(StateTree);

	return FString::Printf(TEXT("removed %d transitions from %s"), Removed, *StatePath);
}

FString UViperStateTreeEditLibrary::AddTransition(UStateTree* StateTree, const FString& StatePath,
	const FString& Trigger, const FString& TargetStatePath)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* State = FindState(EditorData, StatePath);
	if (!State)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *StatePath));
	}
	const UStateTreeState* Target = FindState(EditorData, TargetStatePath);
	if (!Target)
	{
		return ReportError(FString::Printf(TEXT("no target state at '%s'."), *TargetStatePath));
	}
	const int64 TriggerValue = StaticEnum<EStateTreeTransitionTrigger>()->GetValueByNameString(Trigger);
	if (TriggerValue == INDEX_NONE)
	{
		return ReportError(FString::Printf(TEXT("unknown trigger '%s' (OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick)."),
			*Trigger));
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTransition", "Viper: Add Transition"));
	EditorData->Modify();
	State->Modify();

	State->AddTransition(static_cast<EStateTreeTransitionTrigger>(TriggerValue), EStateTreeTransitionType::GotoState, Target);
	MarkDirty(StateTree);

	return FString::Printf(TEXT("%s: Transition[%d] %s -> %s"), *StatePath, State->Transitions.Num() - 1, *Trigger,
		*GetStatePath(Target));
}

FString UViperStateTreeEditLibrary::AddBoolTransitionCondition(UStateTree* StateTree, const FString& StatePath,
	const int32 TransitionIndex, const FString& EvaluatorProperty, const bool bExpected)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* State = FindState(EditorData, StatePath);
	if (!State)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *StatePath));
	}
	if (!State->Transitions.IsValidIndex(TransitionIndex))
	{
		return ReportError(FString::Printf(TEXT("%s has no Transition[%d] (it has %d)."), *StatePath, TransitionIndex,
			State->Transitions.Num()));
	}
	if (!FindEvaluatorWithProperty(EditorData, FName(*EvaluatorProperty)))
	{
		return ReportError(FString::Printf(TEXT("no evaluator has a property called '%s'."), *EvaluatorProperty));
	}

	const FScopedTransaction Transaction(LOCTEXT("AddBoolTransitionCondition", "Viper: Add Bool Transition Condition"));
	EditorData->Modify();
	State->Modify();

	FStateTreeTransition& Transition = State->Transitions[TransitionIndex];
	TStateTreeEditorNode<FStateTreeCompareBoolCondition>& Condition =
		Transition.AddConditionWithOuter<FStateTreeCompareBoolCondition>(State);
	Condition.GetInstanceData().bRight = bExpected;
	const FString BindReport = Bind(EditorData, Condition, TEXT("bLeft"), EvaluatorProperty);
	MarkDirty(StateTree);

	return FString::Printf(TEXT("%s: Transition[%d] Condition[%d] %s == %s; %s"), *StatePath, TransitionIndex,
		Transition.Conditions.Num() - 1, *EvaluatorProperty, bExpected ? TEXT("true") : TEXT("false"), *BindReport);
}

FString UViperStateTreeEditLibrary::AddFloatTransitionCondition(UStateTree* StateTree, const FString& StatePath,
	const int32 TransitionIndex, const FString& EvaluatorProperty, const FString& Operator, const float Value)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	UStateTreeEditorData* EditorData = GetEditorData(StateTree, Err);
	if (!EditorData)
	{
		return Err;
	}
	UStateTreeState* State = FindState(EditorData, StatePath);
	if (!State)
	{
		return ReportError(FString::Printf(TEXT("no state at '%s'."), *StatePath));
	}
	if (!State->Transitions.IsValidIndex(TransitionIndex))
	{
		return ReportError(FString::Printf(TEXT("%s has no Transition[%d] (it has %d)."), *StatePath, TransitionIndex,
			State->Transitions.Num()));
	}
	if (!FindEvaluatorWithProperty(EditorData, FName(*EvaluatorProperty)))
	{
		return ReportError(FString::Printf(TEXT("no evaluator has a property called '%s'."), *EvaluatorProperty));
	}
	UE::StateTree::EComparisonOperator ParsedOperator;
	if (!ParseOperator(Operator, ParsedOperator))
	{
		return ReportError(FString::Printf(TEXT("unknown operator '%s' (Less, LessOrEqual, Equal, NotEqual, GreaterOrEqual, Greater)."),
			*Operator));
	}

	const FScopedTransaction Transaction(LOCTEXT("AddFloatTransitionCondition", "Viper: Add Float Transition Condition"));
	EditorData->Modify();
	State->Modify();

	FStateTreeTransition& Transition = State->Transitions[TransitionIndex];
	TStateTreeEditorNode<FStateTreeCompareFloatCondition>& Condition =
		Transition.AddConditionWithOuter<FStateTreeCompareFloatCondition>(State, ParsedOperator);
	Condition.GetInstanceData().Right = Value;
	const FString BindReport = Bind(EditorData, Condition, TEXT("Left"), EvaluatorProperty);
	MarkDirty(StateTree);

	return FString::Printf(TEXT("%s: Transition[%d] Condition[%d] %s %s %g; %s"), *StatePath, TransitionIndex,
		Transition.Conditions.Num() - 1, *EvaluatorProperty, *Operator, Value, *BindReport);
}

FString UViperStateTreeEditLibrary::CompileStateTree(UStateTree* StateTree)
{
	using namespace ViperStateTreeEdit;

	FString Err;
	if (!GetEditorData(StateTree, Err))
	{
		return Err;
	}

	FStateTreeCompilerLog Log;
	const bool bOk = UStateTreeEditingSubsystem::CompileStateTree(StateTree, Log);

	TArray<FString> Lines;
	for (const TSharedRef<FTokenizedMessage>& Message : Log.ToTokenizedMessages())
	{
		Lines.Add(Message->ToText().ToString());
	}
	StateTree->MarkPackageDirty();

	const FString Head = bOk ? TEXT("compiled OK") : ReportError(TEXT("compile failed"));
	return Lines.IsEmpty() ? Head : Head + TEXT("\n") + FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE
