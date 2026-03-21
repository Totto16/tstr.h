#include "./tstr.h"

// Returns true if the string is heap-allocated.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_long(const tstr* str) {
	return str->type.inner == tstr_type_enum_long;
}

// Returns true if the string is SSO, allocated on the stack
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_sso(const tstr* str) {
	return str->type.inner == tstr_type_enum_sso;
}

// Returns true if the string is a static string, alias not modifiable
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_static(const tstr* str) {
	return str->type.inner == tstr_type_enum_static;
}

// Returns a pointer to the mutable data buffer.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_data(tstr* str) {
	switch(str->type.inner) {
		case tstr_type_enum_sso: return str->short_str.buf;
		case tstr_type_enum_long: return str->long_str.ptr;
		case tstr_type_enum_static: return NULL;
		default: {
			return NULL;
		}
	}
}

// Returns a pointer to the const data buffer (C-string compatible).
TSTR_FUN_ATTRIBUTES [[nodiscard]] const char* tstr_cstr(const tstr* str) {
	switch(str->type.inner) {
		case tstr_type_enum_sso: return str->short_str.buf;
		case tstr_type_enum_long: return str->long_str.ptr;
		case tstr_type_enum_static: return str->static_str.ptr;
		default: {
			return NULL;
		}
	}
}

// Returns the current length of the string (excluding null terminator).
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_len(const tstr* str) {
	switch(str->type.inner) {
		case tstr_type_enum_sso: return str->short_str.len;
		case tstr_type_enum_long: return str->long_str.len;
		case tstr_type_enum_static: return str->static_str.len;
		default: {
			return 0;
		}
	}
}

// Returns true if the string length is 0.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_empty(const tstr* str) {
	return tstr_len(str) == 0;
}

// Returns true if the underlying ptr is NULL, it is always false for SSO strings
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_null(const tstr* str) {
	switch(str->type.inner) {
		case tstr_type_enum_sso: return false;
		case tstr_type_enum_long: return str->long_str.ptr == NULL;
		case tstr_type_enum_static: return str->static_str.ptr == NULL;
		default: {
			return true;
		}
	}
}

// Returns true if the underlying ptr is NULL
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_static_is_null(tstr_static str) {
	return str.ptr == NULL;
}

/* Creation and Destruction */

// Initializes an empty string {0}.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_init(void) {
	tstr str;
	memset(&str, 0, sizeof(tstr));
	return str;
}

// Initializes an string with ptr set to NULL
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_null(void) {
	tstr str = tstr_init();

	str.type.inner = tstr_type_enum_long;
	str.long_str = (tstr_long){ .ptr = NULL, .len = 0, .cap = 0 };

	return str;
}

// Initializes a static string with ptr set to NULL
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_static tstr_static_null(void) {
	return (tstr_static){ .ptr = NULL, .len = 0 };
}

// Frees the string if it is on the heap, and resets it to empty.
TSTR_FUN_ATTRIBUTES void tstr_free(tstr* const str) {
	if(str->type.inner == tstr_type_enum_long) T_STR_FREE(str->long_str.ptr);
	*str = tstr_null();
}

// Clears the content (sets length to 0) but keeps the allocated capacity. static strings get nuked
// in favor of a SSO string
TSTR_FUN_ATTRIBUTES void tstr_clear(tstr* const str) {
	switch(str->type.inner) {
		case tstr_type_enum_sso: {
			str->short_str.len = 0;
			str->short_str.buf[0] = '\0';
			break;
		}
		case tstr_type_enum_long: {
			str->long_str.len = 0;
			str->long_str.ptr[0] = '\0';
			break;
		}
		case tstr_type_enum_static: {
			str->type.inner = tstr_type_enum_sso;
			str->short_str.len = 0;
			str->short_str.buf[0] = '\0';
			break;
		}
		default: {
		}
	}
}

