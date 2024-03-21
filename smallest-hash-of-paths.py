#!/usr/bin/env python3

# inspired by https://orlp.net/blog/worlds-smallest-hash-table/

import zlib
from struct import unpack
import ctypes
import numpy as np
import mimetype_utils
from html import escape as html_escape
from sha256integrity import get_sha256_hash_in_b64_form
from headers import make_static_headers
import compression_utils


mimetype_utils.load_cached_mimetypes("cached_mimetypes.json")


clib = ctypes.CDLL("/home/vangelic/repos/compsky/static-and-chat-server/libsmallesthashofpaths.so")
c_finding_0xedc72f12 = clib.finding_0xedc72f12
c_finding_0xedc72f12.argtypes = [ctypes.POINTER(ctypes.c_uint), ctypes.c_uint, ctypes.c_uint]
c_finding_0xedc72f12.restype = ctypes.c_uint
c_finding_0xedc72f12_w_avoids = clib.finding_0xedc72f12_w_avoids
c_finding_0xedc72f12_w_avoids.argtypes = [ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint), ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
c_finding_0xedc72f12_w_avoids.restype = ctypes.c_uint

def dblqt(s:str):
	return s.replace('\\','\\\\').replace('"','\\"')



'''
def str2cliststr(s:str):
	r:str = "{"
	for c in s:
		if (c == "'") or (c == "\\"):
			c = "\\" + c
		r += "'" + c + "',"
	return r[:-1] + "}"
'''

def get_int_array_from_numpy_array(array):
	contig_array = np.ascontiguousarray(array, dtype=np.uint32)
	return contig_array.ctypes.data_as(ctypes.POINTER(ctypes.c_uint))

def maybe_get_cached(cached_fp:str, inputs_sorted:list):
	if os.path.exists(cached_fp):
		cached_result:int = None
		cached_inputs:list = None
		with open(cached_fp,"r") as f:
			cached_result, cached_inputs = json.load(f)
		if len(cached_inputs) == len(inputs_sorted):
			matched:bool = True
			for i in range(len(inputs_sorted)):
				matched &= (inputs_sorted[i] == cached_inputs[i])
			if matched:
				return cached_result
	return 0

def finding_0xedc72f12(idstr:str, inputs:list, shiftby:int):
	cached_fp:str = f"finding_0xedc72f12.{idstr}.json"
	inputs_sorted:list = sorted(inputs)
	result:int = maybe_get_cached(cached_fp, inputs_sorted)
	if result != 0:
		return result
	result = c_finding_0xedc72f12(
		get_int_array_from_numpy_array(np.array(inputs, dtype=np.uint32)),
		len(inputs),
		shiftby,
		100000000
	)
	if result != 0:
		with open(cached_fp,"w") as f:
			json.dump([result, inputs_sorted],  f)
	return result

def finding_0xedc72f12_w_avoids(idstr:str, inputs:list, anti_inputs:list, shiftby:int):
	cached_fp:str = f"finding_0xedc72f12_w_avoids.{idstr}.json"
	inputs_sorted:list = sorted(inputs) + [None] + sorted(anti_inputs)
	result:int = maybe_get_cached(cached_fp, inputs_sorted)
	if result != 0:
		return result
	for x in anti_inputs:
		if x in inputs:
			raise ValueError(f"{x} in both `inputs` and `anti_inputs`")
	result = c_finding_0xedc72f12_w_avoids(
		get_int_array_from_numpy_array(np.array(inputs, dtype=np.uint32)),
		get_int_array_from_numpy_array(np.array(anti_inputs, dtype=np.uint32)),
		len(inputs),
		len(anti_inputs),
		shiftby,
		100000000
	)
	if result != 0:
		with open(cached_fp,"w") as f:
			json.dump([result, inputs_sorted],  f)
	return result

def get_path_id(path:str):
	b:bytes = path.encode()
	if len(b) == 3:
		b = b + b"T"
	elif len(b) == 2:
		b = b + b"TT"
	elif len(b) != 4:
		raise ValueError("All inputs must be 4 bytes (to be interpreted as u32): "+path)
	# return unpack("<I", b)[0]
	little_endian_u32:int = 0
	for n in b[::-1]:
		little_endian_u32 <<= 8
		little_endian_u32 |= n
	return little_endian_u32

