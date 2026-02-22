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

	#define T_HAS_CLEANUP 1
	#define T_CLEANUP(func) __attribute__((cleanup(func)))
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
	#define T_HAS_CLEANUP 0
	#define T_CLEANUP(func)
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

#ifndef TSTR_H
	#define TSTR_H
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
		#define TSTR_ARG(s) (int)tstr_len(&(s)), tstr_cstr(&(s))
		#define ZSV_ARG(v) (int)(v).len, (v).data
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

// The main string type.
typedef struct {
	uint8_t is_long;
	char _pad[7]; // Padding for alignment on 64-bit systems.
	union {
		tstr_long l;
		tstr_short s;
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

typedef enum : bool {
	TStrResultErr = false,
	TStrResultOk = true,
} TStrResult;

    // maybe some visibility things later, but I just removed the static inline
	#define TSTR_FUN_ATTRIBUTES

/* Internal Helpers and Accessors */

// Returns true if the string is heap-allocated.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_long(const tstr* s) {
	return s->is_long;
}

// Returns a pointer to the mutable data buffer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_data(tstr* s) {
	return s->is_long ? s->l.ptr : s->s.buf;
}

// Returns a pointer to the const data buffer (C-string compatible).
TSTR_FUN_ATTRIBUTES [[nodiscard]] const char* tstr_cstr(const tstr* s) {
	return s->is_long ? s->l.ptr : s->s.buf;
}

// Returns the current length of the string (excluding null terminator).
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_len(const tstr* s) {
	return s->is_long ? s->l.len : s->s.len;
}

// Returns true if the string length is 0.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_empty(const tstr* s) {
	return tstr_len(s) == 0;
}

/* Creation and Destruction */

// Initializes an empty string {0}.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_init(void) {
	tstr s;
	memset(&s, 0, sizeof(tstr));
	return s;
}

// Frees the string if it is on the heap, and resets it to empty.
TSTR_FUN_ATTRIBUTES void tstr_free(tstr* s) {
	if(s->is_long) T_STR_FREE(s->l.ptr);
	*s = tstr_init();
}

// Clears the content (sets length to 0) but keeps the allocated capacity.
TSTR_FUN_ATTRIBUTES void tstr_clear(tstr* s) {
	if(s->is_long) {
		s->l.len = 0;
		s->l.ptr[0] = '\0';
	} else {
		s->s.len = 0;
		s->s.buf[0] = '\0';
	}
}

/* Memory Management */

// Ensures the string has at least `new_cap` capacity.
// Handles the transition from SSO (Stack) to Long (Heap).
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_reserve(tstr* s, size_t new_cap) {
	if(new_cap < TSTR_SSO_CAP) {
		return TStrResultOk;
	}

	if(s->is_long && new_cap <= s->l.cap) {
		return TStrResultOk;
	}

	char* new_ptr;
	if(s->is_long) {
		new_ptr = T_STR_REALLOC(s->l.ptr, new_cap + 1);
	} else {
		new_ptr = T_STR_MALLOC(new_cap + 1);
		if(new_ptr) {
			memcpy(new_ptr, s->s.buf, s->s.len);
			new_ptr[s->s.len] = '\0';
		}
	}

	if(!new_ptr) {
		return TStrResultErr;
	}

	// Transition state if we were short before.
	if(!s->is_long) {
		s->l.len = s->s.len;
		s->is_long = 1;
	}

	s->l.ptr = new_ptr;
	s->l.cap = new_cap;

	return TStrResultOk;
}

// Creates a new empty string with pre-allocated capacity on the heap.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_with_capacity(size_t cap) {
	tstr s = tstr_init();
	if(cap > TSTR_SSO_CAP) {
		tstr_reserve(&s, cap);
	}
	return s;
}

// Reduces heap usage to fit the exact string length (or moves back to SSO if small enough).
TSTR_FUN_ATTRIBUTES void tstr_shrink_to_fit(tstr* s) {
	if(!s->is_long) {
		return;
	}

	// Downgrade to SSO if possible.
	if(s->l.len <= TSTR_SSO_CAP) {
		char temp[TSTR_SSO_CAP];
		memcpy(temp, s->l.ptr, s->l.len);
		temp[s->l.len] = '\0';

		uint8_t old_len = (uint8_t)s->l.len;
		T_STR_FREE(s->l.ptr);

		s->is_long = 0;
		memcpy(s->s.buf, temp, old_len + 1);
		s->s.len = old_len;
		return;
	}

	if(s->l.len < s->l.cap) {
		char* new_ptr = T_STR_REALLOC(s->l.ptr, s->l.len + 1);
		if(new_ptr) {
			s->l.ptr = new_ptr;
			s->l.cap = s->l.len;
		}
	}
}

