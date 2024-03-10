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

#define SECURITY_HEADERS_EXCLUDING_CSP \
	"Referrer-Policy: no-referrer\r\n" /* HEADER__SECURITY__NO_REFERRER */ \
	"Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n" /* preload list is centralised (hstspreload.org) for all major browsers, so avoid "; preload" */ \
	"X-Content-Type-Options: nosniff\r\n" /* HEADER__SECURITY__NOSNIFF */ \
	"X-Frame-Options: SAMEORIGIN\r\n" /* or DENY HEADER__SECURITY__NO_FOREIGN_IFRAME */ \
	"X-Permitted-Cross-Domain-Policies: none\r\n" /* Controls whether Flash and Adobe Acrobat are allowed to load content from a different domain. */ \
	"X-XSS-Protection: 1; mode=block\r\n" /* HEADER__SECURITY__XSS */ \
	/* "Feature-Policy: geolocation 'none'; camera 'none'; microphone 'none'\r\n" // HEADER__SECURITY__FEATURE_POLICY */ \
	/* TODO: "Expect-CT: max-age=86400, enforce\r\n" (expect a valid Signed Certificate Timestamp (SCT) for each certificate in the certificate chain, forcing any SSL certificate forgery to leave a 'paper trail' of logs AFAIK) */ \
	/* NOTE: HPKP header has been deprecated, but it looked good */

constexpr static const std::string_view not_found =
	HEADER__RETURN_CODE__NOT_FOUND
	"Connection: keep-alive\r\n"
	"Content-Length: 9\r\n"
	"Content-Security-Policy: default-src 'none'\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/plain\r\n"
	SECURITY_HEADERS_EXCLUDING_CSP
	"\r\n"
	"Not Found"
;
constexpr static const std::string_view server_error =
	HEADER__RETURN_CODE__SERVER_ERR
	"Connection: keep-alive\r\n"
	"Content-Length: 12\r\n"
	"Content-Security-Policy: default-src 'none'\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/plain\r\n"
	SECURITY_HEADERS_EXCLUDING_CSP
	"\r\n"
	"Server error"
;
constexpr static const std::string_view not_logged_in =
	HEADER__RETURN_CODE__OK
	"Connection: keep-alive\r\n" // HEADER__CONNECTION_KEEP_ALIVE
	"Content-Length: 63\r\n"
	"Content-Security-Policy: default-src 'none'\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/html; charset=UTF-8\r\n" \
	SECURITY_HEADERS_EXCLUDING_CSP
	"\r\n"
	"<!DOCTYPE html>"
	"<html>"
	"<body>"
		"<h1>Not logged in</h1>"
	"</body>"
	"</html>"
;
