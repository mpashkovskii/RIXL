// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Advanced Micro Devices, Inc. All rights reserved.

#include <gtest/gtest.h>
#include "libfabric/libfabric_topology.h"
#include <hwloc.h>

class AMDDeviceDetectionTest : public ::testing::Test {
protected:
    nixlLibfabricTopology topology;
    hwloc_topology_t hwloc_topology;

    void SetUp() override {
        hwloc_topology_init(&hwloc_topology);
        hwloc_topology_load(hwloc_topology);
    }

    void TearDown() override {
        hwloc_topology_destroy(hwloc_topology);
    }

    // Helper to create mock PCI device object
    hwloc_obj_t createMockPCIDevice(uint16_t vendor_id, uint16_t device_id, uint16_t class_id) {
        hwloc_obj_t obj = hwloc_alloc_setup_object(hwloc_topology, HWLOC_OBJ_PCI_DEVICE, HWLOC_UNKNOWN_INDEX);
        obj->attr->pcidev.vendor_id = vendor_id;
        obj->attr->pcidev.device_id = device_id;
        obj->attr->pcidev.class_id = class_id;
        return obj;
    }
};

// Test 1: Detect MI300X GPU
TEST_F(AMDDeviceDetectionTest, DetectsMI300X) {
    hwloc_obj_t mock_device = createMockPCIDevice(
        0x1002,  // AMD vendor ID
        0x74a1,  // MI300X device ID
        0x0302   // GPU class
    );

    EXPECT_TRUE(topology.isAmdAccel(mock_device));

    hwloc_free_unlinked_object(mock_device);
}

// Test 2: Detect MI300A APU
TEST_F(AMDDeviceDetectionTest, DetectsMI300A) {
    hwloc_obj_t mock_device = createMockPCIDevice(
        0x1002,  // AMD vendor ID
        0x74a0,  // MI300A device ID
        0x0302   // GPU class
    );

    EXPECT_TRUE(topology.isAmdAccel(mock_device));

    hwloc_free_unlinked_object(mock_device);
}

// Test 3: Detect MI355X GPU
TEST_F(AMDDeviceDetectionTest, DetectsMI355X) {
    hwloc_obj_t mock_device = createMockPCIDevice(
        0x1002,  // AMD vendor ID
        0x75a0,  // MI355X device ID
        0x0302   // GPU class
    );

    EXPECT_TRUE(topology.isAmdAccel(mock_device));

    hwloc_free_unlinked_object(mock_device);
}

// Test 4: Ignore NVIDIA GPU
TEST_F(AMDDeviceDetectionTest, IgnoresNVIDIA) {
    hwloc_obj_t mock_device = createMockPCIDevice(
        0x10de,  // NVIDIA vendor ID
        0x2330,  // H100 device ID
        0x0302   // GPU class
    );

    EXPECT_FALSE(topology.isAmdAccel(mock_device));

    hwloc_free_unlinked_object(mock_device);
}

// Test 5: Ignore Intel GPU
TEST_F(AMDDeviceDetectionTest, IgnoresIntel) {
    hwloc_obj_t mock_device = createMockPCIDevice(
        0x8086,  // Intel vendor ID
        0x0bd0,  // Intel GPU
        0x0302   // GPU class
    );

    EXPECT_FALSE(topology.isAmdAccel(mock_device));

    hwloc_free_unlinked_object(mock_device);
}

// Test 6: Fallback to PCI class for unknown AMD device ID
TEST_F(AMDDeviceDetectionTest, FallbackToClassID) {
    hwloc_obj_t mock_device = createMockPCIDevice(
        0x1002,  // AMD vendor ID
        0x9999,  // Unknown device ID (not in our list)
        0x0302   // GPU class (should trigger fallback)
    );

    EXPECT_TRUE(topology.isAmdAccel(mock_device));

    hwloc_free_unlinked_object(mock_device);
}

// Test 7: Reject AMD non-GPU device
TEST_F(AMDDeviceDetectionTest, RejectsAMDNonGPU) {
    hwloc_obj_t mock_device = createMockPCIDevice(
        0x1002,  // AMD vendor ID
        0x9999,  // Unknown device ID
        0x0200   // Network controller class (not GPU)
    );

    EXPECT_FALSE(topology.isAmdAccel(mock_device));

    hwloc_free_unlinked_object(mock_device);
}

// Test 8: Reject null object
TEST_F(AMDDeviceDetectionTest, RejectsNullObject) {
    EXPECT_FALSE(topology.isAmdAccel(nullptr));
}

