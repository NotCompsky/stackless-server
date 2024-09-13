# Description

A web server that has:

* serves static content (HTML, CSS, JS, image, video, audio)
* chat rooms
* watch parties (for Youtube or its own content)
* the ability to display wikipedia-like archives ([ugly but readable](https://github.com/NotCompsky/mediawiki))
* very low memory, compute and energy requirements

# Features

* High-performance
  * It can handle ~3000 HTTPS requests per second on my old laptop
* Robust
  * DOS attack only causes requests to be dropped; the server does not crash unless there are so many incoming network connections that the OS crashes or runs out of memory
* Security
  * Everything runs in one process and one thread
    * A far smaller attack surface than alternatives like apache and nginx
  * If an attacker is able to hack this server, if you use the provided AppArmor profile, he won't be able to modify any files
* Some basic features such as rate limiting
* Unusual features such as the ability to block any requests during night time

# Requirements

## Running

Your server computer must have:

* Linux
  * Version must be more recent than around 2017 (for several kernel features)
* At least 1 CPU core
* At least 20MB of memory
* At least 1 network connection

## Initialising

To initialise files for the server - which can be done on a different computer - requires:

* Python 3
* Admin privileges to allow it to use port 443 (HTTPS)

## Compiling

To compile the project:

* C++ compiler
* Unix-style OS (such as Linux or BSD)