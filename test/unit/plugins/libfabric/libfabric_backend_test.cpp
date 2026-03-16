/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <string>
#include <algorithm>
#include <memory>

#include "libfabric/libfabric_backend.h"
#include "test_utils.h"
#include "common/nixl_log.h"

/**
 * Unit tests for nixlLibfabricEngine constructor and basic initialization.
 *
 * These tests require libfabric to be available on the system.
 * They verify constructor behavior under various initialization parameters.
 */

static nixlLibfabricEngine *
createEngine(const std::string &name, bool progress_thread) {
    nixlBackendInitParams init;
    nixl_b_params_t custom_params;

    init.enableProgTh = progress_thread;
    init.pthrDelay = 100;
    init.localAgent = name;
    init.customParams = &custom_params;
    init.type = "LIBFABRIC";
    init.enableTelemetry_ = false;

    return new nixlLibfabricEngine(&init);
}

/**
 * Test that getSupportedMems always includes DRAM and VRAM if CUDA runtime is detected.
 */
static int
testSupportedMems() {
    NIXL_INFO << "=== Test: getSupportedMems includes DRAM ===";

    nixlLibfabricEngine *engine = nullptr;
    try {
        engine = createEngine("test_agent_mems", false);
    }
    catch (const std::exception &e) {
        NIXL_ERROR << "Constructor threw exception: " << e.what();
        return 1;
    }

    auto mems = engine->getSupportedMems();
    bool has_dram = std::find(mems.begin(), mems.end(), DRAM_SEG) != mems.end();
    nixl_exit_on_failure(has_dram, "DRAM_SEG should always be in supported mems");
    
#ifdef HAVE_CUDA
    bool has_vram = std::find(mems.begin(), mems.end(), VRAM_SEG) != mems.end();
    nixl_exit_on_failure(has_vram, "VRAM_SEG should be in supported mems when CUDA is available");
#endif

    NIXL_INFO << "Supported memory types (" << mems.size() << "):";
    for (const auto &m : mems) {
        NIXL_INFO << "  - " << m;
    }

    delete engine;
    NIXL_INFO << "PASSED";
    return 0;
}

int
main(int argc, char *argv[]) {
    int ret = 0;
    int failed = 0;
    int passed = 0;

    NIXL_INFO << "========================================";
    NIXL_INFO << "nixlLibfabricEngine Constructor Unit Tests";
    NIXL_INFO << "========================================";

    struct {
        const char *name;
        int (*func)();
    } tests[] = {
        {"SupportedMems", testSupportedMems},
    };

    size_t num_tests = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < num_tests; ++i) {
        NIXL_INFO << "";
        ret = tests[i].func();
        if (ret != 0) {
            NIXL_ERROR << "FAILED: " << tests[i].name << " (rc=" << ret << ")";
            failed++;
        } else {
            passed++;
        }
    }

    NIXL_INFO << "";
    NIXL_INFO << "========================================";
    NIXL_INFO << "Results: " << passed << " passed, " << failed << " failed out of " << num_tests;
    NIXL_INFO << "========================================";

    return failed > 0 ? 1 : 0;
}
