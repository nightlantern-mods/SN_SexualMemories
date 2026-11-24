Scriptname SN_SexualHistoryGossip extends ReferenceAlias
{Now and then, when two or more of the player's followers are near, one of them talks about the
player behind the player's back to another, thinking the player cannot hear; the player hears it
all. Written and voiced by SkyrimNet. Runs on the player alias of its own quest,
SN_SexualHistory_Gossip. Settings are in the MCM, read through the quest that runs it
(SN_SexualHistory_Chatter).}

Int Property MCMQuestID = 0x801 AutoReadOnly
; While the feature is off, how often the setting is looked at again.
Float Property OffPoll = 60.0 AutoReadOnly
; When SkyrimNet is still speaking, or a menu is open, the next try comes this soon.
Float Property RetryDelay = 20.0 AutoReadOnly

Actor lastSpeaker

Event OnInit()
	RegisterForSingleUpdate(OffPoll)
EndEvent

Event OnPlayerLoadGame()
	UnregisterForUpdate()
	RegisterForSingleUpdate(OffPoll)
EndEvent

Event OnUpdate()
	SN_SexualHistoryChatter mcm = Game.GetFormFromFile(MCMQuestID, "SN_SexualHistory.esp") as SN_SexualHistoryChatter
	If !mcm || !mcm.GetModSettingBool("bGossip:General")
		RegisterForSingleUpdate(OffPoll)
		Return
	EndIf
	Float interval = mcm.GetModSettingInt("iGossipInterval:General") as Float
	If interval < 30.0
		interval = 30.0
	EndIf
	If TryGossip()
		RegisterForSingleUpdate(interval)
	Else
		RegisterForSingleUpdate(RetryDelay)
	EndIf
EndEvent

; Returns False when now was a bad moment (a menu, combat, SkyrimNet speaking) and it should
; try again soon; True when the chance was taken, whether or not anyone spoke.
Bool Function TryGossip()
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
	Actor[] found = SN_SexualHistoryNative.NearbyFollowers(talkRange)
	Actor[] able = new Actor[10]
	Int count = 0
	Int i = 0
	While i < found.Length && count < able.Length
		If CanGossip(found[i])
			able[count] = found[i]
			count += 1
		EndIf
		i += 1
	EndWhile
	If count < 2
		Return True
	EndIf
	Int speakerIndex = Utility.RandomInt(0, count - 1)
	If able[speakerIndex] == lastSpeaker
		speakerIndex = (speakerIndex + Utility.RandomInt(1, count - 1)) % count
	EndIf
	Int listenerIndex = (speakerIndex + Utility.RandomInt(1, count - 1)) % count
	Gossip(able[speakerIndex], able[listenerIndex], player)
	Return True
EndFunction

Bool Function CanGossip(Actor akFollower)
	If !akFollower || !akFollower.Is3DLoaded() || akFollower.IsDead() || akFollower.IsInCombat() \
			|| akFollower.IsUnconscious() || akFollower.GetSleepState() == 3 || akFollower.GetCurrentScene()
		Return False
	EndIf
	Return !SN_SexualHistoryNative.InScene(akFollower)
EndFunction

; What is said is left to the speaker: SkyrimNet gives the LLM their character profile, their
; memories and the recent conversation; the narration only sets up the moment. It says how close
; the two are to the player, so the player is talked about as a companion they know, not as a
; stranger.
Function Gossip(Actor akSpeaker, Actor akListener, Actor akPlayer)
	String speakerName = akSpeaker.GetDisplayName()
	String listenerName = akListener.GetDisplayName()
	String playerName = akPlayer.GetDisplayName()
	String text = speakerName + " and " + listenerName + " travel with " + playerName + " and know " + playerName + " well. " \
		+ speakerName + " is " + playerName + "'s " + BondWord(akSpeaker, akPlayer) + ", and " + listenerName + " is " \
		+ playerName + "'s " + BondWord(akListener, akPlayer) + ". They believe " + playerName + " is out of earshot. " \
		+ speakerName + " leans toward " + listenerName + " and, in a low voice, talks about " + playerName \
		+ " behind " + playerName + "'s back, the way companions talk about someone they know well: something " \
		+ playerName + " did or said, a habit, a choice, their love life, a worry, a suspicion, a joke at " + playerName \
		+ "'s expense, or what " + speakerName + " really thinks of " + playerName + ". The tone follows how " + speakerName \
		+ " actually feels about " + playerName + " and what " + speakerName + " remembers of their time together: fond teasing, concern, gratitude, " \
		+ "envy or real resentment. " + speakerName + " calls " + playerName + " by name or by whatever " + speakerName \
		+ " usually calls " + playerName + ", the way one speaks of a close companion. No distancing words that would fit a stranger: not \"that " + playerName \
		+ "\", not \"that " + playerName + " guy\" or \"this " + playerName + " person\", in any language. " + speakerName + " says something " \
		+ speakerName + " would not say to " + playerName + "'s face. " + speakerName + " speaks only to " + listenerName + ". " \
		+ playerName + " in fact hears every word, but neither " + speakerName + " nor " + listenerName + " knows it."
	SkyrimNetApi.DirectNarration(text, akSpeaker, akListener)
	lastSpeaker = akSpeaker
EndFunction

; The game's relationship of akFollower to the player, as words for the AI. A follower with no
; closer relationship is a travelling companion.
String Function BondWord(Actor akFollower, Actor akPlayer)
	Int rank = akFollower.GetRelationshipRank(akPlayer)
	If rank >= 4
		Return "lover"
	ElseIf rank == 3
		Return "trusted ally and travelling companion"
	ElseIf rank == 2
		Return "confidant and travelling companion"
	ElseIf rank == 1
		Return "friend and travelling companion"
	ElseIf rank == -1
		Return "travelling companion, though a rival"
	ElseIf rank <= -2
		Return "travelling companion, though no friend"
	EndIf
	Return "travelling companion"
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
