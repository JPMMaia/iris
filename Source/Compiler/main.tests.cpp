#include <catch2/catch_session.hpp>

import iris.common;

int main(int argc, char* argv[])
{
    iris::common::install_abort_handlers();

    int const result = Catch::Session().run(argc, argv);
    return result;
}
