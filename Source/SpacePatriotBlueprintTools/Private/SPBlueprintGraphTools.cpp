#include "SPBlueprintGraphTools.h"

#include "SPFlightPawn.h"
#include "SPVesselSystemsComponent.h"
#include "SPSocietySimulationComponent.h"
#include "SPStoryCampaignComponent.h"
#include "SPFieldSurveyComponent.h"
#include "SpacePatriotBlueprintBases.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "FileHelpers.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_InputAction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

namespace
{
constexpr TCHAR ShipPath[] = TEXT("/Game/SpacePatriot/Blueprints/BP_KestrelFlyable.BP_KestrelFlyable");
constexpr TCHAR OwnedTag[] = TEXT("SpacePatriotGraph:ShipV1");
constexpr TCHAR WorldTag[] = TEXT("SpacePatriotGraph:WorldV1");
constexpr TCHAR WorldPath[] = TEXT("/Game/SpacePatriot/Blueprints/BP_WorldRuntime.BP_WorldRuntime");

UEdGraphPin* NeedPin(UEdGraphNode* Node, FName Name, FString& Error)
{
    UEdGraphPin* Pin = Node ? Node->FindPin(Name) : nullptr;
    if (!Pin) Error = FString::Printf(TEXT("Missing pin %s on %s"), *Name.ToString(), Node ? *Node->GetClass()->GetName() : TEXT("null"));
    return Pin;
}

bool Link(UEdGraph* Graph, UEdGraphNode* From, FName OutName, UEdGraphNode* To, FName InName, FString& Error)
{
    UEdGraphPin* Out = NeedPin(From, OutName, Error);
    UEdGraphPin* In = NeedPin(To, InName, Error);
    if (!Out || !In) return false;
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    if (!Schema->TryCreateConnection(Out, In))
    {
        Error = FString::Printf(TEXT("Could not connect %s.%s to %s.%s"), *From->GetClass()->GetName(), *OutName.ToString(), *To->GetClass()->GetName(), *InName.ToString());
        return false;
    }
    return true;
}

template<class T>
T* NewNode(UEdGraph* Graph, int32 X, int32 Y)
{
    FGraphNodeCreator<T> Creator(*Graph);
    T* Node = Creator.CreateNode();
    Node->NodePosX = X;
    Node->NodePosY = Y;
    Node->NodeComment = OwnedTag;
    Creator.Finalize();
    return Node;
}

UK2Node_CallFunction* Call(UEdGraph* Graph, UClass* Owner, FName FunctionName, int32 X, int32 Y, FString& Error, const TCHAR* Tag = OwnedTag)
{
    UFunction* Function = Owner ? Owner->FindFunctionByName(FunctionName) : nullptr;
    if (!Function)
    {
        Error = FString::Printf(TEXT("Missing reflected function %s.%s"), Owner ? *Owner->GetName() : TEXT("null"), *FunctionName.ToString());
        return nullptr;
    }
    FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
    UK2Node_CallFunction* Node = Creator.CreateNode();
    Node->SetFromFunction(Function);
    Node->NodePosX = X;
    Node->NodePosY = Y;
    Node->NodeComment = Tag;
    Creator.Finalize();
    return Node;
}

UK2Node_VariableGet* ComponentGet(UEdGraph* Graph, FName Name, int32 X, int32 Y, const TCHAR* Tag = OwnedTag)
{
    FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
    UK2Node_VariableGet* Node = Creator.CreateNode();
    Node->VariableReference.SetSelfMember(Name);
    Node->NodePosX = X;
    Node->NodePosY = Y;
    Node->NodeComment = Tag;
    Creator.Finalize();
    return Node;
}

UK2Node_InputAction* Action(UEdGraph* Graph, FName Name, int32 X, int32 Y, const TCHAR* Tag = OwnedTag)
{
    FGraphNodeCreator<UK2Node_InputAction> Creator(*Graph);
    UK2Node_InputAction* Node = Creator.CreateNode();
    Node->InputActionName = Name;
    Node->NodePosX = X;
    Node->NodePosY = Y;
    Node->NodeComment = Tag;
    Creator.Finalize();
    return Node;
}

bool CheckLinked(const UEdGraphNode* A, FName OutName, const UEdGraphNode* B, FName InName)
{
    if (!A || !B) return false;
    const UEdGraphPin* Out = A->FindPin(OutName);
    const UEdGraphPin* In = B->FindPin(InName);
    return Out && In && Out->LinkedTo.Contains(In);
}
}

