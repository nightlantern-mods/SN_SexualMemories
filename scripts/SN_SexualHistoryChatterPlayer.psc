Scriptname SN_SexualHistoryChatterPlayer extends ReferenceAlias
{Sets the quest script up after each save load and runs the timer that looks for running OStim
scenes. The timer lives here so it never collides with the quest script's own updates.}

Event OnInit()
	ScheduleCheck()
EndEvent

Event OnPlayerLoadGame()
	(GetOwningQuest() as SN_SexualHistoryChatter).Setup()
	ScheduleCheck()
EndEvent

; Starts the timer over with the full interval, or stops it while comments are off.
Function ScheduleCheck()
	UnregisterForUpdate()
	SN_SexualHistoryChatter chatter = GetOwningQuest() as SN_SexualHistoryChatter
	If chatter.CommentsEnabled()
		RegisterForSingleUpdate(chatter.CheckInterval())
	EndIf
EndFunction

Event OnUpdate()
	SN_SexualHistoryChatter chatter = GetOwningQuest() as SN_SexualHistoryChatter
	If !chatter.CommentsEnabled()
		Return
	EndIf
	RegisterForSingleUpdate(chatter.Check())
EndEvent
