#include <stdexcept>
#include <boost/filesystem.hpp>
#include "formats/kirikiri/xp3_filters/cxdec.h"

using namespace au;
using namespace au::fmt::kirikiri::xp3_filters;

static const size_t encryption_block_size = 4096;
static const std::string encryption_block_magic(
    "\x20\x45\x6E\x63\x72\x79\x70\x74\x69\x6F\x6E\x20\x63\x6F\x6E\x74", 16);

namespace
{
    class KeyDerivationError : public std::runtime_error
    {
    public:
        KeyDerivationError() : std::runtime_error("") { }
    };

    /**
     *A class used to derive the nontrivial key in CXDEC based encryption.
     */
    class KeyDeriver
    {
    public:
        KeyDeriver(
            const CxdecSettings &settings,
            const std::array<char, encryption_block_size> encryption_block);
        u32 derive(u32 seed, u32 parameter);
    private:
        const CxdecSettings &settings;
        const std::array<char, encryption_block_size> encryption_block;
        std::string shellcode;
        u32 seed;
        size_t parameter;
        u32 encryption_block_addr;

        void add_shellcode(const std::string &bytes);
        u32 rand();
        u32 derive_for_stage(size_t stage);
        u32 run_first_stage();
        u32 run_stage_strategy_0(size_t stage);
        u32 run_stage_strategy_1(size_t stage);
    };
}

static std::string u32_to_string(u32 value)
{
    return std::string(reinterpret_cast<char*>(&value), 4);
}

KeyDeriver::KeyDeriver(
    const CxdecSettings &settings,
    const std::array<char, encryption_block_size> encryption_block)
    : settings(settings), encryption_block(encryption_block)
{
    seed = 0;
    parameter = 0;
    encryption_block_addr = reinterpret_cast<size_t>(&encryption_block[0]);
}

u32 KeyDeriver::derive(u32 seed, u32 parameter)
{
    this->seed = seed;
    this->parameter = parameter;

    // What we do: we try to run a code a few times for different
    // "stages".  The first one to succeed yields the key.

    // This mechanism of figuring out the valid stage number is really
    // crappy, but it's important we do it this way. This is because we
    // initialize the seed only once, and even if we fail to get a
    // number from the given stage, the internal state of randomizer is
    // preserved to the next iteration.

    // Maintaining the randomizer state is essential for the decryption
    // to work.

    for (size_t stage = 5; stage >= 1; stage--)
    {
        try
        {
            return derive_for_stage(stage);
        }
        catch (KeyDerivationError)
        {
            continue;
        }
    }

    throw std::runtime_error(
        "Failed to derive the key from the parameter");
}

void KeyDeriver::add_shellcode(const std::string &bytes)
{
    // The execution for current stage must fail when we run code for too long.
    shellcode += bytes;
    if (shellcode.size() > 128)
        throw KeyDerivationError();
}

u32 KeyDeriver::rand()
{
    // This is a modified glibc LCG randomization routine. It is used to
    // make the key as random as possible for each file, which is supposed
    // to maximize confusion.
    u32 old_seed = seed;
    seed = (0x41C64E6D * old_seed) + 12345;
    return seed ^ (old_seed << 16) ^ (old_seed >> 16);
}

u32 KeyDeriver::derive_for_stage(size_t stage)
{
    shellcode = "";

    // push edi, push esi, push ebx, push ecx, push edx
    add_shellcode("\x57\x56\x53\x51\x52");

    // mov edi, dword ptr ss:[esp+18] (esp+18 == parameter)
    add_shellcode("\x86\x7c\x24\x18");

    u32 eax = run_stage_strategy_1(stage);

    // pop edx, pop ecx, pop ebx, pop esi, pop edi
    add_shellcode("\x5a\x59\x5b\x5e\x5f");

    // retn
    add_shellcode("\xc3");

    return eax;
}

u32 KeyDeriver::run_first_stage()
{
    size_t routine_number = settings.key_derivation_order1[rand() % 3];

    u32 eax;
    switch (routine_number)
    {
        case 0:
        {
            // mov eax, rand()
            add_shellcode("\xb8");
            u32 tmp = rand();
            add_shellcode(u32_to_string(tmp));
            eax = tmp;
            break;
        }

        case 1:
            // mov eax, edi
            add_shellcode("\xb8\xc7");
            eax = parameter;
            break;

        case 2:
        {
            // mov esi, &encryption_block
            add_shellcode("\xbe");
            add_shellcode(u32_to_string(encryption_block_addr));

            // mov eax, dword ptr ds:[esi+((rand() & 0x3ff) * 4]
            add_shellcode("\x8b\x86");
            u32 pos = (rand() & 0x3ff) * 4;
            add_shellcode(u32_to_string(pos));

            eax = *reinterpret_cast<const u32*>(&encryption_block[pos]);
            break;
        }

        default:
            throw std::runtime_error("Bad routine number");
    }

    return eax;
}