/* Construction Helpers */

// Helper: Creates tstr from ptr + explicit len.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_len(const char* ptr, size_t len) {
	tstr s = tstr_init();
	if(len >= TSTR_SSO_CAP) {
		if(tstr_reserve(&s, len) != TStrResultOk) {
			return s;
		}
		memcpy(s.l.ptr, ptr, len);
		s.l.ptr[len] = '\0';
		s.l.len = len;
	} else {
		memcpy(s.s.buf, ptr, len);
		s.s.buf[len] = '\0';
		s.s.len = (uint8_t)len;
		s.is_long = 0;
	}
	return s;
}

// Creates a tstr from a standard C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from(const char* cstr) {
	return tstr_from_len(cstr, strlen(cstr));
}

    // Macro for compile-time string literals (avoids runtime strlen).
	#define tstr_lit(s) tstr_from_len((s), sizeof(s) - 1)

// Creates a deep copy of a tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_dup(const tstr* s) {
	return tstr_from_len(tstr_cstr(s), tstr_len(s));
}

// Takes ownership of a malloc'd pointer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_own(char* ptr, size_t len, size_t cap) {
	tstr s = tstr_init();

	if(cap <= TSTR_SSO_CAP) {
		memcpy(s.s.buf, ptr, len);
		s.s.buf[len] = '\0';
		s.s.len = (uint8_t)len;
		s.is_long = 0;
		T_STR_FREE(ptr);
	} else {
		s.is_long = 1;
		s.l.ptr = ptr;
		s.l.len = len;
		s.l.cap = cap;
	}
	return s;
}

// Releases ownership. Returns a malloc'd pointer the user MUST free.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_take(tstr* s) {
	char* ptr;

	if(s->is_long) {
		ptr = s->l.ptr;
	} else {
		ptr = T_STR_MALLOC(s->s.len + 1);
		if(ptr) {
			memcpy(ptr, s->s.buf, s->s.len);
			ptr[s->s.len] = '\0';
		}
	}

	// Reset the source struct so it doesn't double-free.
	*s = tstr_init();
	return ptr;
}

// Reads an entire file into a tstr. Returns empty on failure.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_read_file(const char* path) {
	tstr s = tstr_init();
	FILE* f = fopen(path, "rb");
	if(!f) return s;

	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);

	// Pedantic check.
	if(length <= 0 || (sizeof(long) > sizeof(size_t) && (size_t)length > (size_t)-1)) {
		fclose(f);
		return s;
	}

	if(tstr_reserve(&s, (size_t)length) != TStrResultOk) {
		fclose(f);
		return s;
	}

	char* buf = tstr_data(&s);
	size_t read_count = fread(buf, 1, (size_t)length, f);
	buf[read_count] = '\0';

	if(s.is_long)
		s.l.len = read_count;
	else
		s.s.len = (uint8_t)read_count;

	fclose(f);
	return s;
}

