#!/usr/bin/env python3

# inspired by https://orlp.net/blog/worlds-smallest-hash-table/

import zlib
from struct import unpack
import ctypes
import numpy as np
from mimetype_utils import guess_mimetype, standardise_mimetype
import hashlib
import base64


clib = ctypes.CDLL("/home/vangelic/repos/compsky/static-and-chat-server/libsmallesthashofpaths.so")
c_finding_0xedc72f12 = clib.finding_0xedc72f12
c_finding_0xedc72f12.argtypes = [ctypes.POINTER(ctypes.c_uint), ctypes.c_uint, ctypes.c_uint]
c_finding_0xedc72f12.restype = ctypes.c_uint
c_finding_0xedc72f12_w_avoids = clib.finding_0xedc72f12_w_avoids
c_finding_0xedc72f12_w_avoids.argtypes = [ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint), ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
c_finding_0xedc72f12_w_avoids.restype = ctypes.c_uint

def unretarded_b64_encode(b:bytes):
	return base64.encodebytes(b).replace(b"\n",b"")

def get_sha256_hash_in_b64_form(contents:bytes):
	return unretarded_b64_encode(hashlib.sha256(contents).digest())

def str2cliststr(s:str):
	r:str = "{"
	for c in s:
		if (c == "'") or (c == "\\"):
			c = "\\" + c
		r += "'" + c + "',"
	return r[:-1] + "}"

def gzip_compress(contents:bytes):
	CO = zlib.compressobj(level=9, wbits=31)
	return CO.compress(contents)+CO.flush()

def get_int_array_from_numpy_array(array):
	contig_array = np.ascontiguousarray(array, dtype=np.uint32)
	return contig_array.ctypes.data_as(ctypes.POINTER(ctypes.c_uint))

def finding_0xedc72f12(inputs:list, shiftby:int):
	return c_finding_0xedc72f12(
		get_int_array_from_numpy_array(np.array(inputs, dtype=np.uint32)),
		len(inputs),
		shiftby,
		100000000
	)

