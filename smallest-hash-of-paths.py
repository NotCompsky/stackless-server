#!/usr/bin/env python3

# inspired by https://orlp.net/blog/worlds-smallest-hash-table/

import zlib
from struct import unpack

def gzip_compress(contents:bytes):
	CO = zlib.compressobj(level=9, wbits=31)
	return CO.compress(contents)+CO.flush()

def h(x, c):
	m = (x * c) % 2**32
	return m >> 28

def is_phf(h, inputs):
	return len({h(x) for x in inputs}) == len(inputs)

def make_lut(h, inputs, answers):
	# lookup table
	assert is_phf(h, inputs)
	lut = [0] * (1 + max(h(x) for x in inputs))
	for (x, ans) in zip(inputs, answers):
		lut[h(x)] = ans
	return lut

def finding_0xedc72f12(inputs:list, anti_inputs:list):
	val:int = None
	best = float('inf')
	while best >= len(inputs):
		val = random.randrange(2**32)
		max_idx = max(h(x, val) for x in inputs)
		if max_idx < best and is_phf(lambda x: h(x, val), inputs):
			rejected:bool = False
			for n in anti_inputs:
				if h(n, val) <= max_idx:
					rejected = True
					break
			if rejected:
				continue
			print(max_idx, hex(val))
			best = max_idx
	return val

def get_path_id(path:str):
	b:bytes = path.encode()
	if len(b) != 4:
		raise ValueError("All inputs must be 4 bytes (to be interpreted as u32): "+path)
	# return unpack("<I", b)[0]
	little_endian_u32:int = 0
	for n in b[::-1]:
		little_endian_u32 <<= 8
		little_endian_u32 |= n
	return little_endian_u32

if __name__ == "__main__":
	import argparse
	import random
	from math import log2
	import json
	
	parser = argparse.ArgumentParser()
	
	group1 = parser.add_argument_group('Model 2')
	group1exclusive = group1.add_mutually_exclusive_group()
	group1exclusive.add_argument("-i","--inputs",default=[],action="append")
	group1exclusive.add_argument("--dir")
	
	group2 = parser.add_argument_group('Model 2')
	group2exclusive = group2.add_mutually_exclusive_group()
	group2exclusive.add_argument("--anti-inputs",default=[],action="append",help="Require that these 'anti-inputs' be mapped to numbers OUTSIDE the range")
	group2exclusive.add_argument("--anti-dir")
	
	parser.add_argument("--multiplier", default=0, type=int, help="If you have already run this script")
	parser.add_argument("--pack-files-to", help="For --dir. Create a single file that contains all files, ordered according to the resulting hash")
	parser.add_argument("--write-hpp")
	
	args = parser.parse_args()
	
	input_indx2fp:list = [None for x in args.inputs]
	antiinput_indx2fp:list = [None for x in args.anti_inputs]
	
	if args.dir is not None:
		import os
		args.inputs.append(" / H")
		input_indx2fp.append(None)
		for fname in os.listdir(args.dir):
			fp:str = args.dir + "/" + fname
			if os.path.isdir(fp):
				continue
			if fname == "index.html":
				input_indx2fp[args.inputs.index(" / H")] = fp
				continue
			input_indx2fp.append(fp)
			val:str = " /"+fname[:2]
			if val in args.inputs:
				raise ValueError("2 files have the same preceding 2 chars: "+fname[:2]+". Maybe use uint64_t instead of uint32_t?")
			args.inputs.append(val)
	if args.anti_dir is not None:
		import os
		for fname in os.listdir(args.anti_dir):
			fp:str = args.anti_dir + "/" + fname
			if os.path.isdir(fp):
				continue
			antiinput_indx2fp.append(fp)
			val:str = " /"+fname[:2]
			if val in args.anti_inputs:
				raise ValueError("2 anti-files have the same preceding 2 chars: "+fname[:2]+". Maybe use uint64_t instead of uint32_t?")
			args.anti_inputs.append(val)
	
	if args.pack_files_to is not None:
		if None in input_indx2fp:
			raise ValueError("Some files do not have a file path (index.html not exist?)")
		if args.write_hpp is None:
			raise ValueError("--pack-files-to requires --write-hpp (for safety)")
	
	inputs:list = [get_path_id(x) for x in args.inputs]
	anti_inputs:list = [get_path_id(x) for x in args.anti_inputs]
	
	print(inputs)
	multiplier2:int = 0
	if len(anti_inputs) != 0:
		multiplier2 = finding_0xedc72f12(anti_inputs, [])
	if args.multiplier == 0:
		args.multiplier = finding_0xedc72f12(inputs, anti_inputs)
	print(f"((path_id*{args.multiplier}) & 0xffffffff) >> 28")
	sorteds:list = sorted(zip(args.inputs, inputs, input_indx2fp), key=lambda x:((x[1]*args.multiplier) & 0xffffffff) >> 28)
	for path, path_id, fp in sorteds:
		print(f"{((path_id*args.multiplier) & 0xffffffff) >> 28}:\t{path_id}\t{json.dumps(path)}")
	print(f"((path_id*{multiplier2}) & 0xffffffff) >> 28 // for unpackaged files")
	for path, path_id in zip(args.anti_inputs, anti_inputs):
		print(f"{((path_id*args.multiplier) & 0xffffffff) >> 28}: {((path_id*multiplier2) & 0xffffffff) >> 28}:{path_id}\t{json.dumps(path)}")
	
	if args.pack_files_to is not None:
		offset:int = 0
		files__offsets_and_sizes:list = []
		with open(args.pack_files_to, "wb") as fw:
			for path, path_id, fp in sorteds:
				written_n_bytes:int = 0
				contents:bytes = b""
				with open(fp, "rb") as fr:
					contents = fr.read()
				
				contents_compressed:bytes = gzip_compress(contents)
				if len(contents_compressed) < len(contents)+10:
					contents = contents_compressed
				
				written_n_bytes:int = fw.write(contents)
				if written_n_bytes != len(contents):
					raise ValueError(f"Written {written_n_bytes} bytes != {len(contents)}")
				
				files__offsets_and_sizes.append(offset)
				offset += len(contents)
				files__offsets_and_sizes.append(offset)
		
		def write_int_arr_for_cpp(arr:list):
			s:str = "{"
			for n in arr:
				s += str(n)
				s += ","
			return s[:-1] + "}"
		
		with open(args.write_hpp,"w") as f:
			f.write(f"#define HASH1_FILEPATH {json.dumps(args.pack_files_to)}\n")
			f.write(f"constexpr unsigned HASH1_MULTIPLIER = {args.multiplier};\n")
			f.write(f"constexpr unsigned HASH1_LIST_LENGTH = {len(args.inputs)};\n")
			f.write(f"const uint32_t HASH1_METADATAS[{len(files__offsets_and_sizes)}] = {write_int_arr_for_cpp(files__offsets_and_sizes)};\n")
			
			if len(anti_inputs) == 0:
				f.write("#define HASH2_IS_NONE\n")
			else:
				f.write(f"constexpr unsigned HASH2_MULTIPLIER = {multiplier2};\n")
				f.write(f"constexpr unsigned HASH2_LIST_LENGTH = {len(args.anti_inputs)};\n")
				f.write(f"const char* HASH2_FILEPATHS[{len(args.anti_inputs)}] = {json.dumps(antiinput_indx2fp)};")