// Appends a single character to the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_push_char(tstr* s, char c) {
	size_t len = tstr_len(s);
	if(len + 1 >= (s->is_long ? s->l.cap : TSTR_SSO_CAP)) {
		size_t cap = s->is_long ? s->l.cap : TSTR_SSO_CAP;
		size_t new_cap = T_GROWTH_FACTOR(cap);

		if(tstr_reserve(s, new_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	char* p = tstr_data(s);
	p[len] = c;
	p[len + 1] = '\0';

	if(s->is_long) {
		s->l.len++;
	} else {
		s->s.len++;
	}

	return TStrResultOk;
}

// Removes and returns the last character of the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char tstr_pop_char(tstr* s) {
	size_t len = tstr_len(s);
	if(len == 0) return '\0';

	char* p = tstr_data(s);
	char c = p[len - 1];
	p[len - 1] = '\0';

	if(s->is_long)
		s->l.len--;
	else
		s->s.len--;

	return c;
}

// Appends a raw char buffer of known length.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat_len(tstr* s, const char* src,
                                                          size_t src_len) {
	size_t cur_len = tstr_len(s);
	size_t req_cap = cur_len + src_len;

	if(req_cap >= (s->is_long ? s->l.cap : TSTR_SSO_CAP)) {
		size_t new_cap = s->is_long ? s->l.cap : TSTR_SSO_CAP;
		// Logic fixed: starting cap is 23. If we grow, we just multiply.
		// We do not fallback to 32 because 23 > 0.
		if(new_cap == 0) new_cap = TSTR_SSO_CAP;

		while(new_cap <= req_cap)
			new_cap = T_GROWTH_FACTOR(new_cap);

		if(tstr_reserve(s, new_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	char* dest = tstr_data(s);
	memcpy(dest + cur_len, src, src_len);
	dest[cur_len + src_len] = '\0';

	if(s->is_long) {
		s->l.len += src_len;
	} else {
		s->s.len += (uint8_t)src_len;
	}

	return TStrResultOk;
}

// Appends a null-terminated C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat(tstr* s, const char* cstr) {
	return tstr_cat_len(s, cstr, strlen(cstr));
}

// Joins an array of strings with a delimiter.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_join(const char** strings, size_t count,
                                                 const char* delim) {
	tstr s = tstr_init();
	if(count == 0) {
		return s;
	}

	size_t delim_len = strlen(delim);
	size_t total_len = 0;

	for(size_t i = 0; i < count; i++) {
		total_len += strlen(strings[i]);
		if(i < count - 1) total_len += delim_len;
	}

	if(tstr_reserve(&s, total_len) != TStrResultOk) {
		return s;
	}

	for(size_t i = 0; i < count; i++) {
		tstr_cat(&s, strings[i]);
		if(i < count - 1) tstr_cat(&s, delim);
	}
	return s;
}

// Formats a string (printf style) and appends it.
TSTR_PRINTF_ATTR(2, 3)
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_fmt(tstr* s, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if(len < 0) {
		return TStrResultErr;
	}

	size_t cur_len = tstr_len(s);
	size_t req_cap = cur_len + len;

	if(req_cap >= (s->is_long ? s->l.cap : TSTR_SSO_CAP)) {
		if(tstr_reserve(s, req_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	va_start(args, fmt);
	char* buf = tstr_data(s);
	vsnprintf(buf + cur_len, len + 1, fmt, args);
	va_end(args);

	if(s->is_long) {
		s->l.len += len;
	} else {
		s->s.len += (uint8_t)len;
	}

	return TStrResultOk;
}

/* In-Place Transformations */

// Converts the string to lowercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_lower(tstr* s) {
	char* p = tstr_data(s);
	size_t len = tstr_len(s);
	for(size_t i = 0; i < len; i++) {
		p[i] = (char)tolower((unsigned char)p[i]);
	}
}

// Converts the string to uppercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_upper(tstr* s) {
	char* p = tstr_data(s);
	size_t len = tstr_len(s);
	for(size_t i = 0; i < len; i++) {
		p[i] = (char)toupper((unsigned char)p[i]);
	}
}

// Removes leading and trailing whitespace in-place.
TSTR_FUN_ATTRIBUTES void tstr_trim(tstr* s) {
	if(tstr_len(s) == 0) return;

	char* start = tstr_data(s);
	char* end = start + tstr_len(s) - 1;

	while(end >= start && isspace((unsigned char)*end)) {
		*end = '\0';
		end--;
	}

	size_t final_len = (end >= start) ? (size_t)(end - start + 1) : 0;

	char* new_start = start;
	while(new_start <= end && isspace((unsigned char)*new_start)) {
		new_start++;
	}

	// Adjust final length based on left trim.
	final_len = (new_start <= end) ? (size_t)(end - new_start + 1) : 0;

	if(new_start > start) {
		memmove(start, new_start, final_len);
		start[final_len] = '\0';
	}

	if(s->is_long)
		s->l.len = final_len;
	else
		s->s.len = (uint8_t)final_len;
}

// Replaces all occurrences of "target" with "replacement".
// This may reallocate the string if the size grows.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_replace(tstr* s, const char* target,
                                                          const char* replacement) {
	if(!target || !*target) {
		return TStrResultErr;
	}

	char* src = tstr_data(s);
	char* p = strstr(src, target);

	if(!p) {
		return TStrResultOk;
	}

	size_t target_len = strlen(target);
	size_t repl_len = strlen(replacement);
	size_t count = 0;

	char* scan = src;
	while((scan = strstr(scan, target)) != NULL) {
		count++;
		scan += target_len;
	}

	size_t old_len = tstr_len(s);
	size_t new_len = old_len + (count * (repl_len - target_len));

	tstr res = tstr_init();
	if(tstr_reserve(&res, new_len) != TStrResultOk) {
		return TStrResultErr;
	}

	char* dest = tstr_data(&res);
	char* curr_src = src;
	char* curr_dest = dest;

	while((p = strstr(curr_src, target)) != NULL) {
		size_t segment_len = p - curr_src;
		memcpy(curr_dest, curr_src, segment_len);
		curr_dest += segment_len;

		memcpy(curr_dest, replacement, repl_len);
		curr_dest += repl_len;

		curr_src = p + target_len;
	}

	strcpy(curr_dest, curr_src);

	if(res.is_long) {
		res.l.len = new_len;
	} else {
		res.s.len = (uint8_t)new_len;
	}

	tstr_free(s);
	*s = res;

	return TStrResultOk;
}

/* Comparison */

// Checks equality between two tstr objects (faster than strcmp).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq(const tstr* a, const tstr* b) {
	size_t la = tstr_len(a);
	size_t lb = tstr_len(b);
	if(la != lb) {
		return false;
	}
	return memcmp(tstr_cstr(a), tstr_cstr(b), la) == 0;
}

