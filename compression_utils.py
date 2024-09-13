#!/usr/bin/env python3


import brotli
import zlib


def gzip_compress(contents:bytes):
	CO = zlib.compressobj(level=9, wbits=31)
	return CO.compress(contents)+CO.flush()

def brotli_compress(contents:bytes):
	return brotli.compress(contents, mode=brotli.MODE_GENERIC, quality=11, lgwin=24, lgblock=24)

def identity(contents:bytes):
	return contents

total_bytes_uncompressed:int = 0
total_bytes_saved_if_everything_is_gzip_or_decompressed:int = 0
total_bytes_saved:int = 0

def compress_w_best_compressor(contents:bytes):
	global total_bytes_uncompressed
	global total_bytes_saved_if_everything_is_gzip_or_decompressed
	global total_bytes_saved
	
	total_bytes_uncompressed += len(contents)
	
	ls:list = []
	for encoding_header, fn in (
		("", identity),
		("Content-Encoding: gzip\r\n", gzip_compress),
		("Content-Encoding: br\r\n", brotli_compress),
	):
		ls.append((encoding_header,fn(contents)))
	
	if len(ls[1][1]) < len(contents):
		total_bytes_saved_if_everything_is_gzip_or_decompressed += len(contents) - len(ls[1][1])
	
	res:tuple = sorted(ls, key=lambda x:len(x[1]))[0]
	
	if len(res[1]) < len(contents):
		total_bytes_saved += len(contents) - len(res[1])
	
	return res
