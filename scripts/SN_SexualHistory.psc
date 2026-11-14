Scriptname SN_SexualHistory extends Quest
{Watches OStim and SexLab scenes and passes what happens in them to SN_SexualHistory.dll, which
turns each finished scene into a SkyrimNet memory for everyone who took part. OStim and SexLab
are each optional: the functions of a framework are called only after its plugin has been found.}

String Property OStimPlugin = "OStim.esp" AutoReadOnly
String Property SexLabPlugin = "SexLab.esm" AutoReadOnly
; A scene key at or above this is a SexLab thread ID plus this; below it, an OStim thread ID.
Int Property SexLabBase = 100000 AutoReadOnly

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
EndEvent

Event OnOStimSceneChanged(String asEventName, String asArg, Float afThreadID, Form akSender)
	Int threadID = afThreadID as Int
	SN_SexualHistoryNative.SceneDetail(threadID, SN_SexualHistoryOS.Actors(threadID), SN_SexualHistoryOS.SceneFacts(threadID))
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
EndEvent

Event OnSexLabStage(Int aiThread, Bool abHasPlayer)
	SN_SexualHistoryNative.SceneDetail(SexLabBase + aiThread, SN_SexualHistorySL.Actors(aiThread), SN_SexualHistorySL.SceneFacts(aiThread))
EndEvent

Event OnSexLabOrgasm(Int aiThread, Bool abHasPlayer)
	SN_SexualHistoryNative.Climax(SexLabBase + aiThread, None)
EndEvent

Event OnSexLabEnd(Int aiThread, Bool abHasPlayer)
	SN_SexualHistoryNative.SceneEnded(SexLabBase + aiThread)
EndEvent

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
