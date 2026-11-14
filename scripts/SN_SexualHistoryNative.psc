Scriptname SN_SexualHistoryNative Hidden
{Functions implemented by SKSE/Plugins/SN_SexualHistory.dll. A scene key is an OStim thread ID,
or 100000 + a SexLab thread ID.}

; A scene started: its framework ("OStim" or "SexLab"), who takes part and where it happens.
Function SceneStarted(Int aiKey, String asFramework, Actor[] akActors, String asLocation) Global Native

; What the scene is doing now (an animation and its actions or tags). akActors joins anyone who
; came in after the start. A detail equal to the last one is ignored.
Function SceneDetail(Int aiKey, Actor[] akActors, String asDetail) Global Native

; Somebody climaxed. akActor is None when the framework does not say who.
Function Climax(Int aiKey, Actor akActor) Global Native

; The scene ended: the plugin gathers the SkyrimNet events and memories of the participants, has
; the LLM write a memory for everyone who took part (the player too), and stores it in SkyrimNet.
Function SceneEnded(Int aiKey) Global Native
