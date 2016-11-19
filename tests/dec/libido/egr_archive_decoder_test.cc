// Copyright (C) 2016 by rr-
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

#include "dec/libido/egr_archive_decoder.h"
#include "algo/range.h"
#include "test_support/catch.h"
#include "test_support/decoder_support.h"
#include "test_support/file_support.h"
#include "test_support/image_support.h"

using namespace au;
using namespace au::dec::libido;

static const std::string dir = "tests/dec/libido/files/egr/";

static void do_test(
    const std::string &input_path,
    const std::vector<std::shared_ptr<io::File>> &expected_files)
{
    const auto decoder = EgrArchiveDecoder();
    const auto input_file = tests::zlib_file_from_path(dir + input_path);
    const auto actual_files = tests::unpack(decoder, *input_file);
    tests::compare_images(actual_files, expected_files, false);
}

TEST_CASE("Libido EGR image archives", "[dec]")
{
    SECTION("Unencrypted")
    {
        do_test(
            "test-zlib.EGR",
            {
                tests::file_from_path(dir + "Image000-out.png"),
                tests::file_from_path(dir + "Image001-out.png"),
            });
    }
}
