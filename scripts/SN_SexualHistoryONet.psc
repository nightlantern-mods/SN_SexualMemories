Scriptname SN_SexualHistoryONet Hidden
{The only script that calls OStimNet. OStimNet is optional: SN_SexualHistoryChatter calls these
functions only after it has found TT_OStimNet.esp in the load order.}

; OStimNet's own description of what the thread is doing right now, or "" when it has none.
String Function SceneDescription(Int threadID) Global
	Return OStimNet.GetSceneDescription(threadID)
EndFunction

; "romantic", "lustful", "transactional", "dom", "aggressive", "platonic", or "" when not set.
String Function Intent(Int threadID) Global
	Return OStimNet.GetThreadIntent(threadID)
EndFunction

; "undressing", "foreplay", "oral", "sex", or "" when OStimNet does not track phases for the thread.
String Function Phase(Int threadID) Global
	Return OStimNet.GetThreadPhase(threadID)
EndFunction
