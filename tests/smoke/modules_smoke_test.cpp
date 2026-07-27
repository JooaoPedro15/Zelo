#include <catch2/catch_test_macros.hpp>
#include <collectors/collectors.hpp>
#include <commands/commands.hpp>
#include <core/core.hpp>
#include <scanner/scanner.hpp>
#include <storage/storage.hpp>

TEST_CASE("todos os modulos linkam e respondem", "[smoke]") {
    CHECK(zelo::core::module_name() == "core");
    CHECK(zelo::collectors::module_name() == "collectors");
    CHECK(zelo::scanner::module_name() == "scanner");
    CHECK(zelo::commands::module_name() == "commands");
    CHECK(zelo::storage::module_name() == "storage");
}
