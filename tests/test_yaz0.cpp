// Unit tests for yaz0_decompress() in src/game/rom_decompression.cpp
// These tests validate the decompression function against known inputs,
// truncated streams, and malformed back-references.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include <string_view>

// Forward-declare the function under test (defined in rom_decompression.cpp).
void yaz0_decompress(std::span<const uint8_t> input, std::span<uint8_t> output);

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define EXPECT_TRUE(expr) \
    do { \
        ++g_tests_run; \
        if (!(expr)) { \
            ++g_tests_failed; \
            fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, #expr); \
        } \
    } while (0)

#define EXPECT_EQ(a, b) \
    do { \
        ++g_tests_run; \
        if (!((a) == (b))) { \
            ++g_tests_failed; \
            fprintf(stderr, "FAIL [%s:%d]: (%s) == (%s)\n", __FILE__, __LINE__, #a, #b); \
        } \
    } while (0)

// ---------------------------------------------------------------------------
// Helper: build a Yaz0 stream that encodes 'literals' as all-literal blocks.
// Each block contains one layout byte (0xFF = 8 literals) followed by 8
// raw bytes.  A trailing partial block is supported.
// ---------------------------------------------------------------------------
static std::vector<uint8_t> make_literal_stream(std::span<const uint8_t> data) {
    std::vector<uint8_t> out;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t block_size = std::min<size_t>(8, data.size() - pos);
        // Build a layout byte with 'block_size' literal bits set (MSB-first).
        uint8_t layout = 0;
        for (size_t i = 0; i < block_size; ++i) {
            layout |= static_cast<uint8_t>(0x80 >> i);
        }
        out.push_back(layout);
        for (size_t i = 0; i < block_size; ++i) {
            out.push_back(data[pos + i]);
        }
        pos += block_size;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Test 1: All-literal stream – simple "Hello, World!" round-trip
// ---------------------------------------------------------------------------
static void test_all_literals() {
    const std::string_view text = "Hello, World!";
    std::vector<uint8_t> source(text.begin(), text.end());

    auto compressed = make_literal_stream(source);

    std::vector<uint8_t> decompressed(source.size(), 0);
    yaz0_decompress(compressed, decompressed);

    EXPECT_EQ(decompressed.size(), source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        EXPECT_EQ(decompressed[i], source[i]);
    }
}

// ---------------------------------------------------------------------------
// Test 2: Back-reference (run-length) encoding
// Encodes "ABABABAB" (8 bytes) using 2 literals followed by one back-reference
// that re-uses the preceding 2-byte pattern (offset=2, length=6).
// ---------------------------------------------------------------------------
static void test_back_reference() {
    // Layout byte 0xC0 = 0b11000000:
    //   bit 7 = 1 → literal 'A'
    //   bit 6 = 1 → literal 'B'
    //   bit 5 = 0 → 2-byte back-ref: firstByte=0x40 secondByte=0x01
    //     length = ((0x4001 & 0xF000) >> 12) + 2 = 4 + 2 = 6
    //     offset = ( 0x4001 & 0x0FFF)       + 1 = 1 + 1 = 2
    //   bits 4-0 = 0 → would attempt more back-refs, but output_pos reaches
    //             output_size after the first back-ref so the inner loop exits.
    const std::vector<uint8_t> compressed = {
        0xC0, 'A', 'B', 0x40, 0x01,
    };
    // After 2 literals + back-ref(len=6, off=2) the output is exactly full.
    const std::string_view expected = "ABABABAB";  // 8 bytes

    std::vector<uint8_t> decompressed(expected.size(), 0);
    yaz0_decompress(compressed, decompressed);

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(decompressed[i], static_cast<uint8_t>(expected[i]));
    }
}

// ---------------------------------------------------------------------------
// Test 3: 3-byte back-reference encoding (length ≥ 18)
// Encodes "A" * 20 bytes using:
//   2 literal 'A' bytes, then a 3-byte back-ref with length=18 that
//   copies 18 bytes starting 2 positions back.
// ---------------------------------------------------------------------------
static void test_3byte_back_reference() {
    // 3-byte back-ref: firstByte & 0xF0 == 0
    //   offset = (bytes & 0x0FFF) + 1,  length = thirdByte + 0x12
    // We want offset=1, length=18:
    //   firstByte=0x00, secondByte=0x00 → bytes=0x0000, offset=0x000+1=1
    //   thirdByte=0x00 → length=0+0x12=18
    // So: output[0]='A', output[1]='A', then copy 18 bytes starting at output[1-1]=output[0]
    // Since offset=1 we copy from output[cur_pos - 1], repeating 'A' 18 times.
    const std::vector<uint8_t> compressed = {
        // layout: bit7=1 (literal A), bit6=1 (literal A), bit5=0 (3-byte back-ref)
        0xC0, 'A', 'A', 0x00, 0x00, 0x00,
    };
    const size_t expected_size = 2 + 18;  // 2 literals + 18 from back-ref = 20 'A's
    std::vector<uint8_t> decompressed(expected_size, 0);
    yaz0_decompress(compressed, decompressed);

    for (size_t i = 0; i < expected_size; ++i) {
        EXPECT_EQ(decompressed[i], static_cast<uint8_t>('A'));
    }
}

// ---------------------------------------------------------------------------
// Test 4: Truncated input – the loop must stop without crashing or
//         out-of-bounds access.  Fewer output bytes are written but no
//         undefined behaviour occurs.
// ---------------------------------------------------------------------------
static void test_truncated_input() {
    // Layout byte says 8 literals are coming, but we only supply 4.
    const std::vector<uint8_t> compressed = { 0xFF, 'X', 'Y', 'Z', 'W' };
    std::vector<uint8_t> decompressed(8, 0xFF);  // Pre-filled sentinel

    // Must not crash or write past the end.
    yaz0_decompress(compressed, decompressed);

    // The first 4 bytes should be filled; the rest remain as the sentinel (0xFF).
    EXPECT_EQ(decompressed[0], static_cast<uint8_t>('X'));
    EXPECT_EQ(decompressed[1], static_cast<uint8_t>('Y'));
    EXPECT_EQ(decompressed[2], static_cast<uint8_t>('Z'));
    EXPECT_EQ(decompressed[3], static_cast<uint8_t>('W'));
    // Bytes 4-7 are untouched.
    for (size_t i = 4; i < 8; ++i) {
        EXPECT_EQ(decompressed[i], static_cast<uint8_t>(0xFF));
    }
}

// ---------------------------------------------------------------------------
// Test 5: Malformed back-reference – offset points before the start of the
//         output buffer.  The function must return without crashing or
//         writing corrupted data.
// ---------------------------------------------------------------------------
static void test_malformed_backref_underflow() {
    // Layout byte 0x3F: first two bits are 0 (back-refs), rest are literals.
    // First back-ref at output_pos=0: offset=(0x0001)+1=2, but output_pos=0 → invalid.
    const std::vector<uint8_t> compressed = {
        0x3F, 0x10, 0x01,  // back-ref: firstByte=0x10, secondByte=0x01 → offset=2, length=(0x10>>4)+2=3
        'A', 'B', 'C', 'D', 'E', 'F',  // never reached for the back-ref slots
    };
    std::vector<uint8_t> decompressed(16, 0xAA);  // sentinel

    // The function should detect offset > output_pos and return early.
    yaz0_decompress(compressed, decompressed);

    // Output should remain unchanged (function returned early).
    for (size_t i = 0; i < decompressed.size(); ++i) {
        EXPECT_EQ(decompressed[i], static_cast<uint8_t>(0xAA));
    }
}

// ---------------------------------------------------------------------------
// Test 6: Back-reference that would overrun the output buffer is rejected.
// ---------------------------------------------------------------------------
static void test_backref_overrun() {
    // Produce 2 literal bytes first, then a back-ref requesting more bytes than
    // the output buffer can hold.
    // Output buffer size = 4.  After 2 literals, output_pos=2.
    // Back-ref: offset=1, length=10  (output_pos+length > output_size → reject)
    //   2-byte: firstByte & 0xF0 != 0
    //   We want length=10: length = (firstByte>>4) + 2 → firstByte>>4 = 8 → firstByte = 0x80
    //   We want offset=1: (bytes & 0x0FFF)+1 = 1 → bytes & 0x0FFF = 0 → secondByte = 0x00
    //   firstByte = 0x80, secondByte = 0x00 → length=10, offset=1
    const std::vector<uint8_t> compressed = {
        0xC0, 'P', 'Q',   // layout: literal 'P', literal 'Q', then back-ref
        0x80, 0x00,        // back-ref: length=10, offset=1 (would overrun a 4-byte output)
    };
    std::vector<uint8_t> decompressed(4, 0xBB);

    yaz0_decompress(compressed, decompressed);

    // The 2 literals must have been written before the bad back-ref was detected.
    EXPECT_EQ(decompressed[0], static_cast<uint8_t>('P'));
    EXPECT_EQ(decompressed[1], static_cast<uint8_t>('Q'));
    // The remaining bytes must be untouched (back-ref was rejected).
    EXPECT_EQ(decompressed[2], static_cast<uint8_t>(0xBB));
    EXPECT_EQ(decompressed[3], static_cast<uint8_t>(0xBB));
}

// ---------------------------------------------------------------------------
// Test 7: Empty input – no output, no crash.
// ---------------------------------------------------------------------------
static void test_empty_input() {
    std::vector<uint8_t> decompressed(8, 0xCC);
    yaz0_decompress(std::span<const uint8_t>{}, decompressed);
    // Nothing should have been written.
    for (size_t i = 0; i < decompressed.size(); ++i) {
        EXPECT_EQ(decompressed[i], static_cast<uint8_t>(0xCC));
    }
}

// ---------------------------------------------------------------------------
// Test 8: Output buffer exactly full – the function must stop when it is
//         full and not attempt to write past the end.
// ---------------------------------------------------------------------------
static void test_output_exactly_full() {
    const std::string_view text = "12345678";  // 8 bytes
    std::vector<uint8_t> source(text.begin(), text.end());
    auto compressed = make_literal_stream(source);

    // Output buffer is exactly the right size.
    std::vector<uint8_t> decompressed(source.size(), 0);
    yaz0_decompress(compressed, decompressed);

    for (size_t i = 0; i < source.size(); ++i) {
        EXPECT_EQ(decompressed[i], source[i]);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    test_all_literals();
    test_back_reference();
    test_3byte_back_reference();
    test_truncated_input();
    test_malformed_backref_underflow();
    test_backref_overrun();
    test_empty_input();
    test_output_exactly_full();

    if (g_tests_failed == 0) {
        printf("All %d tests passed.\n", g_tests_run);
        return 0;
    } else {
        fprintf(stderr, "%d / %d tests FAILED.\n", g_tests_failed, g_tests_run);
        return 1;
    }
}
