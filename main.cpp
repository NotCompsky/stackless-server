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

static_assert(OPENSSL_VERSION_MAJOR == 3, "Minimum OpenSSL version not met: major");

#include <cstdio>

#define SECURITY_HEADERS \
	GENERAL_SECURITY_HEADERS \
	"Content-Security-Policy: default-src 'none'; frame-src 'none'; connect-src 'self'; script-src 'self'; img-src 'self' data:; media-src 'self'; style-src 'self'\r\n"

std::vector<Server::ClientContext> all_client_contexts;
std::vector<Server::EWOULDBLOCK_queue_item> EWOULDBLOCK_queue;

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

template<unsigned N>
unsigned constexprstrlen(const char(&s)[N]){
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
char http_response__set_user_cookie[http_response__set_user_cookie__prefix.size() + user_cookie_len + http_response__set_user_cookie__postfix.size()];

constexpr

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


int server_after_hour_a;
int server_after_hour_b;
bool is_currently_within_hours;
bool server_after_hour__a_disables;

#ifdef DISABLE_SERVER_AFTER_HOURS
void determine_is_currently_within_hours(const time_t t){
	struct tm* local_time = gmtime(&t); // NOTE: Does NOT allocate memory, it is a pointer to a static struct
	const int hour = local_time->tm_hour;
	is_currently_within_hours = ((hour >= server_after_hour_a) and (hour < server_after_hour_b)) ^ server_after_hour__a_disables;
}
constexpr static const std::string_view server_out_of_hours_response__pre =
	HEADER__RETURN_CODE__OK
	"Content-Length: 251\r\n"
	"Content-Security-Policy: default-src 'none'; style-src 'sha256-wrWloy50fEZAc/HT+n6+g5BH2EMxYik8NzH3gR6Ge3Y='\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/html; charset=UTF-8\r\n" \
	SECURITY_HEADERS_EXCLUDING_CSP
	"\r\n"
	"<!DOCTYPE html>"
	"<html>"
	"<head>"
		"<style integrity=\"sha256-wrWloy50fEZAc/HT+n6+g5BH2EMxYik8NzH3gR6Ge3Y=\">"
			"body{color:white;background:black;}"
		"</style>"
	"</head>"
	"<body>"
		"<h1>Out of hours</h1>"
		"<p>Website is sleeping</p>"
		"<p>Come back between "
;
constexpr static const std::string_view server_out_of_hours_response__post =
		" (GMT)</p>"
	"</body>"
	"</html>"
;
static
char server_out_of_hours_response__buf[server_out_of_hours_response__pre.size() + 5 + server_out_of_hours_response__post.size()];
static const std::string_view server_out_of_hours_response(server_out_of_hours_response__buf,  server_out_of_hours_response__pre.size() + 5 + server_out_of_hours_response__post.size());
#endif


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
#ifdef DISABLE_SERVER_AFTER_HOURS
		if (not is_currently_within_hours){
			return server_out_of_hours_response;
		}
#endif
		
		// NOTE: str guaranteed to be at least default_req_buffer_sz_minus1
		printf("[%.4s] %u\n", str, reinterpret_cast<uint32_t*>(str)[0]);
		if (likely(reinterpret_cast<uint32_t*>(str)[0] == 542393671)){
			unsigned user_indx = n_users;
			
			constexpr const char checkifprefix[4] = {'u','s','e','r'};
			if (reinterpret_cast<uint32_t*>(str+5)[0] != uint32_value_of(checkifprefix)){
				[[likely]];
				
				constexpr char endofheaders[4] = {'\r','\n','\r','\n'};
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
						return wrong_hostname;
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
							return wrong_hostname;
						}
					}
				}
				
				{
				char* cookies_startatspace = nullptr;
				constexpr char cookienamefld[8] = {'C','o','o','k','i','e',':',' '};
				const char* const headers_endish2 = body_content_start - constexprstrlen(cookienamefld) - user_cookie_len - constexprstrlen(endofheaders);
				char* headers_itr = str + 7; // "GET /\r\n"
				while(headers_itr != headers_endish2){
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
				if (headers_itr != headers_endish2){
					// NOTE: cookies_startatspace guaranteed != nullptr
					
					// "Cookie: " <- NOT after the space
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
					if (cookies_startatspace[2] != '\r'){
						// NOTE: Guaranteed to be user_cookie_len safe-to-access characters here
						const char* const user_cookie = cookies_startatspace + 3;
						
						[[likely]]
						
						for (user_indx = 0;  user_indx < n_users;  ++user_indx){
							const User& user = all_users[user_indx];
							if (compare_secret_path_hashes(user.hash, user_cookie)){
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
					return not_logged_in;
				}
			} else {
				for (unsigned secret_path_indx = 0;  secret_path_indx < secret_path_values.size();  ++secret_path_indx){
					SecretPath& secret_path = secret_path_values[secret_path_indx];
					// "GET /use" is ignored
					if (compare_secret_path_hashes(secret_path.path, str+8)){
						if (secret_path.is_already_used){
							return user_login_url_already_used;
						}
						memcpy(
							http_response__set_user_cookie + http_response__set_user_cookie__prefix.size(),
							all_users[secret_path.user_indx].hash,
							user_cookie_len
						);
						secret_path.is_already_used = true;
						expire_user_login_url(secret_paths_fd, secret_path_indx);
						return std::string_view(http_response__set_user_cookie, http_response__set_user_cookie__prefix.size()+user_cookie_len+http_response__set_user_cookie__postfix.size());
					}
				}
				{
					[[unlikely]]
					printf("Failed user login attempt: %.32s\n", str+8);
					return not_found;
				}
			}
			
			constexpr const char prefix1[4] = {'/','s','t','a'};
			constexpr const char wiki_prefix[4] = {'w','0','0','/'};
			constexpr const char diaryprefix[4] = {'d','0','0','/'};
			if (reinterpret_cast<uint32_t*>(str)[1] == uint32_value_of(prefix1)){
				// "GET /static/"
#ifndef HASH2_IS_NONE
				const uint32_t path_id = reinterpret_cast<uint32_t*>(str+12)[0];
				const uint32_t path_indx2 = ((path_id*HASH2_MULTIPLIER) & 0xffffffff) >> HASH2_shiftby;
				if (path_indx2 < HASH2_LIST_LENGTH){
					
					size_t from;
					size_t to;
					const compsky::http::header::GetRangeHeaderResult rc = compsky::http::header::get_range(str, from, to);
					if (unlikely(rc == compsky::http::header::GetRangeHeaderResult::invalid)){
						return not_found;
					}
					
					if (unlikely( (to != 0) and (to <= from) ))
						return not_found;
					
					const HASH2_indx2metadata_item& metadata = HASH2_indx2metadata[path_indx2];
					
					const size_t bytes_to_read1 = (rc == compsky::http::header::GetRangeHeaderResult::none) ? filestreaming__block_sz : ((to) ? (to - from) : filestreaming__stream_block_sz);
					const size_t bytes_to_read  = (bytes_to_read1 > (metadata.fsz-from)) ? (metadata.fsz-from) : bytes_to_read1;
					
					const int fd = open(metadata.filepath, O_NOATIME|O_RDONLY);
					if (likely(lseek(fd, from, SEEK_SET) == from)){
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
						const ssize_t n_bytes_read = read(fd, server_itr, bytes_to_read);
						close(fd);
						if (unlikely(n_bytes_read != bytes_to_read)){
							// TODO: Maybe read FIRST, then construct headers?
							///server_itr = server_buf;
							///compsky::asciify::asciify(server_itr, n_bytes_read, " == n_bytes_read != bytes_to_read == ", bytes_to_read, "; bytes_to_read1 == ", bytes_to_read1);
							return server_error;
						}
						return std::string_view(server_buf, compsky::utils::ptrdiff(server_itr,server_buf)+n_bytes_read);
					} else {
						close(fd);
						return server_error;
					}
				} else {
					[[unlikely]]
					return not_found;
				}
#endif
			} else {
				// "GET /foobar\r\n" -> "GET " and "foob" as above and "path" respectively
				const uint32_t path_id = reinterpret_cast<uint32_t*>(str+5)[0];
				const uint32_t path_indx = ((path_id*HASH1_MULTIPLIER) & 0xffffffff) >> HASH1_shiftby;
				printf("[%.4s] %u -> %u\n", str+5, path_id, path_indx);
				
				if (path_indx < HASH1_LIST_LENGTH){
					const ssize_t offset = HASH1_METADATAS[2*path_indx+0];
					const ssize_t fsize  = HASH1_METADATAS[2*path_indx+1];
					if (likely(lseek(packed_file_fd, offset, SEEK_SET) == offset)){
						const ssize_t n_bytes_written = read(packed_file_fd, server_buf, fsize);
						return std::string_view(server_buf, n_bytes_written);
					}
				} else if (path_id == HASH_ANTIINPUT_0){
					return request_websocket_open(client_context, nullptr, headers, all_usernames[user_indx].offset, all_usernames[user_indx].length);
				} else if (path_id == uint32_value_of(wiki_prefix)){
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
						return wiki_page_not_found;
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
					if (unlikely(offset_and_pageid.pageid == nullptr))
						return wiki_page_not_found;
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
						return wiki_page_error;
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
				} else if (path_id == uint32_value_of(diaryprefix)){
					// "GET /d00/"<IDSTR>
					const uint32_t diary_idstr = reinterpret_cast<uint32_t*>(str+9)[0];
					const uint32_t diary_indx = ((diary_idstr * DIARY_MULTIPLIER) & 0xffffffff) >> DIARY_SHIFTBY;
					
					if (diary_indx < DIARY_LIST_LENGTH){
						[[likely]];
						const ssize_t offset = HASH1_METADATAS[HASH1_METADATAS__DIARY_OFFSET + 2*diary_indx+0];
						const ssize_t fsize  = HASH1_METADATAS[HASH1_METADATAS__DIARY_OFFSET + 2*diary_indx+1];
						
						if (likely(lseek(packed_file_fd, offset, SEEK_SET) == offset)){
							const ssize_t n_bytes_written = read(packed_file_fd, server_buf, fsize);
							return std::string_view(server_buf, n_bytes_written);
						}
					}
				}
				{
					[[unlikely]]
					return server_error;
				}
			}
		}
		
		return not_found;
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
		
#ifdef DISABLE_SERVER_AFTER_HOURS
		{
			const time_t current_time = time(0);
			determine_is_currently_within_hours(current_time);
		}
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
	
#ifdef DISABLE_SERVER_AFTER_HOURS
	{
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
		
		determine_is_currently_within_hours(time(0));
		
		memcpy(server_out_of_hours_response__buf, server_out_of_hours_response__pre.data(), server_out_of_hours_response__pre.size());
		server_out_of_hours_response__buf[server_out_of_hours_response__pre.size()+0] = '0' + (ennable_server_after_hour/10);
		server_out_of_hours_response__buf[server_out_of_hours_response__pre.size()+1] = '0' + (ennable_server_after_hour%10);
		server_out_of_hours_response__buf[server_out_of_hours_response__pre.size()+2] = '-';
		server_out_of_hours_response__buf[server_out_of_hours_response__pre.size()+3] = '0' + (disable_server_after_hour/10);
		server_out_of_hours_response__buf[server_out_of_hours_response__pre.size()+4] = '0' + (disable_server_after_hour%10);
		memcpy(server_out_of_hours_response__buf+server_out_of_hours_response__pre.size()+5, server_out_of_hours_response__post.data(), server_out_of_hours_response__post.size());
	}
#endif
	
	packed_file_fd = open(HASH1_FILEPATH, O_NOATIME|O_RDONLY); // maybe O_LARGEFILE if >4GiB
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
	
	if (unlikely((packed_file_fd == -1) or (enwiki_fd == -1) or (enwiki_archiveindices_fd == -1))){
		write(2, "Failed to open expired_user_login_urls or packed_file or enwiki\n", 64);
		return 1;
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
