#include "sexual_history.h"

#include "phantasm_faces.h"
#include "skyrimnet/PublicAPI.h"
#include "ui_host.h"

#include <chrono>
#include <format>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <thread>

namespace SexualHistory
{
    namespace
    {
        using json = nlohmann::json;

        constexpr const char* kScriptName = "SN_SexualHistoryNative";
        constexpr const char* kPromptName = "sn_sexual_history_memory";
        constexpr const char* kWitnessPromptName = "sn_sexual_history_witness";
        constexpr RE::FormID  kPlayerID = 0x14;
        constexpr const char* kDashboardMarker = "[Sexual Memories]";
        constexpr const char* kWitnessMarker = "[Sexual Memories / Witness]";

        constexpr std::size_t kMaxDetails = 40;     // scene details kept per scene
        constexpr std::size_t kMaxEvents = 80;      // SkyrimNet events passed to the LLM
        constexpr std::size_t kMaxEventText = 400;  // characters of one event's text
        constexpr int         kMemoriesPerActor = 6;
        constexpr int         kFallbackEvents = 30;  // per actor, when the scene's first event is unknown
        constexpr std::size_t kMaxEventsBefore = 30;  // events from before the scene: how it came about
        // A scene is never a minor memory: the AI's importance is raised to this when it is lower,
        // and an answer without one gets it.
        constexpr float       kMinImportance = 0.8f;

        constexpr std::array kMemoryTypes{ "EXPERIENCE", "RELATIONSHIP", "KNOWLEDGE", "LOCATION", "SKILL", "TRAUMA", "JOY" };

        struct Participant
        {
            RE::FormID  formID = 0;
            std::string name;
        };

        struct Scene
        {
            uint64_t                 token = 0;
            std::string              framework;
            std::vector<Participant> actors;
            std::string              location;
            std::string              gameDate;
            std::vector<std::string> details;   // in the order they played, without repeats in a row
            std::vector<std::string> climaxes;  // a name per climax, "someone" when unknown
            int64_t                  firstEventId = 0;  // events above this id happened during the scene
            std::optional<Participant> witness;         // one bystander who saw it, when there was one
            std::chrono::steady_clock::time_point started;
        };

        std::mutex                g_lock;
        std::map<int32_t, Scene>  g_scenes;  // by scene key
        std::atomic<uint64_t>     g_nextToken = 1;

        std::string ActorName(RE::Actor* a_actor)
        {
            const char* name = a_actor ? a_actor->GetDisplayFullName() : nullptr;
            return name ? name : "";
        }

        std::string GameDateString()
        {
            auto* calendar = RE::Calendar::GetSingleton();
            if (!calendar) {
                return "";
            }
            char buffer[128]{};
            calendar->GetTimeDateString(buffer, sizeof(buffer), true);
            return buffer;
        }

        void AddActors(Scene& a_scene, const std::vector<RE::Actor*>& a_actors)
        {
            for (auto* actor : a_actors) {
                if (!actor) {
                    continue;
                }
                const auto id = actor->GetFormID();
                if (std::ranges::none_of(a_scene.actors, [id](const auto& p) { return p.formID == id; })) {
                    a_scene.actors.push_back({ id, ActorName(actor) });
                }
            }
        }

        std::string Dump(const json& a_json)
        {
            return a_json.dump(-1, ' ', false, json::error_handler_t::replace);
        }

        json ParseArray(const std::string& a_text)
        {
            auto parsed = json::parse(a_text, nullptr, false);
            return parsed.is_array() ? parsed : json::array();
        }

        // Cuts a UTF-8 string to at most a_max bytes without splitting a character.
        std::string Truncate(std::string a_text, std::size_t a_max)
        {
            if (a_text.size() <= a_max) {
                return a_text;
            }
            std::size_t cut = a_max;
            while (cut > 0 && (static_cast<unsigned char>(a_text[cut]) & 0xC0) == 0x80) {
                --cut;
            }
            a_text.resize(cut);
            return a_text + "...";
        }

        // ---- SkyrimNet queries (worker threads only) ------------------------------------------

        int64_t LatestEventId()
        {
            if (!PublicGetRecentEvents || !PublicIsMemorySystemReady || !PublicIsMemorySystemReady()) {
                return 0;
            }
            const auto events = ParseArray(PublicGetRecentEvents(0, 1, ""));
            if (events.empty() || !events[0].contains("id") || !events[0]["id"].is_number()) {
                return 0;
            }
            return events[0]["id"].get<int64_t>();
        }

        // "Lydia -> Farengar (dialogue): I'm sworn to carry your burdens."
        std::string EventLine(const json& a_event, std::size_t a_maxBytes = kMaxEventText)
        {
            std::string text;
            if (const auto data = a_event.find("data"); data != a_event.end()) {
                if (data->is_string()) {
                    text = data->get<std::string>();
                } else if (data->is_object()) {
                    for (const char* key : { "dialogue", "text", "content", "description", "message" }) {
                        if (data->contains(key) && (*data)[key].is_string()) {
                            text = (*data)[key].get<std::string>();
                            break;
                        }
                    }
                    if (text.empty()) {
                        text = Dump(*data);
                    }
                }
            }
            const auto field = [&](const char* a_key) {
                const auto it = a_event.find(a_key);
                return it != a_event.end() && it->is_string() ? it->get<std::string>() : std::string();
            };
            std::string line = field("originatingActorName");
            if (const auto target = field("targetActorName"); !target.empty()) {
                line += (line.empty() ? "" : " ") + std::string("-> ") + target;
            }
            const auto type = field("type");
            line += (line.empty() ? "(" : " (") + type + "): " + Truncate(text, a_maxBytes);
            return line;
        }

        struct SceneEventLines
        {
            std::vector<std::string> before;  // the last events before the scene: how it came about
            std::vector<std::string> during;  // what was recorded while it ran
        };

        // What SkyrimNet recorded for any of the scene's participants just before the scene and
        // while it ran, oldest first. When the scene's first event is unknown, the recent events
        // all count as "during".
        SceneEventLines SceneEvents(const Scene& a_scene)
        {
            SceneEventLines lines;
            if (!PublicGetRecentEvents) {
                return lines;
            }
            std::map<int64_t, json> byId;
            for (const auto& actor : a_scene.actors) {
                const auto events = ParseArray(PublicGetRecentEvents(actor.formID, a_scene.firstEventId > 0 ? 150 : kFallbackEvents, ""));
                for (const auto& event : events) {
                    if (event.is_object() && event.contains("id") && event["id"].is_number()) {
                        byId.emplace(event["id"].get<int64_t>(), event);
                    }
                }
            }
            for (const auto& [id, event] : byId) {
                if (a_scene.firstEventId == 0 || id > a_scene.firstEventId) {
                    lines.during.push_back(EventLine(event));
                } else {
                    lines.before.push_back(EventLine(event));
                }
            }
            if (lines.during.size() > kMaxEvents) {
                lines.during.erase(lines.during.begin(), lines.during.end() - kMaxEvents);
            }
            if (lines.before.size() > kMaxEventsBefore) {
                lines.before.erase(lines.before.begin(), lines.before.end() - kMaxEventsBefore);
            }
            return lines;
        }

        // The memories of an actor that bear most on the others in the scene.
        std::vector<std::string> RelevantMemories(const Scene& a_scene, const Participant& a_actor)
        {
            if (!PublicGetMemoriesForActor) {
                return {};
            }
            std::string query;
            for (const auto& other : a_scene.actors) {
                if (other.formID != a_actor.formID) {
                    query += (query.empty() ? "" : ", ") + other.name;
                }
            }
            // Neutral on purpose: what they are to each other, not only tender moments.
            query = "my relationship and dealings with " + query + ": how we know each other, what we owe each other, money or work between us";
            std::vector<std::string> texts;
            for (const auto& memory : ParseArray(PublicGetMemoriesForActor(a_actor.formID, kMemoriesPerActor, query.c_str()))) {
                if (memory.is_object() && memory.contains("text") && memory["text"].is_string()) {
                    texts.push_back(Truncate(memory["text"].get<std::string>(), kMaxEventText));
                }
            }
            return texts;
        }

