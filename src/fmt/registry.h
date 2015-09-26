#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace au {
namespace fmt {

    class AbstractDecoder;

    class Registry final
    {
    private:
        using DecoderCreator
            = std::function<std::unique_ptr<AbstractDecoder>()>;

    public:
        static Registry &instance();
        const std::vector<std::string> get_names() const;
        std::unique_ptr<AbstractDecoder> create(const std::string &name) const;

        template<typename T> static bool add(const std::string &name)
        {
            Registry::instance().add([]()
            {
                return std::unique_ptr<AbstractDecoder>(new T());
            }, name);
            return true;
        }

    private:
        Registry();
        ~Registry();

        void add(DecoderCreator creator, const std::string &name);

        struct Priv;
        std::unique_ptr<Priv> p;
    };

} }