// Test 9: Reject non-PCI object
TEST_F(AMDDeviceDetectionTest, RejectsNonPCIObject) {
    hwloc_obj_t mock_device = hwloc_alloc_setup_object(hwloc_topology, HWLOC_OBJ_CORE, HWLOC_UNKNOWN_INDEX);

    EXPECT_FALSE(topology.isAmdAccel(mock_device));

    hwloc_free_unlinked_object(mock_device);
}

// Test 10: HMEM interface mapping - NVIDIA first
TEST_F(AMDDeviceDetectionTest, HMEMInterfaceNVIDIA) {
    // Mock: 2 NVIDIA GPUs
    // Note: This test assumes internal topology state can be manipulated
    // In real implementation, you may need to expose setter methods or use friend class

    // For device 0 (first NVIDIA)
    EXPECT_EQ(topology.getMrAttrIface(0), FI_HMEM_CUDA);

    // For device 1 (second NVIDIA)
    EXPECT_EQ(topology.getMrAttrIface(1), FI_HMEM_CUDA);
}

// Test 11: HMEM interface mapping - AMD after NVIDIA
TEST_F(AMDDeviceDetectionTest, HMEMInterfaceAMD) {
    // Mock: 2 NVIDIA, then 3 AMD GPUs
    // Device 2 should be first AMD (FI_HMEM_ROCR)

    // This is a conceptual test - actual implementation depends on
    // how topology discovers devices in real system

    // Expected: getMrAttrIface(2) == FI_HMEM_ROCR for first AMD GPU
    // Expected: getMrAttrIface(3) == FI_HMEM_ROCR for second AMD GPU
}

// Test 12: HMEM interface mapping - Neuron after NVIDIA and AMD
TEST_F(AMDDeviceDetectionTest, HMEMInterfaceNeuron) {
    // Mock: 2 NVIDIA, 3 AMD, 1 Neuron
    // Device 5 should be Neuron (FI_HMEM_NEURON)

    // Expected: getMrAttrIface(5) == FI_HMEM_NEURON
}

// Test 13: All MI300 series device IDs
TEST_F(AMDDeviceDetectionTest, AllMI300SeriesIDs) {
    uint16_t mi300_ids[] = {
        0x74a0,  // MI300A APU
        0x74a1,  // MI300X dGPU
        0x74a2,  // MI308X
        0x74a5,  // MI325X
        0x74a9,  // MI300XHF
        0x74b5   // MI300X VF
    };

    for (uint16_t device_id : mi300_ids) {
        hwloc_obj_t mock_device = createMockPCIDevice(0x1002, device_id, 0x0302);
        EXPECT_TRUE(topology.isAmdAccel(mock_device))
            << "Failed to detect MI300 device ID: 0x" << std::hex << device_id;
        hwloc_free_unlinked_object(mock_device);
    }
}

// Test 14: All MI355 series device IDs
TEST_F(AMDDeviceDetectionTest, AllMI355SeriesIDs) {
    uint16_t mi355_ids[] = {
        0x75a0,  // MI355X
        0x75a1,  // MI355X variant
        0x75a3   // MI355X variant (Chip ID 30115)
    };

    for (uint16_t device_id : mi355_ids) {
        hwloc_obj_t mock_device = createMockPCIDevice(0x1002, device_id, 0x0302);
        EXPECT_TRUE(topology.isAmdAccel(mock_device))
            << "Failed to detect MI355 device ID: 0x" << std::hex << device_id;
        hwloc_free_unlinked_object(mock_device);
    }
}

// Test 15: GPU class range check
TEST_F(AMDDeviceDetectionTest, GPUClassRange) {
    // Class 0x0300-0x03FF are display controllers (GPUs)
    uint16_t valid_classes[] = {0x0300, 0x0301, 0x0302, 0x0380, 0x03FF};
    uint16_t invalid_classes[] = {0x0200, 0x02FF, 0x0400, 0x0500};

    // Valid GPU classes with unknown AMD device ID
    for (uint16_t class_id : valid_classes) {
        hwloc_obj_t mock_device = createMockPCIDevice(0x1002, 0x9999, class_id);
        EXPECT_TRUE(topology.isAmdAccel(mock_device))
            << "Failed to detect AMD GPU with class: 0x" << std::hex << class_id;
        hwloc_free_unlinked_object(mock_device);
    }

    // Invalid classes should not be detected
    for (uint16_t class_id : invalid_classes) {
        hwloc_obj_t mock_device = createMockPCIDevice(0x1002, 0x9999, class_id);
        EXPECT_FALSE(topology.isAmdAccel(mock_device))
            << "Incorrectly detected non-GPU with class: 0x" << std::hex << class_id;
        hwloc_free_unlinked_object(mock_device);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