        json BuildContext(const Scene& a_scene, const Participant& a_owner, const SceneEventLines& a_events,
            const std::map<RE::FormID, std::vector<std::string>>& a_memories)
        {
            json participants = json::array();
            for (const auto& actor : a_scene.actors) {
                const auto it = a_memories.find(actor.formID);
                participants.push_back({
                    { "name", actor.name },
                    { "form_id", actor.formID },
                    { "is_player", actor.formID == kPlayerID },
                    { "is_owner", actor.formID == a_owner.formID },
                    { "memories", it != a_memories.end() ? json(it->second) : json::array() },
                });
            }
            const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
                std::chrono::steady_clock::now() - a_scene.started).count();
            return {
                { "owner_name", a_owner.name },
                { "owner_form_id", a_owner.formID },
                { "owner_memories", a_memories.contains(a_owner.formID) ? json(a_memories.at(a_owner.formID)) : json::array() },
                { "framework", a_scene.framework },
                { "participants", participants },
                { "location", a_scene.location },
                { "game_date", a_scene.gameDate },
                { "scene_details", a_scene.details },
                { "climaxes", a_scene.climaxes },
                { "events_before", a_events.before },
                { "events", a_events.during },
                { "real_minutes", minutes },
            };
        }

        // ---- Storing the answer --------------------------------------------------------------

        struct Memory
        {
            std::string content;
            std::string emotion;
            std::string type = "RELATIONSHIP";
            float       importance = kMinImportance;
            json        tags = json::array();
        };

        std::string Trim(const std::string& a_text)
        {
            const auto first = a_text.find_first_not_of(" \n\r\t");
            const auto last = a_text.find_last_not_of(" \n\r\t");
            return first == std::string::npos ? "" : a_text.substr(first, last - first + 1);
        }

