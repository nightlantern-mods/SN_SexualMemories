Scriptname SN_SexualHistoryChatter extends MCM_ConfigBase
{Every so often an NPC who is in a running OStim or SexLab scene says a line about it, written
and voiced by SkyrimNet. The lines stay in SkyrimNet's event history, so the memory written when
the scene ends (SN_SexualHistory.dll) draws on them too. This quest also runs the mod's MCM;
settings live in MCM/Settings/SN_SexualHistory.ini. Taken over from skyrimnet_OStimChatter.}

; OStim and SexLab are each optional: the scenes of whichever is installed are watched. Their
; functions are called only after the plugin has been found in the load order.
String Property OStimPlugin = "OStim.esp" AutoReadOnly
String Property SexLabPlugin = "SexLab.esm" AutoReadOnly
; A scene key at or above this is a SexLab thread slot plus this; below it, an OStim thread ID.
Int Property SexLabBase = 100000 AutoReadOnly
; OStimNet is optional. When it is installed its scene description, intent and phase are passed on.
String Property OStimNetPlugin = "TT_OStimNet.esp" AutoReadOnly

; When a check finds SkyrimNet busy speaking, or a scene between two animations, it comes back
; this soon instead of waiting a whole interval.
Float Property RetryDelay = 5.0 AutoReadOnly
; How many actions of a scene are listed for the LLM at most.
Int Property MaxActions = 6 AutoReadOnly

; The NPCs who have had their turn in the current round (the first spokenCount entries), and the
; one who spoke last. See PickTurn.
Actor[] spoken
Int spokenCount
Actor lastSpeaker
; Set once the stroll's possible leftover (an AI-driven player) has been cleared in this save.
Bool strollCleared

Event OnInit()
	; MCM Helper and SkyUI register the menu from their own OnInit.
	Parent.OnInit()
	Setup()
EndEvent

; Called from OnInit and by the player alias on every save load.
Function Setup()
	If !spoken
		spoken = new Actor[32]
		spokenCount = 0
	EndIf
	RegisterMapKey()
	PushMapSettings()
	SN_SexualHistoryNative.SetDiaryEnabled(GetModSettingBool("bDiary:General"))
	SN_SexualHistoryNative.SetMemoriesEnabled(GetModSettingBool("bMemories:General"))
	SN_SexualHistoryNative.SetQuestDiaryEnabled(GetModSettingBool("bQuestDiary:General"))
	; The stroll (a trial, taken out again) left the player AI-driven while it ran; a save made
	; during one would keep them so with nothing left to end it. Once per save, control is handed
	; back.
	If !strollCleared
		strollCleared = True
		Game.SetPlayerAIDriven(False)
	EndIf
EndFunction

Event OnSettingChange(String a_ID)
	If a_ID == "iInterval:General" || a_ID == "bComments:General"
		(GetAlias(0) as SN_SexualHistoryChatterPlayer).ScheduleCheck()
	ElseIf a_ID == "iMapKey:General"
		RegisterMapKey()
	ElseIf StringUtil.Find(a_ID, "iMap") == 0
		PushMapSettings()
	ElseIf a_ID == "bDiary:General"
		SN_SexualHistoryNative.SetDiaryEnabled(GetModSettingBool("bDiary:General"))
	ElseIf a_ID == "bMemories:General"
		SN_SexualHistoryNative.SetMemoriesEnabled(GetModSettingBool("bMemories:General"))
	ElseIf a_ID == "bQuestDiary:General"
		SN_SexualHistoryNative.SetQuestDiaryEnabled(GetModSettingBool("bQuestDiary:General"))
	EndIf
EndEvent

; ---------------------------------------------------------------------------------------------
; The relationship map's hotkey
; ---------------------------------------------------------------------------------------------

; How far back the relationship map reads (memories, events, diaries, the length of each passage).
Function PushMapSettings()
	SN_SexualHistoryNative.SetMapSettings(GetModSettingInt("iMapMemoryScan:General"), GetModSettingInt("iMapMemoriesUsed:General"), \
		GetModSettingInt("iMapEventScan:General"), GetModSettingInt("iMapDiaryScan:General"), GetModSettingInt("iMapTextChars:General"))
