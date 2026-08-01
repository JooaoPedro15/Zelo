#include <catch2/catch_test_macros.hpp>
#include <collectors/collectors.hpp>
#include <commands/commands.hpp>
#include <core/core.hpp>
#include <scanner/scanner.hpp>
#include <storage/storage.hpp>

TEST_CASE("todos os modulos linkam e respondem", "[smoke]") {
    CHECK(cleaner::core::module_name() == "core");
    CHECK(cleaner::collectors::module_name() == "collectors");
    CHECK(cleaner::scanner::module_name() == "scanner");
    CHECK(cleaner::commands::module_name() == "commands");
    CHECK(cleaner::storage::module_name() == "storage");
}
