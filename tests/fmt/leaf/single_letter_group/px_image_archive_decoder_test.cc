#include "fmt/leaf/single_letter_group/px_image_archive_decoder.h"
#include "test_support/catch.hh"
#include "test_support/decoder_support.h"
#include "test_support/file_support.h"
#include "test_support/image_support.h"

using namespace au;
using namespace au::fmt::leaf;

static const std::string dir = "tests/fmt/leaf/files/px/";

static void do_test(
    const std::string &input_path,
    const std::vector<std::string> &expected_paths)
{
    const PxImageArchiveDecoder decoder;
    const auto input_file = tests::zlib_file_from_path(dir + input_path);
    const auto actual_files = tests::unpack(decoder, *input_file);
    std::vector<std::shared_ptr<pix::Grid>> expected_images;
    for (auto &path : expected_paths)
        expected_images.push_back(tests::image_from_path(dir + path));
    tests::compare_images(expected_images, actual_files);
}

TEST_CASE("Leaf PX images", "[fmt]")
{
    SECTION("1.8 encoding")
    {
        do_test("fonts-zlib.px", {"fonts_000-out.png", "fonts_001-out.png"});
    }

    SECTION("1.32 encoding")
    {
        do_test("bg097-zlib.px", {"bg097-out.png"});
    }

    SECTION("4.9 encoding")
    {
        do_test(
            "inputName-zlib.px",
            {
                "inputName_000-out.png",
                "inputName_001-out.png",
                "inputName_002-out.png",
            });
    }

    SECTION("4.32 encoding")
    {
        do_test("poi102-zlib.px", {"poi102-out.png"});
    }

    SECTION("4.48 encoding")
    {
        do_test("common_031-zlib.px", {"common_031-out.png"});
    }

    SECTION("7.1 encoding")
    {
        do_test("thumbnail0_001-zlib.px", {"thumbnail0_001-out.png"});
    }
}
