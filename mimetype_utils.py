import os
import magic
import json
from pymediainfo import MediaInfo


cached_mimetypes:dict = {}
cached_mimetypes_modified:bool = False


def load_cached_mimetypes(cache_fp:str):
	global cached_mimetypes
	if os.path.exists(cache_fp):
		with open(cache_fp,"r") as f:
			cached_mimetypes = json.load(f)

def save_cached_mimetypes(cache_fp:str):
	with open(cache_fp,"w") as f:
		json.dump(cached_mimetypes, f)

# Compression tips
# AtomicParsley /path/to.mp4 --artwork file.jpg/file.png


def guess_mimetype(fp:str):
	global cached_mimetypes_modified
	
	realfp:str = os.path.realpath(fp)
	last_modified:float = os.stat(realfp).st_mtime
	
	cached_mimetype, cached_mtime = cached_mimetypes.get(realfp,[None,0.0])
	if cached_mtime == last_modified:
		return cached_mimetype
	
	realfp_lower:str = realfp.lower()
	mimetype:str = None
	for (_endwith,_mimetype) in (
		(".mp3","audio/mp3"),
		(".jpeg","image/jpeg"),
		(".jpg","image/jpeg"),
		(".jpg?name=orig","image/jpeg"),
		(".jpg?name=large","image/jpeg"),
		(".jpg?name=medium","image/jpeg"),
		(".jpg?name=small","image/jpeg"),
		(".jpg?name=4096x4096","image/jpeg"),
		(".png","image/png"),
		(".gif","image/gif"),
		(".mp4","video/mp4"),
		(".webm","video/webm"),
		(".ogg","audio/ogg"),
		(".js","application/javascript"),
		(".txt","text/plain"),
		(".html","text/html"),
		(".css","text/css"),
		(".mkv","video/mkv"), # TODO
		(".ico","image/x-icon"),
		(".m4a","audio/m4a"),
		(".aac","audio/aac"),
		(".json","application/json"),
		(".webp","image/webp"),
		(".opus","audio/ogg"),
	):
		if realfp_lower.endswith(_endwith):
			mimetype = _mimetype
			break
	forced_to_guess = (mimetype is None)
	if forced_to_guess:
		mimetype = magic.from_file(realfp, mime=True)
	if mimetype == "video/webm":
		fileinfo = MediaInfo.parse(realfp)
		has_video:bool = False
		for track in fileinfo.tracks:
			if track.track_type == "Video":
				has_video = True
				break
		if not has_video:
			mimetype = "audio/webm"
	if mimetype in ("video/mkv","video/x-matroska"):
		mimetype = "video/webm"
	if forced_to_guess:
		print(f"Forced to guess mimetype for file: {realfp} ?=? {mimetype}")
	cached_mimetypes[realfp] = [mimetype, last_modified]
	cached_mimetypes_modified = True
	return mimetype


def standardise_mimetype(mimetype:str, fp:str):
	realfp:str = os.path.realpath(fp)
	
	if mimetype.startswith("text/html"):
		if realfp.endswith(".js"): # WHY THE FUCK ARE SOME JAVASCRIPT FILES GUESSED AS text/html??? RETARDS!!!
			mimetype = "text/plain"
		else:
			mimetype = "text/html"
	elif mimetype.startswith("text/x-"):
		mimetype = "text/plain"
	elif mimetype == "video/x-matroska":
		mimetype = "video/webm"
	elif mimetype.startswith("image/vnd.microsoft.icon"):
		mimetype = "image/x-icon"
	elif mimetype == "audio/x-hx-aac-adts":
		mimetype = "audio/aac"
	elif mimetype == "audio/x-m4a":
		mimetype = "audio/m4a"
	elif mimetype == "application/octet-stream":
		print("Forced to guess mimetype from path:", realfp)
		if realfp.endswith(".mp3"):
			print(" Guessed mp3")
			mimetype = "audio/mp3"
	if mimetype not in ("audio/aac","image/png","image/jpeg","image/webp","video/mp4","video/webm","text/html","text/plain","application/json","audio/mpeg","audio/webm","audio/m4a","audio/mp3","audio/ogg","image/x-icon"):
		raise ValueError(f"Bad mimetype \"{mimetype}\" for {fp} ({realfp})")
	if (mimetype == "text/plain") and realfp.endswith(".js"):
		mimetype = "application/javascript"
	elif (mimetype == "text/plain") and realfp.endswith(".json"):
		mimetype = "application/json"
	elif (mimetype == "text/plain") and realfp.endswith(".css"):
		mimetype = "text/css"
	return mimetype
