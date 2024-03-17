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
#include "files/files.hpp"
#include "server_nonhttp.hpp"
#include "request_websocket_open.hpp"
#include "typedefs.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
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
	"; SameSite=Strict; Secure; HttpOnly\r\n"
	"Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n"
	"\r\n"
);
constexpr unsigned secret_path_hash_len = 32;
bool compare_secret_path_hashes(const char(&hash1)[secret_path_hash_len],  const char* const hash2){
	const uint64_t* const a = reinterpret_cast<const uint64_t*>(hash1);
	const uint64_t* const b = reinterpret_cast<const uint64_t*>(hash2);
	static_assert(secret_path_hash_len==32, "compare_secret_path_hashes assumes wrong secret_path_hash_len");
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
char http_response__set_user_cookie[http_response__set_user_cookie__prefix.size() + secret_path_hash_len + http_response__set_user_cookie__postfix.size()];

constexpr

struct SecretPath {
	const uint64_t p1;
	const uint64_t p2;
	const uint64_t p3;
	const uint64_t p4;
	char hash[secret_path_hash_len];
	const uint32_t username_offset;
	const uint16_t username_len;
	SecretPath(
		const uint64_t seed,
		const uint64_t _p1,
		const uint64_t _p2,
		const uint64_t _p3,
		const uint64_t _p4,
		const uint32_t _username_offset,
		const uint16_t _username_len
	)
	: p1(_p1)
	, p2(_p2)
	, p3(_p3)
	, p4(_p4)
	, username_offset(_username_offset)
	, username_len(_username_len)
	{
		uint64_t _hash1 = seed * p1;
		uint64_t _hash2 = seed * p2;
		uint64_t _hash3 = seed * p3;
		uint64_t _hash4 = seed * p4;
		for (unsigned i = 0;  i < 10;  ++i){
			const unsigned n = _hash4&63;
			if (n < 10)
				hash[i] = '0' + n;
			else if (n < 36)
				hash[i] = 'a' + (n - 10);
			else if (n < 62)
				hash[i] = 'A' + (n - 36);
			else if (n == 62)
				hash[i] = '/';
			else
				hash[i] = '+';
			_hash4 >>= 6;
		}
		for (unsigned i = 10;  i < 20;  ++i){
			const unsigned n = _hash3&63;
			if (n < 10)
				hash[i] = '0' + n;
			else if (n < 36)
				hash[i] = 'a' + (n - 10);
			else if (n < 62)
				hash[i] = 'A' + (n - 36);
			else if (n == 62)
				hash[i] = '/';
			else
				hash[i] = '+';
			_hash3 >>= 6;
		}
		for (unsigned i = 20;  i < 30;  ++i){
			const unsigned n = _hash2&63;
			if (n < 10)
				hash[i] = '0' + n;
			else if (n < 36)
				hash[i] = 'a' + (n - 10);
			else if (n < 62)
				hash[i] = 'A' + (n - 36);
			else if (n == 62)
				hash[i] = '/';
			else
				hash[i] = '+';
			_hash2 >>= 6;
		}
		for (unsigned i = 30;  i < secret_path_hash_len;  ++i){
			const unsigned n = _hash2&63;
			if (n < 10)
				hash[i] = '0' + n;
			else if (n < 36)
				hash[i] = 'a' + (n - 10);
			else if (n < 62)
				hash[i] = 'A' + (n - 36);
			else if (n == 62)
				hash[i] = '/';
			else
				hash[i] = '+';
			_hash2 >>= 6;
		}
		static_assert(secret_path_hash_len == 32, "secret_path_hash_len != 32");
	}
};
std::vector<SecretPath> secret_path_values;
char* expired_user_login_urls__metadata;
int expired_user_login_urls_fd;
void expire_user_login_url(const int expired_user_login_urls_fd,  const unsigned user_indx){
	expired_user_login_urls__metadata[user_indx] = '1';
	
	lseek(expired_user_login_urls_fd, 0, SEEK_SET);
	write(expired_user_login_urls_fd, expired_user_login_urls__metadata, secret_path_values.size());
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
		// NOTE: str guaranteed to be at least default_req_buffer_sz_minus1
		printf("[%.4s] %u\n", str, reinterpret_cast<uint32_t*>(str)[0]);
		if (likely(reinterpret_cast<uint32_t*>(str)[0] == 542393671)){
			unsigned user_indx = 0;
			
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
							(headers_itr[0] == 'H') and
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
				const char* const headers_endish2 = body_content_start - constexprstrlen(cookienamefld) - secret_path_hash_len - constexprstrlen(endofheaders);
				char* headers_itr = str + 7; // "GET /\r\n"
				while(headers_itr != headers_endish2){
					if (
						(headers_itr[0] == 'C') and
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
						// NOTE: Guaranteed to be secret_path_hash_len safe-to-access characters here
						const char* const secret_path_hash = cookies_startatspace + 3;
						
						[[likely]]
						
						for (;  user_indx < secret_path_values.size();  ++user_indx){
							const SecretPath& secret_path = secret_path_values[user_indx];
							if (compare_secret_path_hashes(secret_path.hash, secret_path_hash)){
								break;
							}
						}
						if (user_indx == secret_path_values.size()){
							[[unlikely]]
							user_indx = 0;
						}
					}
				}
				}
				
				if (user_indx == 0){
					return not_logged_in;
				}
			} else {
				for (;  user_indx < secret_path_values.size();  ++user_indx){
					const SecretPath& secret_path = secret_path_values[user_indx];
					// "GET /use" is ignored
					printf("secret_path.p1 %lu\n", secret_path.p1);
					if (
						(uint64_value_of__ptr(str+8)  == secret_path.p1) and
						(uint64_value_of__ptr(str+16) == secret_path.p2) and
						(uint64_value_of__ptr(str+24) == secret_path.p3) and
						(uint64_value_of__ptr(str+32) == secret_path.p4)
					){
						if (expired_user_login_urls__metadata[user_indx] == '1'){
							return user_login_url_already_used;
						}
						memcpy(
							http_response__set_user_cookie + http_response__set_user_cookie__prefix.size(),
							secret_path.hash,
							secret_path_hash_len
						);
						expire_user_login_url(expired_user_login_urls_fd, user_indx);
						return std::string_view(http_response__set_user_cookie, http_response__set_user_cookie__prefix.size()+secret_path_hash_len+http_response__set_user_cookie__postfix.size());
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
					return request_websocket_open(client_context, nullptr, headers, secret_path_values[user_indx].username_offset, secret_path_values[user_indx].username_len);
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
		// TODO: Tidy vectors such as EWOULDBLOCK_queue by removing items where client_socket==0
		return false;
	}
	bool is_acceptable_remote_ip_addr(){
		return true;
	}
};

int main(const int argc,  const char* argv[]){
	packed_file_fd = open(HASH1_FILEPATH, O_NOATIME|O_RDONLY); // maybe O_LARGEFILE if >4GiB
	
	if (argc != 3){
		write(2, "USAGE: [port] [seed]\n", 14);
		return 1;
	}
	const unsigned listeningport = a2n<unsigned,const char*,false>(argv[1]);
	const uint64_t seed = a2n<uint64_t,const char*,false>(argv[2]);
	
	enwiki_fd = open("/media/vangelic/DATA/dataset/wikipedia/enwiki-20230620-pages-articles-multistream.xml.bz2", O_NOATIME|O_RDONLY|O_LARGEFILE);
	expired_user_login_urls_fd = open("expired_user_login_urls.txt", O_NOATIME|O_RDWR);
	enwiki_archiveindices_fd = open("/media/vangelic/DATA/dataset/wikipedia/enwiki-20230620-pages-articles-multistream-index.txt.offsetted.gz", O_NOATIME|O_RDONLY);
	
	if (unlikely((expired_user_login_urls_fd == -1) or (packed_file_fd == -1) or (enwiki_fd == -1) or (enwiki_archiveindices_fd == -1))){
		write(2, "Failed to open expired_user_login_urls or packed_file or enwiki\n", 64);
		return 1;
	}
	
	server_buf = reinterpret_cast<char*>(malloc(server_buf_sz));
	extra_buf_1 = reinterpret_cast<char*>(malloc(extra_buf_1__sz));
	extra_buf_2 = reinterpret_cast<char*>(malloc(extra_buf_2__sz));
	if (unlikely((server_buf == nullptr) or (extra_buf_1 == nullptr) or (extra_buf_2 == nullptr))){
		write(2, "Cannot allocate memory for main buffers\n", 41);
		return 1;
	}
	
	compsky_wiki_extractor::pages_articles_multistream_index_txt_offsetted_gz__init();
	
	memcpy(
		http_response__set_user_cookie,
		http_response__set_user_cookie__prefix.data(),
		http_response__set_user_cookie__prefix.size()
	);
	memcpy(
		http_response__set_user_cookie + http_response__set_user_cookie__prefix.size() + secret_path_hash_len,
		http_response__set_user_cookie__postfix.data(),
		http_response__set_user_cookie__postfix.size()
	);
	
	{
		const int secret_path_values_fd = open("secret_file_paths.txt", O_NOATIME|O_RDONLY);
		if (secret_path_values_fd == -1){
			[[unlikely]]
			write(2, "Cannot open secret_file_paths.txt (thus cannot associate users with usernames etc)\n", 83);
			return 1;
		} else {
			struct stat statbuf;
			if (fstat(secret_path_values_fd, &statbuf) != 0){
				[[unlikely]]
				write(2, "fstat error on secret_file_paths.txt\n", 37);
				return 1;
			}
			const off_t fsz = statbuf.st_size;
			
			all_usernames = reinterpret_cast<char*>(malloc(fsz));
			if (unlikely(all_usernames == nullptr)){
				[[unlikely]]
				write(2, "Cannot allocate memory\n", 23);
				return 1;
			}
			
			char* const _buf = reinterpret_cast<char*>(malloc(fsz));
			if (read(secret_path_values_fd, _buf, fsz) != fsz){
				[[unlikely]]
				write(2, "read error on secret_file_paths.txt\n", 36);
				return 1;
			}
			close(secret_path_values_fd);
			unsigned eightchar_indx = 0;
			unsigned n_entries_upperbound = 0;
			for (unsigned i = 0;  i < fsz;  ++i){
				const char c = _buf[i];
				if (c == '\n'){
					++n_entries_upperbound;
				}
			}
			unsigned secret_path_values_indx = 0;
			char* start_of_secret_path = nullptr;
			char* start_of_external_username = nullptr;
			unsigned all_usernames__indx = 0;
			for (unsigned i = 0;  i < fsz;  ++i){
				const char c = _buf[i];
				if (c == '\n'){
					if (start_of_secret_path != nullptr){
						if (start_of_external_username == nullptr){
							[[unlikely]]
							write(2, "ERROR: start_of_external_username is NULL but start_of_secret_path is not\n", 74);
							return 1;
						}
						const unsigned internal_username_len = compsky::utils::ptrdiff(start_of_external_username,start_of_secret_path) - 32 - 2;
						const unsigned external_username_len = compsky::utils::ptrdiff(_buf+i,start_of_external_username);
						
						unsigned already_exists_at_indx = 0;
						for (;  already_exists_at_indx < secret_path_values.size();  ++already_exists_at_indx){
							const SecretPath& _secret_path = secret_path_values[already_exists_at_indx];
							if (_secret_path.username_len == external_username_len){
								bool matches = true;
								for (unsigned j = 0;  j < external_username_len;  ++j){
									matches &= (all_usernames[_secret_path.username_offset + j] != start_of_external_username[j]);
								}
								if (matches)
									break;
							}
						}
						uint32_t str_offset;
						if (already_exists_at_indx == secret_path_values.size()){
							memcpy(all_usernames+all_usernames__indx, start_of_external_username, external_username_len);
							str_offset = all_usernames__indx;
							all_usernames__indx += external_username_len;
						} else {
							str_offset = secret_path_values[already_exists_at_indx].username_offset;
						}
						
						const SecretPath& secret_path = secret_path_values.emplace_back(
							seed,
							uint64_value_of__ptr(start_of_secret_path),
							uint64_value_of__ptr(start_of_secret_path+8),
							uint64_value_of__ptr(start_of_secret_path+16),
							uint64_value_of__ptr(start_of_secret_path+24),
							str_offset,
							external_username_len
						);
					}
					eightchar_indx = 0;
					start_of_secret_path = nullptr;
					start_of_external_username = nullptr;
				} else if (c == ' '){
					if (eightchar_indx == 32){
						start_of_secret_path = _buf+i-32;
						++eightchar_indx;
					} else if (eightchar_indx == 33){
						start_of_external_username = _buf+i+1;
					} else {
						[[unlikely]]
						write(2, "Secret path is not 32 bytes long\n", 33);
						return 1;
					}
				} else if (eightchar_indx == 33){
					// pass
				} else if (c == '#'){
					eightchar_indx = 33;
				} else {
					++eightchar_indx;
				}
			}
			free(_buf);
			
			expired_user_login_urls__metadata = reinterpret_cast<char*>(malloc(secret_path_values.size()));
			if (read(expired_user_login_urls_fd, expired_user_login_urls__metadata, secret_path_values.size()) != secret_path_values.size()){
				[[unlikely]]
				write(2, "Error reading expired_user_login_urls\n", 38);
				return 1;
			}
		}
	}
	
	
	
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
    SSL_CTX_set_options(Server::ssl_ctx, SSL_OP_ENABLE_KTLS);
	SSL_CTX_set_options(Server::ssl_ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);
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
}
