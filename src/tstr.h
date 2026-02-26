/*
 * This file is part of the z-libs collection: https://github.com/z-libs
 * Licensed under the MIT License.
 */

/*
 * tstr.h
 * based on
 * https://github.com/z-libs/tstr.h/commit/5951a7ead6fb0fbba9afea522bb2c97172356066
 *
 * modified to suit my needs
 *
 * License: MIT
 * Author: Zuhaitz
 * Repository: https://github.com/z-libs/tstr.h
 *
 * Modifications by: Totto16
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Memory management.

/* * If the user hasn't defined their own allocator, use the standard C library.
 * To override globally, define these macros before including any ZDK header.
 */
#ifndef T_MALLOC
	#include <stdlib.h>
	#define T_MALLOC(sz) malloc(sz)
	#define T_CALLOC(n, sz) calloc(n, sz)
	#define T_REALLOC(p, sz) realloc(p, sz)
	#define T_FREE(p) free(p)
#endif

// Compiler extensions and optimization.

// Type inference (typeof)
#ifdef __cplusplus
	#include <type_traits>
	#define T_TYPEOF(x) typename std::remove_reference<decltype(x)>::type
	#define T_HAS_TYPEOF 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
	#define T_TYPEOF(x) typeof(x)
	#define T_HAS_TYPEOF 1
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
	#define T_TYPEOF(x) __typeof__(x)
	#define T_HAS_TYPEOF 1
#else
	#define T_HAS_TYPEOF 0
#endif

// Extensions (cleanup, attributes, branch prediction)
#if !defined(T_NO_EXTENSIONS) && (defined(__GNUC__) || defined(__clang__) || defined(__TINYC__))

	#define T_NODISCARD __attribute__((warn_unused_result))

// TCC supports attributes but NOT __builtin_expect
	#if defined(__TINYC__)
		#define T_LIKELY(x) (x)
		#define T_UNLIKELY(x) (x)
	#else
		#define T_LIKELY(x) __builtin_expect(!!(x), 1)
		#define T_UNLIKELY(x) __builtin_expect(!!(x), 0)
	#endif

#else
// Fallback for MSVC or strict standard C.

	#define T_NODISCARD
	#define T_LIKELY(x) (x)
	#define T_UNLIKELY(x) (x)

#endif

// Token concatenation macros (useful for unique variable names in macros).
#define T_CONCAT_(a, b) a##b
#define T_CONCAT(a, b) T_CONCAT_(a, b)
#define T_UNIQUE(prefix) T_CONCAT(prefix, __LINE__)

// Growth strategy.

/* * Determines how containers expand when full.
 * Default is 2.0x (Geometric Growth).
 *
 * Optimization note:
 * 2.0x minimizes realloc calls but can waste memory.
 * 1.5x is often better for memory fragmentation and reuse.
 */
#ifndef T_GROWTH_FACTOR
// Default: Double capacity (2.0x).
	#define T_GROWTH_FACTOR(cap) ((cap) == 0 ? 32 : (cap) * 2)

// Alternative: 1.5x Growth (Uncomment to use in your project).
// #define T_GROWTH_FACTOR(cap) ((cap) == 0 ? 32 : (cap) + (cap) / 2)
#endif

