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

; True while the scene has no witness yet (and is being recorded).
Bool Function NeedsWitness(Int aiKey) Global Native

; A bystander who saw the scene. Only the first one given counts; they get a memory of what they saw.
Function Witness(Int aiKey, Actor akWitness) Global Native

; The actor's spouses and lovers (relationships at the Lover level, the player excepted), as
; "Sigrid (wife)" or "Sigrid (wife) and Hod (lover)"; "" when there are none.
String Function PartnerDescription(Actor akActor) Global Native

; Those spouses and lovers who are loaded near the player right now.
Actor[] Function LoadedPartners(Actor akActor) Global Native

; True while the actor is in an OStim or SexLab scene this plugin is following.
Bool Function InScene(Actor akActor) Global Native

; Has the LLM write a line the player says to akTarget (from recent events, the player's memories
; and their relationship, aiRelationshipRank being akTarget's rank to the player); the player then
; says it and akTarget answers. False when a line is already being written.
Bool Function PlayerTalk(Actor akTarget, Int aiRelationshipRank, String asLocation) Global Native

; The scene ended: the plugin gathers the SkyrimNet events and memories of the participants, has
; the LLM write a memory for everyone who took part (the player too), and stores it in SkyrimNet.
Function SceneEnded(Int aiKey) Global Native