EndFunction

Function RegisterMapKey()
	UnregisterForAllKeys()
	Int mapKey = GetModSettingInt("iMapKey:General")
	If mapKey > 0
		RegisterForKey(mapKey)
	EndIf
EndFunction

; Opens the relationship map, with the NPC under the crosshair chosen when there is one.
Event OnKeyDown(Int keyCode)
	If Utility.IsInMenuMode() || UI.IsTextInputEnabled()
		Return
	EndIf
	If keyCode != GetModSettingInt("iMapKey:General")
		Return
	EndIf
	If !SN_SexualHistoryNative.OpenRelationshipMap(Game.GetCurrentCrosshairRef() as Actor)
		Debug.Notification("$SNSH_MapNoUI")
	EndIf
EndEvent

Bool Function CommentsEnabled()
	Return GetModSettingBool("bComments:General")
EndFunction

Float Function CheckInterval()
	Int seconds = GetModSettingInt("iInterval:General")
	If seconds < 5
		seconds = 5
	EndIf
	Return seconds as Float
EndFunction

; ---------------------------------------------------------------------------------------------
; An NPC in a running scene says a line
; ---------------------------------------------------------------------------------------------

; Run by the player alias. Returns the seconds until the next check.
Float Function Check()
	If Utility.IsInMenuMode()
		Return RetryDelay
	EndIf
	Bool hasOStim = Game.IsPluginInstalled(OStimPlugin)
	Bool hasSexLab = Game.IsPluginInstalled(SexLabPlugin)
	Int[] threads
	If hasOStim
		threads = OThread.GetAllThreadIDs()
	EndIf
	Int sexLabSlots = 0
	If hasSexLab
		sexLabSlots = SN_SexualHistorySL.SlotCount()
	EndIf
	If threads.Length == 0 && sexLabSlots == 0
		Return CheckInterval()
	EndIf
	; Never talks over SkyrimNet: the speech queue is shared by everyone.
	If SkyrimNetApi.GetSpeechQueueSize() > 0
		Return RetryDelay
	EndIf
	Int silentMs = SkyrimNetApi.GetTimeSinceLastAudioEnded()
	If silentMs >= 0 && silentMs < 3000
		Return RetryDelay
	EndIf
	; Everyone who could speak right now: the NPCs of every running scene, OStim's and SexLab's,
	; and only those. The lists are parallel: candidates[n] is in the scene candidateThreads[n],
	; which is an OStim thread ID, or SexLabBase plus a SexLab thread slot.
	Actor player = Game.GetPlayer()
	Float reach = GetModSettingInt("iHearRadius:General") as Float
	Bool playerScenes = GetModSettingBool("bPlayerScene:General")
	Actor[] candidates = new Actor[32]
	Int[] candidateThreads = new Int[32]
	Int count = 0
	Bool retrySoon = False
	Int i = 0
	While i < threads.Length
		Int threadID = threads[i]
		String sceneId = OThread.GetScene(threadID)
		If sceneId == "" || OMetadata.IsTransition(sceneId)
			; Starting up, or between two animations: worth another look soon.
			retrySoon = True
		Else
			count = AddCandidates(OThread.GetActors(threadID), threadID, candidates, candidateThreads, count, player, reach, playerScenes)
		EndIf
		i += 1
	EndWhile
	i = 0
	While i < sexLabSlots
		Actor[] actors = SN_SexualHistorySL.PlayingActors(i)
		If actors.Length > 0
			count = AddCandidates(actors, SexLabBase + i, candidates, candidateThreads, count, player, reach, playerScenes)
		EndIf
		i += 1
	EndWhile
	If count == 0
		If retrySoon
			Return RetryDelay
		EndIf
		Return CheckInterval()
	EndIf
	; One line per check.
	Int pick = PickTurn(candidates, candidateThreads, count)
	Speak(candidateThreads[pick], candidates[pick], player)
	Return CheckInterval()
EndFunction