bool USPBlueprintGraphTools::BuildShipGraph(FString& Report)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, ShipPath);
    if (!Blueprint || !Blueprint->ParentClass->IsChildOf(ASPFlightPawn::StaticClass()))
    {
        Report = TEXT("BP_KestrelFlyable missing or not derived from SPFlightPawn");
        return false;
    }
    if (!Blueprint->SimpleConstructionScript)
    {
        Report = TEXT("BP_KestrelFlyable has no construction script");
        return false;
    }
    USCS_Node* VesselNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(USPVesselSystemsComponent::StaticClass()))
        {
            if (VesselNode)
            {
                Report = TEXT("Duplicate vessel systems components; refusing to wire ambiguous graph");
                return false;
            }
            VesselNode = Node;
        }
    }
    if (!VesselNode)
    {
        VesselNode = Blueprint->SimpleConstructionScript->CreateNode(USPVesselSystemsComponent::StaticClass(), TEXT("VesselSystems"));
        Blueprint->SimpleConstructionScript->AddNode(VesselNode);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }
    const FName ComponentName = VesselNode->GetVariableName();
    UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
    if (!Graph)
    {
        Report = TEXT("BP_KestrelFlyable has no Event Graph");
        return false;
    }

    // Only remove nodes from a previous run of this tool; never erase authored gameplay nodes.
    TArray<UEdGraphNode*> Previous = Graph->Nodes;
    for (UEdGraphNode* Node : Previous)
    {
        if (Node && Node->NodeComment == OwnedTag)
        {
            Node->BreakAllNodeLinks();
            Graph->RemoveNode(Node);
        }
    }

    UK2Node_Event* Tick = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, AActor::StaticClass(), TEXT("ReceiveTick"));
    if (!Tick)
    {
        int32 Y = 0;
        Tick = FKismetEditorUtilities::AddDefaultEventNode(Blueprint, Graph, TEXT("ReceiveTick"), AActor::StaticClass(), Y);
    }
    if (!Tick)
    {
        Report = TEXT("Could not create ReceiveTick event");
        return false;
    }
    Tick->SetEnabledState(ENodeEnabledState::Enabled);
    UEdGraphPin* TickThen = Tick->FindPin(UEdGraphSchema_K2::PN_Then);
    if (!TickThen || !TickThen->LinkedTo.IsEmpty())
    {
        Report = TEXT("ReceiveTick already has non-tool connections; refusing to overwrite user graph");
        return false;
    }
    Tick->NodePosX = 0;
    Tick->NodePosY = 0;
    FString Error;
    UK2Node_VariableGet* Vessel = ComponentGet(Graph, ComponentName, 50, 360);
    UK2Node_CallFunction* Input = Call(Graph, ASPFlightPawn::StaticClass(), GET_FUNCTION_NAME_CHECKED(ASPFlightPawn, BuildVesselSimulationInput), 250, 390, Error);
    UK2Node_CallFunction* Step = Call(Graph, USPVesselSystemsComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(USPVesselSystemsComponent, StepSystems), 500, 0, Error);
    UK2Node_CallFunction* Apply = Call(Graph, ASPFlightPawn::StaticClass(), GET_FUNCTION_NAME_CHECKED(ASPFlightPawn, ApplyVesselSimulationOutput), 820, 0, Error);
    if (!Input || !Step || !Apply
        || !Link(Graph, Tick, UEdGraphSchema_K2::PN_Then, Step, UEdGraphSchema_K2::PN_Execute, Error)
        || !Link(Graph, Tick, TEXT("DeltaSeconds"), Step, TEXT("DeltaSeconds"), Error)
        || !Link(Graph, Vessel, ComponentName, Step, UEdGraphSchema_K2::PN_Self, Error)
        || !Link(Graph, Input, TEXT("ReturnValue"), Step, TEXT("Input"), Error)
        || !Link(Graph, Step, UEdGraphSchema_K2::PN_Then, Apply, UEdGraphSchema_K2::PN_Execute, Error)
        || !Link(Graph, Step, TEXT("ReturnValue"), Apply, TEXT("Output"), Error))
    {
        Report = Error;
        return false;
    }

    const struct FActionSpec { FName Name; ESPVesselProfile Profile; int32 Y; } Presets[] = {
        {TEXT("SPVesselTravel"), ESPVesselProfile::Travel, 700},
        {TEXT("SPVesselCombat"), ESPVesselProfile::Combat, 980}
    };
    for (const FActionSpec& Spec : Presets)
    {
        UK2Node_InputAction* Event = Action(Graph, Spec.Name, 0, Spec.Y);
        UK2Node_CallFunction* Set = Call(Graph, USPVesselSystemsComponent::StaticClass(), GET_FUNCTION_NAME_CHECKED(USPVesselSystemsComponent, ApplyPreset), 500, Spec.Y, Error);
        if (!Set || !Link(Graph, Event, TEXT("Pressed"), Set, UEdGraphSchema_K2::PN_Execute, Error)
            || !Link(Graph, Vessel, ComponentName, Set, UEdGraphSchema_K2::PN_Self, Error))
        {
            Report = Error;
            return false;
        }
        UEdGraphPin* Profile = NeedPin(Set, TEXT("NewProfile"), Error);
        if (!Profile)
        {
            Report = Error;
            return false;
        }
        Profile->DefaultValue = StaticEnum<ESPVesselProfile>()->GetNameStringByValue(static_cast<int64>(Spec.Profile));
    }

    UK2Node_InputAction* NextPage = Action(Graph, TEXT("SPMFDNext"), 0, 1260);
    UK2Node_CallFunction* Cycle = Call(Graph, ASPFlightPawn::StaticClass(), GET_FUNCTION_NAME_CHECKED(ASPFlightPawn, CycleMFDPage), 500, 1260, Error);
    if (!Cycle || !Link(Graph, NextPage, TEXT("Pressed"), Cycle, UEdGraphSchema_K2::PN_Execute, Error))
    {
        Report = Error;
        return false;
    }
    NeedPin(Cycle, TEXT("Direction"), Error)->DefaultValue = TEXT("1");
    UK2Node_InputAction* DimPage = Action(Graph, TEXT("SPMFDDim"), 0, 1520);
    UK2Node_CallFunction* Dim = Call(Graph, ASPFlightPawn::StaticClass(), GET_FUNCTION_NAME_CHECKED(ASPFlightPawn, AdjustMFDBrightness), 500, 1520, Error);
    if (!Dim || !Link(Graph, DimPage, TEXT("Pressed"), Dim, UEdGraphSchema_K2::PN_Execute, Error))
    {
        Report = Error;
        return false;
    }
    NeedPin(Dim, TEXT("Delta"), Error)->DefaultValue = TEXT("-0.1");

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (!Blueprint->IsUpToDate())
    {
        Report = TEXT("BP_KestrelFlyable did not compile cleanly");
        return false;
    }
    Blueprint->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SavePackages({Blueprint->GetOutermost()}, false))
    {
        Report = TEXT("Could not save BP_KestrelFlyable");
        return false;
    }
    return VerifyShipGraph(Report);
}

