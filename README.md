# TOC

* [Description](#Description)
* [Features](#Features)
* [Screenshots](#Screenshots)
* [Requirements](#Requirements)
* [Compiling](#Compiling)

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

# Screenshots

![screenshot1](https://github.com/user-attachments/assets/dfc66f61-d257-4305-bd79-3a7b942e6d6b)
![screenshot chat1](https://github.com/user-attachments/assets/b5f099f4-27a9-4987-868e-75cb58b9300a)
![screenshot chat2](https://github.com/user-attachments/assets/7489e10c-ad72-4674-a37c-e9a02fa2fea1)

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

# Compiling

To compile the project:

* C++ compiler
* Unix-style OS (such as Linux or BSD)