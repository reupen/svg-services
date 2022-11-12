#define NOMINMAX

#include <algorithm>
#include <ranges>

#include <gsl/gsl>

#include "../resvg/c-api/resvg.h"

#include "../pfc/pfc.h"

#include "../foobar2000/SDK/foobar2000.h"

#include "../api/api.h"
#include "version.h"

namespace svg_services {

DECLARE_COMPONENT_VERSION("SVG services", svg_services::version, "compiled: " COMPILATION_DATE);

const std::unordered_map<int32_t, const char*> resvg_error_messages = {
    {RESVG_ERROR_NOT_AN_UTF8_STR, "Only UTF-8 files are supported"},
    {RESVG_ERROR_FILE_OPEN_FAILED, "Failed to open the file"},
    {RESVG_ERROR_MALFORMED_GZIP, "Compressed SVGs must use Gzip"},
    {RESVG_ERROR_ELEMENTS_LIMIT_REACHED, "Maximum number of elements allowed exceeded"},
    {RESVG_ERROR_INVALID_SIZE, "SVG dimensions are invalid"}, {RESVG_ERROR_PARSING_FAILED, "Failed to parse the SVG"}};

class exception_resvg : public exception_svg_services {
public:
    exception_resvg(int32_t code)
        : exception_svg_services(
            resvg_error_messages.contains(code) ? resvg_error_messages.at(code) : "Unknown resvg error")
        , m_code(code)
    {
    }
    [[nodiscard]] int32_t code() const { return m_code; }

private:
    int m_code;
};

void check_resvg_result(int32_t code)
{
    if (code != RESVG_OK)
        throw exception_resvg(code);
}

uint8_t remove_premultiplied_alpha(uint8_t value, uint8_t alpha)
{
    if (alpha == 0)
        return value;

    return gsl::narrow_cast<uint8_t>(
        std::clamp(std::lround(static_cast<float>(value) * 255.f / static_cast<float>(alpha)), 0l, 255l));
}

void prgba_to_bgra(void* data, int width, int height)
{
    const auto rows = static_cast<uint8_t*>(data);
    for (const auto row_index : std::ranges::views::iota(0, height)) {
        for (const auto column_index : std::ranges::views::iota(0, width)) {
            const auto row_offset = row_index * width * 4;
            const auto column_offset = column_index * 4;

            const auto r = rows[row_offset + column_offset];
            const auto g = rows[row_offset + column_offset + 1];
            const auto b = rows[row_offset + column_offset + 2];
            const auto a = rows[row_offset + column_offset + 3];

            rows[row_offset + column_offset] = remove_premultiplied_alpha(b, a);
            rows[row_offset + column_offset + 1] = remove_premultiplied_alpha(g, a);
            rows[row_offset + column_offset + 2] = remove_premultiplied_alpha(r, a);
        }
    }
}

class svg_renderer_impl : public svg_renderer {
    void render(const void* svg_data, size_t svg_data_size, int render_width, int render_height, void* output_buffer,
        size_t output_buffer_size) const override
    {
        resvg_options* opt = resvg_options_create();

        auto _ = gsl::finally([opt] { resvg_options_destroy(opt); });

        resvg_render_tree* tree{};
        check_resvg_result(resvg_parse_tree_from_data(static_cast<const char*>(svg_data), svg_data_size, opt, &tree));

        const auto size = resvg_get_image_size(tree);
        const auto fit_to_height = static_cast<float>(size.width) / static_cast<float>(size.height)
            < static_cast<float>(render_width) / static_cast<float>(render_height);
        const auto fit_to = resvg_fit_to{fit_to_height ? RESVG_FIT_TO_TYPE_HEIGHT : RESVG_FIT_TO_TYPE_WIDTH,
            static_cast<float>(fit_to_height ? render_height : render_width)};

        resvg_render(
            tree, fit_to, resvg_transform_identity(), render_width, render_height, static_cast<char*>(output_buffer));
        prgba_to_bgra(output_buffer, render_width, render_height);
    }
};

namespace {
service_factory_t<svg_renderer_impl> _;
}

} // namespace svg_services
