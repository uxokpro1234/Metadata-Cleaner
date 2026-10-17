//
// Created by user2378 on 10.10.2026.
//

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

using Bytes = std::vector<std::uint8_t>;

struct CleanResult {
    Bytes data;
    std::size_t removed = 0;
    bool valid = false;
    std::string error;
    std::vector<std::string> removedItems;
};

CleanResult cleanJpeg(const Bytes& input);