#include "fmt/riddle_soft/pac_archive_decoder.h"
#include "test_support/archive_support.h"
#include "test_support/catch.hh"
#include "test_support/file_support.h"

using namespace au;
using namespace au::fmt::riddle_soft;

TEST_CASE("RiddleSoft PAC archives", "[fmt]")
{
    std::vector<std::shared_ptr<File>> expected_files
    {
        tests::stub_file("123.txt", "1234567890"_b),
        tests::stub_file("abc.txt", "abcdefghijklmnopqrstuvwxyz"_b),
    };

    PacArchiveDecoder decoder;
    auto actual_files = tests::unpack_to_memory(
        "tests/fmt/riddle_soft/files/pac/test.pac", decoder);

    tests::compare_files(expected_files, actual_files, true);
}