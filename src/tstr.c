#include "./tstr.h"

// Returns true if the string is heap-allocated.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_long(const tstr* s) {
	return s->type.inner == tstr_type_enum_long;
}

// Returns true if the string is SSO, allocated on the stack
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_sso(const tstr* s) {
	return s->type.inner == tstr_type_enum_sso;
}

// Returns true if the string is a static string, alias not modifiable
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_static(const tstr* s) {
	return s->type.inner == tstr_type_enum_static;
}

// Returns a pointer to the mutable data buffer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_data(tstr* s) {
	switch(s->type.inner) {
		case tstr_type_enum_sso: return s->short_str.buf;
		case tstr_type_enum_long: return s->long_str.ptr;
		case tstr_type_enum_static: return NULL;
		default: {
			return NULL;
		}
	}
}

// Returns a pointer to the const data buffer (C-string compatible).
TSTR_FUN_ATTRIBUTES [[nodiscard]] const char* tstr_cstr(const tstr* s) {
	switch(s->type.inner) {
		case tstr_type_enum_sso: return s->short_str.buf;
		case tstr_type_enum_long: return s->long_str.ptr;
		case tstr_type_enum_static: return s->static_str.ptr;
		default: {
			return NULL;
		}
	}
}

// Returns the current length of the string (excluding null terminator).
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_len(const tstr* s) {
	switch(s->type.inner) {
		case tstr_type_enum_sso: return s->short_str.len;
		case tstr_type_enum_long: return s->long_str.len;
		case tstr_type_enum_static: return s->static_str.len;
		default: {
			return 0;
		}
	}
}

// Returns true if the string length is 0.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_empty(const tstr* s) {
	return tstr_len(s) == 0;
}

// Returns true if the underlying ptr is NULL, or the SSO string is empty
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_null(const tstr* s) {
	switch(s->type.inner) {
		case tstr_type_enum_sso: return s->short_str.len == 0;
		case tstr_type_enum_long: return s->long_str.ptr == NULL;
		case tstr_type_enum_static: return s->static_str.ptr == NULL;
		default: {
			return true;
		}
	}
}

/* Creation and Destruction */

// Initializes an empty string {0}.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_init(void) {
	tstr s;
	memset(&s, 0, sizeof(tstr));
	return s;
}

// Frees the string if it is on the heap, and resets it to empty.
TSTR_FUN_ATTRIBUTES void tstr_free(tstr* const s) {
	if(s->type.inner == tstr_type_enum_long) T_STR_FREE(s->long_str.ptr);
	*s = tstr_init();
}

// Clears the content (sets length to 0) but keeps the allocated capacity. static strings get nuked
// in favor of a SSO string
TSTR_FUN_ATTRIBUTES void tstr_clear(tstr* const s) {
	switch(s->type.inner) {
		case tstr_type_enum_sso: {
			s->short_str.len = 0;
			s->short_str.buf[0] = '\0';
			break;
		}
		case tstr_type_enum_long: {
			s->long_str.len = 0;
			s->long_str.ptr[0] = '\0';
			break;
		}
		case tstr_type_enum_static: {
			s->type.inner = tstr_type_enum_sso;
			s->short_str.len = 0;
			s->short_str.buf[0] = '\0';
			break;
		}
		default: {
		}
	}
}

static inline void tstr_attempt_static_string_modification(tstr* const s) {
	if(s->type.inner != tstr_type_enum_static) {
		return;
	}

#if TSTR_STATIC_STRING_MODIFICATION_BEHAVIOR == 0
	*s = tstr_from_len(s->static_str.ptr, s->static_str.len);
#elif TSTR_STATIC_STRING_MODIFICATION_BEHAVIOR == 1
	abort();
#else
	#error "'TSTR_STATIC_STRING_MODIFICATION_BEHAVIOR' not defined"
#endif
}

/* Memory Management */

// Ensures the string has at least `new_cap` capacity.
// Handles the transition from SSO (Stack) to Long (Heap).
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_reserve(tstr* const s, size_t new_cap) {
	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
	}

	if(new_cap < TSTR_SSO_CAP) {
		return TStrResultOk;
	}

	if(s->type.inner == tstr_type_enum_long && new_cap <= s->long_str.cap) {
		return TStrResultOk;
	}

	char* new_ptr;
	if(s->type.inner == tstr_type_enum_long) {
		new_ptr = T_STR_REALLOC(s->long_str.ptr, new_cap + 1);
	} else {
		new_ptr = T_STR_MALLOC(new_cap + 1);
		if(new_ptr) {
			memcpy(new_ptr, s->short_str.buf, s->short_str.len);
			new_ptr[s->short_str.len] = '\0';
		}
	}

	if(!new_ptr) {
		return TStrResultErr;
	}

	// Transition state if we were short before.
	if(s->type.inner != tstr_type_enum_long) {
		s->long_str.len = s->short_str.len;
		s->type.inner = tstr_type_enum_long;
	}

	s->long_str.ptr = new_ptr;
	s->long_str.cap = new_cap;

	return TStrResultOk;
}

