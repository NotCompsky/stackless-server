#define CACHE_CONTROL_HEADER "Cache-Control: max-age=2592000\r\n"

#define COMPSKY_SERVER_NOFILEWRITES
#define COMPSKY_SERVER_NOSENDMSG
#define COMPSKY_SERVER_NOSENDFILE
#define COMPSKY_SERVER_KTLS
#include <compsky/server/server.hpp>
#include <compsky/server/static_response.hpp>
#include <compsky/http/parse.hpp>
#include <compsky/utils/ptrdiff.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> // for O_NOATIME etc
#include <signal.h>
#include <cstring> // for memcpy
#include <bit> // for std::bit_cast
#include "all_users.hpp"
#include "files/files.hpp"
#include "server_nonhttp.hpp"
#include "request_websocket_open.hpp"
#include "typedefs.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <time.h>
#include "/home/vangelic/repos/compsky/bin/wikipedia/src/extract-page.hpp"
#include "/home/vangelic/repos/compsky/bin/wikipedia/src/get-byte-offset-of-page-given-title.hpp"
#include "logline.hpp"
#include "cookies_utils.hpp"

static_assert(OPENSSL_VERSION_MAJOR == 3, "Minimum OpenSSL version not met: major");

#include <cstdio>

#define SECURITY_HEADERS \
	GENERAL_SECURITY_HEADERS \
	"Content-Security-Policy: default-src 'none'; frame-src 'none'; connect-src 'self'; script-src 'self'; img-src 'self' data:; media-src 'self'; style-src 'self'\r\n"

std::vector<Server::ClientContext> all_client_contexts;
std::vector<Server::EWOULDBLOCK_queue_item> EWOULDBLOCK_queue;

int logfile_fd;
int packed_file_fd;
int enwiki_fd;
int enwiki_archiveindices_fd;
char* server_buf;
char* extra_buf_1;
char* extra_buf_2;
constexpr unsigned extra_buf_1__sz = 1024*1024*10; // arbitrary
constexpr unsigned extra_buf_2__sz = 1024*1024*10;

constexpr
uint32_t uint32_value_of(const char(&s)[4]){
	return std::bit_cast<std::uint32_t>(s);
}

uint64_t uint64_value_of__ptr(const char* const s){
	const uint64_t val = reinterpret_cast<const uint64_t*>(s)[0];
	// printf("uint64_value_of__ptr %.8s %lu\n", s, val);
	return val;
}

constexpr
uint64_t uint64_value_of(const char(&s)[8]){
	return std::bit_cast<std::uint64_t>(s);
}

template<typename T,  unsigned N>
constexpr
unsigned constexprstrlen(T(&s)[N]){
	return N;
}

constexpr
std::string_view http_response__set_user_cookie__prefix(
	"HTTP/1.1 302 Moved Temporarily\r\n"
	HEADER__CONNECTION_KEEP_ALIVE
	"Content-Length: 0\r\n"
	"Location: /\r\n"
	"Set-Cookie: u="
);
constexpr
std::string_view http_response__set_user_cookie__postfix(
	"; max-age=31536000; SameSite=Strict; Secure; HttpOnly\r\n" // NOTE: Some browsers as Safari will not allow you to set a cookie expiration bigger than 1 week (for Javascript-created cookies not server-created cookies?)
	"Strict-Transport-Security: max-age=31536000\r\n" // maybe: includeSubDomains
	"\r\n"
);
char http_response__set_user_cookie[http_response__set_user_cookie__prefix.size() + user_cookie_len + http_response__set_user_cookie__postfix.size()];

constexpr static const std::string_view not_logged_in__set_fuck_header__prefix =
	HEADER__RETURN_CODE__OK
	"Content-Length: 256\r\n"
	"Content-Security-Policy: default-src 'none'; style-src 'sha256-wrWloy50fEZAc/HT+n6+g5BH2EMxYik8NzH3gR6Ge3Y='\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/html; charset=UTF-8\r\n" \
	SECURITY_HEADERS_EXCLUDING_CSP
	"Set-Cookie: fuck="
;
constexpr static const std::string_view not_logged_in__set_fuck_header__postfix = "; max-age=31536000; SameSite=Strict; Secure; HttpOnly\r\n" // to mitigate against gmail/microsoft automatically visiting registration URLs that are emailed to users
	"\r\n"
	"<!DOCTYPE html>"
	"<html>"
	"<head>"
		"<style integrity=\"sha256-wrWloy50fEZAc/HT+n6+g5BH2EMxYik8NzH3gR6Ge3Y=\">"
			"body{color:white;background:black;}"
		"</style>"
	"</head>"
	"<body>"
		"<h1>Not logged in</h1>"
		"<p>Please request a login link by contacting the website owner</p>"
	"</body>"
	"</html>"
;
constexpr unsigned not_logged_in__set_fuck_header__totalsz = not_logged_in__set_fuck_header__prefix.size() + 8 + not_logged_in__set_fuck_header__postfix.size();
char not_logged_in__set_fuck_header[not_logged_in__set_fuck_header__totalsz];

struct SecretPath {
	char path[secret_path_hash_len];
	const unsigned user_indx;
	bool is_already_used;
	SecretPath(
		const char* const _path,
		const bool _is_already_used,
		const uint32_t _user_indx
	)
	: is_already_used(_is_already_used)
	, user_indx(_user_indx)
	{
		memcpy(this->path, _path, secret_path_hash_len);
	}
};
std::vector<SecretPath> secret_path_values;
int secret_paths_fd;
char* secret_paths__filecontents__buf;
constexpr unsigned secret_paths__filecontents__line_length_inc_newline = secret_path_hash_len + 1 + 1 + 1 + 19 + 1;
void expire_user_login_url(const int _secret_paths_fd,  const unsigned secret_path_indx){
	const unsigned offset = secret_path_indx * secret_paths__filecontents__line_length_inc_newline;
	
	secret_paths__filecontents__buf[offset+secret_path_hash_len+1] = '1';
	
	lseek(_secret_paths_fd, 0, SEEK_SET);
	const size_t nbytes = secret_path_values.size()*secret_paths__filecontents__line_length_inc_newline;
	if (write(_secret_paths_fd, secret_paths__filecontents__buf, nbytes) != nbytes){
		[[unlikely]]
		fprintf(stderr, "ERROR: Failed to write to secret_paths.txt: %s\n", strerror(errno));
	}
}