static inline void tstr_attempt_static_string_modification(tstr* const str) {
	if(str->type.inner != tstr_type_enum_static) {
		return;
	}

#if TSTR_STATIC_STRING_MODIFICATION_BEHAVIOR == 0
	*str = tstr_from_len(str->static_str.ptr, str->static_str.len);
#elif TSTR_STATIC_STRING_MODIFICATION_BEHAVIOR == 1
	abort();
#else
	#error "'TSTR_STATIC_STRING_MODIFICATION_BEHAVIOR' not defined"
#endif
}

/* Memory Management */

// Ensures the string has at least `new_cap` capacity.
// Handles the transition from SSO (Stack) to Long (Heap).
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_reserve(tstr* const str, size_t new_cap) {
	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	if(new_cap < TSTR_SSO_CAP) {
		return TStrResultOk;
	}

	if(str->type.inner == tstr_type_enum_long && new_cap <= str->long_str.cap) {
		return TStrResultOk;
	}

	char* new_ptr;
	if(str->type.inner == tstr_type_enum_long) {
		new_ptr = T_STR_REALLOC(str->long_str.ptr, new_cap + 1);
	} else {
		new_ptr = T_STR_MALLOC(new_cap + 1);
		if(new_ptr) {
			memcpy(new_ptr, str->short_str.buf, str->short_str.len);
			new_ptr[str->short_str.len] = '\0';
		}
	}

	if(!new_ptr) {
		return TStrResultErr;
	}

	// Transition state if we were short before.
	if(str->type.inner != tstr_type_enum_long) {
		str->long_str.len = str->short_str.len;
		str->type.inner = tstr_type_enum_long;
	}

	str->long_str.ptr = new_ptr;
	str->long_str.cap = new_cap;

	return TStrResultOk;
}

// Creates a new empty string with pre-allocated capacity on the heap.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_with_capacity(size_t cap) {
	tstr str = tstr_init();
	if(cap > TSTR_SSO_CAP) {
		const TStrResult result = tstr_reserve(&str, cap);
		if(result != TStrResultOk) {
			return tstr_null();
		}
	}
	return str;
}

// Reduces heap usage to fit the exact string length (or moves back to SSO if small enough).
TSTR_FUN_ATTRIBUTES void tstr_shrink_to_fit(tstr* const str) {
	if(str->type.inner != tstr_type_enum_long) {
		return;
	}

	// Downgrade to SSO if possible.
	if(str->long_str.len <= TSTR_SSO_CAP) {
		char temp[TSTR_SSO_CAP];
		memcpy(temp, str->long_str.ptr, str->long_str.len);
		temp[str->long_str.len] = '\0';

		uint8_t old_len = (uint8_t)str->long_str.len;
		T_STR_FREE(str->long_str.ptr);

		str->type.inner = tstr_type_enum_sso;
		memcpy(str->short_str.buf, temp, old_len + 1);
		str->short_str.len = old_len;
		return;
	}

	if(str->long_str.len < str->long_str.cap) {
		char* new_ptr = T_STR_REALLOC(str->long_str.ptr, str->long_str.len + 1);
		if(new_ptr) {
			str->long_str.ptr = new_ptr;
			str->long_str.cap = str->long_str.len;
		}
	}
}

/* Construction Helpers */

// Helper: Creates tstr from ptr + explicit len.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_len(const char* ptr, size_t len) {
	tstr s = tstr_init();
	if(len >= TSTR_SSO_CAP) {
		if(tstr_reserve(&s, len) != TStrResultOk) {
			return tstr_null();
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

// Initializes a static string as tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_cstr(const char* str) {
	return tstr_from_static_cstr_with_len(str, strlen(str));
}

// Initializes a static string with length as tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_cstr_with_len(const char* cstr,
                                                                      size_t len) {
	tstr str = tstr_init();
	str.type.inner = tstr_type_enum_static;
	str.static_str = (tstr_static){ .ptr = cstr, .len = len };

	return str;
}

// Initializes a static string as tstr_static.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_static tstr_static_from_static_cstr(const char* str) {
	return tstr_static_from_static_cstr_with_len(str, strlen(str));
}

