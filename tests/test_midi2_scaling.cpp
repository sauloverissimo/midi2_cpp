#include "test_common.h"
#include "midi2_device.h"

uint32_t g_captured_tx[CAPTURE_MAX] = {0};
size_t   g_captured_tx_len = 0;
uint32_t g_test_now_ms = 0;

using namespace midi2;

static void test_scale_up_7to16_extremes(void) {
    TEST("scaleUp7to16 extremes");
    CHECK_EQ(Device::scaleUp7to16(0),    0x0000u, "0 -> 0x0000");
    CHECK_EQ(Device::scaleUp7to16(0x7F), 0xFFFFu, "0x7F -> 0xFFFF");
    PASS();
}

static void test_scale_up_7to32_extremes(void) {
    TEST("scaleUp7to32 extremes");
    CHECK_EQ(Device::scaleUp7to32(0),    0x00000000u, "0 -> 0");
    CHECK_EQ(Device::scaleUp7to32(0x7F), 0xFFFFFFFFu, "0x7F -> 0xFFFFFFFF");
    PASS();
}

static void test_scale_up_14to32_extremes(void) {
    TEST("scaleUp14to32 extremes");
    CHECK_EQ(Device::scaleUp14to32(0),      0x00000000u, "0 -> 0");
    CHECK_EQ(Device::scaleUp14to32(0x3FFF), 0xFFFFFFFFu, "0x3FFF -> 0xFFFFFFFF");
    PASS();
}

static void test_scale_roundtrip_7to16(void) {
    TEST("scaleUp7to16 -> scaleDown16to7 roundtrip (all 128 values)");
    for (int v = 0; v <= 127; ++v) {
        uint8_t v7 = (uint8_t)v;
        uint16_t v16 = Device::scaleUp7to16(v7);
        uint8_t back = Device::scaleDown16to7(v16);
        if (back != v7) {
            std::printf("FAIL: %u -> 0x%04X -> %u\n", v7, v16, back);
            g_failed++;
            return;
        }
    }
    PASS();
}

static void test_scale_roundtrip_7to32(void) {
    TEST("scaleUp7to32 -> scaleDown32to7 roundtrip (all 128 values)");
    for (int v = 0; v <= 127; ++v) {
        uint8_t v7 = (uint8_t)v;
        uint32_t v32 = Device::scaleUp7to32(v7);
        uint8_t back = Device::scaleDown32to7(v32);
        if (back != v7) {
            std::printf("FAIL: %u -> 0x%08X -> %u\n", v7, v32, back);
            g_failed++;
            return;
        }
    }
    PASS();
}

static void test_scale_roundtrip_14to32(void) {
    TEST("scaleUp14to32 -> scaleDown32to14 roundtrip (all 16384 values)");
    for (int v = 0; v <= 0x3FFF; ++v) {
        uint16_t v14 = (uint16_t)v;
        uint32_t v32 = Device::scaleUp14to32(v14);
        uint16_t back = Device::scaleDown32to14(v32);
        if (back != v14) {
            std::printf("FAIL: 0x%04X -> 0x%08X -> 0x%04X\n", v14, v32, back);
            g_failed++;
            return;
        }
    }
    PASS();
}

int main(void) {
    test_scale_up_7to16_extremes();
    test_scale_up_7to32_extremes();
    test_scale_up_14to32_extremes();
    test_scale_roundtrip_7to16();
    test_scale_roundtrip_7to32();
    test_scale_roundtrip_14to32();
    REPORT_AND_EXIT();
}