constexpr unsigned n_timer_intervals_per_ratelimit = 3600 / timer_interval;
static_assert(n_timer_intervals_per_ratelimit >= 3);
constexpr unsigned max_requests_per_ratelimit = 360;
unsigned timer_intervals_within_this_ratelimit = 0;


#ifdef DISABLE_SERVER_AFTER_HOURS
int server_after_hour_a;
int server_after_hour_b;
bool is_currently_within_hours;
bool server_after_hour__a_disables;
void determine_is_currently_within_hours(const int hour){
	is_currently_within_hours = ((hour >= server_after_hour_a) and (hour < server_after_hour_b)) ^ server_after_hour__a_disables;
}
#endif


void set_logline_time(const time_t current_time){
	reinterpret_cast<time_t*>(logline)[0] = current_time;
}


class HTTPResponseHandler {
 public:
	bool keep_alive;
	
	HTTPResponseHandler(const std::int64_t _thread_id,  char* const _buf)
	: keep_alive(true)
	{}
	
	char remote_addr_str[INET6_ADDRSTRLEN];
	unsigned remote_addr_str_len;
	
	std::string_view handle_request(
		Server::ClientContext* client_context,
		std::vector<Server::ClientContext>& client_contexts,
		std::vector<Server::LocalListenerContext>& local_listener_contexts,
		char* str,
		const char* const body_content_start,
		const std::size_t body_len,
		std::vector<char*>& headers
	){
		constexpr const char prefix_GET[4] = {'G','E','T',' '};
		constexpr const char prefix_POST[4] = {'P','O','S','T'};
		constexpr const char websocket_path[4] = {'1','/','t','e'};
		constexpr uint32_t websocket_pathid = uint32_value_of(websocket_path);
		const uint32_t prefix_id = reinterpret_cast<uint32_t*>(str)[0];
		
		std::string_view custom_strview;
		this->keep_alive = false;
		unsigned response_indx = response_enum::NOT_FOUND;
		
#ifdef DISABLE_SERVER_AFTER_HOURS
		if (not is_currently_within_hours){
			this->keep_alive = false;
			response_indx = response_enum::SERVER_OUT_OF_HOURS;
			goto return_goto;
		}
#endif
		
		if (prefix_id == uint32_value_of(prefix_GET)){
			bool has_IfModifiedSince_header = false;
			// NOTE: No modern browsers use HEAD requests. Instead they use If-Modified-Since header.
			// TODO: Parse "If-Modified-Since" header, which is the modern equivalent of HEAD requests
			
			constexpr char cookienamefld[8] = {'C','o','o','k','i','e',':',' '};
			constexpr char endofheaders[4] = {'\r','\n','\r','\n'};
			
			unsigned user_indx = n_users;
			
			constexpr const char checkifprefix[4] = {'u','s','e','r'};
			if (reinterpret_cast<uint32_t*>(str+5)[0] != uint32_value_of(checkifprefix)){
				[[likely]];
				
				constexpr char hostname[11] = {'c','o','m','p','s','k','y','.','c','o','m'};
				
				{
					char* hostname_startatspace = nullptr;
					constexpr char hostnamefld[6] = {'H','o','s','t',':',' '};
					const char* const headers_endish1 = body_content_start - constexprstrlen(hostnamefld) - constexprstrlen(hostname) - constexprstrlen(endofheaders);
					char* headers_itr = str + 7; // "GET /\r\n"
					while(headers_itr != headers_endish1){
						if (
							((headers_itr[0] == 'H') or (headers_itr[0] == 'h')) and
							(headers_itr[1] == 'o') and
							(headers_itr[2] == 's') and
							(headers_itr[3] == 't') and
							(headers_itr[4] == ':') and
							(headers_itr[5] == ' ')
						){
							hostname_startatspace = headers_itr + 6;
							break;
						}
						++headers_itr;
					}
					if (headers_itr == headers_endish1){
						[[unlikely]]
						this->keep_alive = false;
						response_indx = response_enum::WRONG_HOSTNAME;
						goto return_goto;
					} else {
						if (not (
							(hostname_startatspace[0] == hostname[0]) and
							(hostname_startatspace[1] == hostname[1]) and
							(hostname_startatspace[2] == hostname[2]) and
							(hostname_startatspace[3] == hostname[3]) and
							(hostname_startatspace[4] == hostname[4]) and
							(hostname_startatspace[5] == hostname[5]) and
							(hostname_startatspace[6] == hostname[6]) and
							(hostname_startatspace[7] == hostname[7]) and
							(hostname_startatspace[8] == hostname[8]) and
							(hostname_startatspace[9] == hostname[9]) and
							(hostname_startatspace[10]== hostname[10]) and
							(hostname_startatspace[11]== '\r')
						)){
							[[unlikely]]
							this->keep_alive = false;
							response_indx = response_enum::WRONG_HOSTNAME;
							goto return_goto;
						}
					}
				}
				{
					bool has_secfetch_header = false;
					constexpr char SecFetchMode_field[17] = {'S','e','c','-','F','e','t','c','h','-','M','o','d','e',':',' ','n'}; // Sec-Fetch-Mode
					const char* const headers_endish1 = body_content_start - constexprstrlen(SecFetchMode_field) - constexprstrlen(endofheaders);
					char* headers_itr = str + 7; // "GET /\r\n"
					while(headers_itr != headers_endish1){
						if (
							((headers_itr[0] == 'S') or (headers_itr[0] == 's')) and
							(headers_itr[1] == 'e') and
							(headers_itr[2] == 'c') and
							(headers_itr[3] == '-') and
							((headers_itr[4] == 'F') or (headers_itr[4] == 'f')) and
							(headers_itr[5] == 'e') and
							(headers_itr[6] == 't') and
							(headers_itr[7] == 'c') and
							(headers_itr[8] == 'h') and
							(headers_itr[9] == '-') and
							((headers_itr[10]== 'M') or (headers_itr[10] == 'm')) and
							(headers_itr[11]== 'o') and
							(headers_itr[12]== 'd') and
							(headers_itr[13]== 'e') and
							(headers_itr[14]== ':') and
							(headers_itr[15]== ' ') and
							((headers_itr[16]== 'n') or (headers_itr[16]== 'w')) // no-cors (loaded by HTML tag not JavaScirpt) or navigate (direct page click by user) or websocket
						){
							has_secfetch_header = true;
							break;
						}
						++headers_itr;
					}
					if (not has_secfetch_header){
						[[unlikely]]
						this->keep_alive = false;
						response_indx = response_enum::SUSPECTED_ROBOT;
						goto return_goto;
					}
				}
				
				{
				const char* cookies_startatspace = get_cookies_startatspace(str,  body_content_start - constexprstrlen(cookienamefld) - user_cookie_len - constexprstrlen(endofheaders));
				if (cookies_startatspace != nullptr){
					cookies_startatspace = find_start_of_u_cookie(cookies_startatspace);
					if (cookies_startatspace[2] != '\r'){
						// NOTE: Guaranteed to be user_cookie_len safe-to-access characters here
						const char* const user_cookie = cookies_startatspace + 3;
						
						[[likely]]
						
						for (user_indx = 0;  user_indx < n_users;  ++user_indx){
							User& user = all_users[user_indx];
							if (compare_secret_path_hashes(user.hash, user_cookie)){
								if (++user.requests_in_this_ratelimit > max_requests_per_ratelimit){
									this->keep_alive = false;
									response_indx = response_enum::RATELIMITED;
									goto return_goto;
								}
								break;
							}
						}
						if (user_indx == n_users){
							[[unlikely]];
						}
					}
				}
				}
				
				if (user_indx == n_users){
					{
						bool has_secfetch_header = false;
						constexpr char SecFetchMode_field[18] = {'S','e','c','-','F','e','t','c','h','-','M','o','d','e',':',' ','n','a'}; // Sec-Fetch-Mode
						const char* const headers_endish1 = body_content_start - constexprstrlen(SecFetchMode_field) - constexprstrlen(endofheaders);
						char* headers_itr = str + 7; // "GET /\r\n"
						while(headers_itr != headers_endish1){
							if (
								((headers_itr[0] == 'S') or (headers_itr[0] == 's')) and
								(headers_itr[1] == 'e') and
								(headers_itr[2] == 'c') and
								(headers_itr[3] == '-') and
								((headers_itr[4] == 'F') or (headers_itr[4] == 'f')) and
								(headers_itr[5] == 'e') and
								(headers_itr[6] == 't') and
								(headers_itr[7] == 'c') and
								(headers_itr[8] == 'h') and
								(headers_itr[9] == '-') and
								((headers_itr[10]== 'M') or (headers_itr[10] == 'm')) and
								(headers_itr[11]== 'o') and
								(headers_itr[12]== 'd') and
								(headers_itr[13]== 'e') and
								(headers_itr[14]== ':') and
								(headers_itr[15]== ' ') and
								(headers_itr[16]== 'n') and
								(headers_itr[17]== 'a') // navigate (direct page click by user)
							){
								has_secfetch_header = true;
								break;
							}
							++headers_itr;
						}
						if (not has_secfetch_header){
							[[unlikely]]
							this->keep_alive = false;
							memcpy(not_logged_in__set_fuck_header+not_logged_in__set_fuck_header__prefix.size(), "fedcba98", 8); // TODO: Hash of IP
							custom_strview = std::string_view(not_logged_in__set_fuck_header, not_logged_in__set_fuck_header__totalsz);
							response_indx = response_enum::SENDING_FROM_CUSTOM_STRVIEW;
							goto return_goto;
						}
					}
					
					this->keep_alive = false;
					memcpy(not_logged_in__set_fuck_header+not_logged_in__set_fuck_header__prefix.size(), "fedcba98", 8); // TODO: Hash of IP
					custom_strview = std::string_view(not_logged_in__set_fuck_header, not_logged_in__set_fuck_header__totalsz);
					response_indx = response_enum::SENDING_FROM_CUSTOM_STRVIEW;
					goto return_goto;
				}
			} else {
				constexpr unsigned fuckcookie_len = 6;
				const char* cookies_startatspace = get_cookies_startatspace(str,  body_content_start - constexprstrlen(cookienamefld) - fuckcookie_len - constexprstrlen(endofheaders));
				if (cookies_startatspace == nullptr){
					this->keep_alive = false;
					response_indx = response_enum::CANT_REGISTER_USER_DUE_TO_LACK_OF_FUCK_COOKIE;;
					goto return_goto;
				}
				
				while(
					(cookies_startatspace[2] != '\r')
				){
					if (
						(cookies_startatspace[0] == ' ') and
						(cookies_startatspace[1] == 'f') and
						(cookies_startatspace[2] == 'u') and
						(cookies_startatspace[3] == 'c') and
						(cookies_startatspace[4] == 'k') and
						(cookies_startatspace[5] == '=')
						// TODO: Hash of IP
					){
						break;
					}
					++cookies_startatspace;
				}
				
				if (cookies_startatspace[2] == '\r'){
					this->keep_alive = false;
					response_indx = response_enum::CANT_REGISTER_USER_DUE_TO_LACK_OF_FUCK_COOKIE;;
					goto return_goto;
				}
				
				if (has_IfModifiedSince_header){
					[[unlikely]]
					this->keep_alive = true;
					response_indx = response_enum::CANT_REGISTER_USER_DUE_TO_LACK_OF_FUCK_COOKIE;;
					goto return_goto;
				}
				
				for (unsigned secret_path_indx = 0;  secret_path_indx < secret_path_values.size();  ++secret_path_indx){
					SecretPath& secret_path = secret_path_values[secret_path_indx];
					// "GET /use" is ignored
					if (compare_secret_path_hashes(secret_path.path, str+8)){
						if (secret_path.is_already_used){
							this->keep_alive = false;
							response_indx = response_enum::USER_LOGIN_URL_ALREADY_USED;;
							goto return_goto;
						}
						memcpy(
							http_response__set_user_cookie + http_response__set_user_cookie__prefix.size(),
							all_users[secret_path.user_indx].hash,
							user_cookie_len
						);
						secret_path.is_already_used = true;
						expire_user_login_url(secret_paths_fd, secret_path_indx);
						this->keep_alive = true;
						response_indx = response_enum::SENDING_FROM_CUSTOM_STRVIEW;
						custom_strview = std::string_view(http_response__set_user_cookie, http_response__set_user_cookie__prefix.size()+user_cookie_len+http_response__set_user_cookie__postfix.size());
						goto return_goto;
					}
				}
				{
					[[unlikely]]
					printf("Failed user login attempt: %.32s\n", str+8);
					this->keep_alive = false;
					response_indx = response_enum::NOT_FOUND;
					goto return_goto;
				}
			}
			
			constexpr const char prefix1[4] = {'/','s','t','a'};
			constexpr const char wiki_prefix[4] = {'w','0','0','/'};
			constexpr const char diaryprefix[4] = {'d','0','0','/'};
			if (
				(reinterpret_cast<uint32_t*>(str)[1] == uint32_value_of(prefix1)) or // GET
				(reinterpret_cast<uint32_t*>(str+1)[1] == uint32_value_of(prefix1)) // HEAD
			){
				// "GET /static/"
#ifndef HASH2_IS_NONE
				const uint32_t path_id = reinterpret_cast<uint32_t*>(str+12)[0];
				const uint32_t path_indx2 = ((path_id*HASH2_MULTIPLIER) & 0xffffffff) >> HASH2_shiftby;
				if (path_indx2 < HASH2_LIST_LENGTH){
					
					size_t from;
					size_t to;
					const compsky::http::header::GetRangeHeaderResult rc = compsky::http::header::get_range(str, from, to);
					if (unlikely(rc == compsky::http::header::GetRangeHeaderResult::invalid)){
						this->keep_alive = false;
						response_indx = response_enum::NOT_FOUND;
						goto return_goto;
					}
					
					if (unlikely( (to != 0) and (to <= from) )){
						this->keep_alive = false;
						response_indx = response_enum::NOT_FOUND;
						goto return_goto;
					}
					
					const HASH2_indx2metadata_item& metadata = HASH2_indx2metadata[path_indx2];
					
					const size_t bytes_to_read1 = (rc == compsky::http::header::GetRangeHeaderResult::none) ? filestreaming__block_sz : ((to) ? (to - from) : filestreaming__stream_block_sz);
					const size_t bytes_to_read  = (bytes_to_read1 > (metadata.fsz-from)) ? (metadata.fsz-from) : bytes_to_read1;
					
					int fd;
					if (not has_IfModifiedSince_header){
						fd = open(metadata.filepath, O_NOATIME|O_RDONLY);
						if (unlikely(fd == -1)){
							printf("Cannot open file\n\tfile = %s\n\terror = %s\n", metadata.filepath, strerror(errno));
							this->keep_alive = true;
							response_indx = response_enum::SERVER_ERROR;
							goto return_goto;
						}
						if (unlikely(lseek(fd, from, SEEK_SET) != from)){
							close(fd);
							this->keep_alive = true;
							response_indx = response_enum::SERVER_ERROR;
							goto return_goto;
						}
					}
					{
						char* server_itr = server_buf;
						if ((rc == compsky::http::header::GetRangeHeaderResult::none) and (bytes_to_read == metadata.fsz)){
							// Both Firefox and Chrome send a range header for videos, neither for images
							compsky::asciify::asciify(server_itr,
								"HTTP/1.1 200 OK\r\n"
								"Accept-Ranges: bytes\r\n"
								CACHE_CONTROL_HEADER
								HEADER__CONNECTION_KEEP_ALIVE
								"Content-Length: ", metadata.fsz, "\r\n"
								"Content-Type: ", metadata.mimetype, "\r\n"
								"X-Frame-Options: DENY\r\n"
								"X-Permitted-Cross-Domain-Policies: none\r\n" // prevents Adobe Acrobat from loading content from different domain
								"\r\n"
							);
						} else {
							compsky::asciify::asciify(server_itr,
								"HTTP/1.1 206 Partial Content\r\n"
								"Accept-Ranges: bytes\r\n"
								CACHE_CONTROL_HEADER
								HEADER__CONNECTION_KEEP_ALIVE
								"Content-Length: ", bytes_to_read, "\r\n"
								"Content-Range: bytes ", from, '-', from + bytes_to_read - 1, '/', metadata.fsz, "\r\n"
									// The minus one is because the range of n bytes is from zeroth byte to the (n-1)th byte
								"Content-Type: ", metadata.mimetype, "\r\n"
								"X-Frame-Options: DENY\r\n"
								"X-Permitted-Cross-Domain-Policies: none\r\n" // prevents Adobe Acrobat from loading content from different domain
								"\r\n"
							);
						}
						if (has_IfModifiedSince_header){
							this->keep_alive = true;
							response_indx = response_enum::SENDING_FROM_CUSTOM_STRVIEW;
							custom_strview = std::string_view(server_buf, compsky::utils::ptrdiff(server_itr,server_buf));
							goto return_goto;
						}
						const ssize_t n_bytes_read = read(fd, server_itr, bytes_to_read);
						close(fd);
						if (unlikely(n_bytes_read != bytes_to_read)){
							// TODO: Maybe read FIRST, then construct headers?
							///server_itr = server_buf;
							///compsky::asciify::asciify(server_itr, n_bytes_read, " == n_bytes_read != bytes_to_read == ", bytes_to_read, "; bytes_to_read1 == ", bytes_to_read1);
							this->keep_alive = true;
							response_indx = response_enum::SERVER_ERROR;
							goto return_goto;
						}
						this->keep_alive = true;
						response_indx = response_enum::SENDING_FROM_CUSTOM_STRVIEW;
						custom_strview = std::string_view(server_buf, compsky::utils::ptrdiff(server_itr,server_buf)+n_bytes_read);
						goto return_goto;
					}
				} else {
					[[unlikely]]
					this->keep_alive = false;
					response_indx = response_enum::NOT_FOUND;
					goto return_goto;
				}
#endif
			} else {
				// "GET /foobar\r\n" -> "GET " and "foob" as above and "path" respectively
				const uint32_t path_id = reinterpret_cast<uint32_t*>(str+5)[0];
				const uint32_t path_indx = ((path_id*HASH1_MULTIPLIER) & 0xffffffff) >> HASH1_shiftby;
				
				if (path_id == websocket_pathid){
					if (has_IfModifiedSince_header){
						[[unlikely]]
						this->keep_alive = false;
						response_indx = response_enum::NOT_FOUND;
						goto return_goto;
					}
					return request_websocket_open(client_context, nullptr, headers, all_usernames[user_indx].offset, all_usernames[user_indx].length);
				} else if (path_id == uint32_value_of(wiki_prefix)){
					if (has_IfModifiedSince_header){
						// TODO?
						this->keep_alive = true;
						response_indx = response_enum::NOT_FOUND;
						goto return_goto;
					}
					
					constexpr char wikipathprefix[9] = {'G','E','T',' ','/','w','0','0','/'};
					char* const title_requested = str + constexprstrlen(wikipathprefix);
					unsigned title_requested_len = 0;
					while(true){
						const char c = title_requested[title_requested_len];
						if ((c == ' ') or (c == '\0'))
							break;
						if (c == '_')
							title_requested[title_requested_len] = ' ';
						++title_requested_len;
					}
					
					if (title_requested_len > 255){
						[[unlikely]]
						this->keep_alive = true;
						response_indx = response_enum::WIKI_PAGE_NOT_FOUND;
						goto return_goto;
					}
					
					constexpr uint32_t* const all_citation_urls = nullptr;
					constexpr bool is_wikipedia = true;
					constexpr bool extract_as_html = false;
					
					constexpr std::string_view wikipage_headers1(
						HEADER__RETURN_CODE__OK
						"Connection: keep-alive\r\n"
						"Content-Length: "
					);
					constexpr std::string_view wikipage_headers2("\r\n"
						"Content-Security-Policy: default-src 'none'\r\n"
						"Content-Type: text/plain; charset=UTF-8\r\n"
						SECURITY_HEADERS_EXCLUDING_CSP
						"\r\n"
					);
					constexpr unsigned space_for_headers = wikipage_headers1.size() + 19 + wikipage_headers2.size();
					
					const compsky_wiki_extractor::OffsetAndPageid offset_and_pageid(compsky_wiki_extractor::get_byte_offset_and_pageid_given_title(
						enwiki_archiveindices_fd,
						title_requested,
						title_requested_len,
						extra_buf_1,
						extra_buf_1__sz,
						extra_buf_2,
						is_wikipedia
					));
					if (unlikely(offset_and_pageid.pageid == nullptr)){
						this->keep_alive = true;
						response_indx = response_enum::WIKI_PAGE_NOT_FOUND;
						goto return_goto;
					}
					const std::string_view wikipage_html_contents(compsky_wiki_extractor::process_file(
						extra_buf_1,
						extra_buf_2,
						extra_buf_1__sz,
						extra_buf_2__sz,
						enwiki_fd,
						server_buf+space_for_headers,
						offset_and_pageid.pageid,
						offset_and_pageid.offset,
						all_citation_urls,
						extract_as_html
					));
					if (unlikely(wikipage_html_contents.data()[0] == '\0')){
						this->keep_alive = true;
						response_indx = response_enum::WIKI_PAGE_ERROR;
						goto return_goto;
					}
					
					char* _start_of_this_page = const_cast<char*>(wikipage_html_contents.data());
					{
						while(true){
							if (
								(_start_of_this_page[0] == '<') and
								(_start_of_this_page[1] == 't') and
								(_start_of_this_page[2] == 'e') and
								(_start_of_this_page[3] == 'x') and
								(_start_of_this_page[4] == 't') and
								(_start_of_this_page[5] == ' ')
							)
								break;
							++_start_of_this_page;
						}
						_start_of_this_page += 6;
						while(*_start_of_this_page != '>')
							++_start_of_this_page;
						++_start_of_this_page;
					}
					
					unsigned html_sz = wikipage_html_contents.size() - compsky::utils::ptrdiff(_start_of_this_page, wikipage_html_contents.data());
					if (
						(_start_of_this_page[html_sz-7] == '<') and
						(_start_of_this_page[html_sz-6] == '/') and
						(_start_of_this_page[html_sz-5] == 't') and
						(_start_of_this_page[html_sz-4] == 'e') and
						(_start_of_this_page[html_sz-3] == 'x') and
						(_start_of_this_page[html_sz-2] == 't') and
						(_start_of_this_page[html_sz-1] == '>')
					)
						html_sz -= 7;
					
					const char* const html_end = _start_of_this_page + wikipage_html_contents.size();
					
					char* server_itr = _start_of_this_page - wikipage_headers2.size();
					memcpy(server_itr, wikipage_headers2.data(), wikipage_headers2.size());
					do {
						--server_itr;
						server_itr[0] = '0' + (html_sz % 10);
						html_sz /= 10;
					} while(html_sz != 0);
					server_itr -= wikipage_headers1.size();
					memcpy(server_itr, wikipage_headers1.data(), wikipage_headers1.size());
					return std::string_view(server_itr, compsky::utils::ptrdiff(html_end,server_itr));
				} else if (path_indx < HASH1_LIST_LENGTH){
					if (path_id != HASH1_ORIG_INTS[path_indx]){
						[[unlikely]]
						this->keep_alive = false;
						response_indx = response_enum::NOT_FOUND;
						goto return_goto;
					}
					
					const ssize_t offset = HASH1_METADATAS[2*path_indx+0];
					const ssize_t fsize  = HASH1_METADATAS[2*path_indx+1];
					if (likely(lseek(packed_file_fd, offset, SEEK_SET) == offset)){
						const ssize_t n_bytes_written = read(packed_file_fd, server_buf, fsize);
						this->keep_alive = true;
						response_indx = response_enum::SENDING_FROM_CUSTOM_STRVIEW;
						custom_strview = std::string_view(server_buf, n_bytes_written);
						goto return_goto;
					}
				} else if (path_id == uint32_value_of(diaryprefix)){
					if (has_IfModifiedSince_header){
						// TODO?
						this->keep_alive = true;
						response_indx = response_enum::NOT_FOUND;
						goto return_goto;
					}
					// "GET /d00/"<IDSTR>
					const uint32_t diary_idstr = reinterpret_cast<uint32_t*>(str+9)[0];
					const uint32_t diary_indx = ((diary_idstr * DIARY_MULTIPLIER) & 0xffffffff) >> DIARY_SHIFTBY;
					
					if (diary_indx < DIARY_LIST_LENGTH){
						[[likely]];
						const ssize_t offset = HASH1_METADATAS[HASH1_METADATAS__DIARY_OFFSET + 2*diary_indx+0];
						const ssize_t fsize  = HASH1_METADATAS[HASH1_METADATAS__DIARY_OFFSET + 2*diary_indx+1];
						
						if (likely(lseek(packed_file_fd, offset, SEEK_SET) == offset)){
							const ssize_t n_bytes_written = read(packed_file_fd, server_buf, fsize);
							this->keep_alive = true;
							custom_strview = std::string_view(server_buf, n_bytes_written);
							response_indx = response_enum::SENDING_FROM_CUSTOM_STRVIEW;
							goto return_goto;
						}
					}
				}
				{
					[[unlikely]]
					this->keep_alive = true;
					response_indx = response_enum::SERVER_ERROR;
					goto return_goto;
				}
			}
		} else {
			[[unlikely]];
		}
		
		return_goto:
		
		const in_addr_t ip_address = client_context->client_addr.sin_addr.s_addr;
		
		reinterpret_cast<in_addr_t*>(logline+logline_ipaddrindx)[0] = ip_address;
		reinterpret_cast<unsigned*>(logline+logline_customstrviewindx)[0] = custom_strview.size();
		logline[logline_respindxindx] = response_indx;
		static_assert(response_enum::N < 256);
		logline[logline_keepaliveindx] = this->keep_alive;
		
		memcpy(logline + logline_reqheadersindx,  str,  logline_reqheaders_len);
		
		write(logfile_fd, logline, logline_sz);
		
		if (response_indx == response_enum::SENDING_FROM_CUSTOM_STRVIEW){
			return custom_strview;
		} else {
			return all_responses[response_indx];
		}
	}
	
