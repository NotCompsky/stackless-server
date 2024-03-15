#pragma once

#include <compsky/server/server.hpp>
#include <openssl/sha.h>
#include <vector>
#include "typedefs.hpp"

struct WebsocketUserMetadata {
	unsigned socket;
	const uint16_t username_offset;
	const uint16_t username_len;
	WebsocketUserMetadata(
		const unsigned _socket,
		const uint16_t _username_offset,
		const uint16_t _username_len
	)
	: socket(_socket)
	, username_offset(_username_offset)
	, username_len(_username_len)
	{}
};

static
std::vector<WebsocketUserMetadata> websocket_client_metadatas;
static
std::vector<unsigned> websocket_client_ids;

char* all_usernames;


class NonHTTPRequestHandler {
 public:
	NonHTTPRequestHandler()
	{}
	
	static constexpr size_t max_username_len = 20;
	static inline char response_buf[max_username_len + 2 + Server::ClientContext::default_buf_sz];
	
	static
	uint64_t ntoh64(const uint64_t* input){
		// src: https://stackoverflow.com/questions/809902/64-bit-ntohl-in-c
		// link also contains template for arbitrary
		uint64_t rval;
		uint8_t* data = (uint8_t *)&rval;
		data[0] = *input >> 56;
		data[1] = *input >> 48;
		data[2] = *input >> 40;
		data[3] = *input >> 32;
		data[4] = *input >> 24;
		data[5] = *input >> 16;
		data[6] = *input >> 8;
		data[7] = *input >> 0;
		return rval;
	}
	
	static
	size_t decode_incoming_websocket_frame(Server::ClientContext* client_context,  char* const data,  const size_t data_sz,  const uint16_t username_offset,  const uint16_t username_len){
		if (data_sz < 2)
			return 0;
		if (data_sz+3 > Server::ClientContext::default_buf_sz)
			// Rejects 'too-large' requests; but these requests aren't necessarily malicious! See #pegoj0p4jg4. TODO: Handle this properly
			return 0;
		
		// See the structure:  http://localhost:8080/cached/https://miro.medium.com/max/640/1%2ApGpSbWldRuvMV-dqZlPJ7g.webp
		const bool is_final_msg_fragment = data[0] & 0x80;
		if (unlikely(not is_final_msg_fragment)){
			// TODO: Not implemented yet
			client_context->n_bytes_read = 0;
			return 0;
		}
		
		// const bool is_rsv1 = data[0] & 0x40; // ignored; should be set to 0 by sender
		// const bool is_rsv2 = data[0] & 0x20;
		// const bool is_rsv3 = data[0] & 0x10;
		const uint8_t opcode = data[0] & 0x0F; // 4 bits; indicates type of data (e.g. text, binary, control)
		const bool is_masked = data[1] & 0x80;
		uint64_t payload_length = data[1] & 0x7F;
		int offset = 2;
		if (payload_length == 126){
			if (data_sz < 4)
				return 0;
			payload_length = ntohs(*(unsigned short*)(data + offset));
			offset += 2;
		} else if (payload_length == 127){
			if (data_sz < 10)
				return 0;
			payload_length = ntoh64((uint64_t*)(data + offset));
			offset += 8;
		}
		
		if (data_sz != offset + 4*is_masked + payload_length){
			printf("data_sz %lu  !=  %lu (%u + %u + %u)\n", data_sz, (uint64_t)(offset + 4*is_masked + payload_length), (unsigned)offset, (unsigned)(4*is_masked), (unsigned)payload_length);
			if (data_sz > offset + 4*is_masked + payload_length){
				// TODO: Process case where it has read two consecutive payloads
				client_context->n_bytes_read = 0;
				return 0;
			}
			return 0;
		}
		
		data[1] &= 0x7f; // set is_masked bit to false, ready for response
		
		uint32_t masking_key;
		uint32_t* payload_view;
		if (is_masked){
			masking_key  = reinterpret_cast<uint32_t*>(data + offset)[0];
			payload_view = reinterpret_cast<uint32_t*>(data + offset + 4);
			offset += 4;
		}
		char* payload = data + offset;
		if (is_masked){
			// NOTE: This can overstep data_sz by 3 bytes; this is the reason for the restriction #pegoj0p4jg4
			for (size_t i = 0;  i < (payload_length+3)/4;  ++i)
				payload_view[i-1] = payload_view[i] ^ masking_key;
			payload -= 4;
			offset -= 4;
		}
		
		{
			response_buf[0] = data[0];
			const uint64_t new_payload_length = payload_length;
			const uint64_t new_payload_length__plus_username = payload_length + username_len + 2;
			unsigned offset = 2;
			if (new_payload_length__plus_username > (1 << 16)){
				// TODO: Not implemented yet (needs hton64)
				client_context->n_bytes_read = 0;
				return 0;
			} else if (new_payload_length__plus_username >= 126){
				response_buf[1] = 126;
				reinterpret_cast<uint16_t*>(response_buf+2)[0] = htons(new_payload_length__plus_username);
				offset += 2;
			} else {
				response_buf[1] = new_payload_length__plus_username;
			}
			memcpy(response_buf+offset, all_usernames+username_offset, username_len);
			offset += username_len;
			response_buf[offset  ] = ':';
			response_buf[offset+1] = ' ';
			offset += 2;
			memcpy(response_buf+offset, payload, new_payload_length);
			
			client_context->n_bytes_read = 0;
			return offset + new_payload_length;
		}
	}
	
	static
	void handle_client_disconnect(Server::ClientContext* client_context){
		for (unsigned i = 0;  i < websocket_client_metadatas.size();  ++i){
			WebsocketUserMetadata& meta = websocket_client_metadatas[i];
			if (meta.socket == client_context->client_id){
				meta.socket = 0;
				websocket_client_ids[i] = 0;
			}
		}
	}
	
	static
	std::string_view handle_request(Server::ClientContext* client_context,  std::vector<Server::LocalListenerContext>& local_listener_contexts,  char* request_content,  const size_t request_size,  std::vector<unsigned>** broadcast_to_client_ids,  bool* keep_alive){
		*keep_alive = true;
		const WebsocketUserMetadata* meta = nullptr;
		for (unsigned i = 0;  i < websocket_client_metadatas.size();  ++i){
			meta = &websocket_client_metadatas[i];
			if (meta->socket == client_context->client_id){
				break;
			}
		}
		size_t decoded_msg_sz = 0;
		if (meta != nullptr){
			[[likely]];
			decoded_msg_sz = decode_incoming_websocket_frame(client_context, request_content, request_size, meta->username_offset, meta->username_len);
		}
		if (decoded_msg_sz != 0){
			*broadcast_to_client_ids = &websocket_client_ids;
			return std::string_view(response_buf, decoded_msg_sz);
		} else {
			*broadcast_to_client_ids = nullptr;
			return std::string_view("",0);
		}
	}
};