u32 KeyDeriver::run_stage_strategy_0(size_t stage)
{
    if (stage == 1)
        return run_first_stage();

    u32 eax = (rand() & 1)
        ? run_stage_strategy_1(stage - 1)
        : run_stage_strategy_0(stage - 1);

    size_t routine_number = settings.key_derivation_order2[rand() % 8];

    switch (routine_number)
    {
        case 0:
            // not eax
            add_shellcode("\xf7\xd0");
            eax ^= 0xffffffff;
            break;

        case 1:
            // dec eax
            add_shellcode("\x48");
            eax -= 1;
            break;

        case 2:
            // neg eax
            add_shellcode("\xf7\xd8");
            eax = static_cast<u32>(-static_cast<i32>(eax));
            break;

        case 3:
            // inc eax
            add_shellcode("\x40");
            eax += 1;
            break;

        case 4:
            // mov esi, &encryption_block
            add_shellcode("\xbe");
            add_shellcode(u32_to_string(encryption_block_addr));

            // and eax, 3ff
            #ifdef VISUAL_STUDIO_2015_CAME_OUT
                add_shellcode("\x25\xff\x03\x00\x00"_s);
            #else
                add_shellcode(std::string("\x25\xff\x03\x00\x00", 5));
            #endif

            // mov eax, dword ptr ds:[esi+eax*4]
            add_shellcode("\x8b\x04\x86");

            eax = *reinterpret_cast<const u32*>(
                &encryption_block[(eax & 0x3ff) * 4]);
            break;

        case 5:
        {
            // push ebx
            add_shellcode("\x53");

            // mov ebx, eax
            add_shellcode("\x89\xc3");

            // and ebx, aaaaaaaa
            add_shellcode("\x81\xe3\xaa\xaa\xaa\xaa");

            // and eax, 55555555
            add_shellcode("\x25\x55\x55\x55\x55");

            // shr ebx, 1
            add_shellcode("\xd1\xeb");

            // shl eax, 1
            add_shellcode("\xd1\xe0");

            // or eax, ebx
            add_shellcode("\x09\xd8");

            // pop ebx
            add_shellcode("\x5b");

            u32 ebx = eax;
            ebx &= 0xaaaaaaaa;
            eax &= 0x55555555;
            ebx >>= 1;
            eax <<= 1;
            eax |= ebx;
            break;
        }

        case 6:
        {
            // xor eax, rand()
            add_shellcode("\x35");
            u32 tmp = rand();
            add_shellcode(u32_to_string(tmp));

            eax ^= tmp;
            break;
        }

        case 7:
        {
            if (rand() & 1)
            {
                // add eax, rand()
                add_shellcode("\x05");
                u32 tmp = rand();
                add_shellcode(u32_to_string(tmp));

                eax += tmp;
            }
            else
            {
                // sub eax, rand()
                add_shellcode("\x2d");
                u32 tmp = rand();
                add_shellcode(u32_to_string(tmp));

                eax -= tmp;
            }
            break;
        }

        default:
            throw std::runtime_error("Bad routine number");
    }

    return eax;
}