	bool handle_timer_event(){
		unsigned n_nonempty = 0;
		uint64_t total_sz = 0;
		for (unsigned i = 0;  i < EWOULDBLOCK_queue.size();  ++i){
			if (EWOULDBLOCK_queue[i].client_socket != 0){
				++n_nonempty;
				total_sz += EWOULDBLOCK_queue[i].queued_msg_length;
			}
		}
		if (n_nonempty != 0)
			printf("EWOULDBLOCK_queue[%lu] has %u non-empty entries (%luKiB)\n", EWOULDBLOCK_queue.size(), n_nonempty, total_sz/1024);
		
		if (++timer_intervals_within_this_ratelimit == n_timer_intervals_per_ratelimit){
			for (unsigned i = 0;  i < n_users;  ++i){
				all_users[i].requests_in_this_ratelimit = 0;
			}
			timer_intervals_within_this_ratelimit = 0;
		}
		
		const time_t current_time = time(0);
		set_logline_time(current_time);
#ifdef DISABLE_SERVER_AFTER_HOURS
		struct tm* local_time = gmtime(&current_time); // NOTE: Does NOT allocate memory, it is a pointer to a static struct
		determine_is_currently_within_hours(local_time->tm_hour);
#endif
		
		// TODO: Tidy vectors such as EWOULDBLOCK_queue by removing items where client_socket==0
		return false;
	}
	bool is_acceptable_remote_ip_addr(){
		return true;
	}
};

