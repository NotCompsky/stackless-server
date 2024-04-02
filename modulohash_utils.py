#!/usr/bin/env python3

import ctypes
import json
import os.path
import numpy as np


clib = ctypes.CDLL("/home/vangelic/repos/compsky/static-and-chat-server/libsmallesthashofpaths.so")
c_finding_0xedc72f12 = clib.finding_0xedc72f12
c_finding_0xedc72f12.argtypes = [ctypes.POINTER(ctypes.c_uint), ctypes.c_uint, ctypes.c_uint]
c_finding_0xedc72f12.restype = ctypes.c_uint
c_finding_0xedc72f12_w_avoids = clib.finding_0xedc72f12_w_avoids
c_finding_0xedc72f12_w_avoids.argtypes = [ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint), ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
c_finding_0xedc72f12_w_avoids.restype = ctypes.c_uint


def get_int_array_from_numpy_array(array):
	contig_array = np.ascontiguousarray(array, dtype=np.uint32)
	return contig_array.ctypes.data_as(ctypes.POINTER(ctypes.c_uint))


def maybe_get_cached(cached_fp:str, inputs_sorted:list):
	if cached_fp is None:
		return 0
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
	cached_fp:str = None if (idstr is None) else f"finding_0xedc72f12.{idstr}.json"
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
	if (cached_fp is not None) and (result != 0):
		with open(cached_fp,"w") as f:
			json.dump([result, inputs_sorted],  f)
	return result


def finding_0xedc72f12_w_avoids(idstr:str, inputs:list, anti_inputs:list, shiftby:int):
	cached_fp:str = None if (idstr is None) else f"finding_0xedc72f12_w_avoids.{idstr}.json"
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
	if (cached_fp is not None) and (result != 0):
		with open(cached_fp,"w") as f:
			json.dump([result, inputs_sorted],  f)
	return result


def get_path_id__u32(path:str, prefix:bytes):
	b:bytes = path.encode()
	if prefix is not None:
		if len(b) == 3:
			b = b + prefix[3]
		elif len(b) == 2:
			b = b + prefix[2:4]
	if len(b) != 4:
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
	
	parser = argparse.ArgumentParser()
	parser.add_argument("-i","--inputs",default=[],action="append")
	
	args = parser.parse_args()
	
	inputs:list = [get_path_id__u32(x,None) for x in args.inputs]
	
	shiftby:int = get_shiftby(len(inputs)) - 1
	
	print(finding_0xedc72f12(None, inputs, shiftby))
