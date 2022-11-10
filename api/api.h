#pragma once

#include <foobar2000/SDK/foobar2000.h>

namespace svg_services {

PFC_DECLARE_EXCEPTION(exception_svg_services, pfc::exception,
                      "SVG services error");

class NOVTABLE svg_renderer : public service_base {
public:
  /**
   * \brief Render an SVG document
   *
   * \param svg_data Pointer to a buffer containing the SVG document,
   * UTF-8-encoded
   *
   * \param svg_data_size Size of the SVG data in the buffer in bytes
   * \param render_width The width to render the SVG at
   * \param render_height The height to render the SVG at
   * \param output_buffer Pointer to a writable buffer of at least size
   *                      render_width * render_height * 4.
   *                      This will receive a 32-bpp BGRA bitmap.
   * \param output_buffer_size Size of the output buffer in bytes
   */
  virtual void render(const void *svg_data, size_t svg_data_size,
                      int render_width, int render_height, void *output_buffer,
                      size_t output_buffer_size) const = 0;

  FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(svg_renderer)
};

DECLARE_CLASS_GUID(svg_renderer, 0x3a5b8268, 0x408c, 0x4955, 0xbe, 0x5d, 0xa5,
                   0x52, 0xf1, 0x2d, 0x64, 0xa5);

} // namespace svg_services
