# SN Sexual History 0.1.1

**Adult content.** A SkyrimNet add-on for OStim Standalone or SexLab (P+ / 1.6x) in Skyrim SE/AE.

Every OStim or SexLab scene is remembered. When a scene ends, SkyrimNet's AI writes a memory of
it for each character who took part, the player included, and stores it as that character's own
SkyrimNet memory. NPCs then recall it in later conversations like anything else they lived through.

Each memory is written in first person from its owner's point of view and covers:

- who took part, where and when;
- what happened in the scene: the animations played, the acts and who climaxed;
- what was said and done around it, from SkyrimNet's event history during the scene;
- how the owner felt, and how the others seemed to feel, drawing on each participant's earlier
  memories of the others and on their character profile.

The memory also gets an emotion, an importance and a type (relationship, joy, trauma or
experience), chosen by the AI. The importance is never below 0.8: such a scene is never a minor
memory, and the AI's value is raised to 0.8 when it is lower. Nothing to set up: there is no MCM.

## Requirements

| Mod | Notes |
|---|---|
| Skyrim Special Edition / Anniversary Edition | Built and tested on 1.6.1170. |
| [SKSE64](https://skse.silverlock.org/) | |
| [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) | |
| [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin) | Tested on 1.1.1. |
| [OStim Standalone](https://www.nexusmods.com/skyrimspecialedition/mods/98163) **or** [SexLab](https://www.loverslab.com/files/file/8255-sexlab-framework-se/) (P+ or 1.6x) | One is enough; with both installed, the scenes of both are recorded. |

## Installation

Install the archive with your mod manager (Mod Organizer 2 or Vortex) and enable
`SN_SexualHistory.esp`. It is ESL-flagged and needs no load-order slot, and the mod can be added
to an existing save.

## How it works

1. While a scene runs, the mod notes who is in it, where it happens, each animation (with its
   acts and tags) and every climax.
2. A few seconds after the scene ends, it reads from SkyrimNet the events recorded during the
   scene and, for each participant, their memories that bear most on the others.
3. It sends one request per participant to the LLM with the bundled prompt
   (`sn_sexual_history_memory`), and stores the answer with SkyrimNet as that participant's
   memory. A scene with two people makes two requests.

The memories are written in the language of the character profiles and the events, so a game
played in Japanese gets Japanese memories.

## Finding the memories in the dashboard

In SkyrimNet's dashboard (History > Memories, or a character's Memories tab), the memories made
by this mod have `[Sexual History]` after the emotion, under the importance: for example
`joyful [Sexual History]`. They are also tagged `sexual_history` and `sex`, which shows in the
memory's edit form.

## Changing the prompt

The prompt is a SkyrimNet prompt bundle, `nightlantern-mods.sn-sexual-history`. Copy
`prompts/sn_sexual_history_memory.prompt` into your SkyrimNet overlay to change how the memories
are written, as with any SkyrimNet prompt.

## Log

`SN_SexualHistory.log` in the SKSE log folder (`Documents\My Games\Skyrim Special Edition\SKSE`)
records each scene, how many events it found, and every memory stored, with its text.

## Notes and limits

- The memory is made only when the scene ends normally. A scene cut short by loading a save is
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
| `scripts/` | The Papyrus sources. `scripts/stubs/` holds compile-only headers for OStim, SexLab and SKSE; they are not shipped. |
| `plugin/` | `SN_SexualHistory.esp` as YAML (one quest that runs the scripts). |
| `skyrimnet-plugin/` | The SkyrimNet prompt bundle. |

You need [xmake](https://xmake.io/) 3.0 or later and Visual Studio 2022. Point `COMMONLIB_SSE_NG`
at a CommonLibSSE-NG 7.5.4 checkout, then run `xmake build`. The DLL and the prompt bundle are
laid out in `dist/`. Compile the scripts with the Creation Kit's Papyrus compiler, with
`scripts/stubs/` and the vanilla and SKSE sources on the import path.

## Files

- `SN_SexualHistory_0.1.1.zip`: the mod, ready for a mod manager, with `THIRD_PARTY_NOTICES.txt`
  and `COPYING.txt` (see License).

## License

The source in this repository is MIT licensed, see [LICENSE](LICENSE).

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
- The SkyrimNet API headers in `src/skyrimnet/`: MIT, Fateless.Ai (see the LICENSE there).
- nlohmann/json 3.12.0, spdlog 1.16.0, DirectXMath, DirectX Tool Kit and rapidcsv: MIT, or
  BSD-3-Clause for rapidcsv.
