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
#define TSTR_UTF8_INVALID_RUNES ((size_t)-1)

#ifndef TSTR_FMT
	#define TSTR_FMT "%.*s"
	#define TSTR_FMT_ARGS(str) ((int)tstr_len(&(str))), (tstr_cstr(&(str)))
	#define TSTR_STATIC_FMT_ARGS(str) ((int)((str).len)), ((str).ptr)
	#define TSV_FMT_ARGS(str_vw) ((int)(str_vw).len), ((str_vw).data)
#endif

// Alias macro for pushing a single char.
#define tstr_push(str, chr) tstr_push_char(str, chr)

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
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_long(const tstr* str);

// Returns true if the string is SSO, allocated on the stack
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_sso(const tstr* str);

// Returns true if the string is a static string, alias not modifiable
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_static(const tstr* str);

// Returns a pointer to the mutable data buffer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_data(tstr* str);

// Returns a pointer to the const data buffer (C-string compatible).
TSTR_FUN_ATTRIBUTES [[nodiscard]] const char* tstr_cstr(const tstr* str);

// Returns the current length of the string (excluding null terminator).
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_len(const tstr* str);

// Returns true if the string length is 0.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_empty(const tstr* str);

// Returns true if the underlying ptr is NULL, it is always false for SSO strings
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_null(const tstr* str);

// Returns true if the underlying ptr is NULL
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_static_is_null(tstr_static str);

/* Creation and Destruction */

// Initializes an empty string {0}.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_init(void);

// Initializes a string with ptr set to NULL
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_null(void);

// Initializes a static string with ptr set to NULL
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_static tstr_static_null(void);

// Frees the string if it is on the heap, and resets it to empty.
TSTR_FUN_ATTRIBUTES void tstr_free(tstr* str);

// Clears the content (sets length to 0) but keeps the allocated capacity. static strings get nuked
// in favor of a SSO string
TSTR_FUN_ATTRIBUTES void tstr_clear(tstr* str);

/* Memory Management */

// Ensures the string has at least `new_cap` capacity.
// Handles the transition from SSO (Stack) or static string to Long (Heap).
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_reserve(tstr* str, size_t new_cap);

// Creates a new empty string with pre-allocated capacity on the heap.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_with_capacity(size_t cap);

// Reduces heap usage to fit the exact string length (or moves back to SSO if small enough).
TSTR_FUN_ATTRIBUTES void tstr_shrink_to_fit(tstr* str);

/* Construction Helpers */

// Helper: Creates tstr from ptr + explicit len.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_len(const char* ptr, size_t len);

// Creates a tstr from a standard C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from(const char* cstr);

#define REQUIRE_STRING_LITERAL(str) \
	(0 * \
	 sizeof( \
	     char[1][__builtin_types_compatible_p(__typeof__(str), __typeof__(&(str)[0])) ? -1 : 1]))

#define TSTR_SIZE_OF_STR_LIT(str) ((sizeof(str) - 1) + (REQUIRE_STRING_LITERAL(str)))

// Macro for compile-time string literals (avoids runtime strlen).
#define TSTR_STATIC_LIT(str) ((tstr_static){ .ptr = (str), .len = TSTR_SIZE_OF_STR_LIT(str) })

#define TSTR_LIT(str) \
	((tstr){ .type = { .inner = tstr_type_enum_static }, \
	         .static_str = (tstr_static){ .ptr = (str), .len = TSTR_SIZE_OF_STR_LIT(str) } })

#if defined(__GNUC__) && (!(defined(__clang__)))
	#define TSTR_STATIC_LIT_CONST(str) { .ptr = (str), .len = TSTR_SIZE_OF_STR_LIT(str) }

	#define TSTR_LIT_CONST(str) \
		{ \
			.type = { .inner = tstr_type_enum_static }, .static_str = { \
				.ptr = (str), \
				.len = TSTR_SIZE_OF_STR_LIT(str) \
			} \
		}
#else
	#define TSTR_STATIC_LIT_CONST(str) TSTR_STATIC_LIT(str)
	#define TSTR_LIT_CONST(str) TSTR_LIT(str)
#endif

// Initializes a static string as tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_cstr(const char* str);

// Initializes a static string with length as tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_cstr_with_len(const char* cstr, size_t len);

// Initializes a static string as tstr_static.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_static tstr_static_from_static_cstr(const char* str);

// Initializes a static string with length as tstr_static.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_static
tstr_static_from_static_cstr_with_len(const char* cstr, size_t len);

// Initializes a tstr form a static string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_tstr(tstr_static static_str);

// Creates a deep copy of a tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_dup(const tstr* str);

// Takes ownership of a malloc'd pointer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_own(char* ptr, size_t len, size_t cap);

// Takes ownership of a malloc'd pointer, it gets the length and capacity from strlen()
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_own_cstr(char* ptr);

// Releases ownership. Returns a malloc'd pointer the user MUST free.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_take(tstr* str);

// Reads an entire file into a tstr. Returns empty on failure.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_read_file(const char* path);

// Appends a single character to the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_push_char(tstr* str, char chr);

// Removes and returns the last character of the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char tstr_pop_char(tstr* str);

// Appends a raw char buffer of known length.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat_len(tstr* str, const char* src,
                                                          size_t src_len);

