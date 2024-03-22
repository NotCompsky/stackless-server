#pragma once


constexpr unsigned user_cookie_len = 32;
constexpr unsigned secret_path_hash_len = 32;

struct User {
	const char hash[user_cookie_len];
	const unsigned user_indx;
	unsigned requests_in_this_ratelimit;
	/*static_assert(user_cookie_len==32, "user_cookie_len!=32");
	template<unsigned _username_len>
	Username(
		const uint64_t hash1,
		const uint64_t hash2,
		const uint64_t hash3,
		const char(&_buf)[_username_len],
		const uint16_t username_len
	)
	: buf(_buf)
	, username_len(_username_len)
	, */
};
struct Username {
	const uint16_t offset;
	const uint16_t length;
};
constexpr unsigned n_users = 3;
User all_users[n_users]{
	{{'9','G','Y','6','t','s','f','Q','g','p','2','k','u','t','e','q','Z','c','N','N','8','J','Q','R','N','3','m','L','k','2','4','l'},0},
	{{'U','e','U','0','m','w','K','0','Y','2','q','Z','/','D','3','I','h','n','o','d','/','v','I','i','n','A','A','A','N','V','W','I'},1},
	{{'s','d','s','n','t','Y','s','x','E','r','p','r','u','g','/','J','N','x','p','Z','C','6','h','P','E','h','w','B','p','V','h','X'},2}
};
constexpr
Username all_usernames[n_users]{
	{0,4},
	{4,5},
	{4,5}
};
constexpr
const char usernames_buf[] = {
	'A','d','a','m', // +4, 4
	'G','u','e','s','t' // +5, 9
};
