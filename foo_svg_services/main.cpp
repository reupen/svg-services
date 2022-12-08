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

DECLARE_COMPONENT_VERSION("SVG services", version, "compiled: " COMPILATION_DATE);

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

class SVGDocumentImpl : public svg_document {
public:
    explicit SVGDocumentImpl(resvg_render_tree* tree) : m_tree(tree){};

    [[nodiscard]] Size get_size() const noexcept override
    {
        const auto size = resvg_get_image_size(m_tree);
        return {size.width, size.height};
    }

    [[nodiscard]] Rect get_view_box() const noexcept override
    {
        const auto view_box = resvg_get_image_viewbox(m_tree);
        return {view_box.x, view_box.y, view_box.width, view_box.height};
    }

    void render(int output_width, int output_height, Position position, ScalingMode scaling_mode,
        PixelFormat output_pixel_format, void* output_buffer, size_t output_buffer_size) const override
    {
        if (output_width == 0 || output_height == 0)
            return;

        const auto size = get_size();
        const auto wider_than_original = static_cast<float>(size.width) / static_cast<float>(size.height)
            < static_cast<float>(output_width) / static_cast<float>(output_height);

        const auto [width_scaling_factor, height_scaling_factor]
            = [output_width, output_height, size, scaling_mode, wider_than_original]() {
                  const auto width_ratio = static_cast<double>(output_width) / static_cast<double>(size.width);
                  const auto height_ratio = static_cast<double>(output_height) / static_cast<double>(size.height);

                  if (scaling_mode == ScalingMode::None)
                      return std::make_tuple(1.0, 1.0);

                  if (scaling_mode == ScalingMode::Zoom) {
                      auto scaling_factor = wider_than_original ? width_ratio : height_ratio;
                      return std::make_tuple(scaling_factor, scaling_factor);
                  }

                  if (scaling_mode == ScalingMode::Fit) {
                      auto scaling_factor = wider_than_original ? height_ratio : width_ratio;
                      return std::make_tuple(scaling_factor, scaling_factor);
                  }

                  return std::make_tuple(width_ratio, height_ratio);
              }();

        const auto transform = resvg_transform{width_scaling_factor, 0, 0, height_scaling_factor,
            position == Position::TopLeft ? 0.0 : (output_width - width_scaling_factor * size.width) / 2.0,
            position == Position::TopLeft ? 0.0 : (output_height - height_scaling_factor * size.height) / 2.0};

        resvg_render(m_tree, resvg_fit_to{}, transform, output_width, output_height, static_cast<char*>(output_buffer));

        if (output_pixel_format == PixelFormat::BGRA)
            prgba_to_bgra(output_buffer, output_width, output_height);
    }

private:
    resvg_render_tree* m_tree{};
};

class SVGServicesImpl : public svg_services {
    svg_document::ptr open(const void* svg_data, size_t svg_data_size) const override
    {
        resvg_options* opt = resvg_options_create();

        auto _ = gsl::finally([opt] { resvg_options_destroy(opt); });

        resvg_render_tree* tree{};
        check_resvg_result(resvg_parse_tree_from_data(static_cast<const char*>(svg_data), svg_data_size, opt, &tree));

        return fb2k::service_new<SVGDocumentImpl>(tree);
    }
};

namespace {
service_factory_t<SVGServicesImpl> _;
}

} // namespace svg_services