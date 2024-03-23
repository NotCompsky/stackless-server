#!/usr/bin/env python3


from tagem import TagemDB
from collections import Counter
import json
import magic
import base64
import os
import mimetype_utils
import re


mimetype_utils.load_cached_mimetypes("cached_mimetypes.json")


subtitle_langs:tuple = ("en","de")
FAKETAG_hassubtitles:int = 1234567890
tagemdb = TagemDB(user_id=4)


def get_files_directly_tagged(tagid:int):
	return [x[0] for x in tagemdb.qry(f"SELECT file FROM file2tag WHERE tag={tagid}")]

def get_thumb_fp(fileid:int, tagids:list):
	fp:str = fileid2alternative_thumbnail.get(fileid)
	if fp is None:
		if fileid not in (
			1691872,7321,7315,7313,7309,7308, # Civilization 6
			7059, # Civilization 5
			2199868, # Ride of the Valkyries
			
			27221,1726479, # Vikings (use tag thumbnail)
			
			1526,1527,1528,27240,1532,1534,1720, # Santiano
			
			1973848, # Holding out for a hero (orchestral cover)
			1973838, # Firework (orchestral cover)
			
			1746,1919276, # Spanish anthem
			7177,7176,7175,7174,7173,7172,7171,7170,7169,7168,7167,7166,7165,7164,7163,7162,7161,7160,7159,7158,7157,7156,7155,7154,7153,7152,7151,7150,7149,7148,7147,7146,7145,7144,7143,7142, # Endless Space soundtrack
			80899,1719645, # Medieval covers
			3536811, # Retrowave (worse quality version of tag's cover image)
			7190, # Roman Empire (worse quality version of tag's cover image)
			3559968, # French socialist (same as tag's cover image)
			7275,6309, # French socialist song from Les Miserables
		):
			fp_maybe:str = f"/media/vangelic/DATA/tagem_thumbnails__subdir/tagem_thumbnails/{fileid}"
			if os.path.exists(fp_maybe):
				if os.stat(fp_maybe).st_size != 169: # Youtube "Deleted" thumbnail
					fp = fp_maybe
					if not os.path.islink(f"/media/vangelic/DATA/static-server-files/links_to_used_tagem_thumbs/{fileid}"):
						os.symlink(fp, f"/media/vangelic/DATA/static-server-files/links_to_used_tagem_thumbs/{fileid}")
	else:
		del fileid2alternative_thumbnail[fileid]
	return fp

