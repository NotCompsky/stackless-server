#define CACHE_CONTROL_HEADER "Cache-Control: max-age=" MAX_CACHE_AGE "\n"

#define COMPSKY_SERVER_NOFILEWRITES
#define COMPSKY_SERVER_NOSENDMSG
#define COMPSKY_SERVER_NOSENDFILE
#include <compsky/server/server.hpp>
#include <compsky/server/static_response.hpp>
#include <signal.h>
#include "files/files.hpp"

#include <cstdio>

#define SECURITY_HEADERS \
	GENERAL_SECURITY_HEADERS \
	"Content-Security-Policy: default-src 'none'; frame-src 'none'; connect-src 'self'; script-src 'self'; img-src 'self' data:; media-src 'self'; style-src 'self'\r\n"

constexpr static const std::string_view not_found =
	HEADER__RETURN_CODE__NOT_FOUND
	HEADER__CONTENT_TYPE__TEXT
	HEADER__CONNECTION_KEEP_ALIVE
	HEADERS__PLAIN_TEXT_RESPONSE_SECURITY
	"Content-Length: 9\r\n"
	"\r\n"
	"Not Found"
;

constexpr size_t MAX_HEADER_LEN = 4096;
constexpr size_t default_req_buffer_sz_minus1 = 4*4096-1;
class HTTPResponseHandler;
typedef std::nullptr_t NonHTTPRequestHandler;
typedef compsky::server::Server<MAX_HEADER_LEN, default_req_buffer_sz_minus1, HTTPResponseHandler, NonHTTPRequestHandler, 3, 60, 3> Server;

std::vector<Server::ClientContext> all_client_contexts;

int packed_file_fd;

class HTTPResponseHandler {
 public:
	bool keep_alive;
	
	HTTPResponseHandler(const std::int64_t _thread_id,  char* const _buf)
	: keep_alive(false)
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
			// "GET /foobar\r\n" -> "GET " and " /fo" as above and "path" respectively
			const uint32_t path_id = reinterpret_cast<uint32_t*>(str+3)[0];
			const uint32_t path_indx = ((path_id*HASH1_MULTIPLIER) & 0xffffffff) >> 28;
			printf("[%.4s] %u -> %u\n", str+3, path_id, path_indx);
			if (path_indx < HASH1_LIST_LENGTH){
				const ssize_t offset = HASH1_METADATAS[2*path_indx+0];
				const ssize_t fsize  = HASH1_METADATAS[2*path_indx+1];
				return "TODO"; // TODO: read from packed_file_fd
			} else {
#ifndef HASH2_IS_NONE
				const uint32_t path_indx2 = ((path_id*HASH2_MULTIPLIER) & 0xffffffff) >> 28;
				if (likely(path_indx2 < HASH2_LIST_LENGTH)){
					const int fd = open(HASH2_FILEPATHS[path_indx2], O_NOATIME|O_RDONLY);
					close(fd);
					return "TODO2";
				} else {
					return not_found;
				}
#endif
			}
		}
		
		return not_found;
	}
	
	bool handle_timer_event(){
		return false;
	}
	bool is_acceptable_remote_ip_addr(){
		return true;
	}
};

int main(const int argc,  const char* argv[]){
	packed_file_fd = open(HASH1_FILEPATH, O_NOATIME|O_RDONLY); // maybe O_LARGEFILE if >4GiB
	
	if (unlikely(packed_file_fd == 0)){
		return 1;
	}
	
	char* const server_buf = reinterpret_cast<char*>(malloc(4*1024*1024)); // NOTE: Size is arbitrary afaik
	if (unlikely(server_buf == nullptr)){
		return 1;
	}
	
	signal(SIGPIPE, SIG_IGN); // see https://stackoverflow.com/questions/5730975/difference-in-handling-of-signals-in-unix for why use this vs sigprocmask - seems like sigprocmask just causes a queue of signals to build up
	Server::max_req_buffer_sz_minus_1 = 500*1024; // NOTE: Size is arbitrary
	Server::run(8090, server_buf, all_client_contexts);
}
