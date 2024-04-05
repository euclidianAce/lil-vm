#include "sv.h"
#include <string.h>
#include <assert.h>

sv sv_from_c(char const *c) {
	return (sv){ strlen(c), c };
}

bool sv_eq(sv a, sv b) {
	if (a.len != b.len)
		return false;

	for (uint32_t i = 0; i < a.len; ++i)
		if (a.data[i] != b.data[i])
			return false;

	return true;
}

static inline bool whitespace(char c) {
	switch (c) {
	case ' ':
	case '\t':
	case '\n':
		return true;
	}
	return false;
}

char sv_chop_one(sv *s) {
	assert(s->len >= 1);
	char result = *s->data;
	s->data += 1;
	s->len -= 1;
	return result;
}

sv sv_chop(sv *s, uint32_t n) {
	assert(s->len >= n);
	sv result = { n, s->data };
	s->len -= n;
	s->data += n;
	return result;
}

sv sv_chop_end(sv *s, uint32_t n) {
	sv result = { n, s->data + s->len - n - 1 };
	s->len -= n;
	return result;
}

sv sv_chop_whitespace(sv *s) { sv_chop_body(s, c, whitespace(c)); }
sv sv_chop_non_whitespace(sv *s) { sv_chop_body(s, c, !whitespace(c)); }

sv sv_chop_word(sv *s) {
	sv_chop_whitespace(s);
	return sv_chop_non_whitespace(s);
}

char sv_first(sv s) {
	if (s.len == 0) return 0;
	return s.data[0];
}

char sv_last(sv s) {
	if (s.len == 0) return 0;
	return s.data[s.len - 1];
}

bool sv_starts_with(sv haystack, sv needle) {
	if (needle.len > haystack.len)
		return false;

	sv trimmed = { needle.len, haystack.data };
	return sv_eq(trimmed, needle);
}
