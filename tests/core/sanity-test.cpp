// Pipeline sanity check for TEST-1 (Stage 4, plan v8).
//
// This test deliberately does not exercise any project logic -- there is no
// src/ implementation yet to test. Its only job is to prove the test
// pipeline (GoogleTest wired up, built /MT, discovered and run by
// `make test`) works from the very first commit, per 04-FLOW §3.3. Once
// real src/core/ logic lands in Stage 7 (type-conversion, json-path), this
// file should be replaced by tests that mirror those modules; it is not
// meant to accumulate real coverage itself.

#include <gtest/gtest.h>

TEST(Sanity, TrueIsTrue) {
    EXPECT_TRUE(true);
}