bool USPBlueprintGraphTools::VerifyShipGraph(FString& Report)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, ShipPath);
    UEdGraph* Graph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
    if (!Blueprint || !Graph || !Blueprint->IsUpToDate())
    {
        Report = TEXT("Ship Blueprint missing, graph missing, or compile status failed");
        return false;
    }
    int32 Components = 0;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(USPVesselSystemsComponent::StaticClass())) ++Components;
    UK2Node_Event* Tick = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, AActor::StaticClass(), TEXT("ReceiveTick"));
    const UK2Node_CallFunction* Step = nullptr;
    const UK2Node_CallFunction* Apply = nullptr;
    const UK2Node_CallFunction* BuildInput = nullptr;
    const UK2Node_CallFunction* TravelPreset = nullptr;
    const UK2Node_CallFunction* CombatPreset = nullptr;
    const UK2Node_CallFunction* Cycle = nullptr;
    const UK2Node_CallFunction* Dim = nullptr;
    TMap<FName, const UK2Node_InputAction*> Actions;
    int32 OwnedCalls = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
        {
            if (Node->NodeComment != OwnedTag) continue;
            ++OwnedCalls;
            const UFunction* Fn = CallNode->GetTargetFunction();
            if (Fn && Fn->GetFName() == GET_FUNCTION_NAME_CHECKED(USPVesselSystemsComponent, StepSystems)) Step = CallNode;
            if (Fn && Fn->GetFName() == GET_FUNCTION_NAME_CHECKED(ASPFlightPawn, ApplyVesselSimulationOutput)) Apply = CallNode;
            if (Fn && Fn->GetFName() == GET_FUNCTION_NAME_CHECKED(ASPFlightPawn, BuildVesselSimulationInput)) BuildInput = CallNode;
            if (Fn && Fn->GetFName() == GET_FUNCTION_NAME_CHECKED(ASPFlightPawn, CycleMFDPage)) Cycle = CallNode;
            if (Fn && Fn->GetFName() == GET_FUNCTION_NAME_CHECKED(ASPFlightPawn, AdjustMFDBrightness)) Dim = CallNode;
            if (Fn && Fn->GetFName() == GET_FUNCTION_NAME_CHECKED(USPVesselSystemsComponent, ApplyPreset))
            {
                const UEdGraphPin* Profile = CallNode->FindPin(TEXT("NewProfile"));
                if (Profile && Profile->DefaultValue == TEXT("Travel")) TravelPreset = CallNode;
                if (Profile && Profile->DefaultValue == TEXT("Combat")) CombatPreset = CallNode;
            }
        }
        else if (const UK2Node_InputAction* ActionNode = Cast<UK2Node_InputAction>(Node))
        {
            if (Node->NodeComment == OwnedTag) Actions.Add(ActionNode->InputActionName, ActionNode);
        }
    }
    const bool bTickConnected = CheckLinked(Tick, UEdGraphSchema_K2::PN_Then, Step, UEdGraphSchema_K2::PN_Execute);
    const bool bOutputConnected = CheckLinked(Step, UEdGraphSchema_K2::PN_Then, Apply, UEdGraphSchema_K2::PN_Execute)
        && CheckLinked(Step, TEXT("ReturnValue"), Apply, TEXT("Output"));
    const bool bInputConnected = CheckLinked(Tick, TEXT("DeltaSeconds"), Step, TEXT("DeltaSeconds"))
        && CheckLinked(BuildInput, TEXT("ReturnValue"), Step, TEXT("Input"))
        && Step && Step->FindPin(UEdGraphSchema_K2::PN_Self)
        && Step->FindPin(UEdGraphSchema_K2::PN_Self)->LinkedTo.Num() == 1;
    const bool bActions = Actions.Contains(TEXT("SPVesselTravel")) && Actions.Contains(TEXT("SPVesselCombat"))
        && Actions.Contains(TEXT("SPMFDNext")) && Actions.Contains(TEXT("SPMFDDim"))
        && CheckLinked(Actions.FindRef(TEXT("SPVesselTravel")), TEXT("Pressed"), TravelPreset, UEdGraphSchema_K2::PN_Execute)
        && CheckLinked(Actions.FindRef(TEXT("SPVesselCombat")), TEXT("Pressed"), CombatPreset, UEdGraphSchema_K2::PN_Execute)
        && CheckLinked(Actions.FindRef(TEXT("SPMFDNext")), TEXT("Pressed"), Cycle, UEdGraphSchema_K2::PN_Execute)
        && CheckLinked(Actions.FindRef(TEXT("SPMFDDim")), TEXT("Pressed"), Dim, UEdGraphSchema_K2::PN_Execute);
    Report = FString::Printf(TEXT("components=%d calls=%d input_actions=%d tick_to_step=%s step_to_apply=%s saved=%s"),
        Components, OwnedCalls, Actions.Num(), bTickConnected ? TEXT("true") : TEXT("false"),
        bOutputConnected ? TEXT("true") : TEXT("false"), Blueprint->GetOutermost()->IsDirty() ? TEXT("false") : TEXT("true"));
    return Components == 1 && OwnedCalls == 7 && bActions && bTickConnected && bInputConnected && bOutputConnected;
}

