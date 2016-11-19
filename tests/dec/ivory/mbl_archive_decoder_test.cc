﻿// Copyright (C) 2016 by rr-
//
// This file is part of arc_unpacker.
//
// arc_unpacker is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at
// your option) any later version.
//
// arc_unpacker is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with arc_unpacker. If not, see <http://www.gnu.org/licenses/>.

#include "dec/ivory/mbl_archive_decoder.h"
#include "test_support/catch.h"
#include "test_support/decoder_support.h"
#include "test_support/file_support.h"

using namespace au;
using namespace au::dec::ivory;

static const std::string dir = "tests/dec/ivory/files/mbl/";

static void do_test(
    const std::string &input_path,
    const std::vector<std::shared_ptr<io::File>> &expected_files,
    const std::string &plugin_name = "")
{
    MblArchiveDecoder decoder;
    if (!plugin_name.empty())
        decoder.plugin_manager.set(plugin_name);
    const auto input_file = tests::file_from_path(dir + input_path);
    const auto actual_files = tests::unpack(decoder, *input_file);
    tests::compare_files(actual_files, expected_files, true);
}

TEST_CASE("Ivory MBL archives", "[dec]")
{
    SECTION("Version 1")
    {
        do_test(
            "mbl-v1.mbl",
            {
                tests::stub_file("abc.txt", "abc"_b),
                tests::stub_file(u8"テスト", "AAAAAAAAAAAAAAAA"_b),
            });
    }

    SECTION("Version 2")
    {
        do_test(
            "mbl-v2.mbl",
            {
                tests::stub_file("abc.txt", "abc"_b),
                tests::stub_file(u8"テスト", "AAAAAAAAAAAAAAAA"_b),
            });
    }

    SECTION("Dialogs, Candy Toys encryption")
    {
        do_test(
            "mg_data-candy.mbl",
            {tests::file_from_path(dir + "MAIN-candy", "MAIN")},
            "candy");
    }

    SECTION("Dialogs, Wanko to Kurasou encryption")
    {
        do_test(
            "mg_data-wanko.mbl",
            {tests::file_from_path(dir + "TEST-wanko", "TEST")},
            "wanko");
    }
}
