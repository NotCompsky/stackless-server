#include "logline.hpp"
#include <unistd.h> // for write
#include <fcntl.h> // for O_NOATIME etc
#include <cstdio> // for printf


constexpr static const char* all_response_names[11] = {
	"_r::not_found",
	"_r::server_error",
	"_r::wrong_hostname",
	"_r::suspected_robot",
	"_r::user_login_url_already_used",
	"_r::not_logged_in__dont_set_fuck_header",
	"_r::not_logged_in__set_fuck_header",
	"_r::cant_register_user_due_to_lack_of_fuck_cookie",
	"_r::wiki_page_not_found",
	"_r::wiki_page_error",
	"using_server_buf",
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
	
	while(true){
		if (read(logfile_fd, logline, logline_sz) != logline_sz){
			write(1, "Reached end of file or unexpected end\n", 38);
			break;
		}
		const int hour = reinterpret_cast<int*>(logline)[0];
		const int mins = reinterpret_cast<int*>(logline)[1];
		const unsigned custom_strview_size = reinterpret_cast<unsigned*>(logline+logline_datetime_sz)[0];
		const unsigned response_indx = logline[logline_respindxindx];
		const bool keepalive = logline[logline_keepaliveindx];
		const char* const logline_reqheaders = logline + logline_reqheadersindx;
		
		printf("%2d:%2d: response=%s, custom_strview_size %u, keepalive=%u\n%.1024s\n\n", hour, mins, all_response_names[response_indx], custom_strview_size, (unsigned)keepalive, logline_reqheaders);
	}
	close(logfile_fd);
	return 0;
}
