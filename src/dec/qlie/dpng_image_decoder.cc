#include "dec/qlie/dpng_image_decoder.h"
#include "algo/range.h"
#include "dec/png/png_image_decoder.h"
#include "io/memory_stream.h"

using namespace au;
using namespace au::dec::qlie;

static const bstr magic = "DPNG"_b;

bool DpngImageDecoder::is_recognized_impl(io::File &input_file) const
{
    return input_file.stream.read(magic.size()) == magic;
}

res::Image DpngImageDecoder::decode_impl(
    const Logger &logger, io::File &input_file) const
{
    input_file.stream.skip(magic.size());

    input_file.stream.skip(4);
    size_t file_count = input_file.stream.read_u32_le();
    size_t width = input_file.stream.read_u32_le();
    size_t height = input_file.stream.read_u32_le();

    const auto png_image_decoder = dec::png::PngImageDecoder();

    res::Image image(width, height);
    for (auto i : algo::range(file_count))
    {
        size_t subimage_x = input_file.stream.read_u32_le();
        size_t subimage_y = input_file.stream.read_u32_le();
        size_t subimage_width = input_file.stream.read_u32_le();
        size_t subimage_height = input_file.stream.read_u32_le();
        size_t subimage_data_size = input_file.stream.read_u32_le();
        input_file.stream.skip(8);

        if (!subimage_data_size)
            continue;

        io::File tmp_file;
        tmp_file.stream.write(input_file.stream.read(subimage_data_size));
        const auto subimage = png_image_decoder.decode(logger, tmp_file);
        image.paste(subimage, subimage_x, subimage_y);
    }

    return image;
}

static auto _ = dec::register_decoder<DpngImageDecoder>("qlie/dpng");