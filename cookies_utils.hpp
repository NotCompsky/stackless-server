#pragma once

const char* get_cookies_startatspace(const char* const str,  const char* const headers_endish){ // "Cookie: " <- NOT after the space
	const char* cookies_startatspace = nullptr;
	const char* headers_itr = str + 7; // "GET /\r\n"
	while(headers_itr != headers_endish){
		if (
			((headers_itr[0] == 'C') or (headers_itr[0] == 'c')) and
			(headers_itr[1] == 'o') and
			(headers_itr[2] == 'o') and
			(headers_itr[3] == 'k') and
			(headers_itr[4] == 'i') and
			(headers_itr[5] == 'e') and
			(headers_itr[6] == ':') and
			(headers_itr[7] == ' ')
		){
			cookies_startatspace = headers_itr + 8 - 1;
			break;
		}
		++headers_itr;
	}
	return cookies_startatspace;
}

const char* find_start_of_u_cookie(const char* cookies_startatspace){
	while(
		(cookies_startatspace[2] != '\r')
	){
		if (
			(cookies_startatspace[0] == ' ') and
			(cookies_startatspace[1] == 'u') and
			(cookies_startatspace[2] == '=')
		){
			break;
		}
		++cookies_startatspace;
	}
	return cookies_startatspace;
}

template<unsigned hash1_sz>
bool compare_secret_path_hashes(const char(&hash1)[hash1_sz],  const char* const hash2){
	const uint64_t* const a = reinterpret_cast<const uint64_t*>(hash1);
	const uint64_t* const b = reinterpret_cast<const uint64_t*>(hash2);
	static_assert(hash1_sz==32, "compare_secret_path_hashes assumes wrong hash1_sz");
	/*printf("%.32s vs %.32s\n", hash1, hash2);
	printf("%lu vs %lu\n", a[0], b[0]);
	printf("%lu vs %lu\n", a[1], b[1]);
	printf("%lu vs %lu\n", a[2], b[2]);
	printf("%lu vs %lu\n", a[3], b[3]);*/
	return (
		(a[0]==b[0]) and
		(a[1]==b[1]) and
		(a[2]==b[2]) and
		(a[3]==b[3])
	);
}