// Creates a new empty string with pre-allocated capacity on the heap.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_with_capacity(size_t cap) {
	tstr s = tstr_init();
	if(cap > TSTR_SSO_CAP) {
		auto _ = tstr_reserve(&s, cap);
		(void)_;
	}
	return s;
}

// Reduces heap usage to fit the exact string length (or moves back to SSO if small enough).
TSTR_FUN_ATTRIBUTES void tstr_shrink_to_fit(tstr* const s) {
	if(s->type.inner != tstr_type_enum_long) {
		return;
	}

	// Downgrade to SSO if possible.
	if(s->long_str.len <= TSTR_SSO_CAP) {
		char temp[TSTR_SSO_CAP];
		memcpy(temp, s->long_str.ptr, s->long_str.len);
		temp[s->long_str.len] = '\0';

		uint8_t old_len = (uint8_t)s->long_str.len;
		T_STR_FREE(s->long_str.ptr);

		s->type.inner = tstr_type_enum_sso;
		memcpy(s->short_str.buf, temp, old_len + 1);
		s->short_str.len = old_len;
		return;
	}

	if(s->long_str.len < s->long_str.cap) {
		char* new_ptr = T_STR_REALLOC(s->long_str.ptr, s->long_str.len + 1);
		if(new_ptr) {
			s->long_str.ptr = new_ptr;
			s->long_str.cap = s->long_str.len;
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
		memcpy(s.long_str.ptr, ptr, len);
		s.long_str.ptr[len] = '\0';
		s.long_str.len = len;
	} else {
		memcpy(s.short_str.buf, ptr, len);
		s.short_str.buf[len] = '\0';
		s.short_str.len = (uint8_t)len;
		s.type.inner = tstr_type_enum_sso;
	}
	return s;
}

// Creates a tstr from a standard C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from(const char* cstr) {
	return tstr_from_len(cstr, strlen(cstr));
}

// Initializes a static string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_cstr(const char* str) {
	return tstr_from_static_cstr_with_len(str, strlen(str));
}

// Initializes a static string with length
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_cstr_with_len(const char* str, size_t len) {
	tstr s = tstr_init();
	s.type.inner = tstr_type_enum_static;
	s.static_str = (tstr_static){ .ptr = str, .len = len };

	return s;
}

// Creates a deep copy of a tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_dup(const tstr* s) {
	return tstr_from_len(tstr_cstr(s), tstr_len(s));
}

// Takes ownership of a malloc'd pointer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_own(char* ptr, size_t len, size_t cap) {
	tstr s = tstr_init();

	if(cap <= TSTR_SSO_CAP) {
		memcpy(s.short_str.buf, ptr, len);
		s.short_str.buf[len] = '\0';
		s.short_str.len = (uint8_t)len;
		s.type.inner = tstr_type_enum_sso;
		T_STR_FREE(ptr);
	} else {
		s.type.inner = tstr_type_enum_long;
		s.long_str.ptr = ptr;
		s.long_str.len = len;
		s.long_str.cap = cap;
	}
	return s;
}

TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_own_cstr(char* ptr) {
	const size_t size = strlen(ptr);

	const tstr result = tstr_own(ptr, size, size);

	return result;
}

