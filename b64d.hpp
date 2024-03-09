#pragma once


constexpr char base64_chars[64] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'};
inline
void base64_encode(unsigned char* input_arr,  char* output_arr,  const unsigned input_arr_len){
	int i = 0;
	int j = 0;
	unsigned char char_array_4[4];

	while (i < input_arr_len){
		const char a = input_arr[i++];
		const char b = (i==input_arr_len) ? 0 : input_arr[i++];
		const char c = (i==input_arr_len) ? 0 : input_arr[i++];
		
		char_array_4[0] = ((a & 0xfc) >> 2);
		char_array_4[1] = ((a & 0x03) << 4) + ((b & 0xf0) >> 4);
		char_array_4[2] = ((b & 0x0f) << 2) + ((c & 0xc0) >> 6);
		char_array_4[3] = ((c & 0x3f));
		for (int k = 0; k < 4; k++){
			output_arr[j++] = base64_chars[char_array_4[k]];
		}
	}
	while(j%4 != 0)
		output_arr[j++] = '=';
}
inline
void base64_encode__length20(unsigned char* input_arr,  char* output_arr){
	int i = 0;
	int j = 0;
	unsigned char char_array_4[4];

	while (i < 20){
		const char a = input_arr[i++];
		const char b = input_arr[i++];
		const char c = (i==20) ? 0 : input_arr[i++];
		
		char_array_4[0] = ((a & 0xfc) >> 2);
		char_array_4[1] = ((a & 0x03) << 4) + ((b & 0xf0) >> 4);
		char_array_4[2] = ((b & 0x0f) << 2) + ((c & 0xc0) >> 6);
		char_array_4[3] = ((c & 0x3f));
		for (int k = 0; k < 4; k++){
			output_arr[j++] = base64_chars[char_array_4[k]];
		}
	}
	output_arr[27] = '='; // Because input length 20 === -1 modulo 3
}