// [Bundled] "zcommon.h" is included inline in this same file
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// I am thinking of you too, C++ devs.
#ifdef __cplusplus
extern "C" {
#endif

/* Configuration and Macros */

#ifndef T_STR_MALLOC
	#define T_STR_MALLOC(sz) (char*)T_MALLOC(sz)
#endif

#ifndef T_STR_CALLOC
	#define T_STR_CALLOC(n, sz) (char*)T_CALLOC(n, sz)
#endif

#ifndef T_STR_REALLOC
	#define T_STR_REALLOC(p, sz) (char*)T_REALLOC(p, sz)
#endif

#ifndef T_STR_FREE
	#define T_STR_FREE(p) T_FREE(p)
#endif

#if defined(__GNUC__) || defined(__clang__)
	#define TSTR_PRINTF_ATTR(fmt_idx, var_idx) __attribute__((format(printf, fmt_idx, var_idx)))
#else
	#define TSTR_PRINTF_ATTR(fmt_idx, var_idx)
#endif

// SSO Capacity -> 23 bytes available.
// Max string length = 23 chars (if using the last byte for length/flag trick)
// OR 22 chars + null terminator. We stick to 23 bytes total storage.
#define TSTR_SSO_CAP 23
#define TSTR_UTF8_INVALID 0xFFFD

#ifndef TSTR_FMT
	#define TSTR_FMT "%.*s"
	#define TSTR_FMT_ARGS(s) ((int)tstr_len(&(s))), (tstr_cstr(&(s)))
	#define TSV_FMT_ARGS(v) ((int)(v).len), ((v).data)
#endif

// Alias macro for pushing a single char.
#define tstr_push(s, c) tstr_push_char(s, c)

/* Data Structures */

// Heap allocated string layout.
typedef struct {
	char* ptr;
	size_t len;
	size_t cap;
} tstr_long;

// Stack allocated (SSO) layout.
typedef struct {
	char buf[TSTR_SSO_CAP];
	uint8_t len;
} tstr_short;

// static (readonly string)
typedef struct {
	const char* ptr;
	size_t len;
} tstr_static;

typedef enum : uint8_t {
	tstr_type_enum_sso = 0,
	tstr_type_enum_long = 1,
	tstr_type_enum_static = 2,
} tstr_type_enum;

typedef struct {
	tstr_type_enum inner;
	uint8_t _pad[sizeof(void*) - sizeof(tstr_type_enum)]; // Padding for alignment
} tstr_type;

// assert that the type is the appropiate size
static_assert(sizeof(tstr_type) == sizeof(void*));

// The main string type.
typedef struct {
	tstr_type type;
	union {
		tstr_long long_str;
		tstr_short short_str;
		tstr_static static_str;
	};
} tstr;

// A read-only slice of a string (borrowed reference).
typedef struct {
	const char* data;
	size_t len;
} tstr_view;

// Iterator state for splitting strings.
typedef struct {
	tstr_view source;
	tstr_view delim;
	size_t current_pos;
	bool finished;
} tstr_split_iter;

typedef struct {
	bool ok;
	tstr_view first;
	tstr_view second;
} tstr_split_result;

typedef enum : bool {
	TStrResultErr = false,
	TStrResultOk = true,
} TStrResult;

// maybe some visibility things later, but I just removed the static inline
#define TSTR_FUN_ATTRIBUTES

/* Internal Helpers and Accessors */

// Returns true if the string is heap-allocated.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_long(const tstr* s);

// Returns true if the string is SSO, allocated on the stack
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_sso(const tstr* s);

// Returns true if the string is a static string, alias not modifiable
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_static(const tstr* s);

// Returns a pointer to the mutable data buffer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_data(tstr* s);

// Returns a pointer to the const data buffer (C-string compatible).
TSTR_FUN_ATTRIBUTES [[nodiscard]] const char* tstr_cstr(const tstr* s);

// Returns the current length of the string (excluding null terminator).
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_len(const tstr* s);

// Returns true if the string length is 0.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_empty(const tstr* s);

// Returns true if the underlying ptr is NULL, or the SSO string is empty
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_null(const tstr* s);

/* Creation and Destruction */

// Initializes an empty string {0}.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_init(void);

// Frees the string if it is on the heap, and resets it to empty.
TSTR_FUN_ATTRIBUTES void tstr_free(tstr* s);

// Clears the content (sets length to 0) but keeps the allocated capacity. static strings get nuked
// in favor of a SSO string
TSTR_FUN_ATTRIBUTES void tstr_clear(tstr* s);

/* Memory Management */

// Ensures the string has at least `new_cap` capacity.
// Handles the transition from SSO (Stack) or static string to Long (Heap).
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_reserve(tstr* s, size_t new_cap);

// Creates a new empty string with pre-allocated capacity on the heap.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_with_capacity(size_t cap);

// Reduces heap usage to fit the exact string length (or moves back to SSO if small enough).
TSTR_FUN_ATTRIBUTES void tstr_shrink_to_fit(tstr* s);

/* Construction Helpers */

// Helper: Creates tstr from ptr + explicit len.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_len(const char* ptr, size_t len);

// Creates a tstr from a standard C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from(const char* cstr);

#define REQUIRE_STRING_LITERAL(s) \
	(0 * sizeof(char[1][__builtin_types_compatible_p(__typeof__(s), __typeof__(&(s)[0])) ? -1 : 1]))

#define TSTR_SIZE_OF_STR_LIT(s) ((sizeof(s) - 1) + (REQUIRE_STRING_LITERAL(s)))

// Macro for compile-time string literals (avoids runtime strlen).
#define TSTR_LIT(s) \
	((tstr){ .type = { .inner = tstr_type_enum_static }, \
	         .static_str = (tstr_static){ .ptr = (s), .len = TSTR_SIZE_OF_STR_LIT(s) } })

// Initializes a static string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_cstr(const char* str);

// Initializes a static string with length
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_cstr_with_len(const char* str, size_t len);

// Creates a deep copy of a tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_dup(const tstr* s);

// Takes ownership of a malloc'd pointer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_own(char* ptr, size_t len, size_t cap);

// Takes ownership of a malloc'd pointer, it gets the length and capacity from strlen()
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_own_cstr(char* ptr);

// Releases ownership. Returns a malloc'd pointer the user MUST free.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_take(tstr* s);

// Reads an entire file into a tstr. Returns empty on failure.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_read_file(const char* path);

// Appends a single character to the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_push_char(tstr* s, char c);

// Removes and returns the last character of the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char tstr_pop_char(tstr* s);

// Appends a raw char buffer of known length.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat_len(tstr* s, const char* src, size_t src_len);

// Appends a null-terminated C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat(tstr* s, const char* cstr);

// Joins an array of strings with a delimiter.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_join(const char** strings, size_t count,
                                                 const char* delim);

