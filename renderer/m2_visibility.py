"""Conservative proof of always-invisible M2 draw batches; never evaluates time.

MD20/264 texture-weight animation: header +88, lookup +144, batch combo +20.
A single-texture batch whose weight is zero for every declared sequence never
renders, even with an opaque blend mode. Missing/external/unsupported tracks
are NOT proof of invisibility. Keep those batches for the existing extractor.
"""
import struct


def _array(data, offset, count, fmt):
    size = struct.calcsize(fmt)
    if count > 1000000 or offset < 0 or offset + count * size > len(data):
        raise ValueError('M2 visibility array bounds')
    return list(struct.iter_unpack(fmt, data[offset:offset + count * size]))


def zero_weight(data, track_offset):
    """True only for present step/linear tracks entirely zero in all sequences."""
    try:
        kind, global_id, nt, ot, nk, ok = struct.unpack_from('<Hh4I', data, track_offset)
        animations = struct.unpack_from('<I', data, 28)[0]
        globals_count, globals_offset = struct.unpack_from('<II', data, 20)
        if kind not in (0, 1) or nt != nk or not nk:
            return False
        if global_id == -1:
            if nk != max(1, animations):
                return False
        elif 0 <= global_id < globals_count:
            _array(data, globals_offset, globals_count, '<I')
            if nk != 1:
                return False
        else:
            return False
        times = _array(data, ot, nt, '<II')
        keys = _array(data, ok, nk, '<II')
        for (tc, to), (kc, ko) in zip(times, keys):
            if not tc or tc != kc:
                return False
            stamps = [v[0] for v in _array(data, to, tc, '<I')]
            if stamps != sorted(stamps):
                return False
            if any(v[0] != 0 for v in _array(data, ko, kc, '<h')):
                return False
        return True
    except (ValueError, struct.error):
        return False


def hidden_batches(data, batches):
    """Return batch indices with a proven permanently-zero texture weight."""
    try:
        if data[:4] != b'MD20' or struct.unpack_from('<I', data, 4)[0] != 264:
            return set()
        count, offset = struct.unpack_from('<II', data, 88)
        lookup_count, lookup_offset = struct.unpack_from('<II', data, 144)
        lookup = _array(data, lookup_offset, lookup_count, '<h')
        if offset + count * 20 > len(data):
            return set()
        hidden = set()
        for i, batch in enumerate(batches):
            # Multi-texture shaders can combine weights differently; retain them.
            combo = batch[11]
            if batch[8] != 1 or combo == 0xffff or combo >= len(lookup):
                continue
            track = lookup[combo][0]
            if 0 <= track < count and zero_weight(data, offset + track * 20):
                hidden.add(i)
        return hidden
    except (ValueError, struct.error, IndexError):
        return set()