        // The JSON object of the answer; a plain-text answer becomes the content as it is.
        Memory ParseAnswer(const std::string& a_response)
        {
            Memory memory;
            const auto open = a_response.find('{');
            const auto close = a_response.rfind('}');
            if (open != std::string::npos && close != std::string::npos && close > open) {
                const auto answer = json::parse(a_response.substr(open, close - open + 1), nullptr, false);
                if (answer.is_object() && answer.contains("content") && answer["content"].is_string()) {
                    memory.content = Trim(answer["content"].get<std::string>());
                    if (answer.contains("emotion") && answer["emotion"].is_string()) {
                        memory.emotion = answer["emotion"].get<std::string>();
                    }
                    if (answer.contains("importance_score") && answer["importance_score"].is_number()) {
                        memory.importance = std::clamp(answer["importance_score"].get<float>(), kMinImportance, 1.0f);
                    }
                    if (answer.contains("type") && answer["type"].is_string()) {
                        auto type = answer["type"].get<std::string>();
                        std::ranges::transform(type, type.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                        if (std::ranges::find(kMemoryTypes, type) != kMemoryTypes.end()) {
                            memory.type = type;
                        }
                    }
                    if (answer.contains("tags") && answer["tags"].is_array()) {
                        for (const auto& tag : answer["tags"]) {
                            if (tag.is_string()) {
                                memory.tags.push_back(tag);
                            }
                        }
                    }
                    return memory;
                }
            }
            auto text = a_response;
            for (const char* fence : { "```json", "```" }) {
                for (auto at = text.find(fence); at != std::string::npos; at = text.find(fence)) {
                    text.erase(at, std::strlen(fence));
                }
            }
            memory.content = Trim(text);
            return memory;
        }

        void StoreMemory(const Scene& a_scene, const Participant& a_owner, const std::string& a_response, bool a_witness)
        {
            if (!PublicAddMemory) {
                return;
            }
            auto memory = ParseAnswer(a_response);
            if (memory.content.empty()) {
                logger::error("Empty memory for {}", a_owner.name);
                return;
            }
            memory.tags.push_back("sexual_history");
            memory.tags.push_back("sex");
            if (a_witness) {
                memory.tags.push_back("witness");
            }
            // The dashboard's memory list shows the emotion under the importance but not the tags,
            // so the marker goes there: "joyful [Sexual Memories]".
            const std::string marker = a_witness ? kWitnessMarker : kDashboardMarker;
            memory.emotion = memory.emotion.empty() ? marker : memory.emotion + " " + marker;
            json related = json::array();
            for (const auto& actor : a_scene.actors) {
                if (actor.formID != a_owner.formID) {
                    related.push_back(actor.formID);
                }
            }
            const auto id = PublicAddMemory(a_owner.formID, memory.content.c_str(), memory.importance, memory.type.c_str(),
                memory.emotion.c_str(), a_scene.location.c_str(), Dump(memory.tags).c_str(), Dump(related).c_str());
            if (id > 0) {
                logger::info("Memory {} for {} ({:08X}), {} {:.2f} {}: {}", id, a_owner.name, a_owner.formID, memory.type,
                    memory.importance, memory.emotion, memory.content);
            } else {
                logger::error("SkyrimNet refused the memory for {} ({:08X})", a_owner.name, a_owner.formID);
            }
        }

        // Runs on its own thread once a scene has ended.
        void Process(std::shared_ptr<const Scene> a_scene)
        {
            // Lets the last lines of the scene reach SkyrimNet's history first.
            std::this_thread::sleep_for(4s);
            if (!PublicIsMemorySystemReady || !PublicIsMemorySystemReady()) {
                logger::warn("SkyrimNet memory system not ready; scene not recorded");
                return;
            }
            for (int i = 0; PublicGetTimelineState && PublicGetTimelineState() == 1 && i < 60; ++i) {
                std::this_thread::sleep_for(1s);
            }
            const auto events = SceneEvents(*a_scene);
            std::map<RE::FormID, std::vector<std::string>> memories;
            for (const auto& actor : a_scene->actors) {
                memories[actor.formID] = RelevantMemories(*a_scene, actor);
            }
            // A witness who later joined the scene is a participant, not a witness.
            std::optional<Participant> witness = a_scene->witness;
            if (witness && std::ranges::any_of(a_scene->actors, [&](const auto& p) { return p.formID == witness->formID; })) {
                witness.reset();
            }
            if (witness) {
                memories[witness->formID] = RelevantMemories(*a_scene, *witness);
            }
            logger::info("Scene ({}) of {} actor(s): {} event(s) before, {} during, {} detail(s), {} climax(es), witness: {}", a_scene->framework,
                a_scene->actors.size(), events.before.size(), events.during.size(), a_scene->details.size(), a_scene->climaxes.size(),
                witness ? witness->name : std::string("none"));

            // Everyone who took part gets a memory, the player too: SkyrimNet reads the player's
            // memories when it writes the player's lines and thoughts.
            for (const auto& owner : a_scene->actors) {
                const auto context = Dump(BuildContext(*a_scene, owner, events, memories));
                const bool queued = PublicSendCustomPromptToLLM(kPromptName, "", context.c_str(),
                    [a_scene, owner](const char* a_response, int a_success) {
                        const std::string response = a_response ? a_response : "";
                        if (a_success == 1) {
                            StoreMemory(*a_scene, owner, response, false);
                        } else {
                            logger::error("Memory for {} failed: {}", owner.name, response);
                        }
                    });
                if (!queued) {
                    logger::error("Could not queue the memory for {}", owner.name);
                }
            }

            // The bystander's memory of what they saw, with a prompt of its own.
            if (witness) {
                const auto context = Dump(BuildContext(*a_scene, *witness, events, memories));
                const auto owner = *witness;
                const bool queued = PublicSendCustomPromptToLLM(kWitnessPromptName, "", context.c_str(),
                    [a_scene, owner](const char* a_response, int a_success) {
                        const std::string response = a_response ? a_response : "";
                        if (a_success == 1) {
                            StoreMemory(*a_scene, owner, response, true);
                        } else {
                            logger::error("Witness memory for {} failed: {}", owner.name, response);
                        }
                    });
                if (!queued) {
                    logger::error("Could not queue the witness memory for {}", owner.name);
                }
            }
        }

        // ---- Papyrus ------------------------------------------------------------------------

        void SceneStarted(RE::StaticFunctionTag*, int32_t a_key, RE::BSFixedString a_framework,
            std::vector<RE::Actor*> a_actors, RE::BSFixedString a_location)
        {
            Scene scene;
            scene.token = g_nextToken++;
            scene.framework = a_framework.c_str();
            AddActors(scene, a_actors);
            scene.location = a_location.c_str();
            scene.gameDate = GameDateString();
            scene.started = std::chrono::steady_clock::now();
            const auto token = scene.token;
            logger::info("Scene {} ({}) started with {} actor(s)", a_key, scene.framework, scene.actors.size());
            {
                std::scoped_lock lock(g_lock);
                g_scenes[a_key] = std::move(scene);
            }
            // The database is read off the Papyrus thread.
            std::thread([a_key, token] {
                const auto id = LatestEventId();
                std::scoped_lock lock(g_lock);
                const auto it = g_scenes.find(a_key);
                if (it != g_scenes.end() && it->second.token == token) {
                    it->second.firstEventId = id;
                }
            }).detach();
        }

        void SceneDetail(RE::StaticFunctionTag*, int32_t a_key, std::vector<RE::Actor*> a_actors, RE::BSFixedString a_detail)
        {
            std::scoped_lock lock(g_lock);
            const auto it = g_scenes.find(a_key);
            if (it == g_scenes.end()) {
                return;
            }
            auto& scene = it->second;
            AddActors(scene, a_actors);
            const std::string detail = a_detail.c_str();
            if (!detail.empty() && (scene.details.empty() || scene.details.back() != detail) && scene.details.size() < kMaxDetails) {
                scene.details.push_back(detail);
            }
        }

        void Climax(RE::StaticFunctionTag*, int32_t a_key, RE::Actor* a_actor)
        {
            std::scoped_lock lock(g_lock);
            const auto it = g_scenes.find(a_key);
            if (it != g_scenes.end() && it->second.climaxes.size() < kMaxDetails) {
                const auto name = ActorName(a_actor);
                it->second.climaxes.push_back(name.empty() ? "someone" : name);
            }
        }

        bool NeedsWitness(RE::StaticFunctionTag*, int32_t a_key)
        {
            std::scoped_lock lock(g_lock);
            const auto it = g_scenes.find(a_key);
            return it != g_scenes.end() && !it->second.witness;
        }

        void Witness(RE::StaticFunctionTag*, int32_t a_key, RE::Actor* a_witness)
        {
            if (!a_witness || a_witness->GetFormID() == kPlayerID) {
                return;
            }
            std::scoped_lock lock(g_lock);
            const auto it = g_scenes.find(a_key);
            if (it == g_scenes.end() || it->second.witness) {
                return;
            }
            const auto id = a_witness->GetFormID();
            if (std::ranges::any_of(it->second.actors, [id](const auto& p) { return p.formID == id; })) {
                return;
            }
            it->second.witness = Participant{ id, ActorName(a_witness) };
            logger::info("Scene {}: {} ({:08X}) saw it", a_key, it->second.witness->name, id);
        }

        // ---- Partners (for the complaints of SN_SexualHistoryComplaints) ----------------------

        constexpr RE::FormID kPlayerBaseID = 0x7;

        struct Partner
        {
            RE::TESNPC* npc = nullptr;
            std::string label;  // "wife", "husband", "lover"...; "partner" when the record has none
        };

        // The NPCs an actor's base is tied to at the Lover level (spouses and lovers), the player
        // excepted. Relationships set at run time that never made a record are not seen.
        std::vector<Partner> Partners(RE::Actor* a_actor)
        {
            std::vector<Partner> partners;
            auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!base || !data) {
                return partners;
            }
            for (auto* relationship : data->GetFormArray<RE::BGSRelationship>()) {
                if (!relationship || relationship->level.get() != RE::BGSRelationship::RELATIONSHIP_LEVEL::kLover) {
                    continue;
                }
                RE::TESNPC* other = relationship->npc1 == base ? relationship->npc2 :
                                    relationship->npc2 == base ? relationship->npc1 : nullptr;
                if (!other || other->GetFormID() == kPlayerBaseID ||
                    std::ranges::any_of(partners, [other](const auto& p) { return p.npc == other; })) {
                    continue;
                }
                std::string label;
                if (auto* type = relationship->assocType) {
                    const auto sex = other->GetSex() == RE::SEX::kFemale ? RE::BGSAssociationType::Sexes::kFemale :
                                                                            RE::BGSAssociationType::Sexes::kMale;
                    const auto& text = type->associationLabels[RE::BGSAssociationType::Members::kParent][sex];
                    label = text.empty() ? "" : text.c_str();
                }
                std::ranges::transform(label, label.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                partners.push_back({ other, label.empty() ? "partner" : label });
            }
            return partners;
        }

        // "Sigrid (wife)", "Sigrid (wife) and Hod (lover)", or "" when the actor has no partner.
        RE::BSFixedString PartnerDescription(RE::StaticFunctionTag*, RE::Actor* a_actor)
        {
            const auto partners = Partners(a_actor);
            std::string text;
            for (std::size_t i = 0; i < partners.size(); ++i) {
                const char* name = partners[i].npc->GetName();
                if (i > 0) {
                    text += i + 1 == partners.size() ? " and " : ", ";
                }
                text += std::string(name ? name : "someone") + " (" + partners[i].label + ")";
            }
            return text;
        }

        // The partners who are loaded near the player now, so their distance can be checked.
        std::vector<RE::Actor*> LoadedPartners(RE::StaticFunctionTag*, RE::Actor* a_actor)
        {
            std::vector<RE::Actor*> found;
            const auto partners = Partners(a_actor);
            auto* lists = RE::ProcessLists::GetSingleton();
            if (partners.empty() || !lists) {
                return found;
            }
            lists->ForEachHighActor([&](RE::Actor* a_other) {
                if (a_other && a_other != a_actor &&
                    std::ranges::any_of(partners, [a_other](const auto& p) { return p.npc == a_other->GetActorBase(); })) {
                    found.push_back(a_other);
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return found;
        }

        // True while the actor is in an OStim or SexLab scene this plugin is following.
        bool InScene(RE::StaticFunctionTag*, RE::Actor* a_actor)
        {
            if (!a_actor) {
                return false;
            }
            const auto id = a_actor->GetFormID();
            std::scoped_lock lock(g_lock);
            return std::ranges::any_of(g_scenes, [id](const auto& entry) {
                return std::ranges::any_of(entry.second.actors, [id](const auto& p) { return p.formID == id; });
            });
        }

        // ---- The player speaks up (SN_SexualHistoryPlayerTalk) -----------------------------------

        constexpr const char* kPlayerTalkPrompt = "sn_sexual_history_player_talk";
        std::atomic<bool>     g_playerTalkBusy = false;

        // The LLM's line without wrapping quotes, line breaks or a runaway length.
        std::string CleanLine(std::string a_line)
        {
            for (auto& c : a_line) {
                if (c == '\r' || c == '\n') {
                    c = ' ';
                }
            }
            const auto first = a_line.find_first_not_of(" \t\"'");
            const auto last = a_line.find_last_not_of(" \t\"'");
            a_line = first == std::string::npos ? std::string() : a_line.substr(first, last - first + 1);
            return Truncate(a_line, 600);
        }

        std::vector<std::string> MemoryTexts(RE::FormID a_owner, int a_count, const std::string& a_query)
        {
            std::vector<std::string> texts;
            if (!PublicGetMemoriesForActor) {
                return texts;
            }
            for (const auto& memory : ParseArray(PublicGetMemoriesForActor(a_owner, a_count, a_query.c_str()))) {
                if (memory.is_object() && memory.contains("text") && memory["text"].is_string()) {
                    texts.push_back(Truncate(memory["text"].get<std::string>(), kMaxEventText));
                }
            }
            return texts;
        }

        // Has the LLM write a line the player says to a_target, from the recent events, the
        // player's memories (recent ones and those about a_target) and their relationship; then the
        // player says it (in their voice when SkyrimNet voices the player) and a_target is told, so
        // they answer. Returns False when one is already being written.
        bool PlayerTalk(RE::StaticFunctionTag*, RE::Actor* a_target, int32_t a_relationshipRank, RE::BSFixedString a_location)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!a_target || !player || !PublicSendCustomPromptToLLM || g_playerTalkBusy.exchange(true)) {
                return false;
            }
            const RE::FormID targetID = a_target->GetFormID();
            const std::string targetName = ActorName(a_target);
            const std::string playerName = ActorName(player);
            const std::string location = a_location.c_str();
            std::thread([targetID, targetName, playerName, location, a_relationshipRank] {
                json events = json::array();
                if (PublicGetRecentEvents) {
                    std::map<int64_t, json> byId;
                    for (const auto id : { kPlayerID, targetID }) {
                        for (const auto& event : ParseArray(PublicGetRecentEvents(id, 15, ""))) {
                            if (event.is_object() && event.contains("id") && event["id"].is_number()) {
                                byId.emplace(event["id"].get<int64_t>(), event);
                            }
                        }
                    }
                    for (const auto& [id, event] : byId) {
                        events.push_back(EventLine(event));
                    }
                    while (events.size() > 25) {
                        events.erase(events.begin());
                    }
                }
                const json context = {
                    { "player_name", playerName },
                    { "player_form_id", kPlayerID },
                    { "target_name", targetName },
                    { "target_form_id", targetID },
                    { "relationship_rank", a_relationshipRank },
                    { "location", location },
                    { "memories_about_target", MemoryTexts(kPlayerID, 6, targetName) },
                    { "recent_memories", MemoryTexts(kPlayerID, 4, "") },
                    { "events", events },
                };
                const bool queued = PublicSendCustomPromptToLLM(kPlayerTalkPrompt, "", Dump(context).c_str(),
                    [targetID, targetName, playerName](const char* a_response, int a_success) {
                        const std::string line = a_success == 1 && a_response ? CleanLine(a_response) : std::string();
                        if (line.empty()) {
                            logger::error("Player line to {} failed: {}", targetName, a_response ? a_response : "");
                            g_playerTalkBusy = false;
                            return;
                        }
                        SKSE::GetTaskInterface()->AddTask([line, targetID, targetName, playerName]() {
                            const auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
                            const bool voiced = vm && vm->DispatchStaticCall("SkyrimNetApi", "TriggerPlayerTTS", RE::MakeFunctionArguments(std::string(line)), none);
                            // TriggerPlayerTTS shows no subtitle: the HUD's own one shows the line.
                            if (vm) {
                                vm->DispatchStaticCall("SN_SexualHistorySubtitle", "Show", RE::MakeFunctionArguments(std::string(line)), none);
                            }
                            const std::string told = playerName + " says to " + targetName + ": \"" + line + "\"";
                            const auto id = PublicRegisterEvent ? PublicRegisterEvent("direct_narration", told.c_str(), targetID, kPlayerID, nullptr, 0) : 0;
                            logger::info("Player line to {} ({:08X}), narration {}, voice {}: {}", targetName, targetID, id, voiced ? "asked" : "not available", line);
                            g_playerTalkBusy = false;
                        });
                    });
                if (!queued) {
                    logger::error("Could not queue the player line to {}", targetName);
                    g_playerTalkBusy = false;
                }
            }).detach();
            return true;
        }

        // ---- Relationship map ------------------------------------------------------------------
        // A web page (Magelight UI or Prisma UI) lists the player and the NPCs nearby; the one picked
        // is the centre. The map is the AI's reading of the ten or so people the centre shares the
        // most SkyrimNet events with: their bios, the centre's events, the memories each has of the
        // other, the game's own relationships and how often the others meet. A map is kept per save
        // and only remade on request.

        constexpr const char* kRelMapPrompt = "sn_sexual_memories_relmap";
        constexpr const char* kRelMapSummaryPrompt = "sn_sexual_memories_relmap_summary";
        constexpr const char* kRelMapVariant = "sn_sexual_memories_relmap";  // settings/SNSexualMemories.yaml
        constexpr int         kRelMapPeople = 10;
        constexpr double      kGameDay = 86400.0;  // game seconds
        std::atomic<bool>     g_relMapBusy = false;

        // How far back the map reads, from the MCM (SN_SexualHistoryNative.SetMapSettings).
        std::atomic<int> g_mapMemoryScan = 300;    // memories read per person, most important first
        std::atomic<int> g_mapMemoriesUsed = 10;   // memories given per pair and direction
        std::atomic<int> g_mapEventScan = 150;     // the centre's latest events read
        std::atomic<int> g_mapDiaryScan = 30;      // diary entries read per person, latest first
        std::atomic<int> g_mapTextChars = 400;     // characters kept of one memory, event or diary passage

        // Cuts a UTF-8 string after a_max characters (not bytes: a Japanese character is three).
        std::string TruncateChars(const std::string& a_text, int a_max)
        {
            std::size_t i = 0;
            int count = 0;
            while (i < a_text.size() && count < a_max) {
                const auto c = static_cast<unsigned char>(a_text[i]);
                i += c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
                ++count;
            }
            return i >= a_text.size() ? a_text : a_text.substr(0, i) + "...";
        }

        std::filesystem::path RelMapFolder()
        {
            const auto saveId = PublicGetSaveUniqueID ? PublicGetSaveUniqueID() : std::string();
            std::string safe = saveId.empty() ? std::string("unknown_save") : saveId;
            for (auto& c : safe) {
                if (std::string_view("\\/:*?\"<>|").find(c) != std::string_view::npos || static_cast<unsigned char>(c) < 0x20) {
                    c = '_';
                }
            }
            return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "SN_SexualHistory" / "relmaps" /
                   std::filesystem::path(std::u8string(safe.begin(), safe.end()));
        }

        std::filesystem::path RelMapFile(RE::FormID a_center)
        {
            return RelMapFolder() / std::format("{:08X}.json", a_center);
        }

        // Runs a_script in the page, on the game thread.
        void PageCall(std::string a_script)
        {
            UiHost::Post([script = std::move(a_script)]() { UiHost::Invoke(script); });
        }

        void PageError(const std::string& a_message)
        {
            PageCall("window.snsm && window.snsm.showError(" + Dump(json(a_message)) + ")");
        }

        // The map being made is written in English (the page's "Write in English"), whatever the
        // language of the records. Only one map is made at a time (g_relMapBusy).
        std::atomic<bool> g_mapEnglish = false;

        // Faces drawn by Phantasm this session: form ID -> file in the view folder and its version
        // (the page adds the version to the address, so a redrawn face is not taken from its cache).
        std::mutex                                       g_faceLock;
        std::map<RE::FormID, std::pair<std::string, int>> g_faces;
        std::atomic<int>                                 g_faceVersion = 0;

        void SendFace(RE::FormID a_id, const std::string& a_file, int a_version)
        {
            PageCall("window.snsm && window.snsm.setFace(" +
                     Dump(json{ { "id", std::format("{:08X}", a_id) }, { "file", a_file }, { "v", a_version } }) + ")");
        }

        // The faces of a map on screen: those drawn this session at once, the others (every one when
        // a_redraw) drawn by Phantasm and sent as they come. Nothing without Phantasm: the map keeps
        // its plain circles, as it does for anyone Phantasm cannot draw (someone not loaded).
        void MapFaces(const json& a_map, bool a_redraw)
        {
            if (!PhantasmFaces::Ready() || !a_map.is_object() || !a_map.contains("people") || !a_map["people"].is_array()) {
                return;
            }
            const int                  version = ++g_faceVersion;
            std::vector<std::uint32_t> missing;
            {
                std::scoped_lock lock(g_faceLock);
                for (const auto& person : a_map["people"]) {
                    if (!person.is_object() || !person.contains("id") || !person["id"].is_number()) {
                        continue;
                    }
                    const auto id = person["id"].get<RE::FormID>();
                    const auto found = g_faces.find(id);
                    if (found != g_faces.end() && !a_redraw) {
                        SendFace(id, found->second.first, found->second.second);
                    } else {
                        missing.push_back(id);
                    }
                }
            }
            if (missing.empty()) {
                return;
            }
            PhantasmFaces::Draw(missing, UiHost::ViewFolder(), "map", [version](std::uint32_t a_id, std::string a_file) {
                if (a_file.empty()) {
                    return;
                }
                {
                    std::scoped_lock lock(g_faceLock);
                    g_faces[a_id] = { a_file, version };
                }
                SendFace(a_id, a_file, version);
            });
        }

        // The UI texts of the installed language (SKSE/Plugins/SN_SexualHistory/strings.json).
        json UiStrings()
        {
            const auto file = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "SN_SexualHistory" / "strings.json";
            std::ifstream in(file, std::ios::binary);
            if (!in) {
                return json::object();
            }
            std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            auto parsed = json::parse(text, nullptr, false);
            return parsed.is_object() ? parsed : json::object();
        }

        // Skyrim's relationship levels, as words for the AI.
        std::string LevelWord(RE::BGSRelationship::RELATIONSHIP_LEVEL a_level)
        {
            using L = RE::BGSRelationship::RELATIONSHIP_LEVEL;
            switch (a_level) {
            case L::kLover: return "lover or spouse";
            case L::kAlly: return "ally";
            case L::kConfidant: return "confidant";
            case L::kFriend: return "friend";
            case L::kAcquaintance: return "acquaintance";
            case L::kRival: return "rival";
            case L::kFoe: return "foe";
            case L::kEnemy: return "enemy";
            case L::kArchnemesis: return "archnemesis";
            default: return "";
            }
        }

        struct MapPerson
        {
            RE::FormID  formID = 0;
            std::string name;
            std::string key;  // "c" for the centre, "p1".."p10" for the others
            int         sharedEvents = 0;
            bool        follower = false;    // the player's follower now
            std::string withPlayer;          // the game's relationship level to the player, "" when none
        };

        // The ways a name may be written in memories: "Uthgerd the Unbroken" also as "Uthgerd",
        // "不滅のウスガルド" also as "ウスガルド".
        std::vector<std::string> NameVariants(const std::string& a_name)
        {
            std::vector<std::string> names{ a_name };
            const auto add = [&](std::string a_part) {
                const auto first = a_part.find_first_not_of(" ");
                const auto last = a_part.find_last_not_of(" ");
                if (first == std::string::npos) {
                    return;
                }
                a_part = a_part.substr(first, last - first + 1);
                if (a_part.size() >= 4 && std::ranges::find(names, a_part) == names.end()) {
                    names.push_back(a_part);
                }
            };
            if (const auto the = a_name.find(" the "); the != std::string::npos) {
                add(a_name.substr(0, the));
            }
            if (const auto space = a_name.find(' '); space != std::string::npos) {
                add(a_name.substr(0, space));
            }
            const std::string no = "\xE3\x81\xAE";  // "の"
            if (const auto at = a_name.rfind(no); at != std::string::npos) {
                add(a_name.substr(at + no.size()));
            }
            return names;
        }

        // An actor's memories, most important first (up to 300), read once per map.
        json ImportantMemories(RE::FormID a_owner)
        {
            if (!PublicQueryMemoriesForActor) {
                return json::array();
            }
            MemoryQuery query;
            query.maxCount = (std::max)(1, g_mapMemoryScan.load());
            query.orderBy = MemoryOrder::ImportanceDesc;
            return ParseArray(QueryMemoriesForActor(a_owner, query));
        }

        // Up to a_max of a_memories that name someone (any way of writing the name), most important
        // first; topped up by a meaning search when fewer than two name them.
        std::vector<std::string> MemoriesNaming(const json& a_memories, RE::FormID a_owner, const std::string& a_name, std::size_t a_max)
        {
            const auto names = NameVariants(a_name);
            std::vector<std::string> found;
            for (const auto& memory : a_memories) {
                if (found.size() >= a_max) {
                    break;
                }
                if (!memory.is_object() || !memory.contains("text") || !memory["text"].is_string()) {
                    continue;
                }
                const auto& text = memory["text"].get_ref<const std::string&>();
                if (std::ranges::any_of(names, [&](const auto& n) { return text.find(n) != std::string::npos; })) {
                    found.push_back(TruncateChars(text, g_mapTextChars));
                }
            }
            if (found.size() < 2) {
                for (auto& text : MemoryTexts(a_owner, 3, a_name)) {
                    if (found.size() < a_max && std::ranges::find(found, text) == found.end()) {
                        found.push_back(std::move(text));
                    }
                }
            }
            return found;
        }

        // An actor's latest diary entries (count from the MCM), read once per map.
        json LatestDiaries(RE::FormID a_owner)
        {
            if (!PublicQueryDiaryEntries || g_mapDiaryScan <= 0) {
                return json::array();
            }
            DiaryQuery query;
            query.maxCount = g_mapDiaryScan;
            query.orderBy = DiaryOrder::EntryDateDesc;
            return ParseArray(QueryDiaryEntries(a_owner, query));
        }

        std::string DiaryText(const json& a_entry)
        {
            for (const char* key : { "content", "text", "entry" }) {
                if (a_entry.contains(key) && a_entry[key].is_string()) {
                    return a_entry[key].get<std::string>();
                }
            }
            return "";
        }

        // Up to a_max passages from a_diaries that name someone: the text around each mention, up to
        // the character limit, newest entries first.
        std::vector<std::string> DiaryPassages(const json& a_diaries, const std::string& a_name, std::size_t a_max)
        {
            const auto names = NameVariants(a_name);
            std::vector<std::string> passages;
            for (const auto& entry : a_diaries) {
                if (passages.size() >= a_max || !entry.is_object()) {
                    break;
                }
                const auto text = DiaryText(entry);
                std::size_t at = std::string::npos;
                for (const auto& n : names) {
                    at = (std::min)(at, text.find(n));
                }
                if (at == std::string::npos) {
                    continue;
                }
                // Start a little before the first mention, on a character boundary.
                std::size_t start = at > 240 ? at - 240 : 0;
                while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
                    --start;
                }
                auto passage = TruncateChars(text.substr(start), g_mapTextChars);
                if (start > 0) {
                    passage = "..." + passage;
                }
                const auto date = entry.contains("entry_date_str") && entry["entry_date_str"].is_string() ? entry["entry_date_str"].get<std::string>() : std::string();
                passages.push_back(date.empty() ? passage : "(" + date + ") " + passage);
            }
            return passages;
        }

        // Reads a JSON object out of an LLM answer: drops fences and text around it, puts back
        // missing commas between objects, and, when the answer was cut off, keeps every complete
        // element before the cut.
        json ParseLooseJson(const std::string& a_response)
        {
            const auto open = a_response.find('{');
            if (open == std::string::npos) {
                return json();
            }
            std::string text = a_response.substr(open);
            // "}\n  {" without a comma between two objects of a list
            std::string fixed;
            fixed.reserve(text.size() + 16);
            bool inString = false, escaped = false;
            for (std::size_t i = 0; i < text.size(); ++i) {
                const char c = text[i];
                fixed += c;
                if (inString) {
                    if (escaped) {
                        escaped = false;
                    } else if (c == '\\') {
                        escaped = true;
                    } else if (c == '"') {
                        inString = false;
                    }
                    continue;
                }
                if (c == '"') {
                    inString = true;
                } else if (c == '}') {
                    std::size_t j = i + 1;
                    while (j < text.size() && std::isspace(static_cast<unsigned char>(text[j]))) {
                        ++j;
                    }
                    if (j < text.size() && text[j] == '{') {
                        fixed += ',';
                    }
                }
            }
            text = std::move(fixed);
            if (const auto close = text.rfind('}'); close != std::string::npos) {
                auto whole = json::parse(text.substr(0, close + 1), nullptr, false);
                if (whole.is_object()) {
                    return whole;
                }
            }
            // Cut off: try the text up to each '}' from the end, closing what is still open.
            std::vector<std::size_t> cuts;
            for (std::size_t i = text.size(); i-- > 0;) {
                if (text[i] == '}') {
                    cuts.push_back(i);
                    if (cuts.size() >= 60) {
                        break;
                    }
                }
            }
            for (const auto cut : cuts) {
                std::string head = text.substr(0, cut + 1);
                std::vector<char> stack;
                bool str = false, esc = false;
                for (const char c : head) {
                    if (str) {
                        if (esc) {
                            esc = false;
                        } else if (c == '\\') {
                            esc = true;
                        } else if (c == '"') {
                            str = false;
                        }
                        continue;
                    }
                    if (c == '"') {
                        str = true;
                    } else if (c == '{' || c == '[') {
                        stack.push_back(c);
                    } else if ((c == '}' || c == ']') && !stack.empty()) {
                        stack.pop_back();
                    }
                }
                if (str) {
                    continue;
                }
                for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
                    head += *it == '{' ? '}' : ']';
                }
                auto partial = json::parse(head, nullptr, false);
                if (partial.is_object()) {
                    logger::warn("Relationship map: the answer was cut off; kept what came before the cut");
                    return partial;
                }
            }
            return json();
        }

