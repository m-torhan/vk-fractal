/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "util/read_file.hpp"

std::vector<std::uint8_t> read_file_binary(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    f.seekg(0, std::ios::end);
    const auto size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(size);
    if (!f.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size))) {
        throw std::runtime_error("Failed to read file: " + path);
    }
    return data;
}