; Adds the NPCs of one scene who are close enough for the player to hear. Returns the new count.
Int Function AddCandidates(Actor[] actors, Int sceneKey, Actor[] candidates, Int[] candidateThreads, Int count, \
		Actor player, Float reach, Bool playerScenes)
	If !playerScenes && actors.Find(player) >= 0
		Return count
	EndIf
	Int i = 0
	While i < actors.Length && count < candidates.Length
		Actor candidate = actors[i]
		If candidate && candidate != player && !candidate.IsDead() && candidate.Is3DLoaded() \
				&& candidate.GetDistance(player) <= reach
			candidates[count] = candidate
			candidateThreads[count] = sceneKey
			count += 1
		EndIf
		i += 1
	EndWhile
	Return count
EndFunction

; Whose turn it is. Everyone speaks once, in random order, before anyone speaks a second time:
; with five NPCs 1, 4, 5, 3, 2 and then a new round, 3, 4, 5, 2, 1. A new round never opens with
; the NPC who closed the last one, unless there is nobody else. Someone who joins a scene part
; way through a round gets a turn in that round.
; Within a round a partner answers first: when someone in the same scene as the last speaker has
; not had a turn yet, the next turn goes to them (one of them at random in a threesome), so a
; round runs 1, 2 (1's partner), 4, 3 (4's partner) rather than jumping between scenes.
Int Function PickTurn(Actor[] candidates, Int[] candidateThreads, Int count)
	Int[] open = new Int[32]
	Int openCount = 0
	Int i = 0
	While i < count
		If spoken.Find(candidates[i]) < 0
			open[openCount] = i
			openCount += 1
		EndIf
		i += 1
	EndWhile
	Int lastIndex = -1
	If lastSpeaker
		lastIndex = candidates.Find(lastSpeaker)
	EndIf
	If openCount > 0 && lastIndex >= 0 && lastIndex < count
		; Narrow the open turns down to the last speaker's partners, when there are any.
		Int[] partners = new Int[32]
		Int partnerCount = 0
		i = 0
		While i < openCount
			If candidateThreads[open[i]] == candidateThreads[lastIndex]
				partners[partnerCount] = open[i]
				partnerCount += 1
			EndIf
			i += 1
		EndWhile
		If partnerCount > 0
			open = partners
			openCount = partnerCount
		EndIf
	EndIf
	If openCount == 0
		; Everyone here has had a turn: a new round.
		spoken = new Actor[32]
		spokenCount = 0
		i = 0
		While i < count
			If candidates[i] != lastSpeaker
				open[openCount] = i
				openCount += 1
			EndIf
			i += 1
		EndWhile
		If openCount == 0
			; The only one who can speak is the one who spoke last.
			open[0] = 0
			openCount = 1
		EndIf
	EndIf
	Int pick = open[Utility.RandomInt(0, openCount - 1)]
	If spokenCount >= spoken.Length
		spoken = new Actor[32]
		spokenCount = 0
	EndIf
	spoken[spokenCount] = candidates[pick]
	spokenCount += 1
	lastSpeaker = candidates[pick]
	Return pick
EndFunction

; The speaker says a line to the player when the player is in the scene, and otherwise to the
; next actor of the scene.
Function Speak(Int sceneKey, Actor speaker, Actor player)
	Actor[] actors
	String facts
	Int arousal
	; Whether the scene is forced, dominating or rough, and who is on the receiving end when the
	; framework says so (SexLab does; OStim does not).
	Bool forced = False
	Actor[] passive
	If sceneKey >= SexLabBase
		Int slot = sceneKey - SexLabBase
		actors = SN_SexualHistorySL.PlayingActors(slot)
		If actors.Length == 0
			; The scene ended between the check and now.
			Return
		EndIf
		facts = SN_SexualHistorySL.NarrationFacts(slot)
		arousal = SN_SexualHistorySL.Enjoyment(slot, speaker)
		passive = SN_SexualHistorySL.PassiveActors(slot)
		forced = passive.Length > 0
	Else
		actors = OThread.GetActors(sceneKey)
		facts = SceneFacts(sceneKey, OThread.GetScene(sceneKey), actors)
		arousal = OActor.GetExcitement(speaker) as Int
		forced = OStimForceful(sceneKey, OThread.GetScene(sceneKey))
	EndIf
	Actor listener = None
	If actors.Find(player) >= 0
		listener = player
	ElseIf actors.Length > 1
		listener = actors[(actors.Find(speaker) + 1) % actors.Length]
	EndIf
	SkyrimNetApi.DirectNarration(SceneNarration(actors, facts, arousal, speaker, listener, player, forced, passive), speaker, listener)
