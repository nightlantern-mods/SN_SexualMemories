#pragma once

// Collects each OStim or SexLab scene while it runs (who, where, which animations and actions,
// who climaxed) and, when it ends, gathers the SkyrimNet events of the scene and the
// participants' memories, has the LLM write a first-person memory of the scene for everyone
// who took part (the player too), and stores it in SkyrimNet as their memory.
namespace SexualHistory
{
    // Resolves the SkyrimNet API. Call at kDataLoaded.
    void Init();

    // Registers the Papyrus functions of SN_SexualHistoryNative.
    bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm);

    // Drops scenes collected before a load or a new game.
    void Reset();
}
