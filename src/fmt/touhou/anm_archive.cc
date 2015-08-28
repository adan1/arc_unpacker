// ANM file
//
// Company:   Team Shanghai Anime
// Engine:    -
// Extension: .anm
//
// Known games:
// - Touhou 06 - The Embodiment of Scarlet Devil
// - Touhou 07 - Perfect Cherry Blossom
// - Touhou 08 - Imperishable Night
// - Touhou 09 - Phantasmagoria of Flower View
// - Touhou 09.5 - Shoot the Bullet
// - Touhou 10 - Mountain of Faith
// - Touhou 11 - Subterranean Animism
// - Touhou 12 - Undefined Fantastic Object
// - Touhou 12.5 - Double Spoiler
// - Touhou 12.8 - Fairy Wars
// - Touhou 13 - Ten Desires
// - Touhou 14 - Double Dealing Character

#include <algorithm>
#include <map>
#include "fmt/touhou/anm_archive.h"
#include "util/format.h"
#include "util/image.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt;
using namespace au::fmt::touhou;

namespace
{
    struct TableEntry
    {
        size_t width;
        size_t height;
        size_t x, y;
        size_t format;
        std::string name;
        int version;
        size_t texture_offset;
        bool has_data;
    };

    using Table = std::vector<std::unique_ptr<TableEntry>>;
}

static const bstr texture_magic = "THTX"_b;

static std::string read_name(io::IO &file_io, size_t offset)
{
    std::string name;
    file_io.peek(offset, [&]() { name = file_io.read_to_zero().str(); });
    return name;
}

static size_t read_old_entry(
    TableEntry &entry, io::IO &file_io, size_t base_offset)
{
    file_io.skip(4); //sprite count
    file_io.skip(4); //script count
    file_io.skip(4); //zero

    entry.width = file_io.read_u32_le();
    entry.height = file_io.read_u32_le();
    entry.format = file_io.read_u32_le();
    entry.x = file_io.read_u16_le();
    entry.y = file_io.read_u16_le();
    //file_io.skip(4);

    size_t name_offset1 = base_offset + file_io.read_u32_le();
    file_io.skip(4);
    size_t name_offset2 = base_offset + file_io.read_u32_le();
    entry.name = read_name(file_io, name_offset1);

    entry.version = file_io.read_u32_le();
    file_io.skip(4);
    entry.texture_offset = base_offset + file_io.read_u32_le();
    entry.has_data = file_io.read_u32_le() > 0;

    return base_offset + file_io.read_u32_le();
}

static size_t read_new_entry(
    TableEntry &entry, io::IO &file_io, size_t base_offset)
{
    entry.version = file_io.read_u32_le();
    file_io.skip(2); //sprite count
    file_io.skip(2); //script count
    file_io.skip(2); //zero

    entry.width = file_io.read_u16_le();
    entry.height = file_io.read_u16_le();
    entry.format = file_io.read_u16_le();
    size_t name_offset = base_offset + file_io.read_u32_le();
    entry.name = read_name(file_io, name_offset);
    entry.x = file_io.read_u16_le();
    entry.y = file_io.read_u16_le();
    file_io.skip(4);

    entry.texture_offset = base_offset + file_io.read_u32_le();
    entry.has_data = file_io.read_u16_le() > 0;
    file_io.skip(2);

    return base_offset + file_io.read_u32_le();
}

static Table read_table(io::IO &file_io)
{
    Table table;
    u32 base_offset = 0;
    while (true)
    {
        std::unique_ptr<TableEntry> entry(new TableEntry);

        file_io.seek(base_offset);
        file_io.skip(8);
        bool use_old = file_io.read_u32_le() == 0;

        file_io.seek(base_offset);
        size_t next_offset = use_old
            ? read_old_entry(*entry, file_io, base_offset)
            : read_new_entry(*entry, file_io, base_offset);

        table.push_back(std::move(entry));
        if (next_offset == base_offset)
            break;
        base_offset = next_offset;
    }
    return table;
}

static void write_pixels(
    io::IO &file_io, TableEntry &entry, pix::Grid &pixels, size_t stride)
{
    if (!entry.has_data)
        return;

    file_io.seek(entry.texture_offset);
    if (file_io.read(texture_magic.size()) != texture_magic)
        throw std::runtime_error("Corrupt texture data");
    file_io.skip(2);
    auto format = file_io.read_u16_le();
    auto width = file_io.read_u16_le();
    auto height = file_io.read_u16_le();
    auto data_size = file_io.read_u32_le();
    auto data = file_io.read(data_size);
    auto data_ptr = data.get<const u8>();

    for (auto y : util::range(height))
    for (auto x : util::range(width))
    {
        pix::Pixel color;
        switch (format)
        {
            case 1:
                color = pix::read<pix::Format::BGRA8888>(data_ptr);
                break;

            case 3:
                color = pix::read<pix::Format::BGR565>(data_ptr);
                break;

            case 5:
                color = pix::read<pix::Format::BGRA4444>(data_ptr);
                break;

            case 7:
                color = pix::read<pix::Format::Gray8>(data_ptr);
                break;

            default:
                throw std::runtime_error(util::format(
                    "Unknown color format: %d", format));
        }

        pixels.at(x + entry.x, y + entry.y) = color;
    }
}

static std::unique_ptr<File> read_texture(io::IO &file_io, Table &entries)
{
    size_t width = 0;
    size_t height = 0;
    for (auto &entry : entries)
    {
        if (!entry->has_data)
            continue;
        file_io.peek(entry->texture_offset, [&]()
        {
            file_io.skip(texture_magic.size());
            file_io.skip(2);
            file_io.skip(2);
            size_t chunk_width = file_io.read_u16_le();
            size_t chunk_height = file_io.read_u16_le();
            width = std::max(width, entry->x + chunk_width);
            height = std::max(height, entry->y + chunk_height);
        });
    }
    if (!width || !height)
        return nullptr;

    pix::Grid pixels(width, height);
    for (auto &entry : entries)
        write_pixels(file_io, *entry, pixels, width);

    return util::Image::from_pixels(pixels)->create_file(entries[0]->name);
}

FileNamingStrategy AnmArchive::get_file_naming_strategy() const
{
    return FileNamingStrategy::Root;
}

bool AnmArchive::is_recognized_internal(File &arc_file) const
{
    return arc_file.has_extension("anm");
}

void AnmArchive::unpack_internal(File &arc_file, FileSaver &file_saver) const
{
    Table table = read_table(arc_file.io);

    std::map<std::string, Table> table_map;
    for (auto &entry : table)
        table_map[entry->name].push_back(std::move(entry));

    for (auto &kv : table_map)
    {
        auto &name = kv.first;
        auto &entries = kv.second;

        // Ignore both the scripts and sprites and extract raw texture data.
        auto file = read_texture(arc_file.io, entries);
        if (file != nullptr)
            file_saver.save(std::move(file));
    }
}

static auto dummy = fmt::Registry::add<AnmArchive>("th/anm");