// Checks equality ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_ignore_case(const tstr* a, const tstr* b) {
	size_t len = tstr_len(a);
	if(len != tstr_len(b)) {
		return false;
	}

	const char* p1 = tstr_cstr(a);
	const char* p2 = tstr_cstr(b);

	for(size_t i = 0; i < len; i++) {
		if(tolower((unsigned char)p1[i]) != tolower((unsigned char)p2[i])) {
			return false;
		}
	}
	return true;
}

// Standard strcmp behavior for tstr objects.
TSTR_FUN_ATTRIBUTES [[nodiscard]] int tstr_cmp(const tstr* a, const tstr* b) {
	return strcmp(tstr_cstr(a), tstr_cstr(b));
}

/* Search */

// Returns the index of the first occurrence of needle, or -1 if not found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] ptrdiff_t tstr_find(const tstr* s, const char* needle) {
	const char* data = tstr_cstr(s);
	const char* found = strstr(data, needle);
	if(!found) {
		return -1;
	}
	return (ptrdiff_t)(found - data);
}

// Returns true if the string contains the substring.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_contains(const tstr* s, const char* needle) {
	return tstr_find(s, needle) != -1;
}

/* UTF-8 Support */

// Decodes the next rune from the pointer and advances the pointer.
// Returns TSTR_UTF8_INVALID (0xFFFD) on error.
TSTR_FUN_ATTRIBUTES [[nodiscard]]  uint32_t tstr_next_rune(const char** p) {
	const unsigned char* str = (const unsigned char*)*p;
	unsigned char c = *str;

	if(c == '\0') return 0;

	if(c < 0x80) {
		*p += 1;
		return c;
	}

	uint32_t rune = 0;
	int len = 0;

	if((c & 0xE0) == 0xC0) {
		rune = c & 0x1F;
		len = 2;
	} else if((c & 0xF0) == 0xE0) {
		rune = c & 0x0F;
		len = 3;
	} else if((c & 0xF8) == 0xF0) {
		rune = c & 0x07;
		len = 4;
	} else {
		*p += 1;
		return TSTR_UTF8_INVALID;
	}

	for(int i = 1; i < len; i++) {
		unsigned char next = str[i];
		if((next & 0xC0) != 0x80) {
			*p += 1;
			return TSTR_UTF8_INVALID;
		}
		rune = (rune << 6) | (next & 0x3F);
	}

	*p += len;
	return rune;
}

// Counts the number of actual UTF-8 Runes, not bytes.
TSTR_FUN_ATTRIBUTES [[nodiscard]]  size_t tstr_count_runes(const tstr* s) {
	const char* ptr = tstr_cstr(s);
	size_t count = 0;
	while(*ptr) {
		tstr_next_rune(&ptr);
		count++;
	}
	return count;
}

