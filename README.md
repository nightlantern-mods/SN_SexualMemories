# SN Sexual History 0.2.0

**Adult content.** A SkyrimNet add-on for OStim Standalone or SexLab (P+ / 1.6x) in Skyrim SE/AE.

Every OStim or SexLab scene is remembered and talked about:

- **Memories of the scene.** When a scene ends, SkyrimNet's AI writes a memory of it for each
  character who took part, the player included, and stores it as that character's own SkyrimNet
  memory. NPCs then recall it in later conversations like anything else they lived through.
- **A witness remembers too.** One bystander who saw the scene, picked at random among the NPCs
  nearby, gets a memory of what they saw and what they made of it. When nobody saw it, nobody does.
- **Lines during scenes.** Every so often an NPC in a running scene says a line about what is
  happening, written and voiced by SkyrimNet. With several NPCs, they take turns.

Each participant's memory is written in first person and covers:

- who took part, where and when;
- what happened in the scene: the animations played, the acts and who climaxed;
- what was said and done around it, from SkyrimNet's event history during the scene (the lines
  spoken during the scene included);
- how the owner felt, and how the others seemed to feel, drawing on each participant's earlier
  memories of the others and on their character profile.

The witness's memory says whom they saw, where, doing what, and how they took it, true to their
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
| OStimNet (optional) | When installed, the lines during OStim scenes use its description of the scene, its intent and its phase. |

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
| A witness remembers too | On | One bystander who saw the scene also gets a memory. |
| NPCs speak during scenes | On | The lines during scenes. |
| Time between checks (seconds) | 30 | How often running scenes are looked for. Each check lets one NPC speak; nothing is said while SkyrimNet is still speaking. |
| Also in your own scenes | On | Off: only scenes between NPCs get lines. |
| Hearing distance | 3000 | Only NPCs within this distance of you speak. |

The memories of the participants are always made.

## How it works

1. While a scene runs, the mod notes who is in it, where it happens, each animation (with its
   acts and tags) and every climax.
2. At the start and at every change of animation or stage, until one is found, it looks for a
   witness: a random NPC within reach of the scene who is not in it, not the player, not a child,
   awake, and with a participant in sight.
3. A few seconds after the scene ends, it reads from SkyrimNet the events recorded during the
   scene and, for each participant and the witness, their memories that bear most on the others.
4. It sends one request per participant to the LLM with the bundled prompt
   `sn_sexual_history_memory`, and one for the witness with `sn_sexual_history_witness`, and stores
   each answer with SkyrimNet as that character's memory. A scene with two people and a witness
   makes three requests.

The lines during scenes go through SkyrimNet's direct narration: the narration says what is
happening, and the speaker's own character, memories and the recent conversation shape the line.

The memories are written in the language of the character profiles and the events, so a game
played in Japanese gets Japanese memories.

## Finding the memories in the dashboard

In SkyrimNet's dashboard (History > Memories, or a character's Memories tab), the memories made
by this mod have a mark after the emotion, under the importance:

- `[Sexual History]` for a participant's memory, for example `joyful [Sexual History]`;
- `[Sexual History / Witness]` for the witness's memory.

They are also tagged `sexual_history` and `sex` (and `witness`), which shows in the memory's edit
form.

## Changing the prompts

The prompts are a SkyrimNet prompt bundle, `nightlantern-mods.sn-sexual-history`. Copy
`prompts/sn_sexual_history_memory.prompt` or `prompts/sn_sexual_history_witness.prompt` into your
SkyrimNet overlay to change how the memories are written, as with any SkyrimNet prompt.

## Log

`SN_SexualHistory.log` in the SKSE log folder (`Documents\My Games\Skyrim Special Edition\SKSE`)
records each scene, how many events it found, who witnessed it, and every memory stored, with its
text.

## Notes and limits

- The memories are made only when the scene ends normally. A scene cut short by loading a save is
  not recorded.
- SexLab does not say who climaxed, so for SexLab scenes only the number of climaxes is known.
- On SexLab P+ the mod never reads `thread.Animation`, which makes P+ rewrite all its animation
  aliases on every read; it uses the active scene's ID instead.

## Uninstalling

Disable or remove the mod. The memories already made stay in SkyrimNet; delete them in the
dashboard if you want them gone.

## Building

| Folder | |
|---|---|
| `src/` | The SKSE plugin (C++23, [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng)). |
| `scripts/` | The Papyrus sources. `scripts/stubs/` holds compile-only headers, not shipped: stubs written for this repository that declare only the functions used from OStim, OStimNet, SexLab, SKSE and MCM Helper, and SkyrimNet's MIT-licensed Papyrus API `SkyrimNetApi.psc` (license in `scripts/stubs/LICENSE-SkyrimNetApi.txt`). |
| `plugin/` | `SN_SexualHistory.esp` as YAML: one quest for the memories, one for the lines and the MCM. |
| `mcm/` | The MCM config (`MCM/Config/SN_SexualHistory/`). |
| `skyrimnet-plugin/` | The SkyrimNet prompt bundle. |
| `tools/` | `make_strings.py` writes the MCM texts in English or Japanese; `build_fomod.ps1` builds and checks the release installer. |
| `translations/` | The Japanese MCM texts, as written by `make_strings.py`. |

You need [xmake](https://xmake.io/) 3.0 or later and Visual Studio 2022. Point `COMMONLIB_SSE_NG`
at a CommonLibSSE-NG 7.5.4 checkout, then run `xmake build`. The DLL and the prompt bundle are
laid out in `dist/`. Compile the scripts with the Creation Kit's Papyrus compiler, with
`scripts/stubs/` and the vanilla and SKSE sources on the import path.

## Files

- `SN_SexualHistory_0.2.0.zip`: the mod as a FOMOD installer, with the settings menu in English
  or Japanese, and with `THIRD_PARTY_NOTICES.txt` and `COPYING.txt` (see License). Without an
  installer, use `core/<language>/` from the archive.

## License

The source in this repository is MIT licensed, see [LICENSE](LICENSE), except the two prompts
below.

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
- The two prompts above: SkyrimNet Game Plugin License, section 3, Fateless.Ai.
- nlohmann/json 3.12.0, spdlog 1.16.0, DirectXMath, DirectX Tool Kit and rapidcsv: MIT, or
  BSD-3-Clause for rapidcsv.

SN Sexual History is an unofficial add-on that works with SkyrimNet; it is not made or endorsed by
Fateless.Ai. The Elder Scrolls V: Skyrim is a trademark of ZeniMax Media Inc. This mod is not
affiliated with or endorsed by Bethesda Softworks or ZeniMax Media.
