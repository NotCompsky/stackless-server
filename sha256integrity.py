import base64
import hashlib


def unretarded_b64_encode(b:bytes):
	return base64.encodebytes(b).replace(b"\n",b"")

def get_sha256_hash_in_b64_form(contents:bytes):
	return unretarded_b64_encode(hashlib.sha256(contents).digest())
