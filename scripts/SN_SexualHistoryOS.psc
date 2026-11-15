Scriptname SN_SexualHistoryOS Hidden
{The only script that calls OStim. Called only after OStim.esp has been found in the load order.}

; How many actions or tags of a scene are listed at most.
Int Function MaxItems() Global
	Return 8
EndFunction

Actor[] Function Actors(Int aiThreadID) Global
	Return OThread.GetActors(aiThreadID)
EndFunction

; "Animation "X". Actions: kissing (Lydia to Farengar), ... Tags: ... Furniture: bed."
; Empty during a transition.
String Function SceneFacts(Int aiThreadID) Global
	String sceneId = OThread.GetScene(aiThreadID)
	If sceneId == "" || OMetadata.IsTransition(sceneId)
		Return ""
	EndIf
	Actor[] actors = OThread.GetActors(aiThreadID)
	String text = ""
	String sceneName = OMetadata.GetName(sceneId)
	If sceneName != ""
		text += "Animation \"" + sceneName + "\". "
	EndIf
	String actions = ActionList(sceneId, actors)
	If actions != ""
		text += "Actions: " + actions + ". "
	EndIf
	String tags = TagList(sceneId)
	If tags != ""
		text += "Tags: " + tags + ". "
	EndIf
	String furnitureType = OThread.GetFurnitureType(aiThreadID)
	If furnitureType != "" && furnitureType != "none"
		text += "Furniture: " + furnitureType + ". "
	EndIf
	; OStimNet, when installed, says what kind of encounter it is (romantic, lustful,
	; transactional, dom, aggressive, platonic).
	If Game.IsPluginInstalled("TT_OStimNet.esp")
		String intent = SN_SexualHistoryONet.Intent(aiThreadID)
		If intent != ""
			text += "OStimNet intent: " + intent + ". "
		EndIf
	EndIf
	Return text
EndFunction

; "kissing (Lydia to Farengar), vaginalsex (Farengar to Lydia)"
String Function ActionList(String asSceneId, Actor[] akActors) Global
	String[] types = OMetadata.GetActionTypes(asSceneId)
	String text = ""
	Int i = 0
	While i < types.Length && i < MaxItems()
		Int actorIndex = OMetadata.GetActionActor(asSceneId, i)
		Int targetIndex = OMetadata.GetActionTarget(asSceneId, i)
		String entry = types[i]
		If actorIndex >= 0 && actorIndex < akActors.Length && akActors[actorIndex]
			entry += " (" + akActors[actorIndex].GetDisplayName()
			If targetIndex >= 0 && targetIndex < akActors.Length && targetIndex != actorIndex && akActors[targetIndex]
				entry += " to " + akActors[targetIndex].GetDisplayName()
			EndIf
			entry += ")"
		EndIf
		If text != ""
			text += ", "
		EndIf
		text += entry
		i += 1
	EndWhile
	Return text
EndFunction

String Function TagList(String asSceneId) Global
	String[] tags = OMetadata.GetSceneTags(asSceneId)
	String text = ""
	Int i = 0
	While i < tags.Length && i < MaxItems()
		If tags[i] != ""
			If text != ""
				text += ", "
			EndIf
			text += tags[i]
		EndIf
		i += 1
	EndWhile
	Return text
EndFunction