u32 KeyDeriver::run_stage_strategy_1(size_t stage)
{
    if (stage == 1)
        return run_first_stage();

    // push ebx
    add_shellcode("\x53");

    u32 eax = (rand() & 1)
        ? run_stage_strategy_1(stage - 1)
        : run_stage_strategy_0(stage - 1);

    // mov ebx, eax
    add_shellcode("\x89\xc3");
    u32 ebx = eax;

    eax = (rand() & 1)
        ? run_stage_strategy_1(stage - 1)
        : run_stage_strategy_0(stage - 1);

    size_t routine_number = settings.key_derivation_order3[rand() % 6];
    switch (routine_number)
    {
        case 0:
        {
            // push ecx
            add_shellcode("\x51");

            // mov ecx, ebx
            add_shellcode("\x89\xd9");

            // and ecx, 0f
            add_shellcode("\x83\xe1\x0f");

            // shr eax, cl
            add_shellcode("\xd3\xe8");

            // pop ecx
            add_shellcode("\x59");

            u8 ecx = ebx & 0x0f;
            eax >>= ecx;
            break;
        }

        case 1:
        {
            // push ecx
            add_shellcode("\x51");

            // mov ecx, ebx
            add_shellcode("\x89\xd9");

            // and ecx, 0f
            add_shellcode("\x83\xe1\x0f");

            // shl eax, cl
            add_shellcode("\xd3\xe0");

            // pop ecx
            add_shellcode("\x59");

            u8 ecx = ebx & 0x0f;
            eax <<= ecx;
            break;
        }

        case 2:
            // add eax, ebx
            add_shellcode("\x01\xd8");
            eax += ebx;
            break;

        case 3:
            // neg eax
            add_shellcode("\xf7\xd8");
            // add eax, ebx
            add_shellcode("\x01\xd8");
            eax = ebx - eax;
            break;

        case 4:
            // imul eax, ebx
            add_shellcode("\x0f\xaf\xc3");
            eax *= ebx;
            break;

        case 5:
            // sub eax, ebx
            add_shellcode("\x29\xd8");
            eax -= ebx;
            break;

        default:
            throw std::runtime_error("Bad routine number");
    }

    // pop ebx
    add_shellcode("\x5b");

    return eax;
}

static void decrypt_chunk(
    KeyDeriver &key_deriver,
    io::IO &io,
    u32 hash,
    size_t base_offset,
    size_t size)
{
    u32 seed = hash & 0x7f;
    hash >>= 7;
    u32 ret0 = key_deriver.derive(seed, hash);
    u32 ret1 = key_deriver.derive(seed, hash ^ 0xffffffff);

    u8 xor0 = (ret0 >> 8) & 0xff;
    u8 xor1 = (ret0 >> 16) & 0xff;
    u8 xor2 = ret0 & 0xff;
    if (xor2 == 0)
        xor2 = 1;

    size_t offset0 = ret1 >> 16;
    size_t offset1 = ret1 & 0xffff;

    std::unique_ptr<char[]> data(new char[size]);
    char *ptr = data.get();
    io.seek(base_offset);
    io.read(ptr, size);

    if (offset0 >= base_offset && offset0 < base_offset + size)
        ptr[offset0 - base_offset] ^= xor0;

    if (offset1 >= base_offset && offset1 < base_offset + size)
        ptr[offset1 - base_offset] ^= xor1;

    for (size_t i = 0; i < size; i++)
        ptr[i] ^= xor2;

    io.seek(base_offset);
    io.write(ptr, size);
}

struct Cxdec::Priv
{
    std::array<char, encryption_block_size> encryption_block;
};

Cxdec::Cxdec() : p(new Priv())
{
}

Cxdec::~Cxdec()
{
}

void Cxdec::decode(File &file, u32 encryption_key) const
{
    CxdecSettings settings = get_settings();
    KeyDeriver key_deriver(settings, p->encryption_block);

    u32 hash = encryption_key;
    u32 key = (hash & settings.key1) + settings.key2;

    size_t size = file.io.size() > key ? key : file.io.size();

    decrypt_chunk(key_deriver, file.io, hash, 0, size);
    size_t offset = size;
    size = file.io.size() - offset;
    hash = (hash >> 16) ^ hash;
    decrypt_chunk(key_deriver, file.io, hash, offset, size);
}

void Cxdec::set_arc_path(const std::string &path)
{
    auto dir = boost::filesystem::path(path).parent_path();
    for (boost::filesystem::recursive_directory_iterator it(dir);
        it != boost::filesystem::recursive_directory_iterator();
        it++)
    {
        if (!boost::filesystem::is_regular_file(it->path()))
            continue;

        auto fn = it->path().string();
        if (fn.find(".tpm") != fn.length() - 4)
            continue;

        io::FileIO tmp_io(it->path(), io::FileMode::Read);
        std::string content = tmp_io.read_until_end();
        auto pos = content.find(encryption_block_magic);
        if (pos == std::string::npos)
        {
            throw std::runtime_error(
                "Encryption file found, but without encryption block");
        }

        if (pos + encryption_block_size > content.size())
            throw std::runtime_error("Encryption block found, but truncated");

        for (size_t i = 0; i < encryption_block_size; i++)
            p->encryption_block[i] = content[pos + i];
        return;
    }

    throw std::runtime_error("Encryption file not found");
}