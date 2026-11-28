# SN Sexual Memories 0.4.5

**Adult content.** A SkyrimNet add-on for OStim Standalone or SexLab (P+ / 1.6x) in Skyrim SE/AE.

> [!IMPORTANT]
> **This mod keeps adding features that make your world livelier and chattier.** Each of them
> sends extra requests to your LLM. If things get too busy, or you are watching your LLM costs,
> turn features off in the MCM. (The memories of the people in a scene are on by default: one
> request per participant when a scene ends.)

> [!WARNING]
> **The relationship map's model: check it before you make maps.** The map has its own model
> setting in SkyrimNet's dashboard: AI > Models, the `sn_sexual_memories_relmap` card. **When its
> model is left empty, the map uses SkyrimNet's `default` model**, the one your NPCs' dialogue uses (or
> its rotation of several models, with the built-in presets). A map asks that model once for each
> person on it (up to ten) and once more for the summary, with long requests, so with an
> expensive default model each map can cost noticeably. To keep costs down, enter a cheaper model
> there; to get fuller maps, a stronger one.

> [!NOTE]
> **Diary entries after scenes (off by default).** When it is on, everyone who took part in a scene,
> the player too, writes a private diary entry in their own voice, stored as their SkyrimNet diary
> entry. It does not retell the scene (the scene memory keeps that); it is what they never say
> aloud. Turn it on in the MCM ("Diary entries after scenes"). It is one more request to your LLM
> per person per scene.
> - **Its model is set in SkyrimNet's dashboard: AI > Models, `sn_sexual_memories_diary`** (this
>   mod's own card, not SkyrimNet's diary model). Left empty, it uses SkyrimNet's `default` model.
> - When SkyrimNet also writes its usual diary entries, the same scene may appear in both.
> - To try it out, we recommend setting an inexpensive model on that card.
> - An entry can fail to be written: the model may decline to write about a sexual scene or give
>   an answer that cannot be used, the request may fail, or SkyrimNet may refuse the entry (for
>   example while a save is still loading). Then that person gets no entry for that scene; the scene
>   memories are not affected. Failures are recorded in `SN_SexualHistory.log`.

Its files keep the name the mod first had, SN Sexual History: the plugin is `SN_SexualHistory.esp`
and the SKSE plugin `SN_SexualHistory.dll`, so saves and settings carry over.

Every OStim or SexLab scene is remembered and talked about, and the people around you have more
to say:

- **Memories of the scene.** When a scene ends, SkyrimNet's AI writes a memory of it for each
  character who took part, the player included, and stores it as that character's own SkyrimNet
  memory. NPCs then recall it in later conversations like anything else they lived through.
  Its model is set in SkyrimNet's dashboard: AI > Models, `sn_sexual_memories_memory` (the witness
  memory uses it too). Left empty, it uses SkyrimNet's `default` model.
- **A witness remembers too.** One bystander who saw the scene, picked at random among the NPCs
  nearby, gets a memory of what they saw and what they made of it. When nobody saw it, nobody does.
- **Lines during scenes.** Every so often an NPC in a running scene says a line about what is
  happening, written and voiced by SkyrimNet. With several NPCs, they take turns. In a forced
  scene, the one it is done to never sounds willing: at most they admit, with dismay, that their
  body reacts. When the conversation just before the scene shows they agreed to it, they play it
  as a willing partner instead, even in a dominating or rough scene.
- **Complaints about a partner.** Now and then an NPC near you who has a husband, wife or lover
  complains to you about them, but only while that partner is out of earshot.
- **Followers talk behind your back** (off by default). When two or more of your followers are
  near you, now and then one of them talks about you to another, thinking you cannot hear. You
  hear every word.
- **Your character speaks up** (off by default). Now and then your character says something to an
  NPC nearby, drawing on what just happened, on what your character remembers (about that person
  above all) and on how the two of you stand. The NPC answers, and the line is shown as a subtitle.
- **Diary entries after scenes** (off by default). After a scene, everyone who took part (you too) writes a
  private diary entry in their own voice. It does not retell the scene (the memory keeps that):
  it is what they never say aloud, how things have changed with the others over their history
  together, what they mean to do next, and how they see their life so far and where it is going,
  unflattering thoughts included. It is stored as their SkyrimNet diary entry.
