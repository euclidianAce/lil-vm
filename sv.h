#ifndef SV_H
#define SV_H

#include <stdint.h>
#include <stdbool.h>

typedef struct sv {
	uint32_t len;
	char const *data;
} sv;

#define sv_c_(str_lit) { sizeof("" str_lit) - 1, "" str_lit }
#define sv_c(str_lit) ((sv)sv_c_(str_lit))
sv sv_(char const *);
sv sv_from_c(char const *);
bool sv_eq(sv, sv);

sv sv_chop(sv *s, uint32_t);
sv sv_chop_end(sv *s, uint32_t);

// skips whitespace, then slices up to the next ascii whitespace char
sv sv_chop_word(sv *);

char sv_first(sv);
char sv_last(sv);

bool sv_starts_with(sv haystack, sv needle);

#endif // SV_H
