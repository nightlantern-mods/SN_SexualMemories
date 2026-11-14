Scriptname SN_SexualHistory extends Quest
{Watches OStim and SexLab scenes and passes what happens in them to SN_SexualHistory.dll, which
turns each finished scene into a SkyrimNet memory for everyone who took part, and for one
bystander who saw it when there is one. OStim and SexLab are each optional: the functions of a
framework are called only after its plugin has been found.}

String Property OStimPlugin = "OStim.esp" AutoReadOnly
String Property SexLabPlugin = "SexLab.esm" AutoReadOnly
; A scene key at or above this is a SexLab thread ID plus this; below it, an OStim thread ID.
Int Property SexLabBase = 100000 AutoReadOnly
; How far from the scene a witness is looked for, and how many random picks are tried each time.
Float Property WitnessRadius = 1500.0 AutoReadOnly
Int Property WitnessTries = 8 AutoReadOnly
; The quest that runs the MCM (SN_SexualHistory_Chatter).
Int Property MCMQuestID = 0x801 AutoReadOnly

Event OnInit()
	RegisterEvents()
EndEvent

; Called from OnInit and by the player alias on every save load.
Function RegisterEvents()
	If Game.IsPluginInstalled(OStimPlugin)
		RegisterForModEvent("ostim_thread_start", "OnOStimStart")
		RegisterForModEvent("ostim_thread_scenechanged", "OnOStimSceneChanged")
		RegisterForModEvent("ostim_actor_orgasm", "OnOStimOrgasm")
		RegisterForModEvent("ostim_thread_end", "OnOStimEnd")
	EndIf
	If Game.IsPluginInstalled(SexLabPlugin)
		RegisterForModEvent("HookAnimationStart", "OnSexLabStart")
		RegisterForModEvent("HookStageStart", "OnSexLabStage")
		RegisterForModEvent("HookOrgasmStart", "OnSexLabOrgasm")
		RegisterForModEvent("HookAnimationEnd", "OnSexLabEnd")
	EndIf
EndFunction

; ---- OStim ----------------------------------------------------------------------------------

Event OnOStimStart(String asEventName, String asArg, Float afThreadID, Form akSender)
	Int threadID = afThreadID as Int
	Actor[] actors = SN_SexualHistoryOS.Actors(threadID)
	SN_SexualHistoryNative.SceneStarted(threadID, "OStim", actors, LocationName(actors))
	SN_SexualHistoryNative.SceneDetail(threadID, actors, SN_SexualHistoryOS.SceneFacts(threadID))
	LookForWitness(threadID, actors)
EndEvent

Event OnOStimSceneChanged(String asEventName, String asArg, Float afThreadID, Form akSender)
	Int threadID = afThreadID as Int
	Actor[] actors = SN_SexualHistoryOS.Actors(threadID)
	SN_SexualHistoryNative.SceneDetail(threadID, actors, SN_SexualHistoryOS.SceneFacts(threadID))
	LookForWitness(threadID, actors)
EndEvent

Event OnOStimOrgasm(String asEventName, String asArg, Float afThreadID, Form akSender)
	SN_SexualHistoryNative.Climax(afThreadID as Int, akSender as Actor)
EndEvent

Event OnOStimEnd(String asEventName, String asArg, Float afThreadID, Form akSender)
	SN_SexualHistoryNative.SceneEnded(afThreadID as Int)
EndEvent

; ---- SexLab ---------------------------------------------------------------------------------

Event OnSexLabStart(Int aiThread, Bool abHasPlayer)
	Actor[] actors = SN_SexualHistorySL.Actors(aiThread)
	SN_SexualHistoryNative.SceneStarted(SexLabBase + aiThread, "SexLab", actors, LocationName(actors))
	SN_SexualHistoryNative.SceneDetail(SexLabBase + aiThread, actors, SN_SexualHistorySL.SceneFacts(aiThread))
	LookForWitness(SexLabBase + aiThread, actors)
EndEvent

Event OnSexLabStage(Int aiThread, Bool abHasPlayer)
	Actor[] actors = SN_SexualHistorySL.Actors(aiThread)
	SN_SexualHistoryNative.SceneDetail(SexLabBase + aiThread, actors, SN_SexualHistorySL.SceneFacts(aiThread))
	LookForWitness(SexLabBase + aiThread, actors)
EndEvent

Event OnSexLabOrgasm(Int aiThread, Bool abHasPlayer)
	SN_SexualHistoryNative.Climax(SexLabBase + aiThread, None)
EndEvent

Event OnSexLabEnd(Int aiThread, Bool abHasPlayer)
	SN_SexualHistoryNative.SceneEnded(SexLabBase + aiThread)
EndEvent

; ---- Witness ----------------------------------------------------------------------------------

; Looks for one bystander who can see the scene, at the start and at every change of animation or
; stage, until one is found: a random NPC near the scene who is not in it, not the player, not a
; child, awake and with a participant in sight. Nobody is made up when nobody is there. Off when
; the MCM setting "bWitness:General" is off.
Function LookForWitness(Int aiKey, Actor[] akActors)
	If akActors.Length == 0 || !akActors[0] || !WitnessEnabled() || !SN_SexualHistoryNative.NeedsWitness(aiKey)
		Return
	EndIf
	Actor player = Game.GetPlayer()
	Keyword npcKeyword = Keyword.GetKeyword("ActorTypeNPC")
	Int i = 0
	While i < WitnessTries
		Actor candidate = Game.FindRandomActorFromRef(akActors[0], WitnessRadius)
		If candidate && candidate != player && akActors.Find(candidate) < 0 && !candidate.IsDead() 				&& !candidate.IsChild() && candidate.Is3DLoaded() && !candidate.IsUnconscious() 				&& candidate.GetSleepState() != 3 && (!npcKeyword || candidate.HasKeyword(npcKeyword)) 				&& SeesAnyOf(candidate, akActors)
			SN_SexualHistoryNative.Witness(aiKey, candidate)
			Return
		EndIf
		i += 1
	EndWhile
EndFunction

Bool Function SeesAnyOf(Actor akWitness, Actor[] akActors)
	Int i = 0
	While i < akActors.Length
		If akActors[i] && akWitness.HasLOS(akActors[i])
			Return True
		EndIf
		i += 1
	EndWhile
	Return False
EndFunction

; The MCM setting, read through the quest that runs the MCM. On when that quest cannot be found.
Bool Function WitnessEnabled()
	SN_SexualHistoryChatter mcm = Game.GetFormFromFile(MCMQuestID, "SN_SexualHistory.esp") as SN_SexualHistoryChatter
	If !mcm
		Return True
	EndIf
	Return mcm.GetModSettingBool("bWitness:General")
EndFunction

; ---------------------------------------------------------------------------------------------

; The location of the first actor, or its cell when the place has no location.
String Function LocationName(Actor[] akActors)
	If akActors.Length == 0 || !akActors[0]
		Return ""
	EndIf
	Location here = akActors[0].GetCurrentLocation()
	If here && here.GetName() != ""
		Return here.GetName()
	EndIf
	Cell room = akActors[0].GetParentCell()
	If room
		Return room.GetName()
	EndIf
	Return ""
EndFunction
