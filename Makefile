default: gen_media_metadata gen_hash_functions largefile_permissions server

user-cookie-hash-maker:
	c++ make-hash.cpp -std=c++2a -o make-hash -Os

make-user:
	# TODO: look at useradd's --selinux-user option, and SELinux's per-user restrictions (http://www.lurking-grue.org/writingselinuxpolicyHOWTO.html#userpol5.1)
	sudo useradd --expiredate '' --inactive -1 --user-group --no-create-home --system --shell /bin/false staticserver
	sudo passwd staticserver

gen_media_metadata:
	python3 gen_media_metadata.py

gen_hash_functions:
	sudo rm /media/vangelic/DATA/tmp/static_webserver.pack || dummy=1
	python3 smallest-hash-of-paths.py --dir files/static --dir2 files/large --pack-files-to /media/vangelic/DATA/tmp/static_webserver.pack --write-hpp files/files.hpp --anti-inputs "1/te" --anti-inputs "user" --anti-inputs "w00/" --anti-inputs "d00/"
	sudo chown staticserver:staticserver /media/vangelic/DATA/tmp/static_webserver.pack
	sudo chmod 400 /media/vangelic/DATA/tmp/static_webserver.pack
	# NOTE: shouldn't be any need to do this, but apparently normal files refuse to be read by staticserver user without it. But benefit is that files aren't modified accidentally by other users.

largefile_permissions:
	while IFS= read -r fp; do \
		sudo chown staticserver:staticserver "$$fp"; \
		sudo chmod 444 "$$fp"; \
	done < files/all_large_files.txt

apparmor:
	sudo apparmor_parser -r "${PWD}/profile.apparmor"

server:
	c++ main.cpp -std=c++2b -O3 -march=native -o server -s -lcrypto -lssl -lbz2 -lz
	sudo chown staticserver:staticserver server
	sudo chmod 500 server
	sudo setcap CAP_NET_BIND_SERVICE=+eip server

run-server:
	sudo -u staticserver -- ./server 443 8103208574956883562 "ECDHE-ECDSA-AES256-GCM-SHA384" 06-01


smallest-hash-of-paths.so:
	g++ smallest-hash-of-paths.cpp -O3 -o libsmallesthashofpaths.so -shared -fPIC
	export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${PWD}"
	g++ smallest-hash-of-paths.test.cpp -L. -lsmallesthashofpaths -g -osmallest-hash-of-paths.test -fsanitize=address # -fsanitize=address is necessary to avoid some weird ASan error (see https://stackoverflow.com/questions/59853730/asan-issue-with-asan-library-loading)