tagem_fileids:list = list(set(get_files_directly_tagged(237870)+get_files_directly_tagged(65574)+get_files_directly_tagged(239895)+get_files_directly_tagged(239896)+get_files_directly_tagged(239897)+get_files_directly_tagged(239898)+get_files_directly_tagged(239900)+get_files_directly_tagged(116236)+get_files_directly_tagged(3020)+get_files_directly_tagged(6810)+get_files_directly_tagged(239922)+get_files_directly_tagged(236111)+get_files_directly_tagged(239901)+get_files_directly_tagged(239902)+get_files_directly_tagged(239926)+get_files_directly_tagged(239923)+get_files_directly_tagged(240024)+[2200548,4900172,2199768,1985,2203575,2199933,4829561]))
# TODO: 55 Days in Peking: (sure there are other languages too) /media/vangelic/DATA/media/music/by-source/KarlSternau/Botho- Lucas Chor - 55 Tage in Peking _55 Days in Peking [Colonial Song 1900][+ English Translation]-m-GVAGyBZB4.m4a
tagem_fileid2skipfirstseconds:dict = {
	"that fallout video intro":16.108312,
}
introduction_to_each_category:dict = {
	969:1689587, # Roman Empire -> TV intro
	116236:3537631, # Retrowave
	1254:47763, # Vangelis -> conquest of paradise film scene
	104184:1918, # Religious Imagery -> conquest of paradise music video
	1610:2199847, # Happy Music -> Harald Foss - Eirik Jarl
	236111:4829577, # KPop -> techno guitar orchestra
	237005:4825269, # Vietnam War -> Ballad of Ho Chi Minh war video
	2607:1930, # English Language song -> Scotland the Brave
	1987:5832, # Sad Song -> Burning Bridges
	66172:2064058, # Artistic Music video -> Freedom Fighters (Orwellian)
	2539:1743834, # TV Theme Music -> The Expanse
	3020:7169, # Orchestra1 (i.e. Endless Space) -> Riftborn Prologue
	5507:27228, # Yugoslavian Music Video
	1238:1955216, # Vikings -> aurora  (or maybe 1955 for that violin lady)
	239917:2199768, # Historic Event -> Stanford Bridge
	2550:1726594, # Bear McCreary
	2568:2557, # Romantic -> Rowan Atkinson examins Loxie
	1292:3091, # Funny -> Rat hygiene
	40844:25446, # CovidPunk -> street disinfecting
	10057:2652, # French Language Song -> Demain Nous Appartient
	3031:2200324, # European Language Song -> Trava u Doma
	2494:1844, # German Language Song -> Alexa Fesa - Wir Sind Hier
	743:1845, # Traditional Music -> Greensleeves orchestra
	239898:1955972, # Weird song -> moo
	1219:1847, # Progress Music Video -> Baba Yetu
	239922:4895481, # Dictator's Playlist -> Angry German man (or 1936==European Superstate)
	235314:4829650, # Anti-War Song -> Ich Bin Soldat
}
tagem_tagids:dict = { # NOTE: As of Python 3.7, regular dicts are guaranteed to be ordered
	# 65574:"notcompsky.github.io Prepopulated",
	237870:"Bit of everything playlist",
	749:"Instrumental Music",
	1054:"Piano", # Piano Music; NOTE there is also a "Safe Piano Music Playlist",tagid==239901
	116236:"Retrowave", # Sovietwave
	239896:"Film Scores",# SAFE Film Scores
	239897:"Orchestralishy Style", # Safe Epic Music
	3020:"Orchestra1", # Endless Space Soundtrack
	3002:"Orchestra2", # Sword of the Stars Soundtrack
	3065:"Orchestra3", # Civilization VI Soundtrack
	2566:"LotR", # Lord of the Rings Soundtrack
	
	# 1309:"History Music Video",
	81956:"E-Style Orchestral Cover",
	2995:"E-Style Music",
	744:"Sad Music",
	991:"Film Score",
	
	238488:"Bittersweet Song",
	1610:"Happy Music",
	#2498:"Happy Song",
	2568:"Romantic Song",
	1987:"Sad Song",
	2841:"Inspiring Music",# TODO: remove?
	
	239903:"Misc Electronic Musics", # Leftover Electronic Musics
	21346:"Medieval Instrumental Cover",
	
	2607:"English Language Song",
	3031:"European Language Song",
	2494:"German Language Song",
	2652:"French Language Song",
	240031:"Spain",
	5557:"Latin Song",
	5507:"Yugoslavian Music Video",
	237781:"German Pop Song",
	
	235314:"Anti-War Song",
	237178:"Anti-Alien Song",
	239922:"Dictator's Playlist", # Safe Authority Music Playlist
	
	87330:"Song Video",
	# 235305:"Happy Song Video",
	2539:"TV Theme Music",
	#3032:"TV Show Intro",
	236829:"Sad Music Video",
	66172:"Artistic Music Video",
	1219:"Brainwashing", # Progress Music Video",
	1221:"Cinematic Music Video",
	1202:"SciFi Music Video",
	239923:"Apocalypse",
	
	239917:"Historic Event",
	969:"Roman Empire",
	237005:"Vietnam War Song",
	
	743:"Traditional Music",
	239900:"Traditional Song", # Safe Traditional Song Playlist
	5585:"European Culture",
	1238:"Vikings",
	239938:"Sailing",
	
	1254:"Vangelis",
	115438:"Harald Foss",
	9438:"Christopher Tin",
	1010:"Two Steps",
	# 6810:"Santiano",
	
	2550:"Bear McCreary",
	1210:"Alexa Fesa",
	72541:"Karl Sternau",
	237310:"Nationhood", # Fatherland Playlist,
	
	81788:"Covid Culture",
	40844:"CovidPunk",
	1292:"Funny",
	5131:"Meymeys",
	104184:"Religious Imagery",
	#378:"Animal",
	236111:"KPop",
	FAKETAG_hassubtitles:"Has subtitles",
	239898:"Weird songs or songs with weird backstories",
	239895:"CRINGE ALERT! DON'T WATCH!",
	240024:"Cute",
	239926:"Reminds me of a dead person :(",
}
remove_a_if_already_tagged_b:tuple = (
	(743,239900), # Traditional (Music,Song)
	(744,1987), # Sad (Music,Song)
)
tagem_tagids__ordered_keys:list = [key for key in tagem_tagids]
tagem_tagids__as_indices:list = [tagem_tagids[key] for key in tagem_tagids__ordered_keys]

