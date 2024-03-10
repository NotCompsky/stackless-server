#pragma once

constexpr size_t MAX_HEADER_LEN = 4096;
constexpr size_t default_req_buffer_sz_minus1 = 4*4096-1;
constexpr size_t filestreaming__block_sz = 1024 * 1024 * 10;
constexpr size_t filestreaming__stream_block_sz = 1024 * 1024;
constexpr size_t filestreaming__max_response_header_sz = 4321; // TODO: Establish actual value - this is an (over)estimate
static_assert(filestreaming__block_sz >= filestreaming__stream_block_sz, "filestreaming__block_sz must be >= filestreaming__stream_block_sz");
constexpr size_t server_buf_sz = (HASH1_max_file_and_header_sz > (filestreaming__block_sz+filestreaming__max_response_header_sz)) ? HASH1_max_file_and_header_sz : (filestreaming__block_sz+filestreaming__max_response_header_sz);
class HTTPResponseHandler;
class NonHTTPRequestHandler;
typedef compsky::server::Server<MAX_HEADER_LEN, default_req_buffer_sz_minus1, HTTPResponseHandler, NonHTTPRequestHandler, 0, 60, 5> Server;

constexpr static const std::string_view not_found =
	HEADER__RETURN_CODE__NOT_FOUND
	HEADER__CONTENT_TYPE__TEXT
	HEADER__CONNECTION_KEEP_ALIVE
	HEADERS__PLAIN_TEXT_RESPONSE_SECURITY
	"Content-Length: 9\r\n"
	"\r\n"
	"Not Found"
;
constexpr static const std::string_view server_error =
	HEADER__RETURN_CODE__SERVER_ERR
	HEADER__CONTENT_TYPE__TEXT
	HEADER__CONNECTION_KEEP_ALIVE
	HEADERS__PLAIN_TEXT_RESPONSE_SECURITY
	"Content-Length: 12\r\n"
	"\r\n"
	"Server error"
;
constexpr static const std::string_view not_logged_in =
	HEADER__RETURN_CODE__OK
	"Content-Type: text/html\r\n"
	HEADER__CONNECTION_KEEP_ALIVE
	HEADERS__PLAIN_TEXT_RESPONSE_SECURITY
	"Content-Length: 63\r\n"
	"\r\n"
	"<!DOCTYPE html>"
	"<html>"
	"<body>"
		"<h1>Not logged in</h1>"
	"</body>"
	"</html>"
;
