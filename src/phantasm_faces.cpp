#include "phantasm_faces.h"

#include "Phantasm_API.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

namespace
{
    // Shown at most about 120 CSS px on the map; drawn at twice that, the page scales down.
    constexpr std::uint32_t kSize = 384;  // the face is cut from it: room to spare
    constexpr std::uint32_t kMinVersion = 200;  // 0.2.0: the first Phantasm with Portrait stills

    const PHANTASM_API::Interface1* g_api = nullptr;
    PHANTASM_API::ClientId          g_client = 0;
    std::string                     g_version;

    // Only stills are asked for, which answer through their own callback: no view events to hear.
    void OnEvent(const PHANTASM_API::Event*, void*) {}

    // ---- PNG with stored (uncompressed) deflate blocks: a few small pictures, no library ----

    std::uint32_t Crc32(const std::uint8_t* a_data, std::size_t a_size, std::uint32_t a_crc)
    {
        static const auto table = []() {
            std::array<std::uint32_t, 256> t{};
            for (std::uint32_t n = 0; n < 256; ++n) {
                std::uint32_t c = n;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                }
                t[n] = c;
            }
            return t;
        }();
        std::uint32_t c = a_crc ^ 0xFFFFFFFFu;
        for (std::size_t i = 0; i < a_size; ++i) {
            c = table[(c ^ a_data[i]) & 0xFF] ^ (c >> 8);
        }
        return c ^ 0xFFFFFFFFu;
    }

    void Put32(std::vector<std::uint8_t>& a_out, std::uint32_t a_value)
    {
        a_out.push_back(static_cast<std::uint8_t>(a_value >> 24));
        a_out.push_back(static_cast<std::uint8_t>(a_value >> 16));
        a_out.push_back(static_cast<std::uint8_t>(a_value >> 8));
        a_out.push_back(static_cast<std::uint8_t>(a_value));
    }

    void Chunk(std::vector<std::uint8_t>& a_out, const char* a_type, const std::vector<std::uint8_t>& a_data)
    {
        Put32(a_out, static_cast<std::uint32_t>(a_data.size()));
        const auto start = a_out.size();
        a_out.insert(a_out.end(), a_type, a_type + 4);
        a_out.insert(a_out.end(), a_data.begin(), a_data.end());
        Put32(a_out, Crc32(a_out.data() + start, a_out.size() - start, 0));
    }

    // a_rgba: straight (not premultiplied) alpha, a_width * a_height * 4 bytes.
    std::vector<std::uint8_t> EncodePng(std::uint32_t a_width, std::uint32_t a_height, const std::vector<std::uint8_t>& a_rgba)
    {
        // scanlines, each with its filter byte (0: none)
        std::vector<std::uint8_t> raw;
        raw.reserve((static_cast<std::size_t>(a_width) * 4 + 1) * a_height);
        for (std::uint32_t y = 0; y < a_height; ++y) {
            raw.push_back(0);
            const auto row = a_rgba.begin() + static_cast<std::ptrdiff_t>(y) * a_width * 4;
            raw.insert(raw.end(), row, row + static_cast<std::ptrdiff_t>(a_width) * 4);
        }

        // zlib stream of stored blocks (65535 bytes each at most)
        std::vector<std::uint8_t> zlib = { 0x78, 0x01 };
        std::uint32_t             a = 1, b = 0;  // Adler-32
        for (const auto byte : raw) {
            a = (a + byte) % 65521;
            b = (b + a) % 65521;
        }
        for (std::size_t pos = 0; pos < raw.size() || pos == 0;) {
            const auto len = static_cast<std::uint16_t>(std::min<std::size_t>(65535, raw.size() - pos));
            const bool last = pos + len >= raw.size();
            zlib.push_back(last ? 1 : 0);
            zlib.push_back(static_cast<std::uint8_t>(len));
            zlib.push_back(static_cast<std::uint8_t>(len >> 8));
            zlib.push_back(static_cast<std::uint8_t>(~len));
            zlib.push_back(static_cast<std::uint8_t>(~len >> 8));
            zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
            pos += len;
            if (last) {
                break;
            }
        }
        Put32(zlib, (b << 16) | a);

        std::vector<std::uint8_t> png = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
        std::vector<std::uint8_t> header;
        Put32(header, a_width);
        Put32(header, a_height);
        header.insert(header.end(), { 8, 6, 0, 0, 0 });  // 8 bits, RGBA, deflate, no filter set, no interlace
        Chunk(png, "IHDR", header);
        Chunk(png, "IDAT", zlib);
        Chunk(png, "IEND", {});
        return png;
    }

    float ToLinear(std::uint8_t a_srgb)
    {
        const float c = a_srgb / 255.0f;
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    std::uint8_t ToSrgb(float a_linear)
    {
        const float c = a_linear <= 0.0031308f ? a_linear * 12.92f : 1.055f * std::pow(a_linear, 1.0f / 2.4f) - 0.055f;
        return static_cast<std::uint8_t>(std::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    // Phantasm's stills are premultiplied by alpha in linear light; PNG wants straight alpha.
    std::vector<std::uint8_t> Unpremultiply(const std::uint8_t* a_rgba, std::uint32_t a_width, std::uint32_t a_height)
    {
        std::vector<std::uint8_t> out(a_rgba, a_rgba + static_cast<std::size_t>(a_width) * a_height * 4);
        for (std::size_t i = 0; i < out.size(); i += 4) {
            const auto alpha = out[i + 3];
            if (alpha == 0) {
                out[i] = out[i + 1] = out[i + 2] = 0;
            } else if (alpha < 255) {
                const float scale = 255.0f / alpha;
                for (int k = 0; k < 3; ++k) {
                    out[i + k] = ToSrgb(ToLinear(out[i + k]) * scale);
                }
            }
        }
        return out;
    }

    // Where the figure starts from the top of the picture, as a fraction of its height (1: nothing drawn).
    double FigureTop(const std::vector<std::uint8_t>& a_rgba, std::uint32_t a_width, std::uint32_t a_height)
    {
        for (std::uint32_t y = 0; y < a_height; ++y) {
            for (std::uint32_t x = 0; x < a_width; ++x) {
                if (a_rgba[(static_cast<std::size_t>(y) * a_width + x) * 4 + 3] > 24) {
                    return static_cast<double>(y) / a_height;
                }
            }
        }
        return 1.0;
    }

    // Cuts the head and shoulders out of a picture: a square from the top of the figure (the top of the
    // head), centred on the figure's upper part; what falls outside the picture is transparent.
    // a_side: the square's size (a Cutout: from the standing height); 0: as wide as the figure, but
    // not narrower than a third of the picture (a Portrait). a_rgba: straight alpha.
    std::vector<std::uint8_t> CropToFigure(const std::vector<std::uint8_t>& a_rgba, std::uint32_t& a_width, std::uint32_t& a_height, int a_side)
    {
        const int  w = static_cast<int>(a_width), h = static_cast<int>(a_height);
        const auto alpha = [&](int x, int y) { return a_rgba[(static_cast<std::size_t>(y) * w + x) * 4 + 3]; };
        int        left = w, right = -1, top = h, bottom = -1;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (alpha(x, y) > 24) {
                    left = (std::min)(left, x);
                    right = (std::max)(right, x);
                    top = (std::min)(top, y);
                    bottom = (std::max)(bottom, y);
                }
            }
        }
        if (right < 0) {
            return a_rgba;  // nothing drawn: keep it as it is
        }
        const int side = a_side > 0 ? std::clamp(a_side, 16, (std::max)(w, h)) : std::clamp(right - left + 1, (std::max)(1, w / 3), w);
        // the centre of what is drawn in the upper part (the head), weighted by alpha
        double sum = 0, weight = 0;
        for (int y = top; y < (std::min)(h, top + side * 2 / 3); ++y) {
            for (int x = left; x <= right; ++x) {
                const auto a = alpha(x, y);
                sum += static_cast<double>(x) * a;
                weight += a;
            }
        }
        const int                 centre = weight > 0 ? static_cast<int>(sum / weight) : (left + right) / 2;
        const int                 x0 = centre - side / 2;
        const int                 y0 = top - side / 12;  // a little room above the head
        std::vector<std::uint8_t> out(static_cast<std::size_t>(side) * side * 4, 0);
        for (int y = 0; y < side; ++y) {
            const int sy = y0 + y;
            if (sy < 0 || sy >= h) {
                continue;
            }
            for (int x = 0; x < side; ++x) {
                const int sx = x0 + x;
                if (sx >= 0 && sx < w) {
                    std::copy_n(a_rgba.begin() + (static_cast<std::ptrdiff_t>(sy) * w + sx) * 4, 4,
                        out.begin() + (static_cast<std::ptrdiff_t>(y) * side + x) * 4);
                }
            }
        }
        a_width = a_height = static_cast<std::uint32_t>(side);
        return out;
    }

    struct Job
    {
        std::uint32_t                                  id;
        std::string                                    folder;
        std::string                                    tag;
        std::function<void(std::uint32_t, std::string)> done;
        bool                                           cutout = false;  // the second try: the whole figure
    };

    void OnStill(const PHANTASM_API::Still* a_still, void* a_user);

    // A Portrait is framed for the standing head height: someone seated comes out with the head low or
    // cut off at the bottom. Then the whole figure is drawn (a Cutout) and the head taken from it.
    constexpr std::uint32_t kCutoutWidth = 512;
    constexpr std::uint32_t kCutoutHeight = 768;

    // Game thread, once per still; the pixels live for the call only.
    void OnStill(const PHANTASM_API::Still* a_still, void* a_user)
    {
        std::unique_ptr<Job> job(static_cast<Job*>(a_user));
        if (!a_still || !a_still->ok || !a_still->rgba) {
            logger::warn("Phantasm did not draw {:08X}: {}", job->id, a_still && a_still->reason ? a_still->reason : "?");
            job->done(job->id, "");
            return;
        }
        std::uint32_t width = a_still->width;
        std::uint32_t height = a_still->height;
        auto          pixels = Unpremultiply(a_still->rgba, width, height);
        // a Cutout: head and shoulders are about 0.3 of the standing height
        const int side = job->cutout && a_still->standingPx > 0 ? static_cast<int>(a_still->standingPx * 0.3f) : 0;
        // the face is cut out and the file written off the game thread
        std::thread([job = std::move(job), pixels = std::move(pixels), width, height, side]() mutable {
            if (!job->cutout && FigureTop(pixels, width, height) > 1.0 / 3) {
                logger::info("Phantasm portrait of {:08X} has the head low (seated?): drawing the whole figure", job->id);
                job->cutout = true;
                std::uint32_t request = 0;
                const auto    result = g_api->RequestStillHinted(g_client, job->id, kCutoutWidth, kCutoutHeight,
                       PHANTASM_API::Framing::Cutout, nullptr, OnStill, job.get(), &request);
                if (result == PHANTASM_API::Result::Ok) {
                    job.release();  // OnStill owns it again
                    return;
                }
                logger::warn("Phantasm did not take the cutout of {:08X} ({}); keeping the portrait", job->id, static_cast<int>(result));
            }
            const auto name = std::format("face_{}_{:08X}.png", job->tag, job->id);
            pixels = CropToFigure(pixels, width, height, side);
            const auto png = EncodePng(width, height, pixels);
            std::ofstream out(std::filesystem::path(job->folder) / name, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
            if (!out) {
                logger::warn("Could not write {}", name);
                job->done(job->id, "");
                return;
            }
            logger::info("Phantasm drew {:08X}: {} ({}x{})", job->id, name, width, height);
            job->done(job->id, name);
        }).detach();
    }
}

namespace PhantasmFaces
{
    bool SetUp()
    {
        using namespace PHANTASM_API;
        const auto module = GetModuleHandleW(L"Phantasm.dll");
        if (!module) {
            logger::info("Phantasm: not installed");
            return false;
        }
        const auto get = reinterpret_cast<RequestPluginAPIFn>(GetProcAddress(module, "RequestPluginAPI"));
        const auto api = get ? static_cast<const Interface1*>(get(kInterface1)) : nullptr;
        if (!api) {
            logger::warn("Phantasm is installed but has no plugin API (before 0.2.0). Update SeverActions or Phantasm");
            return false;
        }
        const auto v = api->pluginVersion;
        g_version = std::format("{}.{}.{}", v / 10000, v / 100 % 100, v % 100);
        if (v < kMinVersion || api->size < offsetof(Interface1, RequestStillHinted) + sizeof(void*)) {
            logger::warn("Phantasm {} is too old for portraits (0.2.0 or later)", g_version);
            g_version.clear();
            return false;
        }
        if (const auto result = api->RegisterClient("sn_sexual_memories", OnEvent, nullptr, &g_client); result != Result::Ok) {
            logger::warn("Phantasm {} did not register the mod ({})", g_version, static_cast<int>(result));
            g_version.clear();
            return false;
        }
        g_api = api;
        return true;
    }

    bool Ready()
    {
        return g_api != nullptr;
    }

    const char* Version()
    {
        return g_version.c_str();
    }

    void Draw(const std::vector<std::uint32_t>& a_ids, std::string a_folder, std::string a_tag,
        std::function<void(std::uint32_t, std::string)> a_done)
    {
        if (!g_api) {
            return;
        }
        for (const auto id : a_ids) {
            auto          job = std::make_unique<Job>(Job{ id, a_folder, a_tag, a_done });
            std::uint32_t request = 0;
            // the default look: a slight turn, a little from above, the hearth light, no weapons
            const auto result = g_api->RequestStillHinted(g_client, id, kSize, kSize, PHANTASM_API::Framing::Portrait, nullptr, OnStill, job.get(), &request);
            if (result != PHANTASM_API::Result::Ok) {
                logger::warn("Phantasm did not take the portrait of {:08X} ({})", id, static_cast<int>(result));
                a_done(id, "");
                continue;
            }
            job.release();  // OnStill owns it now
        }
    }
}
