default:
	python3 smallest-hash-of-paths.py --dir files/static --dir2 files/large --pack-files-to files/static.pack --write-hpp files/files.hpp --anti-inputs " /1/"
	g++ main.cpp -std=c++23 -o server -lcrypto

server:
	g++ main.cpp -std=c++23 -o server -lcrypto

smallest-hash-of-paths.so:
	g++ smallest-hash-of-paths.cpp -O3 -o smallest-hash-of-paths.so -shared -fPIC