int main(const int argc,  const char* argv[]){
	if (argc != 5){
		write(2, "USAGE: [port] [seed] [openssl_ciphers] [enable_within_hours (HH-HH in GMT, 00-24 is always-on)]\n", 96);
		return 1;
	}
	const unsigned listeningport = a2n<unsigned,const char*,false>(argv[1]);
	const uint64_t seed = a2n<uint64_t,const char*,false>(argv[2]);
	const char* const openssl_ciphers = argv[3];
	
	const time_t current_time = time(0);
	set_logline_time(current_time);
	
#ifdef DISABLE_SERVER_AFTER_HOURS
	{
		struct tm* local_time = gmtime(&current_time); // NOTE: Does NOT allocate memory, it is a pointer to a static struct
		
		const char* const hh_hh = argv[4];
		if (strlen(hh_hh) != 5){
			[[unlikely]]
			write(2, "Invalid time\n", 13);
			return 1;
		}
		if (hh_hh[2] != '-'){
			[[unlikely]]
			write(2, "Invalid time\n", 13);
			return 1;
		}
		
		const int ennable_server_after_hour = 10*(hh_hh[0]-'0') + (hh_hh[1]-'0');
		const int disable_server_after_hour = 10*(hh_hh[3]-'0') + (hh_hh[4]-'0');
		
		if (
			(ennable_server_after_hour < 0) or
			(ennable_server_after_hour > 24) or
			(disable_server_after_hour < 0) or
			(disable_server_after_hour > 24)
		){
			[[unlikely]]
			//printf("Invalid time: %i - %i\n", ennable_server_after_hour, disable_server_after_hour);
			write(2, "Invalid time\n", 13);
			return 1;
		}
		
		if (disable_server_after_hour < ennable_server_after_hour){
			server_after_hour_a = disable_server_after_hour;
			server_after_hour_b = ennable_server_after_hour;
			server_after_hour__a_disables = true;
		} else {
			server_after_hour_a = ennable_server_after_hour;
			server_after_hour_b = disable_server_after_hour;
			server_after_hour__a_disables = false;
		}
		
		determine_is_currently_within_hours(local_time->tm_hour);
		
		constexpr size_t offset1 = _r::server_out_of_hours_response__pre.size();
		memcpy(_r::server_out_of_hours_response__buf, _r::server_out_of_hours_response__pre.data(), offset1);
		_r::server_out_of_hours_response__buf[offset1+0] = '0' + (ennable_server_after_hour/10);
		_r::server_out_of_hours_response__buf[offset1+1] = '0' + (ennable_server_after_hour%10);
		_r::server_out_of_hours_response__buf[offset1+2] = ':';
		_r::server_out_of_hours_response__buf[offset1+3] = '0';
		_r::server_out_of_hours_response__buf[offset1+4] = '0';
		_r::server_out_of_hours_response__buf[offset1+5] = '-';
		
		size_t offset2 = offset1 + 6;
		if (disable_server_after_hour < ennable_server_after_hour){
			_r::server_out_of_hours_response__buf[offset1+6] = '2';
			_r::server_out_of_hours_response__buf[offset1+7] = '4';
			_r::server_out_of_hours_response__buf[offset1+8] = ':';
			_r::server_out_of_hours_response__buf[offset1+9] = '0';
			_r::server_out_of_hours_response__buf[offset1+10]= '0';
			_r::server_out_of_hours_response__buf[offset1+11]= ' ';
			_r::server_out_of_hours_response__buf[offset1+12]= 'o';
			_r::server_out_of_hours_response__buf[offset1+13]= 'r';
			_r::server_out_of_hours_response__buf[offset1+14]= ' ';
			_r::server_out_of_hours_response__buf[offset1+15]= '0';
			_r::server_out_of_hours_response__buf[offset1+16]= '0';
			_r::server_out_of_hours_response__buf[offset1+17]= ':';
			_r::server_out_of_hours_response__buf[offset1+18]= '0';
			_r::server_out_of_hours_response__buf[offset1+19]= '0';
			_r::server_out_of_hours_response__buf[offset1+20]= '-';
			offset2 = offset1 + 21;
		} else {
			_r::server_out_of_hours_response__buf[offset1+11]= '&'; // &nbsp; or &#xA0; (non-break space) just to use up the characters and keep the buffer the same size
			_r::server_out_of_hours_response__buf[offset1+12]= 'n';
			_r::server_out_of_hours_response__buf[offset1+13]= 'b';
			_r::server_out_of_hours_response__buf[offset1+14]= 's';
			_r::server_out_of_hours_response__buf[offset1+15]= 'p';
			_r::server_out_of_hours_response__buf[offset1+16]= ';';
			_r::server_out_of_hours_response__buf[offset1+17]= ' ';
			_r::server_out_of_hours_response__buf[offset1+18]= ' ';
			_r::server_out_of_hours_response__buf[offset1+19]= ' ';
			_r::server_out_of_hours_response__buf[offset1+20]= ' ';
		}
		{
			_r::server_out_of_hours_response__buf[offset2  ] = '0' + (disable_server_after_hour/10);
			_r::server_out_of_hours_response__buf[offset2+1] = '0' + (disable_server_after_hour%10);
			_r::server_out_of_hours_response__buf[offset2+2] = ':';
			_r::server_out_of_hours_response__buf[offset2+3] = '0';
			_r::server_out_of_hours_response__buf[offset2+4] = '0';
		}
		
		memcpy(_r::server_out_of_hours_response__buf+offset1+_r::server_out_of_hours_response__example_of_middle_content.size(), _r::server_out_of_hours_response__post.data(), _r::server_out_of_hours_response__post.size());
	}
#endif
	
	logfile_fd = open(logfile_fp, O_NOATIME|O_WRONLY);
	packed_file_fd = open("/media/vangelic/DATA/tmp/static_webserver.pack", O_NOATIME|O_RDONLY); // maybe O_LARGEFILE if >4GiB
	enwiki_fd                = open("/media/vangelic/DATA/dataset/wikipedia/enwiki-20230620-pages-articles-multistream.xml.bz2",                O_NOATIME|O_RDONLY|O_LARGEFILE);
	enwiki_archiveindices_fd = open("/media/vangelic/DATA/dataset/wikipedia/enwiki-20230620-pages-articles-multistream-index.txt.offsetted.gz", O_NOATIME|O_RDONLY);
	
	{
		secret_paths_fd = open("secret_paths.txt", O_NOATIME|O_RDWR);
		if (secret_paths_fd == -1){
			[[unlikely]]
			write(2, "Cannot open secret_paths.txt\n", 29);
			return 1;
		}
		const off_t f_sz = lseek(secret_paths_fd, 0, SEEK_END);
		constexpr unsigned line_length_inc_newline = secret_paths__filecontents__line_length_inc_newline;
		if ((f_sz % line_length_inc_newline) != 0){
			[[unlikely]]
			write(2, "secret_paths.txt has wrong format (NOTE: vim automatically adds invisible newline on the end)\n", 94);
			return 1;
		}
		secret_paths__filecontents__buf = reinterpret_cast<char*>(malloc(f_sz));
		if (secret_paths__filecontents__buf == nullptr){
			[[unlikely]]
			write(2, "Cannot allocate memory\n", 23);
			return 1;
		}
		lseek(secret_paths_fd, 0, SEEK_SET);
		read(secret_paths_fd, secret_paths__filecontents__buf, f_sz);
		
		for (unsigned offset = 0;  offset != f_sz;  offset += line_length_inc_newline){
			if (secret_paths__filecontents__buf[offset+line_length_inc_newline-1] != '\n'){
				[[unlikely]]
				fprintf(
					stderr,
					"secret_paths.txt: Expected newline at %u on line %u, but got '%c' '%c' '%c'\n",
					offset+line_length_inc_newline,
					offset/line_length_inc_newline,
					secret_paths__filecontents__buf[offset+line_length_inc_newline-1],
					secret_paths__filecontents__buf[offset+line_length_inc_newline],
					secret_paths__filecontents__buf[offset+line_length_inc_newline+1]
				);
				return 1;
			}
			secret_path_values.emplace_back(
				secret_paths__filecontents__buf+offset,
				(secret_paths__filecontents__buf[offset+secret_path_hash_len+1]=='1'), 
				a2n<uint64_t,const char*,false>(secret_paths__filecontents__buf+offset+secret_path_hash_len+3)
			);
		}
	}
	
	if (logfile_fd == -1){
		[[unlikely]]
		write(2, "Failed to open logfile\n", 23);
		return 1;
	}
	if (packed_file_fd == -1){
		[[unlikely]]
		write(2, "Failed to open packed_file\n", 27);
		return 1;
	}
	if (enwiki_fd == -1){
		[[unlikely]]
		write(2, "Failed to open enwiki\n", 22);
		return 1;
	}
	if (enwiki_archiveindices_fd == -1){
		[[unlikely]]
		write(2, "Failed to open enwiki_archiveindices\n", 37);
		return 1;
	}
	
	{
		constexpr unsigned n_response_names_ce = constexprstrlen(all_response_names);
		unsigned n_response_names = n_response_names_ce;
		write(logfile_fd, reinterpret_cast<void*>(&n_response_names), sizeof(unsigned));
		for (unsigned i = 0;  i < n_response_names_ce;  ++i){
			const std::string_view& sv = all_response_names[i];
			const unsigned char sv_size = sv.size();
			write(logfile_fd, &sv_size, sizeof(unsigned char));
			write(logfile_fd, sv.data(), sv.size());
		}
		
		unsigned _n_users = n_users;
		write(logfile_fd, &_n_users, sizeof(unsigned));
		
		unsigned usernames_buf_len = sizeof(usernames_buf);
		write(logfile_fd, &usernames_buf_len, sizeof(unsigned));
		write(logfile_fd, usernames_buf, usernames_buf_len);
		
		for (unsigned i = 0;  i < n_users;  ++i){
			write(logfile_fd, all_users[i].hash, user_cookie_len);
		}
	}
	
	server_buf = reinterpret_cast<char*>(malloc(server_buf_sz));
	extra_buf_1 = reinterpret_cast<char*>(malloc(extra_buf_1__sz));
	extra_buf_2 = reinterpret_cast<char*>(malloc(extra_buf_2__sz));
	if (unlikely((server_buf == nullptr) or (extra_buf_1 == nullptr) or (extra_buf_2 == nullptr))){
		write(2, "Cannot allocate memory\n", 23);
		return 1;
	}
	
	compsky_wiki_extractor::pages_articles_multistream_index_txt_offsetted_gz__init();
	
	memcpy(
		http_response__set_user_cookie,
		http_response__set_user_cookie__prefix.data(),
		http_response__set_user_cookie__prefix.size()
	);
	memcpy(
		http_response__set_user_cookie + http_response__set_user_cookie__prefix.size() + user_cookie_len,
		http_response__set_user_cookie__postfix.data(),
		http_response__set_user_cookie__postfix.size()
	);
	
	memcpy(
		not_logged_in__set_fuck_header,
		not_logged_in__set_fuck_header__prefix.data(),
		not_logged_in__set_fuck_header__prefix.size()
	);
	memcpy(
		not_logged_in__set_fuck_header + not_logged_in__set_fuck_header__prefix.size() + 8,
		not_logged_in__set_fuck_header__postfix.data(),
		not_logged_in__set_fuck_header__postfix.size()
	);
	
	
	
	if (unlikely(SSL_library_init() != 1)){
		fprintf(stderr, "SSL_library_init failed\n");
		return 1;
	}
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
	ERR_load_crypto_strings();

    const SSL_METHOD* method = TLS_server_method();
    Server::ssl_ctx = SSL_CTX_new(method);
    if (Server::ssl_ctx == nullptr){
        ERR_print_errors_fp(stderr);
        return 1;
    }
    
	if (SSL_CTX_set_cipher_list(Server::ssl_ctx, openssl_ciphers) != 1){
		fprintf(stderr, "Error setting cipher list.\n");
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(Server::ssl_ctx);
		return 1;
	}
	{
		SSL* const ssl = SSL_new(Server::ssl_ctx);
		const char *cipher_list = SSL_get_cipher_list(ssl, 0);
		if (cipher_list != NULL) {
			printf("Cipher suites supported by the server:\n%s\n", cipher_list);
		} else {
			printf("Error retrieving cipher suites.\n");
		}
	}
    
    SSL_CTX_set_options(Server::ssl_ctx, SSL_OP_ENABLE_KTLS);
	
	SSL_CTX_set_min_proto_version(Server::ssl_ctx, TLS1_3_VERSION);
	SSL_CTX_set_max_proto_version(Server::ssl_ctx, TLS1_3_VERSION);
	
	// SSL_CTX_set_options(Server::ssl_ctx, SSL_OP_ENABLE_KTLS_TX_ZEROCOPY_SENDFILE);
    if (SSL_CTX_use_certificate_file(Server::ssl_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0){
        ERR_print_errors_fp(stderr);
        return 1;
    }
    if (SSL_CTX_use_PrivateKey_file(Server::ssl_ctx, "server.key", SSL_FILETYPE_PEM) <= 0 ){
        ERR_print_errors_fp(stderr);
        return 1;
    }
    if (unlikely(not SSL_CTX_check_private_key(Server::ssl_ctx))){
		fprintf(stderr, "Private key does not match the public key\n");
		return 1;
	}
	
	
	signal(SIGPIPE, SIG_IGN); // see https://stackoverflow.com/questions/5730975/difference-in-handling-of-signals-in-unix for why use this vs sigprocmask - seems like sigprocmask just causes a queue of signals to build up
	Server::max_req_buffer_sz_minus_1 = 500*1024; // NOTE: Size is arbitrary
	Server::run(listeningport, INADDR_ANY, server_buf, all_client_contexts, EWOULDBLOCK_queue);
	
	return 0;
}
