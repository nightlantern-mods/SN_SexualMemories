Scriptname SN_SexualHistoryPlayerTalk extends ReferenceAlias
{Now and then, the player's character speaks up to one NPC nearby: SN_SexualHistory.dll has the LLM
write the line from the recent events, the player's own memories (about that NPC above all) and
their relationship, the player says it (in their voice when SkyrimNet voices the player) and the
NPC answers. Runs on the player alias of its own quest, SN_SexualHistory_PlayerTalk. Off by
default; the settings are in the MCM, read through SN_SexualHistory_Chatter.}

Int Property MCMQuestID = 0x801 AutoReadOnly
; How many random picks among the NPCs near the player are tried each time.
Int Property Tries = 10 AutoReadOnly
; While the feature is off, how often the setting is looked at again.
Float Property OffPoll = 60.0 AutoReadOnly
; When now is a bad moment (a menu, combat, SkyrimNet speaking), the next try comes this soon.
Float Property RetryDelay = 20.0 AutoReadOnly

Actor lastTarget

Event OnInit()
	RegisterForSingleUpdate(OffPoll)
EndEvent

Event OnPlayerLoadGame()
	UnregisterForUpdate()
	RegisterForSingleUpdate(OffPoll)
EndEvent

Event OnUpdate()
	SN_SexualHistoryChatter mcm = Game.GetFormFromFile(MCMQuestID, "SN_SexualHistory.esp") as SN_SexualHistoryChatter
	If !mcm || !mcm.GetModSettingBool("bPlayerTalk:General")
		RegisterForSingleUpdate(OffPoll)
		Return
	EndIf
	Float interval = mcm.GetModSettingInt("iPlayerTalkInterval:General") as Float
	If interval < 10.0
		interval = 10.0
	EndIf
	If TryTalk()
		RegisterForSingleUpdate(interval)
	ElseIf interval < RetryDelay
		RegisterForSingleUpdate(interval)
	Else
		RegisterForSingleUpdate(RetryDelay)
	EndIf
EndEvent

; False when now was a bad moment and it should try again soon; True when the chance was taken,
; whether or not anyone was there to talk to.
Bool Function TryTalk()
	Actor player = Game.GetPlayer()
	If Utility.IsInMenuMode() || player.IsInCombat() || player.GetCurrentScene() || SN_SexualHistoryNative.InScene(player)
		Return False
	EndIf
	If SkyrimNetApi.GetSpeechQueueSize() > 0
		Return False
	EndIf
	Int silentMs = SkyrimNetApi.GetTimeSinceLastAudioEnded()
	If silentMs >= 0 && silentMs < 8000
		Return False
	EndIf
	Float talkRange = SkyrimNetSetting("interaction.normalMaxDistance", 1600.0)
	Keyword npcKeyword = Keyword.GetKeyword("ActorTypeNPC")
	Int i = 0
	While i < Tries
		Actor target = Game.FindRandomActorFromRef(player, talkRange)
		If CanTalkTo(target, player, npcKeyword)
			String place = ""
			Location here = player.GetCurrentLocation()
			If here
				place = here.GetName()
			EndIf
			If place == "" && player.GetParentCell()
				place = player.GetParentCell().GetName()
			EndIf
			If !SN_SexualHistoryNative.PlayerTalk(target, target.GetRelationshipRank(player), place)
				Return False
			EndIf
			lastTarget = target
			Return True
		EndIf
		i += 1
	EndWhile
	Return True
EndFunction

Bool Function CanTalkTo(Actor akTarget, Actor akPlayer, Keyword akNPC)
	If !akTarget || akTarget == akPlayer || akTarget == lastTarget || !akTarget.Is3DLoaded() || akTarget.IsDead() \
			|| akTarget.IsChild() || akTarget.IsInCombat() || akTarget.IsUnconscious() || akTarget.GetSleepState() == 3 \
			|| akTarget.GetCurrentScene() || (akNPC && !akTarget.HasKeyword(akNPC)) || akTarget.IsHostileToActor(akPlayer)
		Return False
	EndIf
	Return !SN_SexualHistoryNative.InScene(akTarget)
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