def finding_0xedc72f12_w_avoids(inputs:list, anti_inputs:list, shiftby:int):
	for x in anti_inputs:
		if x in inputs:
			raise ValueError(f"{x} in both `inputs` and `anti_inputs`")
	return c_finding_0xedc72f12_w_avoids(
		get_int_array_from_numpy_array(np.array(inputs, dtype=np.uint32)),
		get_int_array_from_numpy_array(np.array(anti_inputs, dtype=np.uint32)),
		len(inputs),
		len(anti_inputs),
		shiftby,
		100000000
	)

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
			args.multiplier2 = finding_0xedc72f12(inputs2, shiftby2)
			if args.multiplier2 == 0:
				raise ValueError(f"Failed to find suitable multiplier2 for {len(inputs2)} inputs2, {shiftby2} shiftby")
	inputs2_mappedoutputs:list = [((path_id*args.multiplier2) & 0xffffffff) >> shiftby2 for path_id in inputs2]
	
	shiftby1:int = get_shiftby(len(inputs)) - 1 # so that less than half of the range is 'empty', making it easier to find space for the anti-inputs
	
	if args.multiplier == 0:
		args.multiplier = finding_0xedc72f12_w_avoids(inputs, anti_inputs, shiftby1)
		if args.multiplier == 0:
			raise ValueError(f"Failed to find suitable multiplier1 for {len(inputs)} inputs, {shiftby1} shiftby, {len(anti_inputs)} anti-inputs")
	inputs_mappedoutputs:list = [((path_id*args.multiplier) & 0xffffffff) >> shiftby1 for path_id in inputs]
	print(f"((path_id*{args.multiplier}) & 0xffffffff) >> {shiftby1}")
	sorteds:list = []
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
			mimetype:str = guess_mimetype(fp)
			dir2_indx2mimetype.append(mimetype)
			stat = os.stat(fp)
			dir2_indx2fsz.append(stat.st_size)
		browser_cache_version_cstr:int = 0
		with open("browser_cache_version.txt","r") as f:
			browser_cache_version_cstr = str(int(f.read())).encode()
		with open(args.pack_files_to, "wb") as fw:
			for path, path_id, fp in sorteds:
				written_n_bytes:int = 0
				contents:bytes = b""
				with open(fp, "rb") as fr:
					contents = fr.read()
				
				mimetype:str = magic.from_buffer(contents, mime=True)
				if mimetype == "application/gzip":
					contents = zlib.decompress(contents, wbits=31)
					mimetype = magic.from_buffer(contents, mime=True)
				mimetype = standardise_mimetype(mimetype, fp)
				
				headers:str = (
					"HTTP/1.1 200 OK\r\n"
					"Cache-Control: max-age=2592000\r\n"
					"Connection: keep-alive\r\n"
					"Content-Type: " + mimetype + "\r\n"
					"Referrer-Policy: no-referrer\r\n"
					"Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n"
					"X-Content-Type-Options: nosniff\r\n"
					"X-Frame-Options: SAMEORIGIN\r\n"
					"X-Permitted-Cross-Domain-Policies: none\r\n"
					"X-XSS-Protection: 1; mode=block\r\n"
				)
				dont_compress:bool = (path=="admi") # Pretend it was produced automatically, not static pre-compressed content
				if mimetype == "text/html":
					if path == "worl": # rpill
						csp:str = re.search(b'''content="(default-src 'none';[^"]+)"''', contents).group(0).decode()
						if not csp.endswith(";"):
							csp += ";"
						headers += "content-security-policy: "+csp+"\r\n"
					else:
						headers += "content-security-policy: default-src 'none'; connect-src 'self'; "
						
						contents = re.sub(b"<source type=\"([^\"]+)\" src=\"[.][.]/large/([^\"]+)\">",b"<source type=\"\\1\" src=\"/static/\\2?v="+browser_cache_version_cstr+b"\">",contents)
						contents = re.sub(b"background-image: *url[(][.][.]/static/([^)]+)[)];",b"background-image:url(\\1);",contents)
						contents = re.sub(b"background-image: *url[(][.][.]/large/([^)]+)[)];",b"background-image:url(/static/\\1?v="+browser_cache_version_cstr+b");",contents)
						
						contents = contents.replace(b"\t",b"")
						contents = contents.replace(b"\n",b"")
						
						if b"<style>" in contents:
							headers += "style-src 'self' 'unsafe-inline'; "
						else:
							headers += "style-src 'self'; "
						
						# NOTE: Do not modify the contents after this, to avoid changing sha256 hashes
						if b"<script>" in contents: # NOTE: Does not account for sha256 attribute
							raise ValueError("Inline <script>")
						elif b"<script src=\"" in contents:
							headers += "script-src"
							while True:
								start_of_script_tag:int = contents.index(b"<script src=\"",0)
								start_of_script_src:int = start_of_script_tag + len(b"<script src=\"")
								end_of_jsfname:int = contents.index(b"\"",start_of_script_src)
								end_of_script_tag:str = contents.index(b"</script>",start_of_script_tag)
								jsfname:str = contents[start_of_script_src:end_of_jsfname]
								associated_js:bytes = None
								with open(b"files/js/"+jsfname,"rb") as f:
									associated_js = f.read()
								
								associated_js = associated_js.replace(b"MACRO__ALL_FILES_JSON_PATH",b"/all_files.json?v="+browser_cache_version_cstr)
								associated_js = associated_js.replace(b"MACRO__GLOBAL_VERSION",browser_cache_version_cstr)
								
								sha256hash_as_b64:bytes = get_sha256_hash_in_b64_form(associated_js)
								contents = contents[0:start_of_script_tag] + b"<script integrity=\"sha256-"+sha256hash_as_b64+b"\">" + associated_js + contents[end_of_script_tag:]
								headers += " 'sha256-"+sha256hash_as_b64.decode()+"'"
								if b"<script src=\"" not in contents:
									break
							headers += "; "
						
						headers += "img-src 'self' data:; media-src 'self' data:;\r\n"
				if mimetype in ("text/css",):
					contents = contents.replace(b"\t",b"")
					contents = contents.replace(b"\n",b"")
					contents = re.sub(b"/[*](?:[^*]|[*][^/])*[*]/",b"",contents)
					contents = contents.replace(b" {",b"{")
					
					contents = re.sub(b"background-image: *url[(][.][.]/static/([^)]+)[)];",b"background-image:url(\\1);",contents)
					contents = re.sub(b"background-image: *url[(][.][.]/large/([^)]+)[)];",b"background-image:url(/static/\\1?v="+browser_cache_version_cstr+b");",contents)
				
				if not dont_compress:
					contents_compressed:bytes = gzip_compress(contents)
					if len(contents_compressed) < len(contents)+10:
						contents = contents_compressed
						headers += "content-encoding: gzip\r\n"
				
				headers += "content-length: " + str(len(contents)) + "\r\n"
				
				contents = headers.encode() + b"\r\n" + contents
				content_len:int = len(contents)
				
				written_n_bytes:int = fw.write(contents)
				if written_n_bytes != content_len:
					raise ValueError(f"Written {written_n_bytes} bytes != {content_len}")
				
				if content_len > max_file_and_header_sz:
					max_file_and_header_sz = content_len
				
				files__offsets_and_sizes.append(offset)
				offset += content_len
				files__offsets_and_sizes.append(content_len)
		
		def write_int_arr_for_cpp(arr:list):
			s:str = "{"
			for n in arr:
				s += str(n)
				s += ","
			return s[:-1] + "}"
		
		with open(args.write_hpp,"w") as f:
			f.write(f"#define HASH1_FILEPATH {json.dumps(args.pack_files_to)}\n")
			f.write(f"constexpr unsigned HASH1_shiftby = {shiftby1};\n")
			f.write(f"constexpr unsigned HASH1_MULTIPLIER = {args.multiplier};\n")
			f.write(f"constexpr unsigned HASH1_LIST_LENGTH = {max(inputs_mappedoutputs)+1};\n")
			f.write(f"const uint32_t HASH1_METADATAS[{len(files__offsets_and_sizes)}] = {write_int_arr_for_cpp(files__offsets_and_sizes)};\n")
			
			if len(inputs2) == 0:
				f.write("#define HASH2_IS_NONE\n")
			else:
				f.write(f"constexpr unsigned HASH2_shiftby = {shiftby2};\n")
				f.write(f"constexpr unsigned HASH2_MULTIPLIER = {args.multiplier2};\n")
				f.write(f"constexpr unsigned HASH2_LIST_LENGTH = {max(inputs2_mappedoutputs)+1};\n")
				f.write("""struct HASH2_indx2metadata_item {
	const char fileid[4];
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
					fname:str = dir2_indx2fname[indx]
					mimetype:str = dir2_indx2mimetype[indx]
					fsz:int = dir2_indx2fsz[indx]
					s += f",{{{str2cliststr(fname)}, {json.dumps(mimetype)}, {fsz}}}"
				f.write(f"const HASH2_indx2metadata_item HASH2_indx2metadata[{max(inputs2_mappedoutputs)+1}] = {{{s[1:]}}};\n")
			
			for i, antiinput_val in enumerate(anti_inputs):
				f.write(f"constexpr uint32_t HASH_ANTIINPUT_{i} = {antiinput_val};\n")
			
			f.write(f"constexpr unsigned HASH1_max_file_and_header_sz = {max_file_and_header_sz};\n")