- **Diary entries when a quest moves on** (off by default). Whenever a quest's journal changes (a
  quest started, an objective shown, completed or failed, or the quest completed), you and every follower near you
  write a diary entry about it in your own voices: what happened as each of you saw it and what
  you really think of it. It uses the same model card as the scene diaries.
- **Relationship map** (needs Magelight UI or Prisma UI). Press a key (Y by default) to open it,
  choose yourself or an NPC nearby, take anyone you do not want off the list, press Create, and
  SkyrimNet's AI writes what lies between them and up to ten people around you: what happened between them, in order, how each feels about the other, the key
  events, and how the others are tied to each other. It reads both sides' memories, their diaries
  and SkyrimNet's events, and writes only what those records say. Cards of people from earlier
  maps of the same person are kept below, as they were when made. A button remakes the map in
  English. With Phantasm installed, everyone's face is shown on the map.

Each participant's memory is written in first person and covers:

- what kind of encounter it was for them, judged by the AI from what happened before and during
  it, the place and their relationship: loving, casual, a paid job or other deal, a duty,
  reluctant or forced. Whether it was consensual is judged first from what was said just before
  the scene. Sex is not taken as affection;
- who took part, where and when;
- what happened in the scene: the animations played, the acts and who climaxed;
- what was said and done around it, from SkyrimNet's event history just before and during the
  scene (the lines spoken during the scene included);
- how the owner felt, and how the others seemed to feel, in keeping with the kind of encounter,
  their earlier memories of the others and their character profile.

The witness's memory says whom they saw, where, doing what, what they took it to be, and how they
took it, true to their
own character and their relationship with those involved. It holds only what an onlooker could
see or hear.

Every memory also gets an emotion, an importance and a type, chosen by the AI. The importance is
never below 0.8: such a scene is never a minor memory, and the AI's value is raised to 0.8 when it
is lower.

## Requirements