bool USPBlueprintGraphTools::BuildWorldGraph(FString& Report)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, WorldPath);
    if (!Blueprint || !Blueprint->ParentClass->IsChildOf(ASPWorldRuntime::StaticClass()) || !Blueprint->SimpleConstructionScript)
    {
        Report = TEXT("BP_WorldRuntime missing or not derived from SPWorldRuntime");
        return false;
    }
    struct FRequired { UClass* Class; FName Name; } Requirements[] = {
        {USPSocietySimulationComponent::StaticClass(), TEXT("SocietySimulation")},
        {USPStoryCampaignComponent::StaticClass(), TEXT("StoryCampaign")},
        {USPFieldSurveyComponent::StaticClass(), TEXT("FieldSurvey")}
    };
    TMap<UClass*, FName> ComponentNames;
    bool bAdded = false;
    for (const FRequired& Required : Requirements)
    {
        USCS_Node* Found = nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(Required.Class))
            {
                if (Found)
                {
                    Report = FString::Printf(TEXT("Duplicate %s components"), *Required.Class->GetName());
                    return false;
                }
                Found = Node;
            }
        }
        if (!Found)
        {
            Found = Blueprint->SimpleConstructionScript->CreateNode(Required.Class, Required.Name);
            Blueprint->SimpleConstructionScript->AddNode(Found);
            bAdded = true;
        }
        ComponentNames.Add(Required.Class, Found->GetVariableName());
    }
    if (bAdded)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }
    UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
    if (!Graph)
    {
        Report = TEXT("BP_WorldRuntime has no Event Graph");
        return false;
    }
    TArray<UEdGraphNode*> Previous = Graph->Nodes;
    for (UEdGraphNode* Node : Previous)
    {
        if (Node && Node->NodeComment == WorldTag)
        {
            Node->BreakAllNodeLinks();
            Graph->RemoveNode(Node);
        }
    }
    FString Error;
    const FName SocietyName = ComponentNames[USPSocietySimulationComponent::StaticClass()];
    const FName CampaignName = ComponentNames[USPStoryCampaignComponent::StaticClass()];
    const FName SurveyName = ComponentNames[USPFieldSurveyComponent::StaticClass()];
    UK2Node_VariableGet* Society = ComponentGet(Graph, SocietyName, 180, 580, WorldTag);
    UK2Node_VariableGet* Campaign = ComponentGet(Graph, CampaignName, 180, 670, WorldTag);
    UK2Node_VariableGet* Survey = ComponentGet(Graph, SurveyName, 180, 760, WorldTag);

    UK2Node_InputAction* QuestKey = Action(Graph, TEXT("SPQuestWater"), 0, 0, WorldTag);
    UK2Node_CallFunction* Start = Call(Graph, USPStoryCampaignComponent::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(USPStoryCampaignComponent, StartQuest), 450, 0, Error, WorldTag);
    if (!Start || !Link(Graph, QuestKey, TEXT("Pressed"), Start, UEdGraphSchema_K2::PN_Execute, Error)
        || !Link(Graph, Campaign, CampaignName, Start, UEdGraphSchema_K2::PN_Self, Error))
    {
        Report = Error;
        return false;
    }
    NeedPin(Start, TEXT("QuestId"), Error)->DefaultValue = TEXT("water");

    UK2Node_InputAction* SaveKey = Action(Graph, TEXT("SPSaveWorld"), 0, 300, WorldTag);
    UK2Node_CallFunction* SaveSociety = Call(Graph, USPSocietySimulationComponent::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(USPSocietySimulationComponent, SaveSociety), 450, 300, Error, WorldTag);
    UK2Node_CallFunction* SaveCampaign = Call(Graph, USPStoryCampaignComponent::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(USPStoryCampaignComponent, SaveCampaign), 750, 300, Error, WorldTag);
    UK2Node_CallFunction* SaveSurvey = Call(Graph, USPFieldSurveyComponent::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(USPFieldSurveyComponent, SaveSurveys), 1050, 300, Error, WorldTag);
    if (!SaveSociety || !SaveCampaign || !SaveSurvey
        || !Link(Graph, SaveKey, TEXT("Pressed"), SaveSociety, UEdGraphSchema_K2::PN_Execute, Error)
        || !Link(Graph, Society, SocietyName, SaveSociety, UEdGraphSchema_K2::PN_Self, Error)
        || !Link(Graph, SaveSociety, UEdGraphSchema_K2::PN_Then, SaveCampaign, UEdGraphSchema_K2::PN_Execute, Error)
        || !Link(Graph, Campaign, CampaignName, SaveCampaign, UEdGraphSchema_K2::PN_Self, Error)
        || !Link(Graph, SaveCampaign, UEdGraphSchema_K2::PN_Then, SaveSurvey, UEdGraphSchema_K2::PN_Execute, Error)
        || !Link(Graph, Survey, SurveyName, SaveSurvey, UEdGraphSchema_K2::PN_Self, Error))
    {
        Report = Error;
        return false;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (!Blueprint->IsUpToDate())
    {
        Report = TEXT("BP_WorldRuntime did not compile cleanly");
        return false;
    }
    ASPWorldRuntime* Defaults = Cast<ASPWorldRuntime>(Blueprint->GeneratedClass->GetDefaultObject());
    if (!Defaults)
    {
        Report = TEXT("Could not find generated world runtime defaults");
        return false;
    }
    Defaults->AutoReceiveInput = EAutoReceiveInput::Player0;
    Blueprint->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SavePackages({Blueprint->GetOutermost()}, false))
    {
        Report = TEXT("Could not save BP_WorldRuntime");
        return false;
    }
    return VerifyWorldGraph(Report);
}

