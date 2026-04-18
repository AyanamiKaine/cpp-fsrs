#pragma once

namespace fsrs {

enum class State : int {
    Learning   = 1,
    Review     = 2,
    Relearning = 3,
};

}  // namespace fsrs
