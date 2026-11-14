Scriptname SN_SexualHistoryPlayer extends ReferenceAlias
{Registers the scene events again after each save load, so a framework installed later is picked up.}

Event OnPlayerLoadGame()
	(GetOwningQuest() as SN_SexualHistory).RegisterEvents()
EndEvent