// Initializes a static string with length as tstr_static.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_static
tstr_static_from_static_cstr_with_len(const char* cstr, size_t len) {
	return (tstr_static){ .ptr = cstr, .len = len };
}

// Initializes a tstr form a static string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_from_static_tstr(const tstr_static static_str) {
	tstr str = tstr_init();
	str.type.inner = tstr_type_enum_static;
	str.static_str = static_str;

	return str;
}

// Creates a deep copy of a tstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_dup(const tstr* str) {

	if(str->type.inner == tstr_type_enum_static) {
		// if the string is a static string, just copy the ptr, it can be safely reused, the static
		// string is never freed, and on modification we copy the contents
		return ((tstr){ .type = { .inner = tstr_type_enum_static },
		                .static_str = (tstr_static){ .ptr = str->static_str.ptr,
		                                             .len = str->static_str.len } });
	}

	// handle the NULL case
	if(str->type.inner == tstr_type_enum_long && str->static_str.ptr == NULL) {
		return tstr_null();
	}

	return tstr_from_len(tstr_cstr(str), tstr_len(str));
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
TSTR_FUN_ATTRIBUTES [[nodiscard]] char* tstr_take(tstr* str) {
	char* ptr;

	switch(str->type.inner) {
		case tstr_type_enum_sso: {
			ptr = T_STR_MALLOC(str->short_str.len + 1);
			if(ptr) {
				memcpy(ptr, str->short_str.buf, str->short_str.len);
				ptr[str->short_str.len] = '\0';
			}
			break;
		}
		case tstr_type_enum_long: {
			ptr = str->long_str.ptr;
			break;
		}
		case tstr_type_enum_static: {
			// can't take ownership of the static string
			ptr = NULL;
			break;
		}
		default: {
			ptr = NULL;
			break;
		}
	}

	// Reset the source struct so it doesn't double-free.
	*str = tstr_init();
	return ptr;
}

// Reads an entire file into a tstr. Returns empty on failure.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_read_file(const char* path) {
	tstr str = tstr_init();
	FILE* file = fopen(path, "rb");
	if(!file) {
		return str;
	}

	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);

	// Pedantic check.
	if(length <= 0 || (sizeof(long) > sizeof(size_t) && (size_t)length > (size_t)-1)) {
		fclose(file);
		return str;
	}

	if(tstr_reserve(&str, (size_t)length) != TStrResultOk) {
		fclose(file);
		return str;
	}

	char* buf = tstr_data(&str);
	size_t read_count = fread(buf, 1, (size_t)length, file);
	buf[read_count] = '\0';

	if(str.type.inner == tstr_type_enum_long) {
		str.long_str.len = read_count;
	} else {
		str.short_str.len = (uint8_t)read_count;
	}

	fclose(file);
	return str;
}

[[nodiscard]] static inline size_t tstr_get_cap(const tstr* const str) {
	switch(str->type.inner) {
		case tstr_type_enum_sso: return TSTR_SSO_CAP;
		case tstr_type_enum_long: return str->long_str.cap;
		case tstr_type_enum_static: return str->static_str.len;
		default: {
			return 0;
		}
	}
}

