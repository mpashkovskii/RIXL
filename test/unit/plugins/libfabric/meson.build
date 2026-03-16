# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-FileCopyrightText: Copyright (c) 2025-2026 Amazon.com, Inc. and affiliates.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Hipify sources that include cuda_runtime.h for ROCm/HIP builds
hipify_cmd = find_program('hipify-perl')
hipify_src = generator(hipify_cmd,
                       output : ['@BASENAME@_hip.cpp'],
                       arguments : ['@INPUT@', '@EXTRA_ARGS@', '@OUTPUT@'])

libfabric_backend_test_hip_src = hipify_src.process('libfabric_backend_test.cpp', extra_args: '-o')

libfabric_test_deps = [
    nixl_dep,
    nixl_infra,
    nixl_common_deps,
    libfabric_dep,
    libfabric_backend_interface,
    libfabric_utils_dep,
    serdes_interface,
    thread_dep,
    dependency('dl', required: true),
] + nixl_test_utils_dep

libfabric_test_cpp_args = ['-DHAVE_LIBFABRIC']

if cuda_dep.found()
    libfabric_test_deps += [ cuda_dep ]
    libfabric_test_cpp_args += [ '-DHAVE_CUDA' ]
endif

libfabric_backend_test = executable('libfabric_backend_test',
        libfabric_backend_test_hip_src,
        dependencies: libfabric_test_deps,
        include_directories: [nixl_inc_dirs, utils_inc_dirs, plugins_inc_dirs],
        cpp_args: libfabric_test_cpp_args,
        install: true)