        // One map: a request per person around the centre (what happened between them and how they
        // feel, from their own memories and events), then one for the circle's summary.
        struct MapJob
        {
            std::mutex             lock;
            std::vector<MapPerson> people;
            std::string            gameDate;
            bool                   english = false;  // written in English (the page's "Write in English")
            std::map<std::string, json> answers;  // by person key
            int                    pending = 0;
        };

        std::string ToneOf(const json& a_object)
        {
            auto tone = a_object.value("tone", std::string("neutral"));
            return tone == "positive" || tone == "negative" || tone == "mixed" ? tone : std::string("neutral");
        }

        // Builds the map from the answers, saves and shows it. a_summary: the circle's summary.
        void RelMapFinish(const std::shared_ptr<MapJob>& a_job, const std::string& a_summary)
        {
            const auto& people = a_job->people;
            json map = { { "center", people.front().formID }, { "generated", a_job->gameDate }, { "summary", a_summary },
                { "language", a_job->english ? "en" : "" } };
            json outPeople = json::array();
            json outLinks = json::array();
            const auto known = [&](const std::string& k) { return std::ranges::any_of(people, [&](const auto& p) { return p.key == k; }); };
            const auto linked = [&](const std::string& a, const std::string& b) {
                return std::ranges::any_of(outLinks, [&](const auto& l) { return (l["a"] == a && l["b"] == b) || (l["a"] == b && l["b"] == a); });
            };
            for (const auto& person : people) {
                json entry = { { "key", person.key }, { "id", person.formID }, { "name", person.name }, { "role", "" }, { "tone", "neutral" },
                    { "summary", "" }, { "feelings", "" }, { "history", json::array() }, { "follower", person.follower } };
                const auto found = a_job->answers.find(person.key);
                if (person.key != "c" && found != a_job->answers.end() && found->second.is_object()) {
                    const auto& answer = found->second;
                    entry["role"] = answer.value("role", std::string());
                    entry["tone"] = ToneOf(answer);
                    entry["summary"] = answer.value("summary", std::string());
                    entry["feelings"] = answer.value("feelings", std::string());
                    if (answer.contains("history") && answer["history"].is_array()) {
                        for (const auto& h : answer["history"]) {
                            if (h.is_string()) {
                                entry["history"].push_back(h);
                            }
                        }
                    }
                    const int strength = answer.contains("strength") && answer["strength"].is_number() ? answer["strength"].get<int>() : 2;
                    outLinks.push_back({ { "a", "c" }, { "b", person.key }, { "label", entry["role"] }, { "tone", entry["tone"] },
                        { "strength", std::clamp(strength, 1, 3) }, { "note", "" } });
                    if (answer.contains("ties") && answer["ties"].is_array()) {
                        for (const auto& tie : answer["ties"]) {
                            const auto other = tie.is_object() ? tie.value("key", std::string()) : std::string();
                            if (other.empty() || other == "c" || other == person.key || !known(other) || linked(person.key, other)) {
                                continue;
                            }
                            outLinks.push_back({ { "a", person.key }, { "b", other }, { "label", tie.value("label", std::string()) },
                                { "tone", ToneOf(tie) }, { "strength", 1 }, { "note", tie.value("note", std::string()) } });
                        }
                    }
                } else if (person.key != "c") {
                    outLinks.push_back({ { "a", "c" }, { "b", person.key }, { "label", "" }, { "tone", "neutral" }, { "strength", 1 }, { "note", "" } });
                }
                outPeople.push_back(std::move(entry));
            }
            map["people"] = outPeople;
            map["links"] = outLinks;
            const auto text = Dump(map);
            std::error_code error;
            std::filesystem::create_directories(RelMapFolder(), error);
            std::ofstream out(RelMapFile(people.front().formID), std::ios::binary);
            if (out) {
                out << text;
            }
            logger::info("Relationship map of {} ({:08X}): {} people, {} answers, {} links", people.front().name, people.front().formID,
                outPeople.size(), a_job->answers.size(), outLinks.size());
            PageCall("window.snsm && window.snsm.showMap(" + text + ")");
            MapFaces(map, true);
            g_relMapBusy = false;
        }