// Appends a single character to the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_push_char(tstr* const str, char chr) {
	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	const size_t len = tstr_len(str);
	const size_t cap = tstr_get_cap(str);
	if(len + 1 >= cap) {
		const size_t new_cap = T_GROWTH_FACTOR(cap);

		if(tstr_reserve(str, new_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	char* p = tstr_data(str);
	p[len] = chr;
	p[len + 1] = '\0';

	if(str->type.inner == tstr_type_enum_long) {
		str->long_str.len++;
	} else {
		str->short_str.len++;
	}

	return TStrResultOk;
}

// Removes and returns the last character of the string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] char tstr_pop_char(tstr* str) {
	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	size_t len = tstr_len(str);

	if(len == 0) {
		return '\0';
	}

	char* p = tstr_data(str);
	char c = p[len - 1];
	p[len - 1] = '\0';

	if(str->type.inner == tstr_type_enum_long) {
		str->long_str.len--;
	} else {
		str->short_str.len--;
	}

	return c;
}

// Appends a raw char buffer of known length.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat_len(tstr* const str, const char* src,
                                                          size_t src_len) {
	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	const size_t cur_len = tstr_len(str);
	const size_t req_cap = cur_len + src_len;

	const size_t cap = tstr_get_cap(str);

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

		if(tstr_reserve(str, new_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	char* dest = tstr_data(str);
	memcpy(dest + cur_len, src, src_len);
	dest[cur_len + src_len] = '\0';

	if(str->type.inner == tstr_type_enum_long) {
		str->long_str.len += src_len;
	} else {
		str->short_str.len += (uint8_t)src_len;
	}

	return TStrResultOk;
}

// Appends a null-terminated C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_cat(tstr* str, const char* cstr) {
	return tstr_cat_len(str, cstr, strlen(cstr));
}

// Joins an array of strings with a delimiter.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr tstr_join(const char** strings, size_t count,
                                                 const char* delim) {
	tstr str = tstr_init();
	if(count == 0) {
		return str;
	}

	size_t delim_len = strlen(delim);
	size_t total_len = 0;

	for(size_t i = 0; i < count; i++) {
		total_len += strlen(strings[i]);
		if(i < count - 1) total_len += delim_len;
	}

	if(tstr_reserve(&str, total_len) != TStrResultOk) {
		return tstr_null();
	}

	for(size_t i = 0; i < count; i++) {
		const TStrResult result = tstr_cat(&str, strings[i]);

		if(result != TStrResultOk) {
			tstr_free(&str);
			return tstr_null();
		}

		if(i < count - 1) {

			const TStrResult result2 = tstr_cat(&str, delim);

			if(result2 != TStrResultOk) {
				tstr_free(&str);
				return tstr_null();
			}
		}
	}
	return str;
}

// Formats a string (printf style) and appends it.
[[nodiscard]] TSTR_PRINTF_ATTR(2, 3) TSTR_FUN_ATTRIBUTES TStrResult
    tstr_fmt(tstr* const str, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if(len < 0) {
		return TStrResultErr;
	}

	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	const size_t cur_len = tstr_len(str);
	const size_t req_cap = cur_len + len;

	const size_t cap = tstr_get_cap(str);

	if(req_cap >= cap) {
		if(tstr_reserve(str, req_cap) != TStrResultOk) {
			return TStrResultErr;
		}
	}

	va_start(args, fmt);
	char* buf = tstr_data(str);
	vsnprintf(buf + cur_len, len + 1, fmt, args);
	va_end(args);

	if(str->type.inner == tstr_type_enum_long) {
		str->long_str.len += len;
	} else {
		str->short_str.len += (uint8_t)len;
	}

	return TStrResultOk;
}

/* In-Place Transformations */

// Converts the string to lowercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_lower(tstr* const str) {
	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	char* p = tstr_data(str);
	size_t len = tstr_len(str);
	for(size_t i = 0; i < len; i++) {
		p[i] = (char)tolower((unsigned char)p[i]);
	}
}

// Converts the string to uppercase in-place (ASCII only).
TSTR_FUN_ATTRIBUTES void tstr_to_upper(tstr* const str) {
	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	char* p = tstr_data(str);
	size_t len = tstr_len(str);
	for(size_t i = 0; i < len; i++) {
		p[i] = (char)toupper((unsigned char)p[i]);
	}
}

// Removes leading and trailing whitespace in-place.
TSTR_FUN_ATTRIBUTES void tstr_trim(tstr* const str) {
	if(tstr_len(str) == 0) {
		return;
	}

	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	char* start = tstr_data(str);
	char* end = start + tstr_len(str) - 1;

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

	if(str->type.inner == tstr_type_enum_long) {
		str->long_str.len = final_len;
	} else {
		str->short_str.len = (uint8_t)final_len;
	}
}

