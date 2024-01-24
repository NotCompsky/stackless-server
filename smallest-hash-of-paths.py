#!/usr/bin/env python3

# inspired by https://orlp.net/blog/worlds-smallest-hash-table/

import zlib

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
				print("--")
				if h(n, val) <= max_idx:
					rejected = True
					break
			print("==")
			if rejected:
				print("REJECTED:", max_idx, hex(val))
				continue
			print(max_idx, hex(val))
			best = max_idx
	return val

def get_path_id(path:str):
	b:bytes = path.encode()
	if len(b) != 4:
		raise ValueError("All inputs must be 4 bytes (to be interpreted as u32): "+path)
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
	parser.add_argument("--anti-inputs",default=[],action="append",help="Require that these 'anti-inputs' be mapped to numbers OUTSIDE the range")
	parser.add_argument("--multiplier", default=0, type=int, help="If you have already run this script")
	parser.add_argument("--pack-files-to", help="For --dir. Create a single file that contains all files, ordered according to the resulting hash")
	
	args = parser.parse_args()
	
	if (args.pack_files_to is not None) and (args.multiplier == 0):
		raise ValueError("For your own protection, can only pack files with a pre-set multiplier")
	
	input_indx2fp:list = [None for x in args.inputs]
	
	if args.dir is not None:
		import os
		args.inputs.append(" /\r\n")
		input_indx2fp.append(None)
		for fname in os.listdir(args.dir):
			fp:str = args.dir + "/" + fname
			if os.path.isdir(fp):
				continue
			if fname == "index.html":
				input_indx2fp[args.inputs.index(" /\r\n")] = fname
				continue
			input_indx2fp.append(fname)
			val:str = " /"+fname[:2]
			if val in args.inputs:
				raise ValueError("2 files have the same preceding 2 chars: "+fname[:2]+". Maybe use uint64_t instead of uint32_t?")
			args.inputs.append(val)
	
	if args.pack_files_to is not None:
		if None in input_indx2fp:
			raise ValueError("Some files do not have a file path (index.html not exist?)")
	
	inputs:list = [get_path_id(x) for x in args.inputs]
	
	print(inputs)
	if args.multiplier == 0:
		args.multiplier = finding_0xedc72f12(inputs, [get_path_id(x) for x in args.anti_inputs])
	print(f"((path_id*{args.multiplier}) & 0xffffffff) >> 28")
	sorteds:list = sorted(zip(args.inputs, inputs, input_indx2fp), key=lambda x:((x[1]*args.multiplier) & 0xffffffff) >> 28)
	for path, path_id, fp in sorteds:
		print(f"{((path_id*args.multiplier) & 0xffffffff) >> 28}: {json.dumps(path)}")
	for anti_input in args.anti_inputs:
		print(f"{((get_path_id(anti_input)*args.multiplier) & 0xffffffff) >> 28}: {json.dumps(anti_input)}")
	
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
				if len(contents_compressed) < len(contents):
					contents = contents_compressed
				
				written_n_bytes:int = fw.write(contents)
				if written_n_bytes != len(contents):
					raise ValueError(f"Written {written_n_bytes} bytes != {len(contents)}")
				
				offset += len(contents)
				
				files__offsets_and_sizes.append(offset)
				files__offsets_and_sizes.append(len(contents))
		print(files__offsets_and_sizes)