// Validates that the string is strictly valid UTF-8.
// Rejects Overlong encodings, Surrogates, and out-of-bounds values.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_valid_utf8(const tstr* s) {
	const unsigned char* p = (const unsigned char*)tstr_cstr(s);
	while(*p) {
		if(*p < 0x80) {
			p++;
		} else if((*p & 0xE0) == 0xC0) // 2-byte.
		{
			if(*p < 0xC2) return false; // Overlong.
			p++;
			if((*p & 0xC0) != 0x80) return false;
			p++;
		} else if((*p & 0xF0) == 0xE0) // 3-byte.
		{
			unsigned char b1 = *p++;
			unsigned char b2 = *p++;
			if((*p & 0xC0) != 0x80) return false;

			if(b1 == 0xE0 && b2 < 0xA0) return false;  // Overlong.
			if(b1 == 0xED && b2 >= 0xA0) return false; // Surrogate.
			if((b2 & 0xC0) != 0x80) return false;
			p++;
		} else if((*p & 0xF8) == 0xF0) // 4-byte.
		{
			unsigned char b1 = *p++;
			if(b1 > 0xF4) return false; // > U+10FFFF.

			unsigned char b2 = *p++;
			if(b1 == 0xF0 && b2 < 0x90) return false;  // Overlong.
			if(b1 == 0xF4 && b2 >= 0x90) return false; // > U+10FFFF.
			if((b2 & 0xC0) != 0x80) return false;

			if((*p++ & 0xC0) != 0x80) return false;
			if((*p++ & 0xC0) != 0x80) return false;
		} else {
			return false;
		}
	}
	return true;
}

    /* Views and Slices (Zero-Copy) */

    // Helper macro to create a view from a string literal.
	#define ZSV(lit) (tstr_view){ .data = (lit), .len = sizeof(lit) - 1 }

// Creates a view from a C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_from(const char* cstr) {
	return (tstr_view){ .data = cstr, .len = strlen(cstr) };
}

// Creates a view covering the entire tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_as_view(const tstr* s) {
	return (tstr_view){ .data = tstr_cstr(s), .len = tstr_len(s) };
}

// Converts a view back into an owning tstr (allocates).
TSTR_FUN_ATTRIBUTES tstr tstr_from_view(tstr_view v) {
	return tstr_from_len(v.data, v.len);
}

// Returns a substring view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_sub(tstr_view v, size_t start, size_t len) {
	if(start >= v.len) return (tstr_view){ "", 0 };
	if(start + len > v.len) len = v.len - start;
	return (tstr_view){ .data = v.data + start, .len = len };
}

// Checks if view equals a C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq(tstr_view v, const char* cstr) {
	if(strlen(cstr) != v.len) return false;
	return memcmp(v.data, cstr, v.len) == 0;
}

// Checks if two views are equal.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_view(tstr_view a, tstr_view b) {
	if(a.len != b.len) return false;
	return memcmp(a.data, b.data, a.len) == 0;
}

// Checks if view starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_starts_with(tstr_view v, const char* prefix) {
	size_t pre_len = strlen(prefix);
	if(pre_len > v.len) return false;
	return memcmp(v.data, prefix, pre_len) == 0;
}

// Checks if view ends with suffix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_ends_with(tstr_view v, const char* suffix) {
	size_t suf_len = strlen(suffix);
	if(suf_len > v.len) return false;
	return memcmp(v.data + v.len - suf_len, suffix, suf_len) == 0;
}

// Wrapper for checking if an owning tstr starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_starts_with(const tstr* s, const char* prefix) {
	return tstr_view_starts_with(tstr_as_view(s), prefix);
}

// Wrapper for checking if an owning tstr ends with suffix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_ends_with(const tstr* s, const char* suffix) {
	return tstr_view_ends_with(tstr_as_view(s), suffix);
}

// Trims whitespace from the start of the view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_lstrip(tstr_view v) {
	const char* start = v.data;
	const char* end = v.data + v.len;
	while(start < end && isspace((unsigned char)*start))
		start++;
	return (tstr_view){ .data = start, .len = (size_t)(end - start) };
}