        // After every person's answer: the summary of the circle, from those answers.
        void RelMapSummary(const std::shared_ptr<MapJob>& a_job)
        {
            json people = json::array();
            for (const auto& person : a_job->people) {
                const auto found = a_job->answers.find(person.key);
                if (person.key != "c" && found != a_job->answers.end() && found->second.is_object()) {
                    people.push_back({ { "name", person.name }, { "role", found->second.value("role", std::string()) },
                        { "summary", found->second.value("summary", std::string()) } });
                }
            }
            if (people.empty()) {
                PageError("ai");
                g_relMapBusy = false;
                return;
            }
            const json context = { { "center_name", a_job->people.front().name }, { "center_form_id", a_job->people.front().formID }, { "people", people },
                { "language", a_job->english ? "English" : "" } };
            const bool queued = PublicSendCustomPromptToLLM(kRelMapSummaryPrompt, kRelMapVariant, Dump(context).c_str(),
                [a_job](const char* a_response, int a_success) {
                    std::string summary;
                    if (a_success == 1 && a_response) {
                        const auto parsed = ParseLooseJson(a_response);
                        summary = parsed.is_object() ? parsed.value("summary", std::string()) : std::string();
                        if (summary.empty()) {
                            summary = Truncate(a_response, 1500);  // plain text is a summary too
                        }
                    }
                    RelMapFinish(a_job, summary);
                });
            if (!queued) {
                RelMapFinish(a_job, "");
            }
        }

