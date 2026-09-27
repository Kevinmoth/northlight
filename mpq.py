"""Small StormLib binding for this client's asset inspection and MPQ builds.

File system paths go to StormLib as wide strings on Windows (the StormLib.dll is built with
UNICODE, so a non-ASCII client path opens) and as UTF-8 bytes elsewhere. Names inside an archive
are always char*."""
import ctypes as C
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent
from northlight_paths import stormlib  # noqa: E402  (sibling module)
lib = C.CDLL(str(stormlib()))
H, U = C.c_void_p, C.c_uint32
WIDE = os.name == 'nt'
FS = C.c_wchar_p if WIDE else C.c_char_p   # TCHAR* file system path

def bind(name, args, result=C.c_bool):
    fn = getattr(lib, name)
    fn.argtypes, fn.restype = args, result
    return fn

open_archive = bind('SFileOpenArchive', [FS, U, U, C.POINTER(H)])
close_archive = bind('SFileCloseArchive', [H])
open_file = bind('SFileOpenFileEx', [H, C.c_char_p, U, C.POINTER(H)])
file_size = bind('SFileGetFileSize', [H, C.POINTER(U)], U)
read_file = bind('SFileReadFile', [H, H, U, C.POINTER(U), H])
close_file = bind('SFileCloseFile', [H])
has_file = bind('SFileHasFile', [H, C.c_char_p])
create_archive = bind('SFileCreateArchive', [FS, U, U, C.POINTER(H)])
add_file = bind('SFileAddFileEx', [H, FS, C.c_char_p, U, U, U])

def fs_path(path):
    return str(path) if WIDE else bytes(Path(path))

class Archive:
    def __init__(self, path, create=False, capacity=4096):
        self.path, self.handle = Path(path), H()
        if create:
            if self.path.exists():
                raise FileExistsError(self.path)
            ok = create_archive(fs_path(self.path), 0x00100000, capacity, C.byref(self.handle))
        else:
            ok = open_archive(fs_path(self.path), 0, 0x100, C.byref(self.handle))
        if not ok:
            raise OSError(f'Cannot open MPQ: {path}')

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def close(self):
        if self.handle:
            if not close_archive(self.handle):
                raise OSError(f'Cannot close MPQ: {self.path}')
            self.handle = H()

    def has(self, name):
        return bool(has_file(self.handle, name.encode()))

    def read(self, name):
        handle = H()
        if not open_file(self.handle, name.encode(), 0, C.byref(handle)):
            raise FileNotFoundError(f'{self.path}: {name}')
        try:
            size = file_size(handle, None)
            if size == 0xffffffff or size > 512 * 1024 * 1024:
                raise ValueError(f'Unexpected file size: {size}')
            data, count = C.create_string_buffer(size), U()
            if not read_file(handle, data, size, C.byref(count), None) or count.value != size:
                raise OSError(f'Cannot read {name}')
            return data.raw
        finally:
            close_file(handle)

    def names(self):
        return self.read('(listfile)').decode('utf-8-sig', errors='replace').splitlines()

    def add(self, path, name):
        if not add_file(self.handle, fs_path(path), name.encode(), 0x200, 2, 2):
            raise OSError(f'Cannot add {name}')
