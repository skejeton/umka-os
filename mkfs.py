# Creates an image of the virutual FS

import sys
import os
import ctypes

# Check for correct number of arguments
if len(sys.argv) != 3:
    print("Usage: python mkfs.py dir img")
    sys.exit()

# Get the directory
dire = sys.argv[1]
img = sys.argv[2]

# Check if the directory exists
if not os.path.exists(dire):
    print("Directory does not exist")
    sys.exit()

image = bytearray()

def encodeDir(path):
    global image
    directory = os.scandir(path)

    for entry in directory:
        if entry.is_dir():
            image += b'd'
            image += entry.name.encode()
            image += b'\0'
            encodeDir(entry.path)
            image += b'p'
        else:
            image += b'f'
            image += entry.name.encode()
            image += b'\0'
            image += bytes(ctypes.c_int32(os.path.getsize(entry.path)))
            with open(entry.path, "rb") as file:
                image += file.read()

encodeDir(dire)

with open(img, "wb") as file:
    file.write(image)