        // Second half, on a worker: each person's memories and events, then a request per person.
        void RelMapAsk(std::vector<MapPerson> a_people, json a_gameRelations, std::string a_gameDate)
        {
            const auto& center = a_people.front();
            std::map<RE::FormID, json> memories;
            std::map<RE::FormID, json> diaries;
            for (const auto& person : a_people) {
                memories[person.formID] = ImportantMemories(person.formID);
                diaries[person.formID] = LatestDiaries(person.formID);
            }
            const auto& centerMemories = memories[center.formID];
            const std::size_t used = static_cast<std::size_t>((std::max)(1, g_mapMemoriesUsed.load()));
            const std::size_t textBytes = static_cast<std::size_t>(g_mapTextChars.load()) * 3;
            json centerEvents = json::array();
            if (PublicGetRecentEvents && g_mapEventScan > 0) {
                centerEvents = ParseArray(PublicGetRecentEvents(center.formID, g_mapEventScan, ""));
                std::reverse(centerEvents.begin(), centerEvents.end());  // oldest first
            }
            std::map<std::pair<std::string, std::string>, int> pairCounts;
            if (PublicGetEventPairCounts) {
                std::string csv;
                for (const auto& person : a_people) {
                    csv += (csv.empty() ? "" : ",") + std::to_string(person.formID);
                }
                for (const auto& pair : ParseArray(PublicGetEventPairCounts(csv.c_str(), 2))) {
                    if (!pair.is_object() || !pair.contains("formId1") || !pair.contains("formId2")) {
                        continue;
                    }
                    const auto a = pair["formId1"].get<RE::FormID>();
                    const auto b = pair["formId2"].get<RE::FormID>();
                    const auto ka = std::ranges::find_if(a_people, [a](const auto& p) { return p.formID == a; });
                    const auto kb = std::ranges::find_if(a_people, [b](const auto& p) { return p.formID == b; });
                    if (ka != a_people.end() && kb != a_people.end()) {
                        pairCounts[{ ka->key, kb->key }] = pair.value("sharedEvents", 0);
                        pairCounts[{ kb->key, ka->key }] = pair.value("sharedEvents", 0);
                    }
                }
            }
            auto job = std::make_shared<MapJob>();
            job->people = a_people;
            job->gameDate = a_gameDate;
            job->english = g_mapEnglish.load();
            job->pending = static_cast<int>(a_people.size()) - 1;
            for (const auto& person : a_people) {
                if (person.key == "c") {
                    continue;
                }
                // The centre's events that name this person.
                const auto names = NameVariants(person.name);
                json events = json::array();
                for (const auto& event : centerEvents) {
                    if (!event.is_object()) {
                        continue;
                    }
                    auto line = EventLine(event, textBytes);
                    if (std::ranges::any_of(names, [&](const auto& n) { return line.find(n) != std::string::npos; })) {
                        events.push_back(std::move(line));
                    }
                }
                while (events.size() > 25) {
                    events.erase(events.begin());
                }
                // The rest of the circle, with what this person and they remember of each other.
                json others = json::array();
                const std::size_t tieMemories = (std::min)(used, std::size_t{ 5 });
                for (const auto& other : a_people) {
                    if (other.key != "c" && other.key != person.key) {
                        const auto count = pairCounts.find({ person.key, other.key });
                        others.push_back({ { "key", other.key }, { "name", other.name },
                            { "is_player", other.formID == kPlayerID },
                            { "follower", other.follower },
                            { "with_player", other.withPlayer },
                            { "shared_events", count != pairCounts.end() ? count->second : 0 },
                            { "person_remembers", MemoriesNaming(memories[person.formID], person.formID, other.name, tieMemories) },
                            { "remembers_person", MemoriesNaming(memories[other.formID], other.formID, person.name, tieMemories) } });
                    }
                }
                json relations = json::array();
                for (const auto& r : a_gameRelations) {
                    if (r.value("a", std::string()) == person.key || r.value("b", std::string()) == person.key) {
                        relations.push_back(r);
                    }
                }
                const json context = {
                    { "center_name", center.name },
                    { "center_form_id", center.formID },
                    { "center_is_player", center.formID == kPlayerID },
                    { "person_key", person.key },
                    { "person_name", person.name },
                    { "person_form_id", person.formID },
                    { "person_is_player", person.formID == kPlayerID },
                    { "follower", person.follower },
                    { "with_player", person.withPlayer },
                    { "shared_events", person.sharedEvents },
                    { "center_remembers", MemoriesNaming(centerMemories, center.formID, person.name, used) },
                    { "remembers_center", MemoriesNaming(memories[person.formID], person.formID, center.name, used) },
                    { "center_diary", DiaryPassages(diaries[center.formID], person.name, 5) },
                    { "person_diary", DiaryPassages(diaries[person.formID], center.name, 5) },
                    { "events", events },
                    { "others", others },
                    { "game_relations", relations },
                    { "game_date", a_gameDate },
                    { "language", job->english ? "English" : "" },
                };
                const auto key = person.key;
                const bool queued = PublicSendCustomPromptToLLM(kRelMapPrompt, kRelMapVariant, Dump(context).c_str(),
                    [job, key](const char* a_response, int a_success) {
                        json parsed;
                        if (a_success == 1 && a_response) {
                            parsed = ParseLooseJson(a_response);
                        }
                        if (!parsed.is_object()) {
                            logger::error("Relationship map, {}: no answer: {}", key, Truncate(a_response ? a_response : "", 400));
                        }
                        bool last = false;
                        {
                            std::scoped_lock lock(job->lock);
                            if (parsed.is_object()) {
                                job->answers[key] = std::move(parsed);
                            }
                            last = --job->pending == 0;
                        }
                        if (last) {
                            RelMapSummary(job);
                        }
                    });
                if (!queued) {
                    logger::error("Relationship map, {}: could not queue", key);
                    bool last = false;
                    {
                        std::scoped_lock lock(job->lock);
                        last = --job->pending == 0;
                    }
                    if (last) {
                        RelMapSummary(job);
                    }
                }
            }
        }