EndFunction

; OStim has no flag for a forced scene: OStimNet's intent says so when it is installed, and the
; animation's tags can.
Bool Function OStimForceful(Int threadID, String sceneId)
	If Game.IsPluginInstalled(OStimNetPlugin)
		String intent = SN_SexualHistoryONet.Intent(threadID)
		If intent == "aggressive" || intent == "dom"
			Return True
		EndIf
	EndIf
	String[] tags = OMetadata.GetSceneTags(sceneId)
	Int i = 0
	While i < tags.Length
		String tag = tags[i]
		If tag == "aggressive" || tag == "rough" || tag == "forced" || tag == "noncon" || tag == "rape"
			Return True
		EndIf
		i += 1
	EndWhile
	Return False
EndFunction

; Narration is taken as something that has already happened and the speaker reacts to it. What is
; said is left to the speaker: SkyrimNet gives the LLM the speaker's character profile, memories
; and the recent conversation, and the narration only says what is going on right now.
String Function SceneNarration(Actor[] actors, String facts, Int arousal, Actor speaker, Actor listener, Actor player, 		Bool forced, Actor[] passive)
	String speakerName = speaker.GetDisplayName()
	String text
	If actors.Length == 1
		text = speakerName + " is alone in the middle of an intimate act " + Situation(player) + ", and it is still going on. "
	Else
		text = NameList(actors) + " are in the middle of an intimate scene together " + Situation(player) + ", and it is still going on. "
	EndIf
	text += facts
	; In a forced scene the one on the receiving end does not want it. Known when the framework
	; names them; otherwise the speaker's side is left to the character and the actions.
	Bool speakerPassive = forced && passive.Find(speaker) >= 0
	Bool speakerForcing = forced && passive.Length > 0 && !speakerPassive
	If arousal > 0
		If speakerPassive
			text += speakerName + "'s body is reacting despite " + speakerName + " (arousal about " + arousal + " out of 100), which " + speakerName + " does not want. "
		Else
			text += speakerName + "'s arousal is at about " + arousal + " out of 100. "
		EndIf
	EndIf
	; What was said just before the scene comes first, both ways; the framework's signals (above)
	; decide only when it does not settle whether it is consensual.
	If forced
		text += "First judge from what was said just before this scene began (the recent conversation). If it clearly shows that the one it is being done to agreed to it freely, they are a willing partner even though the scene is dominating or rough, and the instructions below about not wanting it do not apply. " \
			+ "Agreeing under threat, or when one could not refuse (a captive, someone defeated or overpowered), is not agreeing freely. Only when the conversation does not make it clear, follow these instructions: "
	Else
		text += "If what was said just before this scene began clearly shows it is against someone's will (agreeing under threat, or when one could not refuse, is not agreeing), the one it is done to does not want it: no enthusiasm, consent or eagerness from them. "
	EndIf
	If speakerPassive
		text += "This is being done to " + speakerName + " against " + speakerName + "'s will. The line must show that " + speakerName 			+ " does not want it: reluctance, fear, anger, disgust, shame, pleading or resistance, as fits " + speakerName 			+ ". Never enthusiasm, consent, eagerness or asking for more. At most, " + speakerName 			+ " may admit with dismay or shame that their body is reacting despite themselves. "
	ElseIf speakerForcing
		text += speakerName + " is the one forcing this on the others. The line may be domineering or cruel, as fits " + speakerName 			+ ", but it must not make the others sound willing. "
	ElseIf forced
		text += "This scene is forced, dominating or rough. If " + speakerName + " is the one it is being done to, judging from " 			+ speakerName + "'s character and the actions, the line must show that " + speakerName 			+ " does not want it (reluctance, fear, anger, shame or resistance), never enthusiasm, consent or asking for more; at most a reluctant admission that their body reacts despite themselves. " 			+ "If " + speakerName + " is the one in control, the line may be domineering, but must not make the others sound willing. "
	EndIf
	text += speakerName + " has not said anything about this moment yet. " + speakerName + " now says something out loud"
	If listener
		text += " to " + listener.GetDisplayName()
	EndIf
	text += ", without stopping: a reaction to what is being done right now and how it feels. " \
		+ "One or two short sentences, breathless if that fits. " \
		+ "Say it exactly as " + speakerName + " would, following " + speakerName + "'s character profile: " \
		+ speakerName + "'s personality, way of speaking, memories and relationship with the others decide the wording and the mood. " \
		+ "It must be new: do not repeat or closely echo anything " + speakerName + " said earlier in this scene."
	Return text
