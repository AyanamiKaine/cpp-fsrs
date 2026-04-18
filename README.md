# cpp-fsrs

Header-only C++23 port of [py-fsrs](https://github.com/open-spaced-repetition/py-fsrs) **v6.3.1**, the official Python implementation of the Free Spaced Repetition Scheduler (FSRS) algorithm.

Drop it into any project to build spaced-repetition systems (Anki-style flashcard apps, language-learning tools, etc.). Numerically faithful to py-fsrs — the same rating sequence produces the same interval history.

The upstream reference is tracked as a git submodule at `third_party/py-fsrs/` (pinned to `v6.3.1`) and used by the test suite as a numerical oracle. Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/AyanamiKaine/cpp-fsrs.git
# or, on an existing clone:
git submodule update --init --recursive
```

## Requirements

- A C++23 compiler (GCC 15+ or Clang 18+).
- [nlohmann/json](https://github.com/nlohmann/json) (only if you use `<fsrs/json.hpp>`).

## Installation

### Plain `-I`

The whole library lives under `include/`. Just add it to your include path:

```bash
g++ -std=c++23 -I path/to/cpp-fsrs/include your_app.cpp -o your_app
```

### xmake

```lua
-- your project's xmake.lua
add_includedirs("path/to/cpp-fsrs/include")

-- if you use JSON serialization:
add_requires("nlohmann_json 3.x")
target("your_app")
    add_packages("nlohmann_json")
```

Or, if you depend on the library via a sibling `cpp-fsrs/` directory with its own `xmake.lua`, add:

```lua
includes("path/to/cpp-fsrs")

target("your_app")
    add_deps("fsrs")         -- core, no JSON
    -- or:  add_deps("fsrs-json")   -- core + nlohmann/json
```

### CMake

A `CMakeLists.txt` is provided that exposes two `INTERFACE` targets:

| Target               | What you get                           |
|----------------------|----------------------------------------|
| `fsrs::fsrs`         | Core library only (no deps)            |
| `fsrs::fsrs-json`    | Core + nlohmann/json for serialization |

Requires CMake 3.20+ and a C++23-capable compiler. `cxx_std_23` is propagated to consumers via `target_compile_features`.

**As a submodule / vendored copy:**

```cmake
add_subdirectory(third_party/cpp-fsrs)

target_link_libraries(your_app PRIVATE fsrs::fsrs)
# or, if you want JSON:
target_link_libraries(your_app PRIVATE fsrs::fsrs-json)
```

**Via `FetchContent`:**

```cmake
include(FetchContent)
FetchContent_Declare(cpp-fsrs
    GIT_REPOSITORY https://github.com/AyanamiKaine/cpp-fsrs.git
    GIT_TAG v0.1.0)
FetchContent_MakeAvailable(cpp-fsrs)

target_link_libraries(your_app PRIVATE fsrs::fsrs)
```

**Via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake):**

```cmake
CPMAddPackage("gh:AyanamiKaine/cpp-fsrs@0.1.0")
target_link_libraries(your_app PRIVATE fsrs::fsrs)
```

**After `make install` (system-wide):**

```cmake
find_package(cpp-fsrs 0.1 REQUIRED)
target_link_libraries(your_app PRIVATE fsrs::fsrs)
```

If you don't need JSON, pass `-DCPP_FSRS_WITH_JSON=OFF` when configuring to skip the nlohmann/json lookup entirely.

### NixOS

A `flake.nix` is provided. `nix develop` drops you into a shell with xmake, GCC 15, clang, Python 3 (for using py-fsrs as a test oracle), and nlohmann/json / Catch2 as fallbacks.

## Quickstart

```cpp
#include <fsrs/fsrs.hpp>
#include <print>

int main() {
    fsrs::Scheduler scheduler;
    fsrs::Card      card;

    // Rate the card. Default review time is "now" (UTC).
    auto [reviewed, log] = scheduler.review_card(card, fsrs::Rating::Good);

    std::println("Due at {}", reviewed.due);
}
```

### Ratings

```cpp
fsrs::Rating::Again  // forgot the card
fsrs::Rating::Hard   // remembered with serious difficulty
fsrs::Rating::Good   // remembered after a hesitation
fsrs::Rating::Easy   // remembered easily
```

### Card states

```cpp
fsrs::State::Learning     // new card being studied
fsrs::State::Review       // graduated from Learning
fsrs::State::Relearning   // lapsed from Review (after Again)
```

## Time handling

`cpp-fsrs` uses `std::chrono::sys_time<std::chrono::microseconds>` everywhere a datetime appears. That type is UTC by construction in C++20+, so the py-fsrs "datetime must be timezone-aware and UTC" rule is enforced by the type system — there's no runtime check because there can't be anything else.

```cpp
using TP = fsrs::Card::TimePoint;  // sys_time<microseconds>

TP when = std::chrono::sys_days{std::chrono::year{2025}/3/14}
        + std::chrono::hours{15} + std::chrono::minutes{9};

auto [card, log] = scheduler.review_card(my_card, fsrs::Rating::Good, when);
```

## Custom scheduler parameters

```cpp
using namespace std::chrono_literals;

fsrs::Scheduler::Config cfg{
    .parameters        = fsrs::Scheduler::DEFAULT_PARAMETERS,  // 21 floats
    .desired_retention = 0.9,
    .learning_steps    = {1min, 10min},
    .relearning_steps  = {10min},
    .maximum_interval  = 36500,
    .enable_fuzzing    = true,
};

fsrs::Scheduler scheduler{cfg};
```

- `parameters` — 21 FSRS weights. See py-fsrs docs for what each one does; if you're not optimizing, leave the defaults.
- `desired_retention` — value in [0, 1]. Higher = more reviews, better retention.
- `learning_steps` / `relearning_steps` — short-term intervals for the Learning and Relearning states.
- `maximum_interval` — cap in days.
- `enable_fuzzing` — add small random jitter to scheduled intervals.

## Retrievability

The predicted probability of correctly recalling the card right now (or at a given time):

```cpp
double p = scheduler.get_card_retrievability(card);
// or with an explicit datetime:
double p = scheduler.get_card_retrievability(card, when);
```

Returns 0 for cards that have never been reviewed.

## Rescheduling after parameter changes

If you re-tune your scheduler and want to replay a card's history under the new parameters:

```cpp
std::vector<fsrs::ReviewLog> logs = /* card's past reviews */;
fsrs::Card rescheduled = scheduler.reschedule_card(card, logs);
```

Log order doesn't matter — `reschedule_card` sorts by `review_datetime` internally. A log with a mismatched `card_id` throws `std::invalid_argument`.

## JSON serialization

Opt-in; pull in via a separate header so users who don't serialize don't pay for the nlohmann/json dependency.

```cpp
#include <fsrs/json.hpp>

std::string card_json = fsrs::to_json(card);           // compact
std::string pretty    = fsrs::to_json(card, /*indent=*/2);

// Non-throwing parse — returns std::expected<Card, ParseError>:
auto r = fsrs::card_from_json(card_json);
if (!r) {
    std::println("parse failed: {}", r.error().message);
} else {
    fsrs::Card back = *r;
}
```

Same shape for `Scheduler` (`to_json(scheduler)`, `scheduler_from_json(...)`) and `ReviewLog` (`to_json(log)`, `review_log_from_json(...)`).

The JSON schema is byte-compatible with py-fsrs: ISO-8601 datetimes with `+00:00` offset, `learning_steps`/`relearning_steps` as integer seconds, `review_duration` as integer milliseconds. A Card JSON produced by py-fsrs round-trips cleanly through cpp-fsrs.

## Error handling

Two conventions, both intentional:

- **`std::expected<T, ValidationError>`** for user input. `Scheduler::make(cfg)` returns an expected; errors accumulate all out-of-bounds parameters (not just the first).
- **Exceptions** for programmer errors. The throwing constructor `Scheduler(cfg)` throws `ValidationError` on invalid config; `reschedule_card` throws `std::invalid_argument` on mismatched `card_id`. Both `ValidationError` and `ParseError` derive from `std::exception` — catch as `const std::exception&` if you want.

```cpp
auto r = fsrs::Scheduler::make(cfg);
if (!r) {
    std::println("invalid config: {}", r.error().message);
    for (const auto& detail : r.error().details) std::println("  - {}", detail);
    return 1;
}
fsrs::Scheduler scheduler = std::move(*r);
```

## Deterministic fuzzing (reproducible tests)

`review_card` has an overload taking any `std::uniform_random_bit_generator` — inject a seeded `mt19937` to make fuzzed intervals reproducible:

```cpp
std::mt19937 rng{42};
auto [card, log] = scheduler.review_card(
    my_card, fsrs::Rating::Good,
    when,                // std::optional<TimePoint>
    std::nullopt,        // std::optional<review_duration>
    rng);                // your generator
```

The default overload (without a generator) uses a thread-local `mt19937` seeded from `std::random_device`.

## Building the tests yourself

```bash
cd cpp-fsrs
xmake f -m debug && xmake build cpp-fsrs-tests && xmake test
```

Tests are [Catch2 v3](https://github.com/catchorg/Catch2) — grouped by tags: `[rating]`, `[state]`, `[card]`, `[review_log]`, `[validation]`, `[helpers]`, `[retrievability]`, `[fuzz]`, `[review]`, `[reschedule]`, `[json]`, `[oracle]`. Filter with e.g. `xmake run cpp-fsrs-tests "[oracle]"`.

## What's not included (yet)

- **Optimizer.** py-fsrs's optimizer uses PyTorch autograd + L-BFGS to fit parameters from review logs. Porting it requires a C++ autodiff stack and is a separate project.
- **Language bindings.** Pure C++ for now.
- **Persistent storage.** The library hands you JSON; database integration is a consumer concern.

## License

MIT, matching py-fsrs upstream.
