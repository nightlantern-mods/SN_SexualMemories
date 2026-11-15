Scriptname SN_SexualHistorySubtitle Hidden
{Shows the player's line from SN_SexualHistoryPlayerTalk as a subtitle. SkyrimNet voices the line
with TriggerPlayerTTS, which shows no subtitle, so the line goes into the HUD's own subtitle field
and is taken down when the player's voice has finished (or, with no voice, after a time that
fits its length). Nothing is shown when dialogue subtitles are off in the game's settings.}

; Called by SN_SexualHistory.dll right after it asked SkyrimNet to voice the line.
Function Show(String asText) Global
	If asText == "" || !Utility.GetINIBool("bDialogueSubtitles:Interface")
		Return
	EndIf
	UI.InvokeString("HUD Menu", "_root.HUDMovieBaseInstance.ShowSubtitle", asText)
	; Long enough to read; when the player's voice plays, until it ends (12 s at most).
	Float readTime = 2.0 + StringUtil.GetLength(asText) * 0.06
	If readTime > 12.0
		readTime = 12.0
	EndIf
	Float shown = 0.0
	While shown < 12.0 && (shown < readTime || !SkyrimNetApi.IsPlayerTTSFinished())
		Utility.Wait(0.5)
		shown += 0.5
	EndWhile
	UI.Invoke("HUD Menu", "_root.HUDMovieBaseInstance.HideSubtitle")
EndFunction