// Trims whitespace from the end of the view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_rstrip(tstr_view v) {
	const char* start = v.data;
	const char* end = v.data + v.len;
	while(end > start && isspace((unsigned char)*(end - 1)))
		end--;
	return (tstr_view){ .data = start, .len = (size_t)(end - start) };
}

// Trims whitespace from both ends.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_trim(tstr_view v) {
	return tstr_view_lstrip(tstr_view_rstrip(v));
}

// Converts a view to an integer (simple atoi replacement).
// Returns true if successful, false if empty or invalid chars found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_to_int(tstr_view v, int* out) {
	if(v.len == 0) return false;

	int sign = 1;
	size_t i = 0;

	if(v.data[0] == '-') {
		sign = -1;
		i++;
	} else if(v.data[0] == '+') {
		i++;
	}

	if(i == v.len) return false;

	int result = 0;
	for(; i < v.len; i++) {
		if(v.data[i] < '0' || v.data[i] > '9') return false;
		result = result * 10 + (v.data[i] - '0');
	}

	*out = result * sign;
	return true;
}

// Initializes an iterator for splitting a string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_split_iter tstr_split_init(tstr_view src, const char* delim) {
	return (tstr_split_iter){
		.source = src, .delim = tstr_view_from(delim), .current_pos = 0, .finished = false
	};
}

// Gets the next part in a split iteration. Returns false when done.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_split_next(tstr_split_iter* it, tstr_view* out_part) {
	if(it->finished) return false;

	const char* start = it->source.data + it->current_pos;
	size_t remaining = it->source.len - it->current_pos;

	size_t found_at = remaining;

	for(size_t i = 0; i <= remaining - it->delim.len; i++) {
		if(memcmp(start + i, it->delim.data, it->delim.len) == 0) {
			found_at = i;
			break;
		}
	}

	if(found_at == remaining) {
		*out_part = (tstr_view){ .data = start, .len = remaining };
		it->finished = true;
	} else {
		*out_part = (tstr_view){ .data = start, .len = found_at };
		it->current_pos += found_at + it->delim.len;
	}

	return true;
}

	#if defined(T_HAS_CLEANUP) && T_HAS_CLEANUP
		#define tstr_autofree T_CLEANUP(tstr_free) tstr
	#endif

	#ifdef __cplusplus
} // extern "C"
	#endif

/* C++ Integration Layer -> namespace: tstr */

	#ifdef __cplusplus

		#include <cstring>
		#include <iostream>
		#include <iterator>
		#include <string>
		#include <utility>

		#if __cplusplus >= 201703L
			#include <string_view>
		#endif

namespace z_str {
class string;

class view {
	::tstr_view inner;

  public:
	// Iterator traits for view.
	using value_type = char;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using const_pointer = const char*;
	using const_iterator = const char*;

	view() : inner{ NULL, 0 } {}
	view(const char* s) : inner(::tstr_view_from(s)) {}
	view(const char* s, size_t len) : inner{ s, len } {}

	// Defined later to allow cyclic dependency.
	view(const string& s);
	view(const string&&) = delete;

	const char* data() const { return inner.data; }
	size_t size() const { return inner.len; }
	size_t length() const { return inner.len; }
	bool empty() const { return inner.len == 0; }

	const char* begin() const { return inner.data; }
	const char* end() const { return inner.data + inner.len; }

	char operator[](size_t idx) const { return inner.data[idx]; }

		#if __cplusplus >= 201703L
	operator std::string_view() const { return std::string_view(data(), size()); }
		#endif

	bool starts_with(const char* prefix) const { return ::tstr_view_starts_with(inner, prefix); }
	bool ends_with(const char* suffix) const { return ::tstr_view_ends_with(inner, suffix); }
	bool equals(const char* str) const { return ::tstr_view_eq(inner, str); }

	view sub(size_t start, size_t len) const {
		::tstr_view v = ::tstr_sub(inner, start, len);
		return view(v.data, v.len);
	}

	view lstrip() const {
		::tstr_view v = ::tstr_view_lstrip(inner);
		return view(v.data, v.len);
	}

	view rstrip() const {
		::tstr_view v = ::tstr_view_rstrip(inner);
		return view(v.data, v.len);
	}

	view trim() const {
		::tstr_view v = ::tstr_view_trim(inner);
		return view(v.data, v.len);
	}

	// Returns true if parsing was successful.
	bool to_int(int* out) const { return ::tstr_view_to_int(inner, out); }

