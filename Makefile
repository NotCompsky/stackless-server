default: gen_media_metadata gen_hash_functions server

gen_media_metadata:
	python3 gen_media_metadata.py

gen_hash_functions:
	python3 smallest-hash-of-paths.py --dir files/static --dir2 files/large --pack-files-to files/static.pack --write-hpp files/files.hpp --anti-inputs "1/te"

server:
	g++ main.cpp -std=c++23 -o server -lcrypto


smallest-hash-of-paths.so:
	g++ smallest-hash-of-paths.cpp -O3 -o libsmallesthashofpaths.so -shared -fPIC
	export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${PWD}"
	g++ smallest-hash-of-paths.test.cpp -L. -lsmallesthashofpaths -g -osmallest-hash-of-paths.test -fsanitize=address # -fsanitize=address is necessary to avoid some weird ASan error (see https://stackoverflow.com/questions/59853730/asan-issue-with-asan-library-loading)
