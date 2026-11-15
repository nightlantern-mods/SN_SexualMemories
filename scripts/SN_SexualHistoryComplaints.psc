Scriptname SN_SexualHistoryComplaints extends ReferenceAlias
{Now and then, an NPC near the player who has a spouse or lover lets out a complaint about them
to the player, written and voiced by SkyrimNet, but only while that spouse or lover is out of
earshot. Runs on the player alias of its own quest, SN_SexualHistory_Complaints. Settings are in
the MCM, read through the quest that runs it (SN_SexualHistory_Chatter).}

Int Property MCMQuestID = 0x801 AutoReadOnly
; How many random picks among the NPCs near the player are tried each time.
Int Property Tries = 10 AutoReadOnly
; While the feature is off, how often the setting is looked at again.
Float Property OffPoll = 60.0 AutoReadOnly
; When SkyrimNet is still speaking, or a menu is open, the next try comes this soon.
Float Property RetryDelay = 20.0 AutoReadOnly
; How many of the last speakers are left out, so the same NPC does not complain twice in a row.
Int Property RecentCount = 5 AutoReadOnly

Actor[] recent
Int recentNext

Event OnInit()
	RegisterForSingleUpdate(OffPoll)
EndEvent

Event OnPlayerLoadGame()
	UnregisterForUpdate()
	RegisterForSingleUpdate(OffPoll)
EndEvent

Event OnUpdate()
	SN_SexualHistoryChatter mcm = Game.GetFormFromFile(MCMQuestID, "SN_SexualHistory.esp") as SN_SexualHistoryChatter
	If !mcm || !mcm.GetModSettingBool("bComplaints:General")
		RegisterForSingleUpdate(OffPoll)
		Return
	EndIf
	Float interval = mcm.GetModSettingInt("iComplaintInterval:General") as Float
	If interval < 30.0
		interval = 30.0
	EndIf
	If TryComplaint(mcm.GetModSettingInt("iComplaintSex:General"))
		RegisterForSingleUpdate(interval)
	Else
		RegisterForSingleUpdate(RetryDelay)
	EndIf
EndEvent

; Returns False when now was a bad moment (a menu, combat, SkyrimNet speaking) and it should
; try again soon; True when the chance was taken, whether or not anyone complained.
Bool Function TryComplaint(Int aiSex)
	Actor player = Game.GetPlayer()
	If Utility.IsInMenuMode() || player.IsInCombat()
		Return False
	EndIf
	If SkyrimNetApi.GetSpeechQueueSize() > 0
		Return False
	EndIf
	Int silentMs = SkyrimNetApi.GetTimeSinceLastAudioEnded()
	If silentMs >= 0 && silentMs < 5000
		Return False
	EndIf
	Float talkRange = SkyrimNetSetting("interaction.normalMaxDistance", 1600.0)
	Float earshot = talkRange * SkyrimNetSetting("interaction.nearbyNPCAwarenessMultiplier", 2.0)
	Keyword npcKeyword = Keyword.GetKeyword("ActorTypeNPC")
	Int i = 0
	While i < Tries
		Actor candidate = Game.FindRandomActorFromRef(player, talkRange)
		If CanComplain(candidate, player, npcKeyword, aiSex)
			String partners = SN_SexualHistoryNative.PartnerDescription(candidate)
			If partners != "" && !PartnerCanHear(candidate, earshot)
				Complain(candidate, partners, player)
				Return True
			EndIf
		EndIf
		i += 1
	EndWhile
	Return True
EndFunction

; aiSex: 0 anyone, 1 women only, 2 men only.
Bool Function CanComplain(Actor akCandidate, Actor akPlayer, Keyword akNPC, Int aiSex)
	If !akCandidate || akCandidate == akPlayer || !akCandidate.Is3DLoaded() || akCandidate.IsDead() \
			|| akCandidate.IsChild() || akCandidate.IsInCombat() || akCandidate.IsUnconscious() \
			|| akCandidate.GetSleepState() == 3 || akCandidate.GetCurrentScene() \
			|| (akNPC && !akCandidate.HasKeyword(akNPC))
		Return False
	EndIf
	If recent && recent.Find(akCandidate) >= 0
		Return False
	EndIf
	If aiSex == 1 || aiSex == 2
		Int sex = akCandidate.GetLeveledActorBase().GetSex()
		If (aiSex == 1 && sex != 1) || (aiSex == 2 && sex != 0)
			Return False
		EndIf
	EndIf
	Return !SN_SexualHistoryNative.InScene(akCandidate)
EndFunction

; True when one of the speaker's partners is loaded and close enough to overhear.
Bool Function PartnerCanHear(Actor akSpeaker, Float afEarshot)
	Actor[] nearby = SN_SexualHistoryNative.LoadedPartners(akSpeaker)
	Int i = 0
	While i < nearby.Length
		If nearby[i] && nearby[i].Is3DLoaded() && !nearby[i].IsDead() && akSpeaker.GetDistance(nearby[i]) <= afEarshot
			Return True
		EndIf
		i += 1
	EndWhile
	Return False
EndFunction

; What is said is left to the speaker: SkyrimNet gives the LLM their character profile, their
; memories and the recent conversation; the narration only sets up the moment.
Function Complain(Actor akSpeaker, String asPartners, Actor akPlayer)
	String speakerName = akSpeaker.GetDisplayName()
	String text = speakerName + " is out of earshot of " + asPartners + ". " + speakerName \
		+ " turns to " + akPlayer.GetDisplayName() + " and quietly lets out a complaint about them: a habit, a slight, a worry or a hurt that has been bothering " \
		+ speakerName + ", from a petty gripe to something serious. It fits " + speakerName \
		+ "'s character, their life together and what " + speakerName \
		+ " remembers, and it is said as an aside " + speakerName + " would not want repeated to them."
	SkyrimNetApi.DirectNarration(text, akSpeaker, akPlayer)
	If !recent
		recent = new Actor[5]
	EndIf
	recent[recentNext] = akSpeaker
	recentNext = (recentNext + 1) % RecentCount
EndFunction

; A value of SkyrimNet's own settings (SkyrimNet.yaml), or the fallback when it cannot be read.
; The settings file is only read, never written.
Float Function SkyrimNetSetting(String asPath, Float afFallback)
	Float value = SkyrimNetApi.GetConfigFloat("game", asPath, -1.0)
	If value <= 0.0
		value = SkyrimNetApi.GetConfigFloat("SkyrimNet", asPath, -1.0)
	EndIf
	If value <= 0.0
		Return afFallback
	EndIf
	Return value
EndFunction
