#pragma once

// Minimal deterministic host test harness for LED-L002 pure modules
// (no external framework; built with g++ -Wall -Wextra -Werror).

#include <cstdint>
#include <cstdio>
#include <cstring>

extern int g_checks;
extern int g_failures;

#define CHECK(cond)                                                                      \
  do {                                                                                   \
    ++g_checks;                                                                          \
    if (!(cond)) {                                                                       \
      ++g_failures;                                                                      \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                        \
    }                                                                                    \
  } while (0)

#define CHECK_EQ_INT(actual, expected)                                                   \
  do {                                                                                   \
    ++g_checks;                                                                          \
    const long long a_ = static_cast<long long>(actual);                                 \
    const long long e_ = static_cast<long long>(expected);                               \
    if (a_ != e_) {                                                                      \
      ++g_failures;                                                                      \
      std::printf("FAIL %s:%d: %s == %s (got %lld, want %lld)\n", __FILE__, __LINE__,    \
                  #actual, #expected, a_, e_);                                           \
    }                                                                                    \
  } while (0)

#define CHECK_STR_EQ(actual, expected)                                                   \
  do {                                                                                   \
    ++g_checks;                                                                          \
    const char* a_ = (actual);                                                           \
    const char* e_ = (expected);                                                         \
    if (a_ == nullptr || std::strcmp(a_, e_) != 0) {                                     \
      ++g_failures;                                                                      \
      std::printf("FAIL %s:%d: %s == \"%s\" (got \"%s\")\n", __FILE__, __LINE__,         \
                  #actual, e_, a_ == nullptr ? "(null)" : a_);                           \
    }                                                                                    \
  } while (0)

// test_main.cpp
int run_all_tests();
