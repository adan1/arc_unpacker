// PRS image
//
// Company:   Ivory
// Engine:    MarbleEngine
// Extension: -
// Archives:  MBL
//
// Known games:
// - Candy Toys
// - Wanko to Kurasou

#include "fmt/ivory/prs_converter.h"
#include "util/image.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::ivory;

static const bstr magic = "YB"_b;

static bstr decode_pixels(const bstr &source, size_t width, size_t height)
{
    bstr target;
    target.resize(width * height * 3);
    u8 *target_ptr = target.get<u8>();
    u8 *target_end = target_ptr + target.size();
    const u8 *source_ptr = source.get<const u8>();
    const u8 *source_end = source_ptr + source.size();

    int flag = 0;
    int size_lookup[256];
    for (auto i : util::range(256))
        size_lookup[i] = i + 3;
    size_lookup[0xFF] = 0x1000;
    size_lookup[0xFE] = 0x400;
    size_lookup[0xFD] = 0x100;

    while (source_ptr < source_end && target_ptr < target_end)
    {
        flag <<= 1;
        if ((flag & 0xFF) == 0)
        {
            flag = *source_ptr++;
            flag <<= 1;
            flag += 1;
        }

        if ((flag & 0x100) != 0x100)
        {
            *target_ptr++ = *source_ptr++;
        }
        else
        {
            int tmp = *source_ptr++;
            size_t size = 0;
            size_t shift = 0;

            if (tmp & 0x80)
            {
                shift = (*source_ptr++) | ((tmp & 0x3F) << 8);
                if (tmp & 0x40)
                {
                    if (source_ptr >= source_end)
                        break;
                    auto index = static_cast<size_t>(*source_ptr++);
                    size = size_lookup[index];
                }
                else
                {
                    size = (shift & 0xF) + 3;
                    shift >>= 4;
                }
            }
            else
            {
                size = tmp >> 2;
                tmp &= 3;
                if (tmp == 3)
                {
                    size += 9;
                    for (auto i : util::range(size))
                    {
                        if (source_ptr >= source_end
                            || target_ptr >= target_end)
                        {
                            break;
                        }
                        *target_ptr++ = *source_ptr++;
                    }
                    continue;
                }
                shift = size;
                size = tmp + 2;
            }

            shift += 1;
            for (auto i : util::range(size))
            {
                if (target_ptr >= target_end)
                    break;
                if (target_ptr - shift < target.get<u8>())
                    throw std::runtime_error("Invalid shift value");
                *target_ptr = *(target_ptr - shift);
                target_ptr++;
            }
        }
    }
    return target;
}

bool PrsConverter::is_recognized_internal(File &file) const
{
    return file.io.read(magic.size()) == magic;
}

std::unique_ptr<File> PrsConverter::decode_internal(File &file) const
{
    file.io.skip(magic.size());

    bool using_differences = file.io.read_u8() > 0;
    if (file.io.read_u8() != 3)
        throw std::runtime_error("Unknown PRS version");

    u32 source_size = file.io.read_u32_le();
    file.io.skip(4);
    u16 width = file.io.read_u16_le();
    u16 height = file.io.read_u16_le();

    auto target = decode_pixels(file.io.read(source_size), width, height);

    if (using_differences)
        for (auto i : util::range(3, target.size()))
            target[i] += target[i - 3];

    pix::Grid pixels(width, height, target, pix::Format::BGR888);
    return util::Image::from_pixels(pixels)->create_file(file.name);
}

static auto dummy = fmt::Registry::add<PrsConverter>("ivory/prs");
