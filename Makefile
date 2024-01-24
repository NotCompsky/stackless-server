default:
	python3 smallest-hash-of-paths.py --dir files/static --anti-dir files/large --pack-files-to files/static.pack --write-hpp files/files.hpp
	g++ main.cpp -std=c++23 -o server

server:
	g++ main.cpp -std=c++23 -o server
