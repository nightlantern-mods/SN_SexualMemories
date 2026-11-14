#include "sexual_history.h"

#include "skyrimnet/PublicAPI.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

namespace SexualHistory
{
    namespace
    {
        using json = nlohmann::json;

        constexpr const char* kScriptName = "SN_SexualHistoryNative";
        constexpr const char* kPromptName = "sn_sexual_history_memory";
        constexpr RE::FormID  kPlayerID = 0x14;
        constexpr const char* kDashboardMarker = "[Sexual History]";

        constexpr std::size_t kMaxDetails = 40;     // scene details kept per scene
        constexpr std::size_t kMaxEvents = 80;      // SkyrimNet events passed to the LLM
        constexpr std::size_t kMaxEventText = 400;  // characters of one event's text
        constexpr int         kMemoriesPerActor = 6;
        constexpr int         kFallbackEvents = 30;  // per actor, when the scene's first event is unknown
        constexpr float       kDefaultImportance = 0.6f;

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
        std::string EventLine(const json& a_event)
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
            line += (line.empty() ? "(" : " (") + type + "): " + Truncate(text, kMaxEventText);
            return line;
        }

        // What SkyrimNet recorded during the scene for any of its participants, oldest first.
        std::vector<std::string> SceneEvents(const Scene& a_scene)
        {
            std::map<int64_t, json> byId;
            if (!PublicGetRecentEvents) {
                return {};
            }
            for (const auto& actor : a_scene.actors) {
                const auto events = ParseArray(PublicGetRecentEvents(actor.formID, a_scene.firstEventId > 0 ? 100 : kFallbackEvents, ""));
                for (const auto& event : events) {
                    if (!event.is_object() || !event.contains("id") || !event["id"].is_number()) {
                        continue;
                    }
                    const auto id = event["id"].get<int64_t>();
                    if (a_scene.firstEventId == 0 || id > a_scene.firstEventId) {
                        byId.emplace(id, event);
                    }
                }
            }
            std::vector<std::string> lines;
            for (const auto& [id, event] : byId) {
                lines.push_back(EventLine(event));
            }
            if (lines.size() > kMaxEvents) {
                lines.erase(lines.begin(), lines.end() - kMaxEvents);
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
            query = "feelings, relationship and intimacy with " + query;
            std::vector<std::string> texts;
            for (const auto& memory : ParseArray(PublicGetMemoriesForActor(a_actor.formID, kMemoriesPerActor, query.c_str()))) {
                if (memory.is_object() && memory.contains("text") && memory["text"].is_string()) {
                    texts.push_back(Truncate(memory["text"].get<std::string>(), kMaxEventText));
                }
            }
            return texts;
        }

        json BuildContext(const Scene& a_scene, const Participant& a_owner, const std::vector<std::string>& a_events,
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
                { "framework", a_scene.framework },
                { "participants", participants },
                { "location", a_scene.location },
                { "game_date", a_scene.gameDate },
                { "scene_details", a_scene.details },
                { "climaxes", a_scene.climaxes },
                { "events", a_events },
                { "real_minutes", minutes },
            };
        }

        // ---- Storing the answer --------------------------------------------------------------

        struct Memory
        {
            std::string content;
            std::string emotion;
            std::string type = "RELATIONSHIP";
            float       importance = kDefaultImportance;
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
                        memory.importance = std::clamp(answer["importance_score"].get<float>(), 0.0f, 1.0f);
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

        void StoreMemory(const Scene& a_scene, const Participant& a_owner, const std::string& a_response)
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
            // The dashboard's memory list shows the emotion under the importance but not the tags,
            // so the marker goes there: "joyful [Sexual History]".
            memory.emotion = memory.emotion.empty() ? kDashboardMarker : memory.emotion + " " + kDashboardMarker;
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
            logger::info("Scene ({}) of {} actor(s): {} event(s), {} detail(s), {} climax(es)", a_scene->framework,
                a_scene->actors.size(), events.size(), a_scene->details.size(), a_scene->climaxes.size());

            // Everyone who took part gets a memory, the player too: SkyrimNet reads the player's
            // memories when it writes the player's lines and thoughts.
            for (const auto& owner : a_scene->actors) {
                const auto context = Dump(BuildContext(*a_scene, owner, events, memories));
                const bool queued = PublicSendCustomPromptToLLM(kPromptName, "", context.c_str(),
                    [a_scene, owner](const char* a_response, int a_success) {
                        const std::string response = a_response ? a_response : "";
                        if (a_success == 1) {
                            StoreMemory(*a_scene, owner, response);
                        } else {
                            logger::error("Memory for {} failed: {}", owner.name, response);
                        }
                    });
                if (!queued) {
                    logger::error("Could not queue the memory for {}", owner.name);
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
    }

    bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm)
    {
        a_vm->RegisterFunction("SceneStarted", kScriptName, SceneStarted);
        a_vm->RegisterFunction("SceneDetail", kScriptName, SceneDetail);
        a_vm->RegisterFunction("Climax", kScriptName, Climax);
        a_vm->RegisterFunction("SceneEnded", kScriptName, SceneEnded);
        return true;
    }

    void Reset()
    {
        std::scoped_lock lock(g_lock);
        g_scenes.clear();
    }
}