	// Comparisons.
	bool operator==(const char* other) const { return ::tstr_view_eq(inner, other); }
	bool operator==(const view& other) const { return ::tstr_view_eq_view(inner, other.inner); }
	bool operator!=(const char* other) const { return !(*this == other); }
	bool operator!=(const view& other) const { return !(*this == other); }
};

class split_iterable {
	::tstr_view source;
	const char* delim;

  public:
	split_iterable(::tstr_view s, const char* d) : source(s), delim(d) {}

	struct iterator {
		using iterator_category = std::input_iterator_tag;
		using value_type = view;
		using difference_type = std::ptrdiff_t;
		using pointer = const view*;
		using reference = const view&;

		::tstr_split_iter state;
		::tstr_view current_part;
		bool done;

		iterator(::tstr_view s, const char* d, bool end) : done(end) {
			if(!end) {
				state = ::tstr_split_init(s, d);
				next();
			}
		}

		void next() {
			if(!::tstr_split_next(&state, &current_part)) {
				done = true;
			}
		}

		view operator*() const { return view(current_part.data, current_part.len); }
		iterator& operator++() {
			next();
			return *this;
		}
		bool operator!=(const iterator& other) const { return done != other.done; }
		bool operator==(const iterator& other) const { return done == other.done; }
	};

	iterator begin() { return iterator(source, delim, false); }
	iterator end() { return iterator(source, delim, true); }
};

class string {
	::tstr inner;
	friend class view;

	friend bool operator==(const string& lhs, const string& rhs);
	friend bool operator!=(const string& lhs, const string& rhs);
	friend bool operator<(const string& lhs, const string& rhs);

  public:
	// Iterator traits.
	using value_type = char;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using pointer = char*;
	using const_pointer = const char*;
	using iterator = char*;
	using const_iterator = const char*;

	// Default constructor.
	string() : inner(::tstr_init()) {}

	// C-string constructor.
	string(const char* s) : inner(::tstr_from(s)) {}

	// Length constructor.
	string(const char* s, size_t len) : inner(::tstr_from_len(s, len)) {}

		// This one is for C++17 so we put it like this.
		#if __cplusplus >= 201703L
	string(std::string_view sv) : inner(::tstr_from_len(sv.data(), sv.size())) {}
		#endif

	// Copy constructor.
	string(const string& other) : inner(::tstr_dup(&other.inner)) {}

	// Move constructor (zero cost).
	string(string&& other) noexcept : inner(other.inner) { other.inner = ::tstr_init(); }

	// Destructor.
	~string() { ::tstr_free(&inner); }

	// Copy assignment.
	string& operator=(const string& other) {
		if(this != &other) {
			::tstr_free(&inner);
			inner = ::tstr_dup(&other.inner);
		}
		return *this;
	}

	// Move assignment (transfer ownership).
	string& operator=(string&& other) noexcept {
		if(this != &other) {
			::tstr_free(&inner);
			inner = other.inner;
			other.inner = ::tstr_init();
		}
		return *this;
	}

	// Assignment from C-string.
	string& operator=(const char* s) {
		::tstr_free(&inner);
		inner = ::tstr_from(s);
		return *this;
	}

	// Accessors.
	const char* c_str() const { return ::tstr_cstr(&inner); }
	const char* data() const { return ::tstr_cstr(&inner); }
	char* data() { return ::tstr_data(&inner); }
	size_t size() const { return ::tstr_len(&inner); }
	size_t length() const { return ::tstr_len(&inner); }
	size_t capacity() const { return inner.is_long ? inner.l.cap : TSTR_SSO_CAP; }
	bool is_empty() const { return ::tstr_is_empty(&inner); }

		#if __cplusplus >= 201703L
	operator std::string_view() const { return std::string_view(data(), size()); }
		#endif

	// Iterators.
	char* begin() { return ::tstr_data(&inner); }
	char* end() { return ::tstr_data(&inner) + size(); }
	const char* begin() const { return ::tstr_cstr(&inner); }
	const char* end() const { return ::tstr_cstr(&inner) + size(); }

	char& operator[](size_t idx) { return ::tstr_data(&inner)[idx]; }
	const char& operator[](size_t idx) const { return ::tstr_cstr(&inner)[idx]; }

