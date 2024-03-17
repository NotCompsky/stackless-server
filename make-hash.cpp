#include <random>
#include <unistd.h>

static std::random_device rd;

constexpr unsigned hash_length = 32;
constexpr char base64_chars[64] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'};

int main(){
	char hash[1 + 4*hash_length];
	hash[0] = '{';
	for (unsigned i = 0;  i < (hash_length/4);  ++i){
		uint32_t rand_u24bits = rd() & 0xffffff;
		for (unsigned j = 0;  j < 4;  ++j){
			hash[16*i + 4*j + 1] = '\'';
			hash[16*i + 4*j + 2] = base64_chars[rand_u24bits & 63];
			hash[16*i + 4*j + 3] = '\'';
			hash[16*i + 4*j + 4] = ',';
			rand_u24bits >>= 6;
		}
	}
	hash[4*hash_length] = '}';
	write(1,  hash,  1 + 4*hash_length);
}
