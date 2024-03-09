#!/usr/bin/env python3


from tagem import TagemDB
from collections import Counter
import json
import magic
import base64
import os
from mimetype_utils import guess_mimetype


tagemdb = TagemDB(user_id=4)


def get_files_directly_tagged(tagid:int):
	return [x[0] for x in tagemdb.qry(f"SELECT file FROM file2tag WHERE tag={tagid}")]

tagem_fileids:list = list(set(get_files_directly_tagged(237870)+get_files_directly_tagged(65571)))
tagem_tagids:dict = {
	749:"Instrumental Music",
	968:"Roman Empire",
	1219:"Progress Music Video",
	2841:"Inspiring Music",
	3032:"TV Show Intro",
	# 65574:"notcompsky.github.io",
	237870:"Set B",
	1221:"Cinematic Music Video",
	1202:"SciFi Music Video",
	2607:"English Language Song",
	1010:"Two Steps From Hell",
	1309:"History Music Video",
	81956:"E-Style Orchestral Cover",
	1254:"Vangelis",
	2995:"E-Style Music",
	744:"Sad Music",
	991:"Film Score",
	934:"Historic Event",
	66172:"Artistic Music Video",
	21346:"Medieval Instrumental Cover",
	235314:"Anti-War Song",
	87330:"Song Video",
	237178:"Mankind Music Video",
	235305:"Happy Song Video",
	2568:"Romantic Song",
	5507:"Yugoslavian Music Video",
	237781:"German Pop Song",
	1987:"Sad Song",
	236829:"Sad Music Video",
}

all_tags_associated_with_chosen_files:list = []
fileid2associatedtags:list = []
i:int = 0
for fileid in tagem_fileids:
	fp:str = tagemdb.qry("SELECT CONCAT(d.full_path,f.name) FROM file f JOIN dir d ON d.id=f.dir WHERE f.id="+str(fileid))[0][0]
	if not os.path.isfile(fp):
		fp = None
	for (_fp,) in tagemdb.qry("SELECT CONCAT(d.full_path,f.name) FROM file_backup f JOIN dir d ON d.id=f.dir WHERE f.file="+str(fileid)):
		if os.path.isfile(_fp):
			fp = _fp.decode()
			break
	if fp is None:
		print(f"Can't find file for id {fileid}")
		continue
	
	tagids:list = [x[0] for x in tagemdb.qry(f"SELECT tag FROM file2tag WHERE file={fileid}")]
	all_tags_associated_with_chosen_files += tagids
	thumb_fp:str = f"/media/vangelic/DATA/tagem_thumbnails__subdir/tagem_thumbnails/{fileid}"
	thumb_str:str = "data:,"
	if os.path.isfile(thumb_fp):
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
			raise ValueError("mimetype is "+mimetype)
		thumb_str = "data:"+mimetype+","+base64.encodebytes(contents).replace(b"\n",b"").decode()
	fileid2associatedtags.append([i,thumb_str,tagids])
	os.remove(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}")
	os.symlink(fp, f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}")
	i += 1
while os.path.exists(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}"):
	os.remove(f"/home/vangelic/repos/compsky/static-and-chat-server/files/large/{i:04d}")
	i += 1
all_tags_associated_with_chosen_files = [x for x in all_tags_associated_with_chosen_files if x not in tagem_tagids]

for tagid, count in Counter(all_tags_associated_with_chosen_files).most_common(50):
	tagname:str = tagemdb.qry(f"SELECT name FROM tag WHERE id={tagid}")[0][0].decode()
	print(f"Excluded tag associated with {count} chosen files: {tagid} {tagname}")

with open("/home/vangelic/repos/compsky/static-and-chat-server/files/static/all_.json","w") as f:
	json.dump([tagem_tagids,fileid2associatedtags],f,indent=None,separators=(",",":"))