tagem_tagids__ordered_keys__including_hidden_tagids:list = tagem_tagids__ordered_keys+[
	217409, # French Socialist Song
]

total_filesize:int = 0
filesizes:list = []

all_filepaths_added_to_server:list = []

def fp_of_file_added_to_server(fileid:int, fp:str):
	global total_filesize
	fsz:int = os.stat(fp).st_size
	filesizes.append((fsz,fileid,fp))
	total_filesize += fsz
	all_filepaths_added_to_server.append(fp)

def set_symlink(linkat:str, linkto:str):
	must_add_link:bool = True
	if os.path.islink(linkat):
		if os.path.realpath(linkat) == os.path.realpath(linkto):
			must_add_link = False
		else:
			os.remove(linkat)
	if must_add_link:
		os.symlink(linkto, linkat)

all_tags_associated_with_chosen_files:list = []
fileid2associatedtags:list = []
i:int = 0
for fp in (
	,
):
	set_symlink(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}", fp)
	fp_of_file_added_to_server(None, fp)
	i += 1
captcha_video_fps:list = []
captcha_questions:list = []
process_captcha_clips_and_questions(i, captcha_video_fps, captcha_questions)
for fp in captcha_video_fps:
	set_symlink(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}", fp)
	fp_of_file_added_to_server(None, fp)
	i += 1
tagem_fileid2indx:dict = {}
fileid2alternative_thumbnail:dict = {}
def fileid2alternative_thumbnail__process(dirpath:str):
	for fname in os.listdir(dirpath):
		fp:str = f"{dirpath}/{fname}"
		if os.path.isdir(fp):
			fileid2alternative_thumbnail__process(fp)
		else:
			m = re.search("^([0-9]+)(?:$|_.*)", fname)
			if m is None:
				print(f"ERROR: Bad thumbnail: {fp}")
			else:
				fileid2alternative_thumbnail[int(m.group(1))] = fp
fileid2alternative_thumbnail__process("/media/vangelic/DATA/static-server-files/audio-thumbs")
print(f"Registered {len(fileid2alternative_thumbnail)} alternative thumbnails")
tag2thumbnail:dict = {
	0:"/DEFAULT.jpg",
	240031:"/spain.jpg",
	116236:"/retr.webp", # Retrowave
	969:"/rome.webp",
	239922:"/poli.webp", # police state
	2568:"/roma.jpg", # romantic
	21346:"/medi.jpg", # medieval
	1238:"/vikings.jpg",
	239938:"/sailing.webp",
	217409:"/french_socialist.jpg",
	2652:"/fren.jpg",
	2494:"/deut.jpg",
	236111:"/kpop.jpg",
}
for key,val in tag2thumbnail.items():
	mimetype:str = "image/jpeg" if val.endswith(".jpg") else ("image/webp" if val.endswith(".webp") else "image/png")
	with open(f"/media/vangelic/DATA/static-server-files/tag-thumbs{val}","rb") as f:
		tag2thumbnail[key] = "data:"+mimetype+";base64,"+base64.encodebytes(f.read()).replace(b"\n",b"").decode()
fileid2subtitles:dict = {}
for fname in os.listdir("/media/vangelic/DATA/static-server-files/audio-subtitles"):
	if fname.startswith("test."):
		continue
	fp:str = f"/media/vangelic/DATA/static-server-files/audio-subtitles/{fname}"
	m = re.search("^([0-9]+)[.](.*)$", fname)
	fileid:int = int(m.group(1))
	lang:str = m.group(2)
	if lang not in subtitle_langs:
		print(f"WARNING: Skipping unrecognised subtitle language: {lang}")
		continue
	with open(fp,"rb") as f:
		subtitle:list = [subtitle_langs.index(lang),"data:text/vtt;base64,"+base64.encodebytes(f.read()).replace(b"\n",b"").decode()]
		if fileid in fileid2subtitles:
			fileid2subtitles[fileid].append(subtitle)
		else:
			fileid2subtitles[fileid] = [subtitle]
