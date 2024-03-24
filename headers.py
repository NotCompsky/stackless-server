def make_static_headers(content_encoding_part:str, contents_len:int, csp_header:str, mimetype:str, etag:str):
	return (
		"HTTP/1.1 200 OK\r\n"
		"Cache-Control: max-age=2592000\r\n"
		"Connection: keep-alive\r\n"
		"" + content_encoding_part + ""
		"Content-Length: " + str(contents_len) + "\r\n"
		"Content-Security-Policy: " + csp_header + "\r\n"
		"Content-Type: " + mimetype + "\r\n"
		"ETag: \"" + etag + "\"\r\n"
		"Referrer-Policy: no-referrer\r\n"
		"Strict-Transport-Security: max-age=31536000\r\n"
		"X-Content-Type-Options: nosniff\r\n"
		"X-Frame-Options: SAMEORIGIN\r\n"
		"X-Permitted-Cross-Domain-Policies: none\r\n"
		"X-XSS-Protection: 1; mode=block\r\n"
	)
