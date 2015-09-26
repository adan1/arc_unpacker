#pragma once

#include "fmt/archive_decoder.h"

namespace au {
namespace fmt {
namespace touhou {

    class Pbg3Archive final : public ArchiveDecoder
    {
    public:
        Pbg3Archive();
        ~Pbg3Archive();
    protected:
        bool is_recognized_internal(File &) const override;
        void unpack_internal(File &, FileSaver &) const override;
    private:
        struct Priv;
        std::unique_ptr<Priv> p;
    };

} } }
