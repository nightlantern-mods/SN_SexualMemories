Scriptname SN_SexualHistorySL Hidden
{The only script that calls SexLab, for both the memories and the lines. Called only after
SexLab.esm has been found in the load order.
Works with SexLab 1.6x and with SexLab P+; what only P+ has is called only when P+ is running.}

; How many tags of a scene are listed at most.
Int Function MaxTags() Global
	Return 10
EndFunction

; SexLab P+ ships SexLabUtil as an SKSE plugin with a version above 2.0; 1.6x answers 16xxx.
Bool Function IsPPlus() Global
	Int version = SKSE.GetPluginVersion("SexLabUtil")
	If version <= 0
		version = SexLabUtil.GetVersion()
	EndIf
	Return version > 20000
EndFunction

; The thread with this ID (its slot), or None.
sslThreadController Function Thread(Int aiThread) Global
	SexLabFramework api = SexLabUtil.GetAPI()
	If !api || !api.ThreadSlots
		Return None
	EndIf
	sslThreadController[] threads = api.ThreadSlots.Threads
	If aiThread < 0 || aiThread >= threads.Length
		Return None
	EndIf
	Return threads[aiThread]
EndFunction

Actor[] Function Actors(Int aiThread) Global
	sslThreadController thread = Thread(aiThread)
	If !thread
		Actor[] nobody
		Return nobody
	EndIf
	Return thread.Positions
EndFunction

; "Animation "X". Stage 2 of 5. Involves: vaginal sex. Tags: ... Not in control: Lydia."
String Function SceneFacts(Int aiThread) Global
	sslThreadController thread = Thread(aiThread)
	If !thread
		Return ""
	EndIf
	String text = ""
	String sceneName
	String[] tags
	Actor[] passive
	If IsPPlus()
		; Never thread.Animation on P+: every read of it makes P+ write all its animation aliases.
		String sceneId = thread.GetActiveScene()
		sceneName = SexLabRegistry.GetSceneName(sceneId)
		tags = SexLabRegistry.GetSceneTags(sceneId)
		String kinds = ""
		If thread.IsSceneVaginal()
			kinds = "vaginal sex"
		EndIf
		If thread.IsSceneAnal()
			kinds = Append(kinds, "anal sex")
		EndIf
		If thread.IsSceneOral()
			kinds = Append(kinds, "oral sex")
		EndIf
		If kinds != ""
			text += "Involves: " + kinds + ". "
		EndIf
		If !thread.IsConsent()
			passive = thread.GetSubmissives()
		EndIf
	Else
		sslBaseAnimation animation = thread.Animation
		If animation
			sceneName = animation.Name
			tags = animation.GetTags()
			If animation.StageCount > 0
				text += "Stage " + thread.Stage + " of " + animation.StageCount + ". "
			EndIf
		EndIf
		If thread.IsAggressive
			passive = thread.Victims
		EndIf
	EndIf
	If sceneName != ""
		text = "Animation \"" + sceneName + "\". " + text
	EndIf
	String tagList = ""
	Int i = 0
	While i < tags.Length && i < MaxTags()
		If tags[i] != ""
			tagList = Append(tagList, tags[i])
		EndIf
		i += 1
	EndWhile
	If tagList != ""
		text += "Tags: " + tagList + ". "
	EndIf
	String passiveNames = ""
	i = 0
	While i < passive.Length
		If passive[i]
			passiveNames = Append(passiveNames, passive[i].GetDisplayName())
		EndIf
		i += 1
	EndWhile
	If passiveNames != ""
		text += "Forceful and one-sided; not in control: " + passiveNames + ". "
	EndIf
	Return text
EndFunction

; ---- For the lines spoken during scenes (SN_SexualHistoryChatter) ----------------------------

; The number of thread slots SexLab has, running or not.
Int Function SlotCount() Global
	SexLabFramework api = SexLabUtil.GetAPI()
	If !api || !api.ThreadSlots
		Return 0
	EndIf
	Return api.ThreadSlots.Threads.Length
EndFunction