EndFunction

; What the scene is doing right now. OStimNet's description when it has one, and otherwise what
; OStim's own scene data says: the scene's name, its actions and its tags.
String Function SceneFacts(Int threadID, String sceneId, Actor[] actors)
	String text = ""
	Bool described = False
	If Game.IsPluginInstalled(OStimNetPlugin)
		String description = SN_SexualHistoryONet.SceneDescription(threadID)
		If description != ""
			text += "What is happening right now: " + description + " "
			described = True
		EndIf
		String intent = SN_SexualHistoryONet.Intent(threadID)
		If intent != ""
			text += "The mood between them is " + intent + ". "
		EndIf
		String phase = SN_SexualHistoryONet.Phase(threadID)
		If phase != ""
			text += "They are at the " + phase + " stage. "
		EndIf
	EndIf
	If !described
		String sceneName = OMetadata.GetName(sceneId)
		If sceneName != ""
			text += "The animation playing is called \"" + sceneName + "\". "
		EndIf
		String actions = ActionList(sceneId, actors)
		If actions != ""
			text += "What is being done right now: " + actions + ". "
		EndIf
		String tags = TagList(sceneId)
		If tags != ""
			text += "Position details: " + tags + ". "
		EndIf
	EndIf
	String furnitureType = OThread.GetFurnitureType(threadID)
	If furnitureType != "" && furnitureType != "none"
		text += "They are using a " + furnitureType + ". "
	EndIf
	Return text
EndFunction

; "kissing (Lydia to Farengar), vaginalsex (Farengar to Lydia)"
String Function ActionList(String sceneId, Actor[] actors)
	String[] types = OMetadata.GetActionTypes(sceneId)
	String text = ""
	Int i = 0
	While i < types.Length && i < MaxActions
		Int actorIndex = OMetadata.GetActionActor(sceneId, i)
		Int targetIndex = OMetadata.GetActionTarget(sceneId, i)
		String entry = types[i]
		If actorIndex >= 0 && actorIndex < actors.Length && actors[actorIndex]
			entry += " (" + actors[actorIndex].GetDisplayName()
			If targetIndex >= 0 && targetIndex < actors.Length && targetIndex != actorIndex && actors[targetIndex]
				entry += " to " + actors[targetIndex].GetDisplayName()
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

String Function TagList(String sceneId)
	String[] tags = OMetadata.GetSceneTags(sceneId)
	String text = ""
	Int i = 0
	While i < tags.Length && i < MaxActions
		If text != ""
			text += ", "
		EndIf
		text += tags[i]
		i += 1
	EndWhile
	Return text
EndFunction

; "Lydia", "Lydia and Farengar", "Lydia, Farengar and Ulfric"
String Function NameList(Actor[] actors)
	String text = ""
	Int i = 0
	While i < actors.Length
		If i > 0
			If i == actors.Length - 1
				text += " and "
			Else
				text += ", "
			EndIf
		EndIf
		text += actors[i].GetDisplayName()
		i += 1
	EndWhile
	Return text
EndFunction

; "in Whiterun, indoors"
String Function Situation(Actor player)
	String text = ""
	Location here = player.GetCurrentLocation()
	If here && here.GetName() != ""
		text = "in " + here.GetName() + ", "
	EndIf
	If player.IsInInterior()
		text += "indoors"
	Else
		text += "outdoors"
	EndIf
	Return text
EndFunction
