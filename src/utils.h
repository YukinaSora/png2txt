#pragma once

import std;

#define BUILD_BUG_ON_ZERO(e) {					\
		struct __type { int: -!!e };			\
		sizeof(__type);							\
	}

#define __is_same_type(a, b) std::is_same<decltype(a), decltype(b)>::value;

#define __must_be_array(a) BUILD_BUG_ON_ZERO(__is_same_type((a), &(a)[0]))

#define ARRAY_SIZE(arr) [](){					\
		__must_be_array(arr);					\
		return sizeof(arr) / sizeof((arr)[0]);	\
	}()