def get_shiftby(inputs_sz:int):
	shiftby:int = 32
	while True:
		shiftby -= 1
		if (1 << (32-shiftby)) > inputs_sz:
			break
	return shiftby

if __name__ == "__main__":
	import argparse
	import random
	from math import log2
	import json
	
	parser = argparse.ArgumentParser()
	
	group1 = parser.add_argument_group('Model 1')
	group1exclusive = group1.add_mutually_exclusive_group()
	group1exclusive.add_argument("-i","--inputs",default=[],action="append")
	group1exclusive.add_argument("--dir")
	
	parser.add_argument("--dir2")
	
	parser.add_argument("--anti-inputs",default=[],action="append",help="Require that these 'anti-inputs' be mapped to numbers OUTSIDE the range")
	
	parser.add_argument("--multiplier", default=0, type=int, help="If you have already run this script")
	parser.add_argument("--multiplier2", default=0, type=int)
	parser.add_argument("--pack-files-to", help="For --dir. Create a single file that contains all files, ordered according to the resulting hash")
	parser.add_argument("--write-hpp")
	
	args = parser.parse_args()
	
	input_indx2fp:list = []
	dir2_indx2fname:list = []
	
	if args.dir is not None:
		import os
		args.inputs.append(" HTT")
		input_indx2fp.append(None)
		for fname in os.listdir(args.dir):
			fp:str = args.dir + "/" + fname
			if os.path.isdir(fp):
				continue
			if fname == "index.html":
				input_indx2fp[args.inputs.index(" HTT")] = fp
				continue
			if fname.endswith(".kate-swp"):
				continue
			input_indx2fp.append(fp)
			val:str = fname[:4]
			if val in args.inputs:
				raise ValueError("2 files have the same preceding 2 chars: "+fname[:2]+". Maybe use uint64_t instead of uint32_t?")
			args.inputs.append(val)
	inputs2_paths:list = []
	if args.dir2 is not None:
		import os
		for fname in os.listdir(args.dir2):
			fp:str = args.dir2 + "/" + fname
			if os.path.isdir(fp):
				continue
			dir2_indx2fname.append(fname)
			val:str = fname[:4]
			if val in inputs2_paths:
				raise ValueError("2 anti-files have the same preceding 2 chars: "+fname[:2]+". Maybe use uint64_t instead of uint32_t?")
			inputs2_paths.append(val)
	
	if args.pack_files_to is not None:
		if None in input_indx2fp:
			raise ValueError("Some files do not have a file path (index.html not exist?)")
		if args.write_hpp is None:
			raise ValueError("--pack-files-to requires --write-hpp (for safety)")
	
	for x in args.inputs:
		if x.startswith("sta"):
			raise ValueError("ERROR: Cannot have ANY `inputs` starting with \"sta\" due to \"GET /sta\" being prefix of `inputs2`")
	
	inputs:list = [get_path_id(x) for x in args.inputs]
	inputs2:list = [get_path_id(x) for x in inputs2_paths]
	anti_inputs:list = [get_path_id(x) for x in args.anti_inputs]
	
	shiftby2:int = get_shiftby(len(inputs2)) - 1 # so that less than half of the range is 'empty', making it easier to find space for the anti-inputs
	if len(inputs2) != 0:
		if args.multiplier2 == 0:
			args.multiplier2 = finding_0xedc72f12("multiplier2", inputs2, shiftby2)
			if args.multiplier2 == 0:
				raise ValueError(f"Failed to find suitable multiplier2 for {len(inputs2)} inputs2, {shiftby2} shiftby")
	inputs2_mappedoutputs:list = [((path_id*args.multiplier2) & 0xffffffff) >> shiftby2 for path_id in inputs2]
	
	shiftby1:int = get_shiftby(len(inputs)) - 1 # so that less than half of the range is 'empty', making it easier to find space for the anti-inputs
	
	if args.multiplier == 0:
		args.multiplier = finding_0xedc72f12_w_avoids("multiplier1", inputs, anti_inputs, shiftby1)
		if args.multiplier == 0:
			raise ValueError(f"Failed to find suitable multiplier1 for {len(inputs)} inputs, {shiftby1} shiftby, {len(anti_inputs)} anti-inputs")
	inputs_mappedoutputs:list = [((path_id*args.multiplier) & 0xffffffff) >> shiftby1 for path_id in inputs]
	print(f"((path_id*{args.multiplier}) & 0xffffffff) >> {shiftby1}")
	sorteds:list = []
	inputs__sorted_to_new_order_w_gaps_etc:list = []
	for i in range(max(inputs_mappedoutputs)+1):
		indx:int = 0
		try:
			indx = inputs_mappedoutputs.index(i)
		except ValueError:
			pass
		sorteds.append([args.inputs[indx], inputs[indx], input_indx2fp[indx]])
	for path, path_id, fp in sorteds:
		print(f"{((path_id*args.multiplier) & 0xffffffff) >> shiftby1}:\t{path_id}\t{json.dumps(path)}")
	is_errors:bool = False
	for path, path_id in zip(args.inputs, inputs):
		if path_id not in [x[1] for x in sorteds]:
			is_errors = True
			print(f"ERROR: input not included: {((path_id*args.multiplier) & 0xffffffff) >> shiftby1}:\t{path_id}\t{json.dumps(path)}")
	if is_errors:
		raise ValueError("Error")
	print(f"((path_id*{args.multiplier2}) & 0xffffffff) >> {shiftby2} // for unpackaged files")
	for path, path_id in sorted(zip(inputs2_paths, inputs2), key=lambda x:((x[1]*args.multiplier2)&0xffffffff)>>shiftby2):
		print(f"{((path_id*args.multiplier) & 0xffffffff) >> shiftby1}: {((path_id*args.multiplier2) & 0xffffffff) >> shiftby2}:{path_id}\t{json.dumps(path)}")
	
	print("--anti-inputs:")
	for path, path_id in zip(args.anti_inputs, anti_inputs):
		print(f"{((path_id*args.multiplier) & 0xffffffff) >> shiftby1}: {((path_id*args.multiplier2) & 0xffffffff) >> shiftby2}:{path_id}\t{json.dumps(path)}")
	
	diary_entries_old:list = []
	prev_diary_ids:list = []
	diaryidchars:str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-"
	def make_random_diary_page_id():
		while True:
			idstr:str = ""
			for i in range(4):
				idstr += random.choice(diaryidchars)
			if idstr not in prev_diary_ids:
				prev_diary_ids.append(idstr)
				return idstr
	diary_multiplier:int = 0
	diary_shiftby:int = 0
	diary_fname2idstr:dict = None
	diary_fname2idstr_modified:bool = False
	diary_mappedoutputs:list = None
	with open("files/diary_entries_fname2idstr.json","rb") as f:
		diary_fname2idstr, diary_multiplier, diary_shiftby, diary_mappedoutputs = json.load(f)
	for fname in sorted(os.listdir("files/diary_entries")):
		fp:str = f"files/diary_entries/{fname}"
		if os.path.isfile(fp):
			idstr:str = diary_fname2idstr.get(fname)
			if idstr is None:
				idstr = make_random_diary_page_id()
				diary_fname2idstr[fname] = idstr
				diary_fname2idstr_modified = True
			diary_entries_old.append([None,idstr,None,fp])
	if diary_fname2idstr_modified:
		diary_shiftby = get_shiftby(len(diary_entries_old)) - 1
		diary_pathids:list = [get_path_id(ls[1]) for ls in diary_entries_old]
		diary_multiplier = finding_0xedc72f12("diary", diary_pathids, diary_shiftby)
		if diary_multiplier == 0:
			raise ValueError("Cannot find suitable diary_multiplier")
		diary_mappedoutputs = [((pathid*diary_multiplier)&0xffffffff)>>diary_shiftby for pathid in diary_pathids]
		with open("files/diary_entries_fname2idstr.json","w") as f:
			json.dump([diary_fname2idstr,diary_multiplier,diary_shiftby,diary_mappedoutputs], f)
	for i in range(len(diary_entries_old)):
		if i==0:
			diary_entries_old[i][0] = diary_entries_old[i][1]
		else:
			diary_entries_old[i][0] = diary_entries_old[i-1][1]
		if i==len(diary_entries_old)-1:
			diary_entries_old[i][2] = diary_entries_old[i][1]
		else:
			diary_entries_old[i][2] = diary_entries_old[i+1][1]
	diary_entries:list = []
	for i in range(max(diary_mappedoutputs)+1):
		indx:int = 0
		try:
			indx = diary_mappedoutputs.index(i)
		except ValueError:
			pass
		diary_entries.append(diary_entries_old[indx])
	# diary_entries = [diary_entries[((get_path_id(ls[1])*diary_multiplier)&0xffffffff)>>diary_shiftby] for ls in diary_entries]
	if args.pack_files_to is not None:
		import re
		import magic
		offset:int = 0
		files__offsets_and_sizes:list = []
		max_file_and_header_sz:int = 0
		dir2_indx2mimetype:list = []
		dir2_indx2fsz:list = []
		for fname in dir2_indx2fname:
			fp = args.dir2 + "/" + fname
			mimetype:str = mimetype_utils.guess_mimetype(fp)
			mimetype = mimetype_utils.standardise_mimetype(mimetype, fp)
			dir2_indx2mimetype.append(mimetype)
			stat = os.stat(fp)
			dir2_indx2fsz.append(stat.st_size)
		browser_cache_version_cstr:int = 0
		with open("browser_cache_version.txt","r") as f:
			browser_cache_version_cstr = str(int(f.read())).encode()
		files_to_pack:list = []
		for path, path_id, fp in sorteds:
			files_to_pack.append((False,(path=="admi"),(path=="worl"),fp,""))
		for prev_idstr, idstr, next_idstr, fp in diary_entries:
			files_to_pack.append((True,False,False,fp,prev_idstr+"\n"+next_idstr+"\n"))
		with open(args.pack_files_to, "wb") as fw:
			realname2aliasname:dict = {}
			prev_processed_fp_dfkjskfdjs:dict = {}
			for is_diary, dont_compress, is_rpill, fp, content_prefix in files_to_pack:
				# dont_compress allows us to pretend it was produced automatically, not static pre-compressed content
				
				if fp in prev_processed_fp_dfkjskfdjs:
					offset, content_len = prev_processed_fp_dfkjskfdjs[fp]
					files__offsets_and_sizes.append(offset)
					files__offsets_and_sizes.append(content_len)
					continue
				
				written_n_bytes:int = 0
				contents:bytes = None
				
				with open(fp, "rb") as fr:
					contents = fr.read()
				
				mimetype:str = None
				if is_diary:
					mimetype = "text/plain"
					new_contents:bytes = b""
					def fn(m):
						realname:str = m.group(1)
						try:
							return realname2aliasname[realname]
						except KeyError:
							realname2aliasname[realname] = b"Person"+str(len(realname2aliasname)).encode()
					
					for line in contents.split(b"\n"):
						if line == b"":
							continue
						elif line.startswith(b"// "):
							continue
						elif line.startswith(b"# "):
							line = b"<h2>" + line + b"</h2>"
						elif line.startswith(b"## "):
							line = b"<h3>" + line + b"</h3>"
						else:
							line = b"<div class=\"diary_paragraph\">" + html_escape(line.decode()).encode() + b"</div>"
						new_contents += line
					contents = new_contents
					contents = re.sub(b"(?:DATE|HIDDEN_NOTE)[(][^()]+[)]", b"", contents)
					contents = re.sub(b"NAME[(]([^()]+)[)]", fn, contents)
					contents = re.sub(b"FAKE_CONTENT[(]([^()]+)[)]", b"\\1", contents)
					contents = re.sub(b"HIDE_LOCATION[(]([^()]+)[)]", b"HIDDEN_LOCATION", contents)
					contents = re.sub(b"CENSOR[(]([^()]+)[)]", b"CENSORED", contents)
					contents = re.sub(b"CENSOR[(][(][(]([^()]+)[)][)][)]", b"CENSORED", contents)
					contents = content_prefix.encode() + contents
				if mimetype is None:
					mimetype = magic.from_buffer(contents, mime=True)
				if mimetype == "application/gzip":
					contents = zlib.decompress(contents, wbits=31)
					mimetype = magic.from_buffer(contents, mime=True)
				mimetype = mimetype_utils.standardise_mimetype(mimetype, fp)
				
				csp_header:str = "default-src 'none'"
				if mimetype == "text/html":
					mimetype = "text/html; charset=utf-8"
					if is_rpill:
						csp:str = re.search(b'''content="(default-src 'none';[^"]+)"''', contents).group(0).decode()
						if not csp.endswith(";"):
							csp += ";"
						csp_header = ""+csp
					else:
						contents = re.sub(b"<source type=\"([^\"]+)\" src=\"[.][.]/large/([^\"]+)\">",b"<source type=\"\\1\" src=\"/static/\\2?v="+browser_cache_version_cstr+b"\">",contents)
						contents = re.sub(b"background-image: *url[(][.][.]/static/([^)]+)[)];",b"background-image:url(\\1);",contents)
						contents = re.sub(b"background-image: *url[(][.][.]/large/([^)]+)[)];",b"background-image:url(/static/\\1?v="+browser_cache_version_cstr+b");",contents)
						
						contents = contents.replace(b"\t",b"")
						contents = contents.replace(b"\n",b"")
						
						csp_header += "; "
						if b"<style>" in contents:
							csp_header += "style-src 'self' 'unsafe-inline'; "
						elif b"<link rel=\"stylesheet" in contents:
							csp_header += "style-src 'self'; "
						
						REQUIRES_CSP__MEDIA_SRC_BLOB:bool = False
						
						# NOTE: Do not modify the contents after this, to avoid changing sha256 hashes
						if b"<script>" in contents: # NOTE: Does not account for sha256 attribute
							raise ValueError("Inline <script>")
						elif b"<script src=\"" in contents:
							csp_header += "script-src"
							associated_js:bytes = None
							while True:
								start_of_script_tag:int = contents.index(b"<script src=\"",0)
								start_of_script_src:int = start_of_script_tag + len(b"<script src=\"")
								end_of_jsfname:int = contents.index(b"\"",start_of_script_src)
								end_of_script_tag:str = contents.index(b"</script>",start_of_script_tag)
								jsfname:str = contents[start_of_script_src:end_of_jsfname]
								with open(b"files/js/"+jsfname,"rb") as f:
									associated_js = f.read()
								
								associated_js = associated_js.replace(b"MACRO__ALL_FILES_JSON_PATH",b"/all_files.json?v="+browser_cache_version_cstr)
								associated_js = associated_js.replace(b"MACRO__GLOBAL_VERSION",browser_cache_version_cstr)
								
								REQUIRES_CSP__MEDIA_SRC_BLOB = (b"REQUIRES_CSP__MEDIA_SRC_BLOB" in associated_js)
								
								sha256hash_as_b64:bytes = get_sha256_hash_in_b64_form(associated_js)
								contents = contents[0:start_of_script_tag] + b"<script integrity=\"sha256-"+sha256hash_as_b64+b"\">" + associated_js + contents[end_of_script_tag:]
								csp_header += " 'sha256-"+sha256hash_as_b64.decode()+"'"
								if b"<script src=\"" not in contents:
									break
							
							if b"WebAssembly.instantiate" in associated_js:
								csp_header += " 'wasm-unsafe-eval'"
							
							if b"fetch(" in associated_js:
								csp_header += "; connect-src 'self'"
							
							csp_header += "; "
						
						csp_header += "img-src 'self' data:; media-src 'self' data:"
						if REQUIRES_CSP__MEDIA_SRC_BLOB:
							csp_header += " blob:"
				if mimetype in ("text/css",):
					contents = contents.replace(b"\t",b"")
					contents = contents.replace(b"\n",b"")
					contents = re.sub(b"/[*](?:[^*]|[*][^/])*[*]/",b"",contents)
					contents = contents.replace(b" {",b"{")
					
					contents = re.sub(b"background-image: *url[(][.][.]/static/([^)]+)[)];",b"background-image:url(\\1);",contents)
					contents = re.sub(b"background-image: *url[(][.][.]/large/([^)]+)[)];",b"background-image:url(/static/\\1?v="+browser_cache_version_cstr+b");",contents)
				
				content_encoding_part, contents = compression_utils.compress_w_best_compressor(contents)
				
				headers:str = make_static_headers(content_encoding_part, len(contents), csp_header, mimetype)
				
				contents = headers.encode() + b"\r\n" + contents
				content_len:int = len(contents)
				
				written_n_bytes:int = fw.write(contents)
				if written_n_bytes != content_len:
					raise ValueError(f"Written {written_n_bytes} bytes != {content_len}")
				
				if content_len > max_file_and_header_sz:
					max_file_and_header_sz = content_len
				
				files__offsets_and_sizes.append(offset)
				files__offsets_and_sizes.append(content_len)
				prev_processed_fp_dfkjskfdjs[fp] = (offset, content_len)
				
				offset += content_len
		
		def write_int_arr_for_cpp(arr:list):
			s:str = "{"
			for n in arr:
				s += str(n)
				s += ","
			return s[:-1] + "}"
		
		with open(args.write_hpp,"w") as f:
			f.write(f"#define HASH1_FILEPATH {json.dumps(args.pack_files_to)}\n")
			f.write(f"constexpr unsigned HASH1_shiftby = {shiftby1};\n")
			f.write(f"constexpr uint32_t HASH1_MULTIPLIER = {args.multiplier};\n")
			f.write(f"constexpr unsigned HASH1_LIST_LENGTH = {max(inputs_mappedoutputs)+1};\n")
			f.write(f"constexpr uint32_t HASH1_METADATAS[{len(files__offsets_and_sizes)}] = {write_int_arr_for_cpp(files__offsets_and_sizes)};\n")
			
			f.write(f"constexpr unsigned HASH1_METADATAS__DIARY_OFFSET = {len(sorteds)*2};\n")
			f.write(f"constexpr uint32_t DIARY_MULTIPLIER = {diary_multiplier};\n")
			f.write(f"constexpr unsigned DIARY_LIST_LENGTH = {max(diary_mappedoutputs)+1};\n")
			f.write(f"constexpr uint32_t DIARY_SHIFTBY = {diary_shiftby};\n")
			
			f.write(f"constexpr uint32_t HASH1_ORIG_INTS[{max(inputs_mappedoutputs)+1}] = {write_int_arr_for_cpp(inputs__sorted_to_new_order_w_gaps_etc)};\n")
			
			if len(inputs2) == 0:
				f.write("#define HASH2_IS_NONE\n")
			else:
				f.write(f"constexpr unsigned HASH2_shiftby = {shiftby2};\n")
				f.write(f"constexpr uint32_t HASH2_MULTIPLIER = {args.multiplier2};\n")
				f.write(f"constexpr unsigned HASH2_LIST_LENGTH = {max(inputs2_mappedoutputs)+1};\n")
				f.write("""struct HASH2_indx2metadata_item {
	const char* const filepath;
	const char* const mimetype;
	const size_t fsz;
};\n""")
				s:str = ""
				for i in range(max(inputs2_mappedoutputs)+1):
					indx:int = 0
					try:
						indx = inputs2_mappedoutputs.index(i)
					except ValueError:
						pass
					fpath:str = args.dir2 + "/" + dir2_indx2fname[indx]
					mimetype:str = dir2_indx2mimetype[indx]
					fsz:int = dir2_indx2fsz[indx]
					s += f",{{\"{dblqt(os.path.realpath(fpath))}\", {json.dumps(mimetype)}, {fsz}}}"
				f.write(f"constexpr HASH2_indx2metadata_item HASH2_indx2metadata[{max(inputs2_mappedoutputs)+1}] = {{{s[1:]}}};\n")
			
			for i, antiinput_val in enumerate(anti_inputs):
				f.write(f"constexpr uint32_t HASH_ANTIINPUT_{i} = {antiinput_val};\n")
			
			f.write(f"constexpr unsigned HASH1_max_file_and_header_sz = {max_file_and_header_sz};\n")
		with open("files/all_large_files.txt","w") as f:
			for fname in dir2_indx2fname:
				f.write(args.dir2 + "/" + fname + "\n")
	
	mimetype_utils.save_cached_mimetypes("cached_mimetypes.json")
