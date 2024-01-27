#define CACHE_CONTROL_HEADER "Cache-Control: max-age=" MAX_CACHE_AGE "\n"

#define COMPSKY_SERVER_NOFILEWRITES
#define COMPSKY_SERVER_NOSENDMSG
#define COMPSKY_SERVER_NOSENDFILE
#include <compsky/server/server.hpp>
#include <compsky/server/static_response.hpp>
#include <compsky/http/parse.hpp>
#include <compsky/utils/ptrdiff.hpp>
#include <signal.h>
#include "files/files.hpp"
#include "server_nonhttp.hpp"
#include "request_websocket_open.hpp"
#include "typedefs.hpp"

#include <cstdio>

#define SECURITY_HEADERS \
	GENERAL_SECURITY_HEADERS \
	"Content-Security-Policy: default-src 'none'; frame-src 'none'; connect-src 'self'; script-src 'self'; img-src 'self' data:; media-src 'self'; style-src 'self'\r\n"

std::vector<Server::ClientContext> all_client_contexts;
std::vector<Server::EWOULDBLOCK_queue_item> EWOULDBLOCK_queue;

int packed_file_fd;
char* server_buf;

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
				} else {
					return server_error;
				}
			} else {
#ifndef HASH2_IS_NONE
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
					
					const int fd = open(metadata.fp, O_NOATIME|O_RDONLY);
					if (likely(lseek(fd, from, SEEK_SET) == from)){
						char* server_itr = server_buf;
						if ((rc == compsky::http::header::GetRangeHeaderResult::none) and (bytes_to_read == metadata.fsz)){
							// Both Firefox and Chrome send a range header for videos, neither for images
							compsky::asciify::asciify(server_itr,
								"HTTP/1.1 200 OK\r\n"
								"Accept-Ranges: bytes\r\n"
								"Content-Type: ", metadata.mimetype, "\r\n"
								HEADER__CONNECTION_KEEP_ALIVE
								// TODO: Surely content-range header should be here?
								"Content-Length: ", metadata.fsz, "\r\n"
								"\r\n"
							);
						} else {
							compsky::asciify::asciify(server_itr,
								"HTTP/1.1 206 Partial Content\r\n"
								"Accept-Ranges: bytes\r\n"
								"Content-Type: ", metadata.mimetype, "\r\n"
								HEADER__CONNECTION_KEEP_ALIVE
								"Content-Range: bytes ", from, '-', from + bytes_to_read - 1, '/', metadata.fsz, "\r\n"
								// The minus one is because the range of n bytes is from zeroth byte to the (n-1)th byte
								"Content-Length: ", bytes_to_read, "\r\n"
								"\r\n"
							);
						}
						const ssize_t n_bytes_read = read(fd, server_itr, bytes_to_read);
						if (unlikely(n_bytes_read != bytes_to_read)){
							// TODO: Maybe read FIRST, then construct headers?
							///server_itr = server_buf;
							///compsky::asciify::asciify(server_itr, n_bytes_read, " == n_bytes_read != bytes_to_read == ", bytes_to_read, "; bytes_to_read1 == ", bytes_to_read1);
							close(fd);
							return server_error;
						}
						close(fd);
						return std::string_view(server_buf, compsky::utils::ptrdiff(server_itr,server_buf)+n_bytes_read);
					} else {
						close(fd);
						return server_error;
					}
				} else if (path_id == HASH_ANTIINPUT_0){
					return request_websocket_open(client_context, nullptr, headers);
				} else {
					[[unlikely]]
					return not_found;
				}
#endif
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
	
	if (unlikely(packed_file_fd == 0)){
		return 1;
	}
	
	server_buf = reinterpret_cast<char*>(malloc(server_buf_sz));
	if (unlikely(server_buf == nullptr)){
		return 1;
	}
	
	signal(SIGPIPE, SIG_IGN); // see https://stackoverflow.com/questions/5730975/difference-in-handling-of-signals-in-unix for why use this vs sigprocmask - seems like sigprocmask just causes a queue of signals to build up
	Server::max_req_buffer_sz_minus_1 = 500*1024; // NOTE: Size is arbitrary
	Server::run(8090, server_buf, all_client_contexts, EWOULDBLOCK_queue);
}