// Appends a null-terminated C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat(tstr* str, const char* cstr);

// Joins an array of strings with a delimiter.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_join(const char** strings, size_t count,
                                                 const char* delim);

// Formats a string (printf style) and appends it.
[[nodiscard]] TSTR_PRINTF_ATTR(2, 3) TSTR_FUN_ATTRIBUTES TStrResult
    tstr_fmt(tstr* str, const char* fmt, ...);

/* In-Place Transformations */

// Converts the string to lowercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_lower(tstr* str);

// Converts the string to uppercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_upper(tstr* str);

// Removes leading and trailing whitespace in-place.
TSTR_FUN_ATTRIBUTES void tstr_trim(tstr* str);

// Replaces all occurrences of "target" with "replacement".
// This may reallocate the string if the size grows.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_replace(tstr* str, const char* target,
                                                          const char* replacement);

/* Comparison */

// Checks equality between two tstr objects (faster than strcmp).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq(const tstr* str1, const tstr* str2);

// Checks equality between a tstr and a cstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_cstr(const tstr* str1, const char* str2);

// Checks equality ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_ignore_case(const tstr* str1, const tstr* str2);

// Checks equality ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_ignore_case_cstr(const tstr* str1, const char* str2);

// Standard strcmp behavior for tstr objects.
TSTR_FUN_ATTRIBUTES [[nodiscard]] int tstr_cmp(const tstr* str1, const tstr* str2);

// Checks equality between two tstr_static objects (faster than strcmp).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_static_eq(tstr_static str1, tstr_static str2);

// Checks equality between a tstr_static and a cstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_static_eq_cstr(tstr_static str1, const char* str2);

/* Search */

// Returns the index of the first occurrence of needle, or -1 if not found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] ptrdiff_t tstr_find(const tstr* str, const char* needle);

// Returns true if the string contains the substring.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_contains(const tstr* str, const char* needle);

/* UTF-8 Support */

// Decodes the next rune from the pointer and advances the pointer.
// Returns TSTR_UTF8_INVALID (0xFFFD) on error.
TSTR_FUN_ATTRIBUTES [[nodiscard]] uint32_t tstr_next_rune(const char** ptr);

// Counts the number of actual UTF-8 Runes, not bytes.
// Returns TSTR_UTF8_INVALID_RUNES ((size_t)-1) on error.
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_count_runes(const tstr* str);

// Validates that the string is strictly valid UTF-8.
// Rejects Overlong encodings, Surrogates, and out-of-bounds values.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_valid_utf8(const tstr* str);

/* Views and Slices (Zero-Copy) */

// Helper macro to create a view from a string literal.
#define TSTR_TSV(lit) ((tstr_view){ .data = (lit), .len = TSTR_SIZE_OF_STR_LIT(lit) })

#define TSTR_EMPTY_VIEW (tstr_view){ .data = NULL, .len = 0 }

// Creates a view from a C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_from(const char* cstr);

// Creates a view covering the entire tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_as_view(const tstr* str);

// Creates a view covering the entire tstr_static.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_static_as_view(tstr_static str);

// Converts a view back into an owning tstr (allocates).
TSTR_FUN_ATTRIBUTES tstr tstr_from_view(tstr_view view);

// Returns a substring view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_sub(tstr_view view, size_t start, size_t len);

// Returns a substring view, from start until end
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_sub_until_end(tstr_view view, size_t start);

// Returns the view, that starts after the first occurrence of needle, or NULL for data if not
// found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_find(tstr_view view, const char* needle);

// Checks if view equals a C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq(tstr_view view, const char* cstr);

// Checks if view equals a C-string, ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_ignore_case(tstr_view view, const char* cstr);

// Checks if two views are equal.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_view(tstr_view vw1, tstr_view vw2);

// Standard strcmp behavior for tstr_view objects.
TSTR_FUN_ATTRIBUTES [[nodiscard]] int tstr_view_cmp(tstr_view vw1, tstr_view vw2);

// Checks if view starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_starts_with(tstr_view view, const char* prefix);

// Checks if view ends with suffix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_ends_with(tstr_view view, const char* suffix);

// Wrapper for checking if an owning tstr starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_starts_with(const tstr* str, const char* prefix);

// Wrapper for checking if an owning tstr ends with suffix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_ends_with(const tstr* str, const char* suffix);

// Trims whitespace from the start of the view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_lstrip(tstr_view view);

// Trims whitespace from the end of the view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_rstrip(tstr_view view);

// Trims whitespace from both ends.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_trim(tstr_view view);

// Converts a view to an integer (simple atoi replacement).
// Returns true if successful, false if empty or invalid chars found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_to_int(tstr_view view, int* out);

// Initializes an iterator for splitting a string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_split_iter tstr_split_init(tstr_view src, const char* delim);

// Gets the next part in a split iteration. Returns false when done.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_split_next(tstr_split_iter* iter, tstr_view* out_part);

// Splits a tstr_view into two parts, return false if it couldn#t be split, the second part can also
// be of length 0, if the delimiter is at the end of the src
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_split_result tstr_split(tstr_view src, const char* delim);

#ifdef __cplusplus
} // extern "C"
#endif