// Releases ownership. Returns a malloc'd pointer the user MUST free.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_take(tstr* s) {
	char* ptr;

	switch(s->type.inner) {
		case tstr_type_enum_sso: {
			ptr = T_STR_MALLOC(s->short_str.len + 1);
			if(ptr) {
				memcpy(ptr, s->short_str.buf, s->short_str.len);
				ptr[s->short_str.len] = '\0';
			}
			break;
		}
		case tstr_type_enum_long: {
			ptr = s->long_str.ptr;
			break;
		}
		case tstr_type_enum_static: {
			// can't take ownership of the static string
			ptr = NULL;
		}
		default: {
			ptr = NULL;
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

	if(s.type.inner == tstr_type_enum_long) {
		s.long_str.len = read_count;
	} else {
		s.short_str.len = (uint8_t)read_count;
	}

	fclose(f);
	return s;
}

[[nodiscard]] static inline size_t tstr_get_cap(const tstr* const s) {
	switch(s->type.inner) {
		case tstr_type_enum_sso: return TSTR_SSO_CAP;
		case tstr_type_enum_long: return s->long_str.cap;
		case tstr_type_enum_static: return s->static_str.len;
		default: {
			return 0;
		}
	}
}

// Appends a single character to the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_push_char(tstr* const s, char c) {
	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
	}

	const size_t len = tstr_len(s);
	const size_t cap = tstr_get_cap(s);
	if(len + 1 >= cap) {
		const size_t new_cap = T_GROWTH_FACTOR(cap);

		if(tstr_reserve(s, new_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	char* p = tstr_data(s);
	p[len] = c;
	p[len + 1] = '\0';

	if(s->type.inner == tstr_type_enum_long) {
		s->long_str.len++;
	} else {
		s->short_str.len++;
	}

	return TStrResultOk;
}

// Removes and returns the last character of the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char tstr_pop_char(tstr* s) {
	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
	}

	size_t len = tstr_len(s);

	if(len == 0) {
		return '\0';
	}

	char* p = tstr_data(s);
	char c = p[len - 1];
	p[len - 1] = '\0';

	if(s->type.inner == tstr_type_enum_long) {
		s->long_str.len--;
	} else {
		s->short_str.len--;
	}

	return c;
}

// Appends a raw char buffer of known length.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat_len(tstr* const s, const char* src,
                                                          size_t src_len) {
	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
	}

	const size_t cur_len = tstr_len(s);
	const size_t req_cap = cur_len + src_len;

	const size_t cap = tstr_get_cap(s);

	if(req_cap >= cap) {
		size_t new_cap = cap;
		// Logic fixed: starting cap is 23. If we grow, we just multiply.
		// We do not fallback to 32 because 23 > 0.
		if(new_cap == 0) {
			new_cap = TSTR_SSO_CAP;
		}

		while(new_cap <= req_cap) {
			new_cap = T_GROWTH_FACTOR(new_cap);
		}

		if(tstr_reserve(s, new_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	char* dest = tstr_data(s);
	memcpy(dest + cur_len, src, src_len);
	dest[cur_len + src_len] = '\0';

	if(s->type.inner == tstr_type_enum_long) {
		s->long_str.len += src_len;
	} else {
		s->short_str.len += (uint8_t)src_len;
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
		auto _ = tstr_cat(&s, strings[i]);
		(void)_;
		if(i < count - 1) {

			auto _ = tstr_cat(&s, delim);
			(void)_;
		}
	}
	return s;
}

// Formats a string (printf style) and appends it.
TSTR_PRINTF_ATTR(2, 3)
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_fmt(tstr* const s, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if(len < 0) {
		return TStrResultErr;
	}

	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
	}

	const size_t cur_len = tstr_len(s);
	const size_t req_cap = cur_len + len;

	const size_t cap = tstr_get_cap(s);

	if(req_cap >= cap) {
		if(tstr_reserve(s, req_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	va_start(args, fmt);
	char* buf = tstr_data(s);
	vsnprintf(buf + cur_len, len + 1, fmt, args);
	va_end(args);

	if(s->type.inner == tstr_type_enum_long) {
		s->long_str.len += len;
	} else {
		s->short_str.len += (uint8_t)len;
	}

	return TStrResultOk;
}

/* In-Place Transformations */

// Converts the string to lowercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_lower(tstr* const s) {
	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
	}

	char* p = tstr_data(s);
	size_t len = tstr_len(s);
	for(size_t i = 0; i < len; i++) {
		p[i] = (char)tolower((unsigned char)p[i]);
	}
}

// Converts the string to uppercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_upper(tstr* const s) {
	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
	}

	char* p = tstr_data(s);
	size_t len = tstr_len(s);
	for(size_t i = 0; i < len; i++) {
		p[i] = (char)toupper((unsigned char)p[i]);
	}
}

// Removes leading and trailing whitespace in-place.
TSTR_FUN_ATTRIBUTES void tstr_trim(tstr* const s) {
	if(tstr_len(s) == 0) {
		return;
	}

	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
	}

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

	if(s->type.inner == tstr_type_enum_long) {
		s->long_str.len = final_len;
	} else {
		s->short_str.len = (uint8_t)final_len;
	}
}

