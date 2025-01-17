#ifndef SIMDJSON_SRC_GENERIC_STAGE1_JSON_ESCAPE_SCANNER_H

#ifndef SIMDJSON_CONDITIONAL_INCLUDE
#define SIMDJSON_SRC_GENERIC_STAGE1_JSON_ESCAPE_SCANNER_H
#include <x86intrin.h>
#include <generic/stage1/base.h>
#include <generic/stage1/buf_block_reader.h>
#endif // SIMDJSON_CONDITIONAL_INCLUDE

namespace simdjson {
namespace SIMDJSON_IMPLEMENTATION {
namespace {
namespace stage1 {

/**
 * Scans for escape characters in JSON, taking care with multiple backslashes (\\n vs. \n).
 */
struct json_escape_scanner {
  unsigned char next_is_escaped = 0;

  struct escaped_and_escape {
    /**
     * Mask of escaped characters.
     *
     * ```
     * \n \\n \\\n \\\\n \
     * 0100100010100101000
     *  n  \   \ n  \ \
     * ```
     */
    uint64_t escaped;
  };

  /**
   * Get a mask of both escape and escaped characters (the characters following a backslash).
   *
   * @param backslash A mask of the character that can escape others (but could be
   *        escaped itself). e.g. block.eq('\\')
   */
  simdjson_really_inline escaped_and_escape next(uint64_t backslash) noexcept {
#if !SIMDJSON_SKIP_BACKSLASH_SHORT_CIRCUIT
    if (!backslash) {
      uint64_t escaped = this->next_is_escaped;
      this->next_is_escaped = 0;
      return {escaped};
    }
#endif

    // |                | Mask                                 | Depth | Instructions        |
    // |----------------|--------------------------------------|-------|---------------------|
    // | string         | `\\n_\\\n___\\\n___\\\\___\\\\__\\\` |       |                     |
    // | backslash      | `11  111    111    1111   1111  111` |       |                     |
    // | even           | `1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ` |       |                     |
    // | odd            | ` 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1` |       |                     |
    // | e1             | ` 11  1  1 11 11 1  1 11 11 1  11 1` | 1     | 1 (backslash ^ even)
    // | e2             | `  1    11 1111  1     1 11111 1111` | 2 (a) | 3 (a) (set carry flag; sbb odd, e1) |
    // | borrow out (b) | `  1     1 11111 1     1 11111 1111` | 3 (a) | 4 (a) setnc         |
    // | e3             | ` 1   1 1    1 1    1 1    1 1   1 ` | 3     | 5 (e1 ^ e2)         |
    //  (a) Spilling and restoring borrows are only necessary across loop iterations.
    //      Within an unrolled loop iteration, the corresponding instructions are eliminated.
    //  (b) next_is_escaped is equal to the final borrow out.
    uint64_t e1 = backslash ^ EVEN_BITS;
    unsigned long long e2;
    this->next_is_escaped = _subborrow_u64(this->next_is_escaped, ODD_BITS, e1, &e2);
    uint64_t e3 = e2 ^ e1;
    return {e3};
  }

private:
  static constexpr const uint64_t EVEN_BITS = 0x5555555555555555ULL;
  static constexpr const uint64_t ODD_BITS = ~EVEN_BITS;
};

} // namespace stage1
} // unnamed namespace
} // namespace SIMDJSON_IMPLEMENTATION
} // namespace simdjson

#endif // SIMDJSON_SRC_GENERIC_STAGE1_JSON_STRING_SCANNER_H
