/*
 * Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>

#include "app/app.hpp"

int main() {
    try {
        App app;
        app.run();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