| Mod | Notes |
|---|---|
| Skyrim Special Edition / Anniversary Edition | Built and tested on 1.6.1170. |
| [SKSE64](https://skse.silverlock.org/) | |
| [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) | |
| [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin) | Tested on 1.1.1. |
| [OStim Standalone](https://www.nexusmods.com/skyrimspecialedition/mods/98163) **or** [SexLab](https://www.loverslab.com/files/file/8255-sexlab-framework-se/) (P+ or 1.6x) | One is enough; with both installed, the scenes of both are used. |
| [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604) | The settings menu. |
| [MCM Helper](https://www.nexusmods.com/skyrimspecialedition/mods/53000) | The settings menu. |
| OStimNet (optional) | When installed, the lines during OStim scenes use its description of the scene, its intent and its phase, and the memories its intent. |
| [Magelight UI](https://www.nexusmods.com/skyrimspecialedition/mods/193303) **or** [Prisma UI](https://www.nexusmods.com/skyrimspecialedition/mods/148718) (optional) | Only for the relationship map. Everything else works without them. |
| [Phantasm](https://www.nexusmods.com/skyrimspecialedition/mods/197470) 0.2.0 or later (optional) | The faces on the relationship map. Without it, the map shows plain circles. |

## Installation

Install the archive with your mod manager (Mod Organizer 2 or Vortex) and pick the language of
the settings menu in the installer: English or Japanese. The choice is independent of the game's
language. Enable `SN_SexualHistory.esp`; it is ESL-flagged and needs no load-order slot, and the
mod can be added to an existing save.

Installing by hand: copy the contents of `core/<language>/` from the archive (`en` or `ja`) into
the game's `Data` folder.

## MCM settings

| Setting | Default | |
|---|---|---|
| Those who took part remember the scene | On | Everyone who took part gets a memory of the scene. Off stops only these memories; the witness and the diary entries have their own switches. |
| A witness remembers too | On | One bystander who saw the scene also gets a memory. |
| Diary entries when a quest moves on | Off | You and the followers near you write a diary entry whenever a quest's journal changes. One more request per person each time. |
| Diary entries after scenes | Off | After a scene, everyone who took part (you too) writes a diary entry. One more request per person. |
| NPCs speak during scenes | On | The lines during scenes. |
| Time between checks (seconds) | 30 | How often running scenes are looked for. Each check lets one NPC speak; nothing is said while SkyrimNet is still speaking. |
| Also in your own scenes | On | Off: only scenes between NPCs get lines. |
| Hearing distance | 3000 | Only NPCs within this distance of you speak. |
| NPCs complain about their partner | On | The complaints. |
| Time between chances (seconds) | 600 | How often a chance for a complaint comes. |
| Who complains | Women and men | Or women only, or men only. |
| Followers gossip about you | Off | The followers' talk behind your back. |
| Time between chances (seconds) | 600 | How often a chance for it comes. |
| Speak to people nearby | Off | Your character speaks up. |
| Time between chances (seconds) | 900 | How often your character may speak up, from 10 seconds. Nothing is said in combat, in a menu, during a scene or while SkyrimNet is speaking. |
| Open the relationship map | Y | The key that opens the relationship map. |
| Memories read per person | 300 | How many of each person's memories are read, most important first, to find the ones that name the others. |
| Memories given per pair | 10 | How many memories naming the other are given to the AI for each pair, in each direction. |
| Latest events read | 150 | How many of the centre's latest SkyrimNet events are read. |
| Diary entries read per person | 30 | How many of each person's latest diary entries are read (0: none). |
| Length kept of each record (characters) | 400 | How much of each memory, event or diary passage the AI sees. |

The memories of the participants are on by default; they can be turned off on their own.

## How it works

1. While a scene runs, the mod notes who is in it, where it happens, each animation (with its
   acts and tags) and every climax.
2. At the start and at every change of animation or stage, until one is found, it looks for a
   witness: a random NPC within reach of the scene who is not in it, not the player, not a child,
   awake, and with a participant in sight.
3. A few seconds after the scene ends, it reads from SkyrimNet the events recorded just before
   and during the scene and, for each participant and the witness, their memories that bear most
   on the others (how they know each other, money or work between them).
4. It sends one request per participant to the LLM with the bundled prompt
   `sn_sexual_history_memory`, and one for the witness with `sn_sexual_history_witness`, and stores
   each answer with SkyrimNet as that character's memory. A scene with two people and a witness
   makes three requests. All of them go to the model on the `sn_sexual_memories_memory` card
   (AI > Models); left empty, SkyrimNet's `default` model.

The lines during scenes go through SkyrimNet's direct narration: the narration says what is
happening, and the speaker's own character, memories and the recent conversation shape the line.
A scene counts as forced when SexLab marks it as non-consensual or aggressive (it also says who is
on the receiving end), or for OStim when OStimNet's intent is aggressive or dom or the animation's
tags say so. The AI is told to judge first from the conversation just before the scene: when it
clearly shows the one on the receiving end agreed freely, they play it as consensual; when it
clearly shows it is against their will, as forced. Agreeing under threat, or when one could not
refuse (a captive, someone defeated), does not count as agreeing. Only when the conversation does
not settle it does the scene's own marking decide. This is the AI's judgement, so it is not
guaranteed.

A complaint comes from a random NPC within SkyrimNet's talking distance of you who has a
relationship at the Lover level (spouses and lovers) with someone other than you, when that
partner is not loaded or is farther than SkyrimNet's talking distance times its awareness
multiplier. SkyrimNet's own settings are only read, never changed. SkyrimNet has no whisper mode
for NPCs, so this distance rule is what keeps the partner from overhearing.

The talk behind your back needs two or more followers within SkyrimNet's talking distance of you
(your teammates, or members of the game's follower faction), none of them in combat, asleep or in
a scene. One of them, picked at random, talks to another about you; SkyrimNet is told that the
two believe you are out of earshot, though you hear it all. What they say comes from the
speaker's character, memories and feelings about you, so it can be fond teasing as well as real
resentment.

When your character speaks up, the line is written with the bundled prompt
`sn_sexual_history_player_talk`, spoken with your character's voice when SkyrimNet voices the
player, shown in the game's dialogue subtitle (when dialogue subtitles are on), and passed to the
NPC as something your character said, so they answer.

Opening the map or choosing someone only shows a map already made: nothing is sent to the AI until
you press Create (or Refresh). The map puts the chosen person in the middle and, around them, up
to ten people from the list on the left, nearest first (you too, when an NPC is chosen), whether or
not the records tie them together; the × next to a name leaves that person off the maps you make
until you close the map.
For each of them it sends one request with the bundled prompt `sn_sexual_memories_relmap`: what
each of the two remembers of the other (most important first), the passages of their latest
diaries that name the other, the centre's events that name them, what the others in the circle
remember of each other, who travels with you, and the relationships the game records. A last
request, `sn_sexual_memories_relmap_summary`, sums the circle up. The AI is told to write only what
those records say, never to take events from the character profiles or Skyrim's usual
storylines, and never to say that two people have nothing to do with each other: the records read
are only part of what happened. The map is saved for each save and shown again until you press
Refresh. When a map is made again, the people of the earlier map who are not on the new one keep
their cards under "Earlier records", folded and dated, with a name filter over all cards. Its AI model is set in SkyrimNet's dashboard: AI > Models, `sn_sexual_memories_relmap`.

The memories are written in the language of the character profiles and the events, so a game
played in Japanese gets Japanese memories. The relationship map is written in the language of the
memories and events, or in English when you press the English button.

## Finding the memories in the dashboard

In SkyrimNet's dashboard (History > Memories, or a character's Memories tab), the memories made
by this mod have a mark after the emotion, under the importance:

- `[Sexual Memories]` for a participant's memory, for example `joyful [Sexual Memories]`;
- `[Sexual Memories / Witness]` for the witness's memory.

They are also tagged `sexual_history` and `sex` (and `witness`), which shows in the memory's edit
form. The diary entries, when that option is on, are tagged `sexual_history` and `sex` too.

## Changing the prompts

The prompts are a SkyrimNet prompt bundle, `nightlantern-mods.sn-sexual-history`. Copy
`prompts/sn_sexual_history_memory.prompt`, `prompts/sn_sexual_history_witness.prompt` or
`prompts/sn_sexual_history_player_talk.prompt` (or `prompts/sn_sexual_history_diary.prompt` for
the diary entries) into your SkyrimNet overlay to change how the
memories or your character's lines are written, as with any SkyrimNet prompt; the relationship
map's are `prompts/sn_sexual_memories_relmap.prompt` and
`prompts/sn_sexual_memories_relmap_summary.prompt`.

## Log

`SN_SexualHistory.log` in the SKSE log folder (`Documents\My Games\Skyrim Special Edition\SKSE`)
records each scene, how many events it found, who witnessed it, every memory stored with its text,
every line your character says, and every relationship map made.

## Notes and limits

- The memories are made only when the scene ends normally. A scene cut short by loading a save is
  not recorded.
- SexLab does not say who climaxed, so for SexLab scenes only the number of climaxes is known.
- Complaints only see relationships the game has a record for; a relationship another mod sets up
  during play without one may be missed.
- The relationship maps are saved in `Data/SKSE/Plugins/SN_SexualHistory/relmaps/`, one folder per
  save; Phantasm's faces are written next to the map's page. Both can be deleted at any time.
- On SexLab P+ the mod never reads `thread.Animation`, which makes P+ rewrite all its animation
  aliases on every read; it uses the active scene's ID instead.

## Uninstalling

Disable or remove the mod. The memories already made stay in SkyrimNet; delete them in the
dashboard if you want them gone.

## Building

| Folder | |
|---|---|
| `src/` | The SKSE plugin (C++23, [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng)), with the API headers of Prisma UI, Magelight UI and Phantasm. |
| `view/` | The relationship map's page, shown by Magelight UI or Prisma UI. |
| `scripts/` | The Papyrus sources. `scripts/stubs/` holds compile-only headers, not shipped: stubs written for this repository that declare only the functions used from OStim, OStimNet, SexLab, SKSE and MCM Helper, and SkyrimNet's MIT-licensed Papyrus API `SkyrimNetApi.psc` (license in `scripts/stubs/LICENSE-SkyrimNetApi.txt`). |
| `plugin/` | `SN_SexualHistory.esp` as YAML: one quest each for the memories, the lines and the MCM, the complaints, your character speaking up, and the followers' talk behind your back. |
| `mcm/` | The MCM config (`MCM/Config/SN_SexualHistory/`). |
| `skyrimnet-plugin/` | The SkyrimNet prompt bundle. |
| `tools/` | `make_strings.py` writes the MCM and relationship map texts in English or Japanese; `build_fomod.ps1` builds and checks the release installer. |
| `translations/` | The Japanese MCM and relationship map texts, as written by `make_strings.py`. |

You need [xmake](https://xmake.io/) 3.0 or later and Visual Studio 2022. Point `COMMONLIB_SSE_NG`
at a CommonLibSSE-NG 7.5.4 checkout, then run `xmake build`. The DLL, the prompt bundle and the
relationship map's page are laid out in `dist/`. Compile the scripts with the Creation Kit's Papyrus compiler, with
`scripts/stubs/` and the vanilla and SKSE sources on the import path.

## Files

- `SN_SexualHistory_0.4.2.zip`: the mod as a FOMOD installer, with the settings menu in English
  or Japanese, and with `THIRD_PARTY_NOTICES.txt` and `COPYING.txt` (see License). Without an
  installer, use `core/<language>/` from the archive.

## License

The source in this repository is MIT licensed, see [LICENSE](LICENSE), except the two prompts
below (`sn_sexual_history_player_talk.prompt` and the relationship map's two prompts are the mod's
own and MIT licensed).

The prompts `sn_sexual_history_memory.prompt` and `sn_sexual_history_witness.prompt`
(`skyrimnet-plugin/nightlantern-mods.sn-sexual-history/prompts/`) are partly based on SkyrimNet's
own `prompts/memory/generate_memory.prompt`. They are modified and distributed under section 3
(Content packs) of the [SkyrimNet Game Plugin License](https://github.com/MinLL/SkyrimNet-GamePlugin/blob/main/LICENSE),
for use with SkyrimNet only, not under the MIT License.
Copyright (c) 2025-2026 Fateless.Ai. All rights reserved.

The built plugin, `SKSE/Plugins/SN_SexualHistory.dll`, is distributed under the
[GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.txt): it statically links
[CommonLibSSE NG](https://github.com/alandtse/CommonLibSSE-NG) 7.5.4 (branch `ng`), which is
GPL-3.0-or-later with a Modding Exception and a GPL-3.0 Linking Exception, so the DLL is a
combined work under its terms. Its corresponding source is this repository together with
CommonLibSSE NG [v7.5.4](https://github.com/alandtse/CommonLibSSE-NG/tree/v7.5.4) and the
libraries pinned in `xmake-requires.lock`. The release archive carries the GPL text
(`COPYING.txt`) and the third-party notices.

Third-party code, with the full license texts in [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt):

- CommonLibSSE NG: GPL-3.0-or-later with exceptions; originally based on MIT-licensed code.
- The SkyrimNet API headers in `src/skyrimnet/` and the SkyrimNet Papyrus API in
  `scripts/stubs/SkyrimNetApi.psc`: MIT, Fateless.Ai (see `src/skyrimnet/LICENSE` and
  `scripts/stubs/LICENSE-SkyrimNetApi.txt`).
- The API headers of Prisma UI (`src/PrismaUI_API.h`, no license text; copying it into a project
  is invited), Magelight UI (`src/MagelightUI_API.h`) and Phantasm (`src/Phantasm_API.h`): MIT,
  Severause for the last two. The mods themselves are not included.
- The two prompts above: SkyrimNet Game Plugin License, section 3, Fateless.Ai.
- nlohmann/json 3.12.0, spdlog 1.16.0, DirectXMath, DirectX Tool Kit and rapidcsv: MIT, or
  BSD-3-Clause for rapidcsv.

SN Sexual Memories is an unofficial add-on that works with SkyrimNet; it is not made or endorsed by
Fateless.Ai. The Elder Scrolls V: Skyrim is a trademark of ZeniMax Media Inc. This mod is not
affiliated with or endorsed by Bethesda Softworks or ZeniMax Media.
