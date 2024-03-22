#include "logline.hpp"
#include "cookies_utils.hpp"
#include "typedefs2.hpp"
#include <unistd.h> // for write
#include <fcntl.h> // for O_NOATIME etc
#include <cstdio> // for printf
#include <cstring> // for memset
#include <cstdlib> // for malloc
#include <arpa/inet.h> // for inet_ntop


struct UserCookie {
	char value[user_cookie_len];
};


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
	
	unsigned n_users;
	read(logfile_fd, &n_users, sizeof(unsigned));
	
	unsigned usernames_buf_len;
	read(logfile_fd, &usernames_buf_len, sizeof(unsigned));
	char* const usernames_buf = reinterpret_cast<char*>(malloc(usernames_buf_len));
	if (usernames_buf == nullptr){
		write(2, "Cannot allocate memory\n", 23);
		return 1;
	}
	read(logfile_fd, usernames_buf, usernames_buf_len);
	
	UserCookie* const user_cookies = reinterpret_cast<UserCookie*>(malloc(n_users*sizeof(UserCookie)));
	
	for (unsigned i = 0;  i < n_users;  ++i){
		read(logfile_fd, user_cookies[i].value, user_cookie_len);
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
		
		unsigned user_indx = n_users;
		const char* cookies_startatspace = get_cookies_startatspace(logline_reqheaders, logline_reqheaders+logline_reqheaders_len);
		if (cookies_startatspace != nullptr){
			cookies_startatspace = find_start_of_u_cookie(cookies_startatspace);
			if (cookies_startatspace[2] != '\r'){
				const char* const user_cookie = cookies_startatspace + 3;
				
				for (user_indx = 0;  user_indx < n_users;  ++user_indx){
					if (compare_secret_path_hashes(user_cookies[user_indx].value, user_cookie)){
						break;
					}
				}
			}
		}
		
		printf("%2d:%2d %.*s user=%u response=%s custom_strview_size %u keepalive=%u\n%.*s\n\n", hour, mins, (int)INET_ADDRSTRLEN, _buf, user_indx, all_response_names+100*response_indx, custom_strview_size, (unsigned)keepalive, (int)logline_reqheaders_len, logline_reqheaders);
	}
	close(logfile_fd);
	return 0;
}