        // Game thread: the game's own relationships between the people (static records), who is
        // the player's follower, then the rest on a worker.
        void RelMapRelations(std::vector<MapPerson> a_people, std::string a_gameDate)
        {
            auto*                    player = RE::PlayerCharacter::GetSingleton();
            auto*                    playerBase = player ? player->GetActorBase() : nullptr;
            std::vector<RE::TESNPC*> bases;
            for (auto& person : a_people) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(person.formID);
                bases.push_back(actor ? actor->GetActorBase() : nullptr);
                person.follower = actor && actor != player && actor->IsPlayerTeammate();
            }
            json relations = json::array();
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                for (auto* relationship : data->GetFormArray<RE::BGSRelationship>()) {
                    if (!relationship || !relationship->npc1 || !relationship->npc2) {
                        continue;
                    }
                    // Each one's standing with the player, for the people who have one.
                    if (playerBase && (relationship->npc1 == playerBase || relationship->npc2 == playerBase)) {
                        const auto other = relationship->npc1 == playerBase ? relationship->npc2 : relationship->npc1;
                        const auto k = std::ranges::find(bases, other);
                        if (k != bases.end()) {
                            a_people[k - bases.begin()].withPlayer = LevelWord(relationship->level.get());
                        }
                    }
                    const auto i = std::ranges::find(bases, relationship->npc1);
                    const auto j = std::ranges::find(bases, relationship->npc2);
                    if (i == bases.end() || j == bases.end() || i == j) {
                        continue;
                    }
                    std::string label;
                    if (auto* type = relationship->assocType) {
                        const auto& text = type->associationLabels[RE::BGSAssociationType::Members::kParent][RE::BGSAssociationType::Sexes::kMale];
                        label = text.empty() ? "" : text.c_str();
                    }
                    relations.push_back({ { "a", a_people[i - bases.begin()].key }, { "b", a_people[j - bases.begin()].key },
                        { "level", LevelWord(relationship->level.get()) }, { "kind", label } });
                }
            }
            std::thread(RelMapAsk, std::move(a_people), std::move(relations), std::move(a_gameDate)).detach();
        }

        // Worker: the centre and the people around the player (nearest first), with how many
        // SkyrimNet events each shares with the centre.
        void RelMapGenerate(std::vector<MapPerson> people, std::string a_gameDate)
        {
            if (!PublicSendCustomPromptToLLM || !PublicIsMemorySystemReady || !PublicIsMemorySystemReady()) {
                PageError("skyrimnet");
                g_relMapBusy = false;
                return;
            }
            if (PublicGetEventPairCounts && people.size() >= 2) {
                std::string csv;
                for (const auto& person : people) {
                    csv += (csv.empty() ? "" : ",") + std::to_string(person.formID);
                }
                const auto center = people.front().formID;
                for (const auto& pair : ParseArray(PublicGetEventPairCounts(csv.c_str(), 1))) {
                    if (!pair.is_object() || !pair.contains("formId1") || !pair.contains("formId2")) {
                        continue;
                    }
                    const auto a = pair["formId1"].get<RE::FormID>();
                    const auto b = pair["formId2"].get<RE::FormID>();
                    if (a != center && b != center) {
                        continue;
                    }
                    const auto other = a == center ? b : a;
                    for (auto& person : people) {
                        if (person.formID == other && person.key != "c") {
                            person.sharedEvents = pair.value("sharedEvents", 0);
                        }
                    }
                }
            }
            if (people.size() < 2) {
                PageError("nobody");
                g_relMapBusy = false;
                return;
            }
            SKSE::GetTaskInterface()->AddTask([people = std::move(people), a_gameDate]() mutable { RelMapRelations(std::move(people), std::move(a_gameDate)); });
        }

        // Game thread: the living adult NPCs within 4096 units of the player, nearest first.
        std::vector<std::pair<RE::Actor*, float>> NearbyNPCs()
        {
            std::vector<std::pair<RE::Actor*, float>> found;
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* npcKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeNPC");
            auto* lists = RE::ProcessLists::GetSingleton();
            if (!player || !lists) {
                return found;
            }
            lists->ForEachHighActor([&](RE::Actor* a_actor) {
                if (a_actor && a_actor != player && !a_actor->IsDead() && !a_actor->IsChild() &&
                    (!npcKeyword || a_actor->HasKeyword(npcKeyword))) {
                    const float distance = player->GetPosition().GetDistance(a_actor->GetPosition());
                    if (distance <= 4096.0f) {
                        found.emplace_back(a_actor, distance);
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            std::ranges::sort(found, {}, &std::pair<RE::Actor*, float>::second);
            return found;
        }

        // Game thread. A_refresh: remake it even when one is saved; a_english: remake it in English.
        void RelMapPick(RE::FormID a_center, bool a_refresh, bool a_english = false)
        {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_center);
            if (!actor) {
                PageError("nobody");
                return;
            }
            if (!a_refresh) {
                std::ifstream in(RelMapFile(a_center), std::ios::binary);
                if (in) {
                    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    if (json::accept(text)) {
                        UiHost::Invoke("window.snsm && window.snsm.showMap(" + text + ")");
                        MapFaces(json::parse(text), false);
                        return;
                    }
                }
            }
            if (g_relMapBusy.exchange(true)) {
                PageError("busy");
                return;
            }
            const auto name = ActorName(actor);
            // The circle: the people around the player now (the player too when the centre is an
            // NPC), nearest first, whether or not the records tie them to the centre.
            std::vector<MapPerson> people{ { a_center, name, "c", 0, false, "" } };
            auto add = [&](RE::Actor* a_person) {
                const auto personName = ActorName(a_person);
                if (a_person->GetFormID() != a_center && !personName.empty() && static_cast<int>(people.size()) <= kRelMapPeople) {
                    people.push_back({ a_person->GetFormID(), personName, "p" + std::to_string(people.size()), 0, false, "" });
                }
            };
            if (auto* player = RE::PlayerCharacter::GetSingleton(); player && player->GetFormID() != a_center) {
                add(player);
            }
            for (const auto& [nearby, distance] : NearbyNPCs()) {
                add(nearby);
            }
            g_mapEnglish = a_english;
            UiHost::Invoke("window.snsm && window.snsm.showLoading(" + Dump(json(name)) + ")");
            std::thread(RelMapGenerate, std::move(people), GameDateString()).detach();
        }

        RE::FormID ParseFormID(const char* a_text)
        {
            try {
                return a_text ? static_cast<RE::FormID>(std::stoul(a_text, nullptr, 16)) : 0;
            } catch (...) {
                return 0;
            }
        }

        void OnPagePick(const char* a_arg)
        {
            const auto id = ParseFormID(a_arg);
            UiHost::Post([id]() { RelMapPick(id, false); });
        }

        void OnPageRefresh(const char* a_arg)
        {
            const auto id = ParseFormID(a_arg);
            UiHost::Post([id]() { RelMapPick(id, true); });
        }

        void OnPageEnglish(const char* a_arg)
        {
            const auto id = ParseFormID(a_arg);
            UiHost::Post([id]() { RelMapPick(id, true, true); });
        }

        std::atomic<bool> g_mapOpen = false;

        void CloseMap()
        {
            g_mapOpen = false;
            UiHost::Unfocus();
            UiHost::Hide();
        }

        void OnPageClose(const char*)
        {
            UiHost::Post([]() { CloseMap(); });
        }

        // Papyrus: opens the map with the player and the NPCs nearby to choose from, a_aimed first
        // when it is one of them. False when no UI host is installed.
        bool OpenRelationshipMap(RE::StaticFunctionTag*, RE::Actor* a_aimed)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!UiHost::Ready() || !player) {
                return false;
            }
            struct Choice
            {
                RE::FormID  id;
                std::string name;
                float       distance;
            };
            std::vector<Choice> choices;
            for (const auto& [nearby, distance] : NearbyNPCs()) {
                choices.push_back({ nearby->GetFormID(), ActorName(nearby), distance });
            }
            json list = json::array();
            list.push_back({ { "id", std::format("{:08X}", player->GetFormID()) }, { "name", ActorName(player) }, { "player", true } });
            for (const auto& choice : choices) {
                if (!choice.name.empty()) {
                    list.push_back({ { "id", std::format("{:08X}", choice.id) }, { "name", choice.name }, { "player", false } });
                }
            }
            const auto aimed = a_aimed && std::ranges::any_of(choices, [a_aimed](const auto& c) { return c.id == a_aimed->GetFormID(); }) ?
                                   a_aimed->GetFormID() : player->GetFormID();
            const json open = { { "people", list }, { "selected", std::format("{:08X}", aimed) }, { "strings", UiStrings() } };
            UiHost::Invoke("window.snsm && window.snsm.open(" + Dump(open) + ")");
            UiHost::Show();
            UiHost::Focus(true);
            g_mapOpen = true;
            return true;
        }

        // Esc on the map does what the page's Esc does: the page does not always get the key itself
        // while it has the focus.
        class EscapeWatcher : public RE::BSTEventSink<RE::InputEvent*>
        {
        public:
            static EscapeWatcher* GetSingleton()
            {
                static EscapeWatcher singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
            {
                if (!g_mapOpen || !a_events) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                for (auto event = *a_events; event; event = event->next) {
                    const auto button = event->AsButtonEvent();
                    if (button && button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                        button->GetIDCode() == RE::BSKeyboardDevice::Key::kEscape && button->IsDown()) {
                        SKSE::GetTaskInterface()->AddUITask([]() {
                            if (g_mapOpen && UiHost::HasFocus()) {
                                UiHost::Invoke("window.snsm && window.snsm.escape()");
                            } else {
                                CloseMap();  // it cannot hear the key: Esc still gets rid of it
                            }
                        });
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        void SetMapSettings(RE::StaticFunctionTag*, int32_t a_memoryScan, int32_t a_memoriesUsed, int32_t a_eventScan, int32_t a_diaryScan,
            int32_t a_textChars)
        {
            g_mapMemoryScan = std::clamp(a_memoryScan, 10, 5000);
            g_mapMemoriesUsed = std::clamp(a_memoriesUsed, 1, 50);
            g_mapEventScan = std::clamp(a_eventScan, 0, 2000);
            g_mapDiaryScan = std::clamp(a_diaryScan, 0, 500);
            g_mapTextChars = std::clamp(a_textChars, 50, 3000);
        }

        bool RelationshipMapAvailable(RE::StaticFunctionTag*)
        {
            return UiHost::Ready();
        }

        void SceneEnded(RE::StaticFunctionTag*, int32_t a_key)
        {
            std::shared_ptr<Scene> scene;
            {
                std::scoped_lock lock(g_lock);
                const auto it = g_scenes.find(a_key);
                if (it == g_scenes.end()) {
                    return;
                }
                scene = std::make_shared<Scene>(std::move(it->second));
                g_scenes.erase(it);
            }
            logger::info("Scene {} ended", a_key);
            if (!PublicSendCustomPromptToLLM || !PublicAddMemory) {
                logger::error("SkyrimNet API without custom prompts or memory creation; scene not recorded");
                return;
            }
            std::thread(Process, std::shared_ptr<const Scene>(scene)).detach();
        }
    }

    void Init()
    {
        if (!FindFunctions() || !PublicGetVersion) {
            logger::error("SkyrimNet API not found; scenes will not be recorded");
            return;
        }
        logger::info("SkyrimNet API v{}", PublicGetVersion());
        if (UiHost::SetUp([]() { CloseMap(); })) {
            UiHost::Listen("snsmPick", OnPagePick);
            UiHost::Listen("snsmRefresh", OnPageRefresh);
            UiHost::Listen("snsmEnglish", OnPageEnglish);
            if (PhantasmFaces::SetUp()) {
                logger::info("Relationship map faces drawn by Phantasm {}", PhantasmFaces::Version());
            }
            UiHost::Listen("snsmClose", OnPageClose);
            if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
                input->AddEventSink(EscapeWatcher::GetSingleton());
            }
        }
    }

    bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm)
    {
        a_vm->RegisterFunction("SceneStarted", kScriptName, SceneStarted);
        a_vm->RegisterFunction("SceneDetail", kScriptName, SceneDetail);
        a_vm->RegisterFunction("Climax", kScriptName, Climax);
        a_vm->RegisterFunction("SceneEnded", kScriptName, SceneEnded);
        a_vm->RegisterFunction("NeedsWitness", kScriptName, NeedsWitness);
        a_vm->RegisterFunction("Witness", kScriptName, Witness);
        a_vm->RegisterFunction("PartnerDescription", kScriptName, PartnerDescription);
        a_vm->RegisterFunction("LoadedPartners", kScriptName, LoadedPartners);
        a_vm->RegisterFunction("InScene", kScriptName, InScene);
        a_vm->RegisterFunction("PlayerTalk", kScriptName, PlayerTalk);
        a_vm->RegisterFunction("OpenRelationshipMap", kScriptName, OpenRelationshipMap);
        a_vm->RegisterFunction("RelationshipMapAvailable", kScriptName, RelationshipMapAvailable);
        a_vm->RegisterFunction("SetMapSettings", kScriptName, SetMapSettings);
        return true;
    }

    void Reset()
    {
        if (UiHost::Ready()) {
            CloseMap();
        }
        {
            std::scoped_lock lock(g_faceLock);
            g_faces.clear();  // another game: the same IDs may be other people
        }
        std::scoped_lock lock(g_lock);
        g_scenes.clear();
    }
}
