#include "sexual_history.h"

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        SexualHistory::Init();
        break;
    case SKSE::MessagingInterface::kPreLoadGame:
        SexualHistory::Reset();
        break;
    case SKSE::MessagingInterface::kNewGame:
        SexualHistory::Reset();
        SexualHistory::AfterLoad();
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        SexualHistory::AfterLoad();
        break;
    }
}

extern "C" DLLEXPORT bool SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    REL::Module::reset();

    auto* messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
    if (!messaging) {
        logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
        return false;
    }

    SKSE::Init(a_skse);
    logger::info("SN_SexualHistory loaded");

    SKSE::GetPapyrusInterface()->Register(SexualHistory::RegisterPapyrus);
    messaging->RegisterListener("SKSE", SKSEMessageHandler);
    return true;
}