// Formats a string (printf style) and appends it.
TSTR_PRINTF_ATTR(2, 3)
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_fmt(tstr* s, const char* fmt, ...);

/* In-Place Transformations */

// Converts the string to lowercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_lower(tstr* s);

// Converts the string to uppercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_upper(tstr* s);

// Removes leading and trailing whitespace in-place.
TSTR_FUN_ATTRIBUTES void tstr_trim(tstr* s);

// Replaces all occurrences of "target" with "replacement".
// This may reallocate the string if the size grows.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_replace(tstr* s, const char* target,
                                                          const char* replacement);

/* Comparison */

// Checks equality between two tstr objects (faster than strcmp).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq(const tstr* a, const tstr* b);

// Checks equality between two tstr objects (faster than strcmp).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_cstr(const tstr* a, const char* b);

// Checks equality ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_ignore_case(const tstr* a, const tstr* b);

// Checks equality ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_ignore_case_cstr(const tstr* a, const char* b);

// Standard strcmp behavior for tstr objects.
TSTR_FUN_ATTRIBUTES [[nodiscard]] int tstr_cmp(const tstr* a, const tstr* b);

/* Search */

// Returns the index of the first occurrence of needle, or -1 if not found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] ptrdiff_t tstr_find(const tstr* s, const char* needle);

// Returns true if the string contains the substring.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_contains(const tstr* s, const char* needle);

/* UTF-8 Support */

// Decodes the next rune from the pointer and advances the pointer.
// Returns TSTR_UTF8_INVALID (0xFFFD) on error.
TSTR_FUN_ATTRIBUTES [[nodiscard]] uint32_t tstr_next_rune(const char** p);

// Counts the number of actual UTF-8 Runes, not bytes.
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_count_runes(const tstr* s);

// Validates that the string is strictly valid UTF-8.
// Rejects Overlong encodings, Surrogates, and out-of-bounds values.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_valid_utf8(const tstr* s);

/* Views and Slices (Zero-Copy) */

// Helper macro to create a view from a string literal.
#define TSTR_TSV(lit) ((tstr_view){ .data = (lit), .len = TSTR_SIZE_OF_STR_LIT(lit) })

#define TSTR_EMPTY_VIEW (tstr_view){ .data = NULL, .len = 0 }

// Creates a view from a C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_from(const char* cstr);

// Creates a view covering the entire tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_as_view(const tstr* s);

// Converts a view back into an owning tstr (allocates).
TSTR_FUN_ATTRIBUTES tstr tstr_from_view(tstr_view v);

// Returns a substring view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_sub(tstr_view v, size_t start, size_t len);

// Returns a substring view, from start until end
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_sub_until_end(tstr_view v, size_t start);

// Returns the view, that starts after the first occurrence of needle, or NULL for data if not
// found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_find(tstr_view v, const char* needle);

// Checks if view equals a C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq(tstr_view v, const char* cstr);

// Checks if view equals a C-string, ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_ignore_case(tstr_view v, const char* cstr);

// Checks if two views are equal.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_view(tstr_view a, tstr_view b);

// Checks if view starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_starts_with(tstr_view v, const char* prefix);

// Checks if view ends with suffix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_ends_with(tstr_view v, const char* suffix);

// Wrapper for checking if an owning tstr starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_starts_with(const tstr* s, const char* prefix);

// Wrapper for checking if an owning tstr ends with suffix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_ends_with(const tstr* s, const char* suffix);

// Trims whitespace from the start of the view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_lstrip(tstr_view v);

// Trims whitespace from the end of the view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_rstrip(tstr_view v);

// Trims whitespace from both ends.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_trim(tstr_view v);

// Converts a view to an integer (simple atoi replacement).
// Returns true if successful, false if empty or invalid chars found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_to_int(tstr_view v, int* out);

// Initializes an iterator for splitting a string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_split_iter tstr_split_init(tstr_view src, const char* delim);

// Gets the next part in a split iteration. Returns false when done.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_split_next(tstr_split_iter* it, tstr_view* out_part);

// Splits a tstr_view into two parts, return false if it couldn#t be split, the second part can also
// be of length 0, if the delimiter is at the end of the src
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_split_result tstr_split(tstr_view src, const char* delim);

#ifdef __cplusplus
} // extern "C"
#endif
