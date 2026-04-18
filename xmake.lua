set_project("cpp-fsrs")
set_version("0.1.0")
set_languages("c++23")
set_warnings("all", "extra")
add_rules("mode.debug", "mode.release")

add_requires("nlohmann_json 3.x", "catch2 3.x")

-- Core: headers only, no deps. Downstream includers of <fsrs/json.hpp>
-- must pull nlohmann_json themselves.
target("fsrs")
    set_kind("headeronly")
    add_includedirs("include", { public = true })
    add_headerfiles("include/(fsrs/**.hpp)")

-- Convenience target: core + nlohmann_json as a public package.
target("fsrs-json")
    set_kind("headeronly")
    add_deps("fsrs")
    add_packages("nlohmann_json", { public = true })

-- Catch2 test binary. `xmake test` hooks in via add_tests.
target("cpp-fsrs-tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/*.cpp")
    add_deps("fsrs-json")
    add_packages("catch2")
    add_tests("default")

-- Quickstart example — built on demand, not by default.
target("quickstart")
    set_kind("binary")
    set_default(false)
    add_files("examples/quickstart.cpp")
    add_deps("fsrs")
