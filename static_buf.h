#ifndef STATIC_BUF_H
#define STATIC_BUF_H

#include <assert.h>

#define static_buf(type, name, max_count) \
	static type name[max_count]; \
	typedef char name##__static_buf_size_type[max_count]; \
	static size_t name##__static_buf_count = 0


#define static_buf_add(name) ( \
	assert(name##__static_buf_count < sizeof(name##__static_buf_size_type)), \
	&name[name##__static_buf_count++] \
)

#define static_buf_count(name) name##__static_buf_count
#define static_buf_max_count(name) sizeof(name##__static_buf_size_type)

#endif // STATIC_BUF_H
