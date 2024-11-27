#ifndef LAZY_EXCEPTION_HPP_
#define LAZY_EXCEPTION_HPP_

#include "lazylog.h"
#include <cstdio> // std::snprintf()
#include <stdexcept>

class lazyexecption : public std::runtime_error
{
public:
	explicit lazyexecption(const char* description);
};

/* Inline methods. */

inline lazyexecption::lazyexecption(const char* description)
  : std::runtime_error(description)
{
}

// clang-format off
#define LAZY_THROW_EXCEPTION(desc, ...) \
	do \
	{ \
		static char buffer[2000]; \
		\
		std::snprintf(buffer, 2000, desc, ##__VA_ARGS__); \
		lberror("throwing lazy exception: %s", buffer); \
		throw lazyexecption(buffer); \
	} while (false)

/*#define MSC_THROW_TYPE_ERROR(desc, ...) \
	do \
	{ \
		MSC_ERROR("throwing MediaSoupClientTypeError: " desc, ##__VA_ARGS__); \
		\
		static char buffer[2000]; \
		\
		std::snprintf(buffer, 2000, desc, ##__VA_ARGS__); \
		throw MediaSoupClientTypeError(buffer); \
	} while (false)

#define MSC_THROW_UNSUPPORTED_ERROR(desc, ...) \
	do \
	{ \
		MSC_ERROR("throwing MediaSoupClientUnsupportedError: " desc, ##__VA_ARGS__); \
		\
		static char buffer[2000]; \
		\
		std::snprintf(buffer, 2000, desc, ##__VA_ARGS__); \
		throw MediaSoupClientUnsupportedError(buffer); \
	} while (false)

#define MSC_THROW_INVALID_STATE_ERROR(desc, ...) \
	do \
	{ \
		MSC_ERROR("throwing MediaSoupClientInvalidStateError: " desc, ##__VA_ARGS__); \
		\
		static char buffer[2000]; \
		\
		std::snprintf(buffer, 2000, desc, ##__VA_ARGS__); \
		throw MediaSoupClientInvalidStateError(buffer); \
	} while (false)
	*/
// clang-format on

#endif
