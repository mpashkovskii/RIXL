FROM vllm/vllm-openai-rocm:v0.15.1

# Install build dependencies
ENV DEBIAN_FRONTEND=noninteractive
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    apt-get update && apt-get install -y \
    # Build essentials
    build-essential \
    cmake \
    pkg-config \
    autoconf \
    automake \
    libtool \
    git \
    clang-format \
    libgtest-dev \
    # UCX dependencies
    libaio-dev \
    liburing-dev \
    # Meson and Ninja for RIXL
    ninja-build \
    # RDMA packages
    libibverbs-dev \
    ibverbs-utils \
    librdmacm-dev \
    rdma-core \
    unzip \
    wget

# Broadcom RDMA drivers
RUN wget https://docs.broadcom.com/docs-and-downloads/ethernet-network-adapters/NXE/Thor2/GCA1/bcm5760x_230.2.52.0a.zip && \
    unzip bcm5760x_230.2.52.0a.zip && \
    cd bcm5760x_230.2.52.0a/drivers_linux/bnxt_rocelib/ && \
    results=$(find -name "libbnxt*.tar.gz") && tar -xf $results && \
    untar_dir=$(find . -maxdepth 1 -type d -name "libbnxt*" \! -name "*.tar.gz" | head -n 1) && cd $untar_dir && sh autogen.sh && ./configure && make && \
    find /usr/lib64/ /usr/lib -name "libbnxt_re-rdmav*.so" -exec mv {} {}.inbox \; && \
    make install all && sudo sh -c "echo /usr/local/lib >> /etc/ld.so.conf" && \
    sudo ldconfig && \
    cp -f bnxt_re.driver /etc/libibverbs.d/ && \
    ibv_devices
RUN rm -rf bcm5760x_230.2.52.0a && \
    rm -rf bcm5760x_230.2.52.0a.zip

# Upgrade pip and install Python tools and Ray
ARG RAY_VERSION=2.54.0
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir \
    aiohttp-cors \
    "meson>=0.64.0" \
    opencensus \
    opentelemetry-api \
    opentelemetry-exporter-otlp \
    opentelemetry-exporter-prometheus \
    opentelemetry-sdk \
    pre-commit \
    "pybind11[global]" \
    "ray[all,default,serve]==${RAY_VERSION}"

# Build and install UCX "develop" version (ROCm-specific)
ENV ROCM_PATH=/opt/rocm
ENV PREFIX=/usr/local
ARG UCX_VERSION=develop
RUN git clone --depth 1 --branch ${UCX_VERSION} https://github.com/ROCm/ucx && \
    cd ucx && \
    ./autogen.sh && \
    mkdir -p build && \
    cd build && \
    ../configure \
        --prefix=${PREFIX} \
        --enable-shared \
        --disable-static \
        --disable-doxygen-doc \
        --enable-optimizations \
        --enable-devel-headers \
        --with-rocm=${ROCM_PATH} \
        --with-verbs \
        --with-dm \
        --enable-mt && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Verify UCX installation
RUN ucx_info -v

# Build and install libfabric (v1.22.0 with ROCr/HMEM support for AMD GPUs)
ARG LIBFABRIC_VERSION=v2.4.x
RUN git clone --depth 1 --branch ${LIBFABRIC_VERSION} https://github.com/ofiwg/libfabric.git && \
    cd libfabric && \
    ./autogen.sh && \
    ./configure \
        --prefix=${PREFIX} \
        --with-rocr=${ROCM_PATH} \
        --enable-rocr-dlopen && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Verify libfabric installation and ROCr support
RUN fi_info -l && \
    echo "Checking for ROCr support in libfabric:" && \
    strings ${PREFIX}/lib/libfabric.so.1 | grep -i rocr | head -10 || echo "No ROCr symbols found"

RUN pip uninstall -y nixl nixl-cu12
# # Build and install RIXL with LIBFABRIC plugin support
# # The RIXL build below fails because the official ROCm/RIXL    
# # develop branch doesn't have HIPification for the libfabric
# # plugin. The error shows it's looking for cuda.h and            
# # cuda_runtime.h which don't exist in AMD/ROCm environments.
# ARG RIXL_VERSION=develop
# RUN --mount=type=cache,target=/root/.cache/pip \
#     git clone --depth 1 --branch ${RIXL_VERSION} https://github.com/ROCm/RIXL.git && \
#     cd RIXL && \
#     meson setup build/ \
#         --prefix=${PREFIX} \
#         -Denable_plugins="POSIX,UCX,LIBFABRIC" \
#         -Drocm_path=${ROCM_PATH} \
#         -Ducx_path=${PREFIX} && \
#     cd build && \
#     ninja && \
#     ninja install && \
#     ldconfig

# # Verify RIXL installation
# RUN echo "Checking RIXL installation:" && \
#     ls -la ${PREFIX}/lib/x86_64-linux-gnu/libnixl.so* && \
#     echo "Checking RIXL plugins:" && \
#     ls -la ${PREFIX}/lib/x86_64-linux-gnu/plugins/ && \
#     echo "Checking for LIBFABRIC plugin:" && \
#     ls -la ${PREFIX}/lib/x86_64-linux-gnu/plugins/ | grep -i libfabric || echo "LIBFABRIC plugin not found"

# Create symlink for RIXL Python module to correct site-packages
RUN ln -sf ${PREFIX}/lib/python3/dist-packages/rixl \
    ${PREFIX}/lib/python3.12/dist-packages/rixl

RUN --mount=type=cache,target=/root/.cache/pip \
    git clone --depth 1 https://github.com/LMCache/LMCache.git && \
    cd LMCache && \
    PYTORCH_ROCM_ARCH="gfx942" \
    TORCH_DONT_CHECK_COMPILER_ABI=1 \
    CXX=hipcc \
    BUILD_WITH_HIP=1 \
    python3 -m pip install --no-build-isolation -e .

# Set library paths
ENV LD_LIBRARY_PATH=${PREFIX}/lib/x86_64-linux-gnu:${PREFIX}/lib:${ROCM_PATH}/lib:${LD_LIBRARY_PATH}