bool USPBlueprintGraphTools::VerifyWorldGraph(FString& Report)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, WorldPath);
    UEdGraph* Graph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
    if (!Blueprint || !Graph || !Blueprint->IsUpToDate())
    {
        Report = TEXT("World Blueprint missing, graph missing, or compile status failed");
        return false;
    }
    TMap<FName, const UK2Node_CallFunction*> Functions;
    TMap<FName, const UK2Node_InputAction*> Actions;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || Node->NodeComment != WorldTag) continue;
        if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
        {
            if (const UFunction* Function = CallNode->GetTargetFunction()) Functions.Add(Function->GetFName(), CallNode);
        }
        else if (const UK2Node_InputAction* ActionNode = Cast<UK2Node_InputAction>(Node)) Actions.Add(ActionNode->InputActionName, ActionNode);
    }
    const ASPWorldRuntime* Defaults = Cast<ASPWorldRuntime>(Blueprint->GeneratedClass->GetDefaultObject());
    const bool bFunctions = Functions.Contains(TEXT("StartQuest")) && Functions.Contains(TEXT("SaveSociety"))
        && Functions.Contains(TEXT("SaveCampaign")) && Functions.Contains(TEXT("SaveSurveys"));
    const bool bInputs = Actions.Contains(TEXT("SPQuestWater")) && Actions.Contains(TEXT("SPSaveWorld"));
    const UK2Node_CallFunction* Start = Functions.FindRef(TEXT("StartQuest"));
    const UK2Node_CallFunction* SaveSociety = Functions.FindRef(TEXT("SaveSociety"));
    const UK2Node_CallFunction* SaveCampaign = Functions.FindRef(TEXT("SaveCampaign"));
    const UK2Node_CallFunction* SaveSurvey = Functions.FindRef(TEXT("SaveSurveys"));
    const bool bWired = bFunctions && bInputs
        && CheckLinked(Actions.FindRef(TEXT("SPQuestWater")), TEXT("Pressed"), Start, UEdGraphSchema_K2::PN_Execute)
        && Start->FindPin(TEXT("QuestId")) && Start->FindPin(TEXT("QuestId"))->DefaultValue == TEXT("water")
        && CheckLinked(Actions.FindRef(TEXT("SPSaveWorld")), TEXT("Pressed"), SaveSociety, UEdGraphSchema_K2::PN_Execute)
        && CheckLinked(SaveSociety, UEdGraphSchema_K2::PN_Then, SaveCampaign, UEdGraphSchema_K2::PN_Execute)
        && CheckLinked(SaveCampaign, UEdGraphSchema_K2::PN_Then, SaveSurvey, UEdGraphSchema_K2::PN_Execute)
        && Start->FindPin(UEdGraphSchema_K2::PN_Self)->LinkedTo.Num() == 1
        && SaveSociety->FindPin(UEdGraphSchema_K2::PN_Self)->LinkedTo.Num() == 1
        && SaveCampaign->FindPin(UEdGraphSchema_K2::PN_Self)->LinkedTo.Num() == 1
        && SaveSurvey->FindPin(UEdGraphSchema_K2::PN_Self)->LinkedTo.Num() == 1;
    const bool bReceivesInput = Defaults && Defaults->AutoReceiveInput == EAutoReceiveInput::Player0;
    Report = FString::Printf(TEXT("calls=%d input_actions=%d receives_player0=%s saved=%s"),
        Functions.Num(), Actions.Num(), bReceivesInput ? TEXT("true") : TEXT("false"),
        Blueprint->GetOutermost()->IsDirty() ? TEXT("false") : TEXT("true"));
    return bWired && bReceivesInput;
}