for fileid in tagem_fileids:
	if fileid == 7155: # Endless Space 2 hour compilation
		continue
	
	fp:str = None
	if os.path.exists(f"/media/vangelic/DATA/static-server-files/compressed_videos/by-tagem-fileid/{fileid}"):
		fp = f"/media/vangelic/DATA/static-server-files/compressed_videos/by-tagem-fileid/{fileid}"
	if fp is None:
		fp = tagemdb.qry("SELECT CONCAT(d.full_path,f.name) FROM file f JOIN dir d ON d.id=f.dir WHERE f.id="+str(fileid))[0][0]
	if type(fp) is bytes:
		fp = fp.decode()
	if not os.path.isfile(fp):
		fp = None
	if fp is None:
		for (_fp,) in tagemdb.qry("SELECT CONCAT(d.full_path,f.name) FROM file_backup f JOIN dir d ON d.id=f.dir WHERE f.file="+str(fileid)):
			if os.path.isfile(_fp):
				fp = _fp.decode()
				break
	if fp is None:
		print(f"Can't find file for id {fileid}")
		continue
	
	tagids:list = [x[0] for x in tagemdb.qry(f"SELECT t2pt.parent FROM file2tag f2t JOIN tag2parent_tree t2pt ON t2pt.id=f2t.tag WHERE f2t.file={fileid}")]
	# tagids = [116236 if (x==229017) else x for x in tagids] # Include Fashwave in Retrowave
	
	if fileid == 4902000:
		tagids = [239926]
	if fileid == 7678:
		tagids.append(1219)
	if fileid == 4900220: # Field of poppies piano
		tagids.append(235314) # Anti-War Song
	if fileid == 6722: # Dear White People
		tagids = [239898] # weird songs
	
	all_tags_associated_with_chosen_files += tagids
	thumb_fp:str = get_thumb_fp(fileid, tagids)
	
	thumb_str:str = 0
	if fileid == 1919273: # German anthem -> deutsch
		thumb_str = tagem_tagids__ordered_keys__including_hidden_tagids.index(2494)
	elif fileid == 1919275: # French anthem -> fren
		thumb_str = tagem_tagids__ordered_keys__including_hidden_tagids.index(2652)
	elif thumb_fp is None:
		for tagid in tag2thumbnail:
			if fileid == 1534:
				print(tagid, tagid in tagids)
			if tagid == 0:
				continue
			if tagid in tagids:
				thumb_str = tagem_tagids__ordered_keys__including_hidden_tagids.index(tagid)
				break
	else:
		contents:bytes = None
		with open(thumb_fp,"rb") as f:
			contents = f.read()
		mimetype:str = None
		if thumb_fp.endswith(".jpg"):
			mimetype = "image/jpeg"
		elif thumb_fp.endswith(".webp"):
			mimetype = "image/webp"
		elif thumb_fp.endswith(".png"):
			mimetype = "image/png"
		else:
			mimetype = magic.from_buffer(contents, mime=True)
		if mimetype not in ("image/jpeg","image/webp","image/png"):
			raise ValueError(f"ERROR: mimetype is {mimetype} for {thumb_fp}")
		thumb_str = "data:"+mimetype+";base64,"+base64.encodebytes(contents).replace(b"\n",b"").decode()
	
	file_mimetype:str = mimetype_utils.guess_mimetype(fp)
	file_mimetype = mimetype_utils.standardise_mimetype(file_mimetype, fp)
	
	filtered_tagids:list = [x for x in tagids]
	for a,b in remove_a_if_already_tagged_b:
		if (b in filtered_tagids) and (a in filtered_tagids):
			filtered_tagids = [x for x in filtered_tagids if x!=a]
	
	file_visible_tagids:list = list(set([tagem_tagids__ordered_keys.index(x) for x in filtered_tagids if x in tagem_tagids]))
	
	subtitles:list = fileid2subtitles.get(fileid)
	if subtitles is None:
		subtitles = []
	else:
		file_visible_tagids.append(tagem_tagids__ordered_keys.index(FAKETAG_hassubtitles))
	
	if len(file_visible_tagids) == 0:
		tagnames:list = [x[0].decode() for x in tagemdb.qry(f"SELECT name FROM tag t JOIN file2tag f2t ON f2t.tag=t.id WHERE f2t.file={fileid}")]
		print(f"WARNING: No visible tagids for file {fileid} {fp} {json.dumps(tagnames)}")
		continue
	
	tagem_fileid2indx[fileid] = i
	fileid2associatedtags.append([i,thumb_str,file_visible_tagids,file_mimetype,subtitles])
	set_symlink(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}", fp)
	fp_of_file_added_to_server(fileid, fp)
	i += 1
