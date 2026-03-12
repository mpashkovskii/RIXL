#!/usr/bin/env python3
"""
Test: RIXL GPU-to-GPU memory transfer via libfabric (verbs) on AMD GPUs.

Performs a loopback transfer between two GPUs on the same machine:
  1. Initialises a RIXL agent with the LIBFABRIC backend (verbs;ofi_rxm)
  2. Allocates GPU memory on two different devices via PyTorch/HIP
  3. Registers both regions with RIXL
  4. Writes data from GPU-0 to GPU-1 using the loopback path
  5. Verifies the transferred data matches

Usage (inside the container):
    export PYTHONPATH=/app/repos/RIXL/install/lib/python3/dist-packages
    export LD_LIBRARY_PATH=/app/repos/RIXL/install/lib/x86_64-linux-gnu:/usr/local/lib:/opt/rocm/lib
    python3 test_libfabric_gpu_transfer.py [--backend LIBFABRIC|UCX] [--src-gpu 0] [--dst-gpu 1] [--size 4096]

Known issue:
    The LIBFABRIC backend currently fails to initialise on Broadcom bnxt_re0 NICs
    with verbs;ofi_rxm due to fi_av_insert returning EINVAL during self-connection.
    Use --backend UCX as fallback (UCX uses verbs transport underneath).
"""

import argparse
import sys
import time

import torch


def parse_args():
    p = argparse.ArgumentParser(description="RIXL GPU-to-GPU transfer test")
    p.add_argument("--backend", default="LIBFABRIC", choices=["LIBFABRIC", "UCX"],
                   help="RIXL backend to use (default: LIBFABRIC)")
    p.add_argument("--src-gpu", type=int, default=0, help="Source GPU index")
    p.add_argument("--dst-gpu", type=int, default=1, help="Destination GPU index")
    p.add_argument("--size", type=int, default=4096,
                   help="Number of float32 elements to transfer")
    p.add_argument("--timeout", type=float, default=10.0,
                   help="Max seconds to wait for transfer completion")
    return p.parse_args()


def main():
    args = parse_args()

    # ------------------------------------------------------------------ #
    #  Pre-flight checks                                                  #
    # ------------------------------------------------------------------ #
    if not torch.cuda.is_available():
        print("FAIL: No HIP/CUDA GPUs available")
        sys.exit(1)

    num_gpus = torch.cuda.device_count()
    if max(args.src_gpu, args.dst_gpu) >= num_gpus:
        print(f"FAIL: Requested GPUs {args.src_gpu},{args.dst_gpu} "
              f"but only {num_gpus} available")
        sys.exit(1)

    if args.src_gpu == args.dst_gpu:
        print("FAIL: src-gpu and dst-gpu must differ for a meaningful transfer")
        sys.exit(1)

    # Late import so help/pre-flight runs even without rixl installed
    from rixl._api import nixl_agent, nixl_agent_config  # noqa: E402

    # ------------------------------------------------------------------ #
    #  1. Create RIXL agent                                               #
    # ------------------------------------------------------------------ #
    agent_name = "gpu_xfer_test"
    config = nixl_agent_config(
        enable_prog_thread=True,
        backends=[args.backend],
    )

    try:
        agent = nixl_agent(agent_name, config)
    except Exception as exc:
        print(f"FAIL: Could not create agent with {args.backend} backend: {exc}")
        sys.exit(1)

    print(f"Agent '{agent_name}' created with backend {args.backend}")
    print(f"  Supported mem types: {agent.get_backend_mem_types(args.backend)}")

    # ------------------------------------------------------------------ #
    #  2. Allocate GPU memory                                             #
    # ------------------------------------------------------------------ #
    src_tensor = torch.ones(args.size, dtype=torch.float32,
                            device=f"cuda:{args.src_gpu}")
    dst_tensor = torch.zeros(args.size, dtype=torch.float32,
                             device=f"cuda:{args.dst_gpu}")
    nbytes = args.size * src_tensor.element_size()
    print(f"Allocated {nbytes} bytes: "
          f"src=GPU{args.src_gpu} (ones), dst=GPU{args.dst_gpu} (zeros)")

    # ------------------------------------------------------------------ #
    #  3. Register memory with RIXL                                       #
    # ------------------------------------------------------------------ #
    src_reg = agent.register_memory(src_tensor)
    dst_reg = agent.register_memory(dst_tensor)
    print("Memory registered with RIXL")

    # ------------------------------------------------------------------ #
    #  4. Set up and execute loopback transfer (WRITE src → dst)          #
    # ------------------------------------------------------------------ #
    src_descs = agent.get_xfer_descs(src_tensor)
    dst_descs = agent.get_xfer_descs(dst_tensor)

    # "" means NIXL_INIT_AGENT (local); agent_name means loopback target
    local_side = agent.prep_xfer_dlist("", src_descs)
    remote_side = agent.prep_xfer_dlist(agent_name, dst_descs)

    xfer = agent.make_prepped_xfer(
        "WRITE", local_side, [0], remote_side, [0],
    )

    status = agent.transfer(xfer)
    print(f"Transfer posted, initial status: {status}")

    # Poll for completion
    t0 = time.monotonic()
    while status == "PROC":
        if time.monotonic() - t0 > args.timeout:
            print(f"FAIL: Transfer timed out after {args.timeout}s")
            xfer.release()
            sys.exit(1)
        status = agent.check_xfer_state(xfer)
        time.sleep(0.001)

    if status != "DONE":
        print(f"FAIL: Transfer ended with status {status}")
        xfer.release()
        sys.exit(1)

    elapsed_ms = (time.monotonic() - t0) * 1000
    print(f"Transfer completed in {elapsed_ms:.2f} ms")

    # ------------------------------------------------------------------ #
    #  5. Verify                                                          #
    # ------------------------------------------------------------------ #
    torch.cuda.synchronize(args.dst_gpu)
    expected = float(args.size)
    actual = dst_tensor.sum().item()
    if actual != expected:
        print(f"FAIL: Data mismatch — expected sum {expected}, got {actual}")
        xfer.release()
        sys.exit(1)

    print(f"PASS: GPU{args.src_gpu} → GPU{args.dst_gpu} transfer verified "
          f"({nbytes} bytes, sum={actual})")

    # ------------------------------------------------------------------ #
    #  Cleanup                                                            #
    # ------------------------------------------------------------------ #
    xfer.release()
    local_side.release()
    remote_side.release()
    agent.deregister_memory(src_reg)
    agent.deregister_memory(dst_reg)
    print("Cleanup done")


if __name__ == "__main__":
    main()
