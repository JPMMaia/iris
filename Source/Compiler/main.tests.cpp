#include <catch2/catch_session.hpp>

import h.common;

int main(int argc, char* argv[])
{
    h::common::install_abort_handlers();

    int const result = Catch::Session().run(argc, argv);
    return result;
}