; The thread in a slot while its animation is playing, and None otherwise (idle, setting up,
; ending).
sslThreadController Function PlayingThread(Int slot) Global
	SexLabFramework api = SexLabUtil.GetAPI()
	If !api || !api.ThreadSlots
		Return None
	EndIf
	sslThreadController[] threads = api.ThreadSlots.Threads
	If slot < 0 || slot >= threads.Length || !threads[slot]
		Return None
	EndIf
	sslThreadController thread = threads[slot]
	If IsPPlus()
		; SexLabThread.STATUS_INSCENE
		If thread.GetStatus() != 3
			Return None
		EndIf
	Else
		String threadState = thread.GetState()
		If threadState != "Animating" && threadState != "Advancing"
			Return None
		EndIf
	EndIf
	Return thread
EndFunction

; The actors of the scene playing in a slot, or an empty array when none is playing there.
Actor[] Function PlayingActors(Int slot) Global
	sslThreadController thread = PlayingThread(slot)
	If !thread
		Actor[] nobody
		Return nobody
	EndIf
	Return thread.Positions
EndFunction

; SexLab's own measure of how far along an actor is, 0 to 100.
Int Function Enjoyment(Int slot, Actor akActor) Global
	sslThreadController thread = PlayingThread(slot)
	If !thread
		Return 0
	EndIf
	Return thread.GetEnjoyment(akActor)
EndFunction

; What the scene is doing right now, worded for the narration of a line: the animation's name,
; its tags, and whether somebody is not in control.
String Function NarrationFacts(Int slot) Global
	sslThreadController thread = PlayingThread(slot)
	If !thread
		Return ""
	EndIf
	String text = ""
	String sceneName
	String[] tags
	Actor[] passive
	If IsPPlus()
		; Never thread.Animation on P+: every read of it makes P+ write all its animation aliases.
		String sceneId = thread.GetActiveScene()
		sceneName = SexLabRegistry.GetSceneName(sceneId)
		tags = SexLabRegistry.GetSceneTags(sceneId)
		String kinds = ""
		If thread.IsSceneVaginal()
			kinds = "vaginal sex"
		EndIf
		If thread.IsSceneAnal()
			kinds = Append(kinds, "anal sex")
		EndIf
		If thread.IsSceneOral()
			kinds = Append(kinds, "oral sex")
		EndIf
		If kinds != ""
			text += "The scene involves " + kinds + ". "
		EndIf
		If !thread.IsConsent()
			passive = thread.GetSubmissives()
		EndIf
	Else
		sslBaseAnimation animation = thread.Animation
		If animation
			sceneName = animation.Name
			tags = animation.GetTags()
			If animation.StageCount > 0
				text += "It is at stage " + thread.Stage + " of " + animation.StageCount + ". "
			EndIf
		EndIf
		If thread.IsAggressive
			passive = thread.Victims
		EndIf
	EndIf
	If sceneName != ""
		text = "The animation playing is called \"" + sceneName + "\". " + text
	EndIf
	String tagList = ""
	Int i = 0
	While i < tags.Length && i < MaxTags()
		If tags[i] != ""
			tagList = Append(tagList, tags[i])
		EndIf
		i += 1
	EndWhile
	If tagList != ""
		text += "Its tags: " + tagList + ". "
	EndIf
	String passiveNames = ""
	i = 0
	While i < passive.Length
		If passive[i]
			passiveNames = Append(passiveNames, passive[i].GetDisplayName())
		EndIf
		i += 1
	EndWhile
	If passiveNames != ""
		text += "It is forceful and one-sided; not in control of it: " + passiveNames + ". "
	EndIf
	Return text
EndFunction

; Who is on the receiving end of a forced scene playing in a slot: P+'s submissives when the scene
; is not consensual, 1.6x's victims when it is aggressive. Empty when the scene is consensual.
Actor[] Function PassiveActors(Int slot) Global
	sslThreadController thread = PlayingThread(slot)
	Actor[] nobody
	If !thread
		Return nobody
	EndIf
	If IsPPlus()
		If !thread.IsConsent()
			Return thread.GetSubmissives()
		EndIf
	ElseIf thread.IsAggressive
		Return thread.Victims
	EndIf
	Return nobody
EndFunction

String Function Append(String asList, String asItem) Global
	If asList == ""
		Return asItem
	EndIf
	Return asList + ", " + asItem
EndFunction