	char& front() { return operator[](0); }
	const char& front() const { return operator[](0); }

	char& back() { return operator[](size() - 1); }
	const char& back() const { return operator[](size() - 1); }

	// Modifiers.
	void clear() { ::tstr_clear(&inner); }
	void reserve(size_t cap) { ::tstr_reserve(&inner, cap); }
	void shrink_to_fit() { ::tstr_shrink_to_fit(&inner); }

	void push_back(char c) { ::tstr_push_char(&inner, c); }
	void pop_back() { ::tstr_pop_char(&inner); }

	string& append(const char* s) {
		::tstr_cat(&inner, s);
		return *this;
	}
	string& append(const char* s, size_t len) {
		::tstr_cat_len(&inner, s, len);
		return *this;
	}

	// Operators.
	string& operator+=(const char* s) { return append(s); }
	string& operator+=(char c) {
		push_back(c);
		return *this;
	}
	string& operator+=(const string& other) { return append(other.c_str(), other.size()); }

	// Search
	std::ptrdiff_t find(const char* needle) const { return ::tstr_find(&inner, needle); }
	bool contains(const char* needle) const { return ::tstr_contains(&inner, needle); }
	bool starts_with(const char* prefix) const { return ::tstr_starts_with(&inner, prefix); }
	bool ends_with(const char* suffix) const { return ::tstr_ends_with(&inner, suffix); }

	// Some utilities.
	void to_lower() { ::tstr_to_lower(&inner); }
	void to_upper() { ::tstr_to_upper(&inner); }
	void trim() { ::tstr_trim(&inner); }

	void replace(const char* target, const char* replacement) {
		::tstr_replace(&inner, target, replacement);
	}

	// Ownership.
	// WARNING: Returns a raw malloc'd pointer. You MUST free() this yourself.
	// The string object becomes empty after this call.
	char* release() { return ::tstr_take(&inner); }

	static string own(char* ptr, size_t len, size_t cap) {
		string s;
		s.inner = ::tstr_own(ptr, len, cap);
		return s;
	}

	// UTF-8.
	size_t rune_count() const { return ::tstr_count_runes(&inner); }
	bool is_valid_utf8() const { return ::tstr_is_valid_utf8(&inner); }

	// Splitting.
	// Usage: for(auto part : str.split(",")) { ... }
	split_iterable split(const char* delim) const& {
		return split_iterable(::tstr_as_view(&inner), delim);
	}

	split_iterable split(const char* delim) const&& = delete;

	// Static Factories.
	static string from_file(const char* path) {
		string s;
		s.inner = ::tstr_read_file(path);
		return s;
	}

	// WARNING: Only POD types (int, double, char*) are safe here.
	// Passing std::string or objects will crash.
	template <typename... Args> static string fmt(const char* format, Args... args) {
		string s;
		::tstr_fmt(&s.inner, format, args...);
		return s;
	}
};

// View constructor implementation.
inline view::view(const string& s) : inner(::tstr_as_view(&s.inner)) {}

// The global operators...
inline std::ostream& operator<<(std::ostream& os, const string& s) {
	return os.write(s.data(), s.size());
}

inline std::ostream& operator<<(std::ostream& os, const view& s) {
	return os.write(s.data(), s.size());
}

// Comparison Operators (string vs string).
inline bool operator==(const string& lhs, const string& rhs) {
	return ::tstr_eq(&lhs.inner, &rhs.inner);
}
inline bool operator!=(const string& lhs, const string& rhs) {
	return !::tstr_eq(&lhs.inner, &rhs.inner);
}
inline bool operator<(const string& lhs, const string& rhs) {
	return ::tstr_cmp(&lhs.inner, &rhs.inner) < 0;
}

// Comparison Operators (string vs const char*).
inline bool operator==(const string& lhs, const char* rhs) {
	return strcmp(lhs.c_str(), rhs) == 0;
}
inline bool operator==(const char* lhs, const string& rhs) {
	return strcmp(lhs, rhs.c_str()) == 0;
}
inline bool operator!=(const string& lhs, const char* rhs) {
	return strcmp(lhs.c_str(), rhs) != 0;
}
inline bool operator!=(const char* lhs, const string& rhs) {
	return strcmp(lhs, rhs.c_str()) != 0;
}
} // namespace z_str

	#endif // __cplusplus
