#pragma once

#include <openssl/sha.h>
#include "typedefs.hpp"
#include "b64d.hpp"

constexpr size_t websocketfirstresponse_sz = 97+28+2+27+2;
static
char websocketfirstresponse[websocketfirstresponse_sz+1] =
	"HTTP/1.1 101 Switching Protocols\r\n"
	"Upgrade: websocket\r\n"
	"Connection: upgrade\r\n"
	"Sec-WebSocket-Accept: ABCDEFGHIJKLMNOPQRSTUVWXYZ12\r\n"
	"Sec-WebSocket-Version: 13\r\n" // length 27
	"\r\n"
;

constexpr
bool is_b64_char(const char c){
	return ((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z')) or ((c >= '0') and (c <= '9')) or (c == '+') or (c == '/') or (c == '=');
}

	std::string_view request_websocket_open(Server::ClientContext* client_context,  char* s,  std::vector<char*>& headers,  const uint16_t username_offset,  const uint16_t username_len){
		
		bool has_websocketupgrade_header = false;
		char* websocketkey = nullptr;
		char* origin_header_start = nullptr;
		char* hostname_start = nullptr;
		for (char* header : headers){
			if (
				(header[0] =='U') and
				(header[1] =='p') and
				(header[2] =='g') and
				(header[3] =='r') and
				(header[4] =='a') and
				(header[5] =='d') and
				(header[6] =='e') and
				(header[7] ==':') and
				(header[8] ==' ') and
				(header[9] =='w') and
				(header[10]=='e') and
				(header[11]=='b') and
				(header[12]=='s') and
				(header[13]=='o') and
				(header[14]=='c') and
				(header[15]=='k') and
				(header[16]=='e') and
				(header[17]=='t')
			){
				has_websocketupgrade_header = true;
			} else if (
				(header[0] =='S') and
				(header[1] =='e') and
				(header[2] =='c') and
				(header[3] =='-') and
				(header[4] =='W') and
				(header[5] =='e') and
				(header[6] =='b') and
				(header[7] =='S') and
				(header[8] =='o') and
				(header[9] =='c') and
				(header[10]=='k') and
				(header[11]=='e') and
				(header[12]=='t') and
				(header[13]=='-') and
				(header[14]=='K') and
				(header[15]=='e') and
				(header[16]=='y') and
				(header[17]==':') and
				(header[18]==' ')
			){
				// Should be a random 16-byte value encoded in Base64 (encoded is 24 char long)
				bool is_valid = true;
				for (unsigned i = 0;  i < 24;  ++i)
					is_valid &= is_b64_char(header[19+i]);
				if (likely(is_valid))
					websocketkey = header + 19;
			} else if (
				(header[0] =='O') and
				(header[1] =='r') and
				(header[2] =='i') and
				(header[3] =='g') and
				(header[4] =='i') and
				(header[5] =='n') and
				(header[6] ==':') and
				(header[7] ==' ')
			){
				origin_header_start = header + 8;
			} else if (
				(header[0] =='H') and
				(header[1] =='o') and
				(header[2] =='s') and
				(header[3] =='t') and
				(header[4] ==':') and
				(header[5] ==' ')
			){
				hostname_start = header + 6;
			}
		}
		
		if (
			(not has_websocketupgrade_header) or
			(websocketkey == nullptr) or
			(origin_header_start == nullptr) or
			(hostname_start == nullptr) // NOTE: Not sure if required to be non-null?
		){
			return not_found;
		}
		
		bool is_valid_origin = false;
		{
			// TODO: Check Origin: is the same as Host: to avoid XSS. We can 'trust' the client for this task, because this is a check that protects the client, not the server
			if (
				(origin_header_start[0] == 'h') and
				(origin_header_start[1] == 't') and
				(origin_header_start[2] == 't') and
				(origin_header_start[3] == 'p')
			){
				origin_header_start += 4 + (origin_header_start[4]=='s');
				if (
					(origin_header_start[0] == ':') and
					(origin_header_start[1] == '/') and
					(origin_header_start[2] == '/')
				){
					origin_header_start += 3;
					unsigned i;
					for (i = 0;  (origin_header_start[i]==hostname_start[i])and(origin_header_start[i]!=0)and(origin_header_start[i]!='\r');  ++i);
					is_valid_origin = (origin_header_start[i]=='\r');
				}
			}
		}
		if (unlikely(not is_valid_origin)){
			printf("Invalid origin vs hostname: %.20s vs %.20s\n", origin_header_start ? origin_header_start : "(NULL)", hostname_start ? hostname_start : "(NULL)");
			return not_found;
		}
		
		char server_key_input[24 + 36]; // TODO: See if client key is decoded first - it probably isn't because the purpose is just to be random
		// websocketkey = "pmy7gYaucTXk6PcRWKNoFw=="; // TODO: Placeholder; it should give a response of Q84YEyJBa+2usswbZNNOqbzPj/w=
		memcpy(server_key_input,    websocketkey, 24);
		memcpy(server_key_input+24, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
		printf("server_key_input == %.60s\n", server_key_input);
		unsigned char sha1[20];
		static_assert(20 == SHA_DIGEST_LENGTH);
		SHA1((unsigned char*)server_key_input, 60, sha1);
		
		base64_encode__length20(sha1, websocketfirstresponse+97);
		printf("websocket_client_ids emplaced %u\n", client_context->client_id);
		websocket_client_ids.emplace_back(client_context->client_id); // TODO: Re-use spaces
		websocket_client_metadatas.emplace_back(client_context->client_id, username_offset, username_len); // TODO: Re-use spaces
		client_context->expecting_http = false;
		return std::string_view(websocketfirstresponse, websocketfirstresponse_sz);
	}