// Replaces all occurrences of "target" with "replacement".
// This may reallocate the string if the size grows.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_replace(tstr* s, const char* target,
                                                          const char* replacement) {
	if(!target || !*target) {
		return TStrResultErr;
	}

	if(s->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(s);
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

	if(res.type.inner == tstr_type_enum_long) {
		res.long_str.len = new_len;
	} else {
		res.short_str.len = (uint8_t)new_len;
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

// Checks equality between two tstr objects (faster than strcmp).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_cstr(const tstr* const a, const char* const b) {
	size_t la = tstr_len(a);
	size_t lb = strlen(b);
	if(la != lb) {
		return false;
	}
	return memcmp(tstr_cstr(a), b, la) == 0;
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

// Checks equality ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_ignore_case_cstr(const tstr* const a,
                                                                const char* const b) {
	size_t len = tstr_len(a);
	if(len != strlen(b)) {
		return false;
	}

	const char* p1 = tstr_cstr(a);

	for(size_t i = 0; i < len; i++) {
		if(tolower((unsigned char)p1[i]) != tolower((unsigned char)b[i])) {
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
TSTR_FUN_ATTRIBUTES [[nodiscard]] uint32_t tstr_next_rune(const char** p) {
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
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_count_runes(const tstr* s) {
	const char* ptr = tstr_cstr(s);
	size_t count = 0;
	while(*ptr) {
		auto _ = tstr_next_rune(&ptr);
		(void)_;
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
	if(start >= v.len) {
		return (tstr_view){ .data = NULL, .len = 0 };
	}
	if(start + len > v.len) {
		len = v.len - start;
	}

	return (tstr_view){ .data = v.data + start, .len = len };
}

// Returns a substring view, from start until end
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_sub_until_end(tstr_view v, size_t start) {
	if(start >= v.len) {
		return (tstr_view){ .data = NULL, .len = 0 };
	}

	const size_t len = v.len - start;

	return (tstr_view){ .data = v.data + start, .len = len };
}

// Returns the view, that starts after the first occurrence of needle, or NULL for data if not
// found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_find(tstr_view v, const char* needle) {

	const size_t needle_len = strlen(needle);

	for(size_t i = 0; i <= v.len - needle_len; i++) {
		if(memcmp(v.data + i, needle, needle_len) == 0) {
			return (tstr_view){ .data = v.data + i + needle_len, .len = v.len - i - needle_len };
		}
	}

	return (tstr_view){ .data = NULL, .len = 0 };
}

// Checks if view equals a C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq(tstr_view v, const char* cstr) {
	if(strlen(cstr) != v.len) {
		return false;
	}
	return memcmp(v.data, cstr, v.len) == 0;
}

// Checks if view equals a C-string, ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_ignore_case(tstr_view v, const char* cstr) {
	if(strlen(cstr) != v.len) {
		return false;
	}

	for(size_t i = 0; i < v.len; i++) {
		if(tolower((unsigned char)v.data[i]) != tolower((unsigned char)cstr[i])) {
			return false;
		}
	}
	return true;
}

// Checks if two views are equal.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_view(tstr_view a, tstr_view b) {
	if(a.len != b.len) {
		return false;
	}
	return memcmp(a.data, b.data, a.len) == 0;
}

// Checks if view starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_starts_with(tstr_view v, const char* prefix) {
	size_t pre_len = strlen(prefix);
	if(pre_len > v.len) {
		return false;
	}
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
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_split_iter tstr_split_init(tstr_view src,
                                                                  const char* delim) {
	return (tstr_split_iter){
		.source = src, .delim = tstr_view_from(delim), .current_pos = 0, .finished = false
	};
}

// Gets the next part in a split iteration. Returns false when done.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_split_next(tstr_split_iter* it, tstr_view* out_part) {
	if(it->finished) {
		return false;
	}

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

// Splits a tstr_view into two parts, return false if it couldn#t be split, the second part can also
// be of length 0, if the delimiter is at the end of the src
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_split_result tstr_split(tstr_view src, const char* delim) {

	// nearly the same as tstr_view_find, but not doing the strlen twice

	const size_t delim_len = strlen(delim);

	for(size_t i = 0; i <= src.len - delim_len; i++) {
		if(memcmp(src.data + i, delim, delim_len) == 0) {
			const tstr_view first = { .data = src.data, .len = i };
			const tstr_view second = { .data = src.data + i + delim_len,
				                       .len = src.len - i - delim_len };

			return (tstr_split_result){ .ok = true, .first = first, .second = second };
		}
	}

	return (tstr_split_result){ .ok = false, .first = TSTR_EMPTY_VIEW, .second = TSTR_EMPTY_VIEW };
}