// Replaces all occurrences of "target" with "replacement".
// This may reallocate the string if the size grows.
TSTR_FUN_ATTRIBUTES [[nodiscard]] TStrResult tstr_replace(tstr* str, const char* target,
                                                          const char* replacement) {
	if(!target || !*target) {
		return TStrResultErr;
	}

	if(str->type.inner == tstr_type_enum_static) {
		tstr_attempt_static_string_modification(str);
	}

	char* src = tstr_data(str);
	char* ptr = strstr(src, target);

	if(!ptr) {
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

	size_t old_len = tstr_len(str);
	size_t new_len = old_len + (count * (repl_len - target_len));

	tstr res = tstr_init();
	if(tstr_reserve(&res, new_len) != TStrResultOk) {
		return TStrResultErr;
	}

	char* dest = tstr_data(&res);
	char* curr_src = src;
	char* curr_dest = dest;

	while((ptr = strstr(curr_src, target)) != NULL) {
		size_t segment_len = ptr - curr_src;
		memcpy(curr_dest, curr_src, segment_len);
		curr_dest += segment_len;

		memcpy(curr_dest, replacement, repl_len);
		curr_dest += repl_len;

		curr_src = ptr + target_len;
	}

	strcpy(curr_dest, curr_src);

	if(res.type.inner == tstr_type_enum_long) {
		res.long_str.len = new_len;
	} else {
		res.short_str.len = (uint8_t)new_len;
	}

	tstr_free(str);
	*str = res;

	return TStrResultOk;
}

/* Comparison */

// Checks equality between two tstr objects (faster than strcmp).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq(const tstr* str1, const tstr* str2) {
	size_t la = tstr_len(str1);
	size_t lb = tstr_len(str2);
	if(la != lb) {
		return false;
	}
	return memcmp(tstr_cstr(str1), tstr_cstr(str2), la) == 0;
}

// Checks equality between a tstr and a cstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_cstr(const tstr* const str1,
                                                    const char* const str2) {
	size_t la = tstr_len(str1);
	size_t lb = strlen(str2);
	if(la != lb) {
		return false;
	}
	return memcmp(tstr_cstr(str1), str2, la) == 0;
}

// Checks equality ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_ignore_case(const tstr* str1, const tstr* str2) {
	size_t len = tstr_len(str1);
	if(len != tstr_len(str2)) {
		return false;
	}

	const char* ptr1 = tstr_cstr(str1);
	const char* ptr2 = tstr_cstr(str2);

	for(size_t i = 0; i < len; i++) {
		if(tolower((unsigned char)ptr1[i]) != tolower((unsigned char)ptr2[i])) {
			return false;
		}
	}
	return true;
}

// Checks equality ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_eq_ignore_case_cstr(const tstr* const str1,
                                                                const char* const str2) {
	size_t len = tstr_len(str1);
	if(len != strlen(str2)) {
		return false;
	}

	const char* ptr1 = tstr_cstr(str1);

	for(size_t i = 0; i < len; i++) {
		if(tolower((unsigned char)ptr1[i]) != tolower((unsigned char)str2[i])) {
			return false;
		}
	}
	return true;
}

// Standard strcmp behavior for tstr objects.
TSTR_FUN_ATTRIBUTES [[nodiscard]] int tstr_cmp(const tstr* str1, const tstr* str2) {
	return strcmp(tstr_cstr(str1), tstr_cstr(str2));
}

// Checks equality between two tstr_static objects (faster than strcmp).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_static_eq(const tstr_static str1,
                                                      const tstr_static str2) {
	if(str1.len != str2.len) {
		return false;
	}
	return memcmp(str1.ptr, str2.ptr, str1.len) == 0;
}

// Checks equality between a tstr_static and a cstr.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_static_eq_cstr(const tstr_static str1,
                                                           const char* const str2) {
	const size_t lb = strlen(str2);
	if(str1.len != lb) {
		return false;
	}
	return memcmp(str1.ptr, str2, lb) == 0;
}

/* Search */

