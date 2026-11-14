Scriptname SN_SexualHistorySL Hidden
{The only script that calls SexLab. Called only after SexLab.esm has been found in the load order.
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

String Function Append(String asList, String asItem) Global
	If asList == ""
		Return asItem
	EndIf
	Return asList + ", " + asItem
EndFunction