tag2thumbnail = {(0 if (key==0) else tagem_tagids__ordered_keys__including_hidden_tagids.index(key)):val for key,val in tag2thumbnail.items()}
for (name,description,filepaths) in (
	,
):
	tagindx:int = None
	thumb_str:str = 0
	if name in tagem_tagids__as_indices:
		tagindx = tagem_tagids__as_indices.index(name)
		if tagindx in tag2thumbnail:
			thumb_str = tag2thumbnail[tagindx]
	else:
		tagindx = len(tagem_tagids__as_indices)
		tagem_tagids__as_indices.append(name)
	for fp in filepaths:
		file_mimetype:str = mimetype_utils.guess_mimetype(fp)
		file_mimetype = mimetype_utils.standardise_mimetype(file_mimetype, fp)
		subtitles:list = []
		fileid2associatedtags.append([i,thumb_str,[tagindx],file_mimetype,subtitles])
		set_symlink(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}", fp)
		fp_of_file_added_to_server(None, fp)
		i += 1
while os.path.exists(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}"):
	os.remove(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}")
	i += 1
all_tags_associated_with_chosen_files = [x for x in all_tags_associated_with_chosen_files if x not in tagem_tagids]

for tagid, count in Counter(all_tags_associated_with_chosen_files).most_common(50):
	tagname:str = tagemdb.qry(f"SELECT name FROM tag WHERE id={tagid}")[0][0].decode()
	print(f"Excluded tag associated with {count} chosen files: {tagid} {tagname}")


prev_all_filepaths_added_to_server:list = None
with open("prev_all_filepaths_added_to_server.json","r") as f:
	prev_all_filepaths_added_to_server = json.load(f)

browser_cache_is_different:bool = False
for i in range(len(all_filepaths_added_to_server)):
	if i == len(prev_all_filepaths_added_to_server):
		break
	if all_filepaths_added_to_server[i] != prev_all_filepaths_added_to_server[i]:
		browser_cache_is_different = True
		break

browser_cache_version:int = 0
with open("browser_cache_version.txt","r") as f:
	browser_cache_version = int(f.read())

if browser_cache_is_different:
	browser_cache_version += 1
	with open("browser_cache_version.txt","w") as f:
		f.write(str(browser_cache_version))


introduction_to_each_category = {tagem_tagids__ordered_keys.index(tagid):tagem_fileid2indx[fileid] for tagid,fileid in introduction_to_each_category.items() if (fileid in tagem_fileid2indx) and (tagid in tagem_tagids__ordered_keys)}
with open("/home/vangelic/repos/compsky/static-and-chat-server/files/static/all_.json","w") as f:
	json.dump([tagem_tagids__as_indices,fileid2associatedtags,tag2thumbnail,introduction_to_each_category],f,indent=None,separators=(",",":"))

for fsz,fileid,fp in sorted(filesizes,key=lambda x:x[0],reverse=True)[:50]:
	print(f"{fsz}\t{fileid}\t{fp}")
print(f"Total size of all files used by server: {total_filesize//(1024*1024)} MiB")
for fileid in fileid2alternative_thumbnail:
	print(f"ERROR: Failed to use alternative thumbnail for {fileid}")

def apparmor_escstr(s:str):
	return s.replace("\\","\\\\").replace("\"","\\\"").replace("[","\\[")

if browser_cache_is_different or (len(all_filepaths_added_to_server) != len(prev_all_filepaths_added_to_server)):
	with open("prev_all_filepaths_added_to_server.json.new","w") as f:
		json.dump(all_filepaths_added_to_server, f)
	os.rename("prev_all_filepaths_added_to_server.json.new","prev_all_filepaths_added_to_server.json")
	
	with open("profile.largefiles.apparmor","w") as f:
		for fp in all_filepaths_added_to_server:
			f.write(f'"{apparmor_escstr(fp)}" r,\n')

if True:
	tagem_tagnames:list = [tagem_tagids[tagid] for tagid in tagem_tagids]
	def subfn(m):
		return m.group(1) + str(1+tagem_tagnames.index(m.group(3))) + m.group(2)
	mainpage_contents:str = None
	with open("files/static/index.html","r") as f:
		mainpage_contents = f.read()
	mainpage_contents = re.sub('(<a href="rand.html#\\[)[0-9]+(,[^"]+\\]"><img .*alt="([^"]+)".*</img></a>)', subfn, mainpage_contents)
	with open("files/static/index.html","w") as f:
		f.write(mainpage_contents)

mimetype_utils.save_cached_mimetypes("cached_mimetypes.json")
