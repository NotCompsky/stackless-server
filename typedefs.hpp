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
	"Strict-Transport-Security: max-age=31536000\r\n" /* preload list is centralised (hstspreload.org) for all major browsers, so avoid "; preload" */ \
	"X-Content-Type-Options: nosniff\r\n" /* HEADER__SECURITY__NOSNIFF */ \
	"X-Frame-Options: SAMEORIGIN\r\n" /* or DENY HEADER__SECURITY__NO_FOREIGN_IFRAME */ \
	"X-Permitted-Cross-Domain-Policies: none\r\n" /* Controls whether Flash and Adobe Acrobat are allowed to load content from a different domain. */ \
	"X-XSS-Protection: 1; mode=block\r\n" /* HEADER__SECURITY__XSS */ \
	/* "Feature-Policy: geolocation 'none'; camera 'none'; microphone 'none'\r\n" // HEADER__SECURITY__FEATURE_POLICY */ \
	/* TODO: "Expect-CT: max-age=86400, enforce\r\n" (expect a valid Signed Certificate Timestamp (SCT) for each certificate in the certificate chain, forcing any SSL certificate forgery to leave a 'paper trail' of logs AFAIK) */ \
	/* NOTE: HPKP header has been deprecated, but it looked good */

constexpr static const std::string_view not_found =
	HEADER__RETURN_CODE__NOT_FOUND
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
constexpr static const std::string_view wrong_hostname =
	HEADER__RETURN_CODE__SERVER_ERR
	"Content-Length: 10\r\n"
	"Content-Security-Policy: default-src 'none'\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/plain\r\n"
	SECURITY_HEADERS_EXCLUDING_CSP
	"\r\n"
	"Wrong host"
;
constexpr static const std::string_view suspected_robot =
	HEADER__RETURN_CODE__NOT_AUTHORISED
	"Content-Length: 15\r\n"
	"Content-Security-Policy: default-src 'none'\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/plain\r\n"
	SECURITY_HEADERS_EXCLUDING_CSP
	"\r\n"
	"Suspected robot"
;
constexpr static const std::string_view user_login_url_already_used =
	HEADER__RETURN_CODE__OK
	"Content-Length: 288\r\n"
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
		"<h1>This user login was already used or has expired</h1>"
		"<p>Please request a new link by contacting the website owner</p>"
	"</body>"
	"</html>"
;
constexpr static const std::string_view not_logged_in__dont_set_fuck_header =
	HEADER__RETURN_CODE__OK
	"Content-Length: 364\r\n"
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
		"<h1>Not logged in</h1>"
		"<p>Please request a login link by contacting the website owner</p>"
		"<p>You also look like a robot, so maybe use a different browser or device, because you might be blocked.</p>"
	"</body>"
	"</html>"
;
constexpr static const std::string_view not_logged_in__set_fuck_header =
	HEADER__RETURN_CODE__OK
	"Content-Length: 256\r\n"
	"Content-Security-Policy: default-src 'none'; style-src 'sha256-wrWloy50fEZAc/HT+n6+g5BH2EMxYik8NzH3gR6Ge3Y='\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/html; charset=UTF-8\r\n" \
	SECURITY_HEADERS_EXCLUDING_CSP
	"Set-Cookie: fuck=1; max-age=31536000; SameSite=Strict; Secure; HttpOnly\r\n" // to mitigate against gmail/microsoft automatically visiting registration URLs that are emailed to users
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
constexpr static const std::string_view cant_register_user_due_to_lack_of_fuck_cookie =
	HEADER__RETURN_CODE__OK
	"Content-Length: 936\r\n"
	"Content-Security-Policy: default-src 'none'; style-src 'sha256-wrWloy50fEZAc/HT+n6+g5BH2EMxYik8NzH3gR6Ge3Y='; img-src 'i.imgur.com'\r\n" // HEADER__SECURITY__CSP__NONE
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
		"<h1>You are attempting to register on an unrecognised device</h1>"
		"<img src=\"https://i.imgur.com/9dlLLM1.gif\"></img>"
		"<p>Apologies if you are human, but <a href=\"https://support.google.com/mail/thread/16878288/gmail-is-opening-and-caching-urls-within-emails-without-user-intervention-how-and-why?hl=en\">some email services such as GMail and Hotmail automatically visit any URLs in emails</a> and this would break account registrations without this mitigation.</p>"
		"<p>You must use the same device to register an account as the device you requested the account with. You can still have multiple devices, but each would register their own account.</p>"
		"<p>I could instead block Google's IP addresses, but then they'd mark my website as 'malicious'! So it has to be this way.</p>"
	"</body>"
	"</html>"
;
constexpr static const std::string_view wiki_page_not_found =
	HEADER__RETURN_CODE__NOT_FOUND
	"Connection: keep-alive\r\n"
	"Content-Length: 122\r\n"
	"Content-Security-Policy: default-src 'none'; style-src 'self'; img-src 'self'\r\n" // HEADER__SECURITY__CSP__NONE
	"Content-Type: text/html; charset=UTF-8\r\n"
	SECURITY_HEADERS_EXCLUDING_CSP
	"\r\n"
	"Page not found, probably because the title contains special characters which I haven't bothered to code into my parser yet"
;
constexpr static const std::string_view wiki_page_error =
	HEADER__RETURN_CODE__NOT_FOUND
	"Connection: keep-alive\r\n"
	"Content-Length: 55\r\n"
	"Content-Security-Policy: default-src 'none'\r\n"
	"Content-Type: text/plain\r\n"
	SECURITY_HEADERS_EXCLUDING_CSP
	"\r\n"
	"Page exists but cannot be displayed due to software bug"
;