// Returns the index of the first occurrence of needle, or -1 if not found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] ptrdiff_t tstr_find(const tstr* str, const char* needle) {
	const char* data = tstr_cstr(str);
	const char* found = strstr(data, needle);
	if(!found) {
		return -1;
	}
	return (ptrdiff_t)(found - data);
}

// Returns true if the string contains the substring.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_contains(const tstr* str, const char* needle) {
	return tstr_find(str, needle) != -1;
}

/* UTF-8 Support */

// Decodes the next rune from the pointer and advances the pointer.
// Returns TSTR_UTF8_INVALID (0xFFFD) on error.
TSTR_FUN_ATTRIBUTES [[nodiscard]] uint32_t tstr_next_rune(const char** ptr) {
	const unsigned char* str = (const unsigned char*)*ptr;
	unsigned char c = *str;

	if(c == '\0') {
		return 0;
	}

	if(c < 0x80) {
		*ptr += 1;
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
		*ptr += 1;
		return TSTR_UTF8_INVALID;
	}

	for(int i = 1; i < len; i++) {
		unsigned char next = str[i];
		if((next & 0xC0) != 0x80) {
			*ptr += 1;
			return TSTR_UTF8_INVALID;
		}
		rune = (rune << 6) | (next & 0x3F);
	}

	*ptr += len;
	return rune;
}

// Counts the number of actual UTF-8 Runes, not bytes.
// Returns TSTR_UTF8_INVALID_RUNES ((size_t)-1) on error.
TSTR_FUN_ATTRIBUTES [[nodiscard]] size_t tstr_count_runes(const tstr* str) {
	const char* ptr = tstr_cstr(str);
	size_t count = 0;
	while(*ptr) {
		const uint32_t rune = tstr_next_rune(&ptr);
		if(rune == TSTR_UTF8_INVALID) {
			return TSTR_UTF8_INVALID_RUNES;
		}
		count++;
	}
	return count;
}

// Validates that the string is strictly valid UTF-8.
// Rejects Overlong encodings, Surrogates, and out-of-bounds values.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_is_valid_utf8(const tstr* str) {
	const unsigned char* p = (const unsigned char*)tstr_cstr(str);
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
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_as_view(const tstr* str) {
	return (tstr_view){ .data = tstr_cstr(str), .len = tstr_len(str) };
}

// Converts a view back into an owning tstr (allocates).
TSTR_FUN_ATTRIBUTES tstr tstr_from_view(tstr_view view) {
	return tstr_from_len(view.data, view.len);
}

// Returns a substring view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_sub(tstr_view view, size_t start, size_t len) {
	if(start >= view.len) {
		return (tstr_view){ .data = NULL, .len = 0 };
	}
	if(start + len > view.len) {
		len = view.len - start;
	}

	return (tstr_view){ .data = view.data + start, .len = len };
}

// Returns a substring view, from start until end
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_sub_until_end(tstr_view view, size_t start) {
	if(start >= view.len) {
		return (tstr_view){ .data = NULL, .len = 0 };
	}

	const size_t len = view.len - start;

	return (tstr_view){ .data = view.data + start, .len = len };
}

// Returns the view, that starts after the first occurrence of needle, or NULL for data if not
// found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_find(tstr_view view, const char* needle) {

	const size_t needle_len = strlen(needle);

	for(size_t i = 0; i <= view.len - needle_len; i++) {
		if(memcmp(view.data + i, needle, needle_len) == 0) {
			return (tstr_view){ .data = view.data + i + needle_len,
				                .len = view.len - i - needle_len };
		}
	}

	return (tstr_view){ .data = NULL, .len = 0 };
}

// Checks if view equals a C-string.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq(tstr_view view, const char* cstr) {
	if(strlen(cstr) != view.len) {
		return false;
	}
	return memcmp(view.data, cstr, view.len) == 0;
}

// Checks if view equals a C-string, ignoring case (ASCII only).
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_ignore_case(tstr_view view, const char* cstr) {
	if(strlen(cstr) != view.len) {
		return false;
	}

	for(size_t i = 0; i < view.len; i++) {
		if(tolower((unsigned char)view.data[i]) != tolower((unsigned char)cstr[i])) {
			return false;
		}
	}
	return true;
}

