#include "logline.hpp"
#include <unistd.h> // for write
#include <fcntl.h> // for O_NOATIME etc
#include <cstdio> // for printf
#include <cstring> // for memset
#include <cstdlib> // for malloc
#include <arpa/inet.h> // for inet_ntop


int main(const int argc,  const char* argv[]){
	if (argc != 2){
		write(2, "USAGE: [log_filepath]\n", 22);
		return 1;
	}
	const char* const logfile_fp = argv[1];
	const int logfile_fd = open(logfile_fp, O_NOATIME|O_RDONLY);
	
	if (logfile_fd == -1){
		[[unlikely]]
		write(1, "Cannot open file\n", 17);
	}
	
	unsigned n_response_names;
	read(logfile_fd, &n_response_names, sizeof(unsigned));
	
	char* const all_response_names = reinterpret_cast<char*>(malloc(n_response_names*100));
	memset(all_response_names, 0, n_response_names*100);
	
	for (unsigned i = 0;  i < n_response_names;  ++i){
		unsigned char response_name_len;
		read(logfile_fd, &response_name_len, sizeof(unsigned char));
		read(logfile_fd,  all_response_names + 100*i,  response_name_len);
		printf("Registered response name: [%u] %.*s\n", (unsigned)response_name_len, (int)response_name_len, all_response_names + 100*i);
	}
	
	while(true){
		if (read(logfile_fd, logline, logline_sz) != logline_sz){
			write(1, "Reached end of file or unexpected end\n", 38);
			break;
		}
		const time_t t = reinterpret_cast<time_t*>(logline)[0];
		struct tm* local_time = gmtime(&t);
		const int hour = local_time->tm_hour;
		const int mins = local_time->tm_min;
		const in_addr_t ip_address = reinterpret_cast<in_addr_t*>(logline+logline_ipaddrindx)[0];
		
		const unsigned custom_strview_size = reinterpret_cast<unsigned*>(logline+logline_datetime_sz)[0];
		const unsigned response_indx = logline[logline_respindxindx];
		const bool keepalive = logline[logline_keepaliveindx];
		const char* const logline_reqheaders = logline + logline_reqheadersindx;
		
		char _buf[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &ip_address, _buf, INET_ADDRSTRLEN);
		
		printf("%2d:%2d %.*s response=%s, custom_strview_size %u, keepalive=%u\n%.1024s\n\n", hour, mins, (int)INET_ADDRSTRLEN, _buf, all_response_names+100*response_indx, custom_strview_size, (unsigned)keepalive, logline_reqheaders);
	}
	close(logfile_fd);
	return 0;
}
