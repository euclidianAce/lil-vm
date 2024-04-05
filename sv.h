#ifndef SV_H
#define SV_H

#include <stdint.h>
#include <stdbool.h>

typedef struct sv {
	uint32_t len;
	char const *data;
} sv;

#define sv_fstr "%.*s"
#define sv_farg(s) (int)(s).len, (s).data

#define sv_c_(str_lit) { sizeof("" str_lit) - 1, "" str_lit }
#define sv_c(str_lit) ((sv)sv_c_(str_lit))
sv sv_from_c(char const *);
bool sv_eq(sv, sv);

sv sv_chop(sv *s, uint32_t);
sv sv_chop_end(sv *s, uint32_t);

#define sv_chop_body(ptr, char_name, predicate) do { \
	sv *_ptr_ = ptr; \
	uint32_t _count_ = 0; \
	while (_count_ < _ptr_->len) { \
		char char_name = _ptr_->data[_count_]; \
		if (predicate) { ++_count_; } \
		else { break; } \
	} \
	return sv_chop(_ptr_, _count_); \
} while (0)

char sv_chop_one(sv *);

sv sv_chop_whitespace(sv *);
sv sv_chop_non_whitespace(sv *);

// skips whitespace, then slices up to the next ascii whitespace char
sv sv_chop_word(sv *);

char sv_first(sv);
char sv_last(sv);

bool sv_starts_with(sv haystack, sv needle);

#endif // SV_H