// Checks if two views are equal.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_eq_view(tstr_view vw1, tstr_view vw2) {
	if(vw1.len != vw2.len) {
		return false;
	}
	return memcmp(vw1.data, vw2.data, vw1.len) == 0;
}

// Standard strcmp behavior for tstr_view objects.
TSTR_FUN_ATTRIBUTES [[nodiscard]] int tstr_view_cmp(tstr_view vw1, tstr_view vw2) {
	if(vw1.len != vw2.len) {
		return vw2.len - vw1.len;
	}

	return strncmp(vw1.data, vw2.data, vw1.len);
}

// Checks if view starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_starts_with(tstr_view view, const char* prefix) {
	size_t pre_len = strlen(prefix);
	if(pre_len > view.len) {
		return false;
	}
	return memcmp(view.data, prefix, pre_len) == 0;
}

// Checks if view ends with suffix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_ends_with(tstr_view view, const char* suffix) {
	size_t suf_len = strlen(suffix);
	if(suf_len > view.len) {
		return false;
	}
	return memcmp(view.data + view.len - suf_len, suffix, suf_len) == 0;
}

// Wrapper for checking if an owning tstr starts with prefix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_starts_with(const tstr* str, const char* prefix) {
	return tstr_view_starts_with(tstr_as_view(str), prefix);
}

// Wrapper for checking if an owning tstr ends with suffix.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_ends_with(const tstr* str, const char* suffix) {
	return tstr_view_ends_with(tstr_as_view(str), suffix);
}

// Trims whitespace from the start of the view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_lstrip(tstr_view view) {
	const char* start = view.data;
	const char* end = view.data + view.len;
	while(start < end && isspace((unsigned char)*start)) {
		start++;
	}
	return (tstr_view){ .data = start, .len = (size_t)(end - start) };
}

// Trims whitespace from the end of the view.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_rstrip(tstr_view view) {
	const char* start = view.data;
	const char* end = view.data + view.len;
	while(end > start && isspace((unsigned char)*(end - 1))) {
		end--;
	}
	return (tstr_view){ .data = start, .len = (size_t)(end - start) };
}

// Trims whitespace from both ends.
TSTR_FUN_ATTRIBUTES [[nodiscard]] tstr_view tstr_view_trim(tstr_view view) {
	return tstr_view_lstrip(tstr_view_rstrip(view));
}

// Converts a view to an integer (simple atoi replacement).
// Returns true if successful, false if empty or invalid chars found.
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_view_to_int(tstr_view view, int* out) {
	if(view.len == 0) {
		return false;
	}

	int sign = 1;
	size_t i = 0;

	if(view.data[0] == '-') {
		sign = -1;
		i++;
	} else if(view.data[0] == '+') {
		i++;
	}

	if(i == view.len) {
		return false;
	}

	int result = 0;
	for(; i < view.len; i++) {
		if(view.data[i] < '0' || view.data[i] > '9') {
			return false;
		}
		result = (result * 10) + (view.data[i] - '0');
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
TSTR_FUN_ATTRIBUTES [[nodiscard]] bool tstr_split_next(tstr_split_iter* iter, tstr_view* out_part) {
	if(iter->finished) {
		return false;
	}

	const char* start = iter->source.data + iter->current_pos;
	size_t remaining = iter->source.len - iter->current_pos;

	size_t found_at = remaining;

	for(size_t i = 0; i <= remaining - iter->delim.len; i++) {
		if(memcmp(start + i, iter->delim.data, iter->delim.len) == 0) {
			found_at = i;
			break;
		}
	}

	if(found_at == remaining) {
		*out_part = (tstr_view){ .data = start, .len = remaining };
		iter->finished = true;
	} else {
		*out_part = (tstr_view){ .data = start, .len = found_at };
		iter->current_pos += found_at + iter->delim.len;
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
