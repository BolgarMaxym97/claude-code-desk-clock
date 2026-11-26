// Saved Wi-Fi in two raw flash sectors (A/B): record format, CRC, newest wins, never touch foreign data.
#include <unity.h>
#include <vector>
#include "rawstore.h"

void setUp() {}
void tearDown() {}

static std::vector<uint8_t> erased(size_t n = 256) { return std::vector<uint8_t>(n, 0xFF); }

void test_roundtrip() {
    StoreRec r{7, "Дім 5G", "pa;ss word"}, back;
    std::vector<uint8_t> b = encodeRec(r);
    TEST_ASSERT_EQUAL(0, int(b.size() % 4));  // flash writes are word aligned
    TEST_ASSERT_TRUE(decodeRec(b.data(), b.size(), back));
    TEST_ASSERT_EQUAL(7, int(back.seq));
    TEST_ASSERT_EQUAL_STRING("Дім 5G", back.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("pa;ss word", back.pass.c_str());
}

void test_corrupt_or_erased_is_rejected() {
    StoreRec r{1, "Home", "12345678"}, back;
    std::vector<uint8_t> b = encodeRec(r);
    b[12] ^= 0x01;  // flipped bit in payload -> CRC fails
    TEST_ASSERT_FALSE(decodeRec(b.data(), b.size(), back));
    std::vector<uint8_t> e = erased();
    TEST_ASSERT_FALSE(decodeRec(e.data(), e.size(), back));
    std::vector<uint8_t> shortBuf(6, 0);
    TEST_ASSERT_FALSE(decodeRec(shortBuf.data(), shortBuf.size(), back));
}

void test_invalid_creds_inside_valid_crc_are_rejected() {
    StoreRec r{1, "", "12345678"}, back;  // empty SSID
    std::vector<uint8_t> b = encodeRec(r);
    TEST_ASSERT_FALSE(decodeRec(b.data(), b.size(), back));
}

void test_foreign_sector_detection() {
    std::vector<uint8_t> e = erased(4096);
    TEST_ASSERT_FALSE(sectorForeign(e.data(), e.size()));
    std::vector<uint8_t> ours = encodeRec(StoreRec{3, "Home", "12345678"});
    ours.resize(4096, 0xFF);
    TEST_ASSERT_FALSE(sectorForeign(ours.data(), ours.size()));
    std::vector<uint8_t> jpeg = erased(4096);
    jpeg[0] = 0xFF; jpeg[1] = 0xD8; jpeg[100] = 0x12;  // somebody else's data
    TEST_ASSERT_TRUE(sectorForeign(jpeg.data(), jpeg.size()));
}

void test_newest_wins_and_write_goes_to_the_other_sector() {
    TEST_ASSERT_EQUAL(-1, pickNewest(false, 0, false, 0));
    TEST_ASSERT_EQUAL(0, pickNewest(true, 5, false, 0));
    TEST_ASSERT_EQUAL(1, pickNewest(true, 5, true, 6));
    TEST_ASSERT_EQUAL(0, pickNewest(true, 9, true, 6));
    TEST_ASSERT_EQUAL(0, writeTarget(-1));  // nothing yet -> A
    TEST_ASSERT_EQUAL(1, writeTarget(0));   // newest in A -> write B, A stays as fallback
    TEST_ASSERT_EQUAL(0, writeTarget(1));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_corrupt_or_erased_is_rejected);
    RUN_TEST(test_invalid_creds_inside_valid_crc_are_rejected);
    RUN_TEST(test_foreign_sector_detection);
    RUN_TEST(test_newest_wins_and_write_goes_to_the_other_sector);
    return UNITY_END();
}
