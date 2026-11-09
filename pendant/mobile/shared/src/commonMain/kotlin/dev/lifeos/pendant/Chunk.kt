package dev.lifeos.pendant

/** Record container, ported from fw/lib/chunk (docs/spec/formats.md §2). Little endian; u32 as Long, u64 as Long. */
const val HDR = 32
const val TRL = 8
const val MAX_RECORD = 1 shl 20 // FW writes <= 32+65536+8; anything bigger is a corrupt length
const val T_AUDIO = 1
const val T_TIMEFIX = 2
const val T_GAP = 3
const val T_EVENT = 4
const val F_RTC_INVALID = 1
const val SHARED_VERSION = "0.1.0"

private val CRC_TABLE = LongArray(256) { n ->
    var c = n.toLong()
    repeat(8) { c = if (c and 1L != 0L) (c ushr 1) xor 0xEDB88320L else c ushr 1 }
    c
}

/** CRC-32/ISO-HDLC (zlib). */
fun crc32(b: ByteArray, off: Int = 0, len: Int = b.size - off): Long {
    var c = 0xFFFFFFFFL
    for (i in off until off + len) c = CRC_TABLE[((c xor b[i].toLong()) and 0xFF).toInt()] xor (c ushr 8)
    return c xor 0xFFFFFFFFL
}

fun ByteArray.u8(o: Int) = this[o].toInt() and 0xFF
fun ByteArray.u32(o: Int): Long = u8(o).toLong() or (u8(o + 1).toLong() shl 8) or (u8(o + 2).toLong() shl 16) or (u8(o + 3).toLong() shl 24)
fun ByteArray.u64(o: Int): Long = u32(o) or (u32(o + 4) shl 32)
fun ByteArray.put32(o: Int, v: Long) { for (i in 0 until 4) this[o + i] = (v ushr (8 * i)).toByte() }
fun ByteArray.put64(o: Int, v: Long) { put32(o, v); put32(o + 4, v ushr 32) }

data class Header(
    val type: Int, val codec: Int, val flags: Int, val seq: Long, val boot: Long,
    val monoMs: Long, val rtcMs: Long, val len: Int,
)

sealed interface Decoded {
    data class Timefix(val beforeMs: Long, val afterMs: Long) : Decoded
    data class Gap(val first: Long, val last: Long, val reason: Int) : Decoded
    data class Event(val kind: Int, val arg: Int, val monoMs: Long) : Decoded
}

/** A record that passed magic, version, length, CRC and commit. [raw] is stored as is. */
class Record(val raw: ByteArray, val hdr: Header, val known: Boolean, val decoded: Decoded?) {
    /** A gap whose range we cannot trust: no coverage, the session must stop (F5). */
    val opaqueGap get() = hdr.type == T_GAP && !known
    val first get() = (decoded as? Decoded.Gap)?.first ?: hdr.seq
    val last get() = (decoded as? Decoded.Gap)?.last ?: hdr.seq
}

sealed interface Parsed {
    class Ok(val rec: Record) : Parsed
    class Bad(val why: String) : Parsed
}

/** Total record size from its first 32 bytes, or null if the header is not ours or the length is absurd. */
fun recordSize(head: ByteArray): Int? {
    if (head.size < HDR || head[0] != 'L'.code.toByte() || head[1] != 'O'.code.toByte() || head[2] != 'S'.code.toByte() ||
        head[3] != 'C'.code.toByte() || head[4].toInt() != 1
    ) return null
    val len = head.u32(28)
    return if (len > MAX_RECORD) null else (HDR + len + TRL).toInt()
}

private val COMMIT = byteArrayOf('C'.code.toByte(), 'M'.code.toByte(), 'T'.code.toByte(), '!'.code.toByte())

/** chunk_decode plus the format judgement (F5). b[off, off+avail) must start at a record boundary. */
fun parseRecord(b: ByteArray, off: Int = 0, avail: Int = b.size - off): Parsed {
    if (avail < HDR) return Parsed.Bad("short")
    val head = b.copyOfRange(off, off + HDR)
    val size = recordSize(head) ?: return Parsed.Bad("magic/version/length")
    if (size > avail) return Parsed.Bad("truncated")
    val len = size - HDR - TRL
    val t = off + HDR + len
    if (b.copyOfRange(t + 4, t + 8).contentEquals(byteArrayOf(-1, -1, -1, -1))) return Parsed.Bad("uncommitted")
    if (!b.copyOfRange(t + 4, t + 8).contentEquals(COMMIT) || b.u32(t) != crc32(b, off, HDR + len)) return Parsed.Bad("crc")
    val raw = b.copyOfRange(off, off + size)
    val h = Header(raw.u8(5), raw.u8(6), raw.u8(7), raw.u32(8), raw.u32(12), raw.u32(16), raw.u64(20), len)
    val known = isKnown(h, raw)
    return Parsed.Ok(Record(raw, h, known, if (known) decode(h, raw) else null))
}

private fun gapOf(raw: ByteArray) = Decoded.Gap(raw.u32(HDR), raw.u32(HDR + 4), raw.u8(HDR + 8))

/** known = flags only bit0, type 1..4, codec 1 for audio else 0, fixed payload sizes, gap.first == seq <= last. */
private fun isKnown(h: Header, raw: ByteArray): Boolean {
    if (h.flags and F_RTC_INVALID.inv() != 0 || h.type !in T_AUDIO..T_EVENT) return false
    if (h.codec != (if (h.type == T_AUDIO) 1 else 0)) return false
    return when (h.type) {
        T_TIMEFIX -> h.len == 16
        T_EVENT -> h.len == 8
        T_GAP -> h.len == 12 && gapOf(raw).let { it.first == h.seq && it.first <= it.last }
        else -> true
    }
}

private fun decode(h: Header, raw: ByteArray): Decoded? = when (h.type) {
    T_TIMEFIX -> Decoded.Timefix(raw.u64(HDR), raw.u64(HDR + 8))
    T_GAP -> gapOf(raw)
    T_EVENT -> Decoded.Event(raw.u8(HDR), raw.u8(HDR + 1), raw.u32(HDR + 4))
    else -> null
}

/** Test and demo helper: the same bytes chunk_encode writes. */
fun encodeRecord(h: Header, payload: ByteArray, commit: Boolean = true): ByteArray {
    val out = ByteArray(HDR + payload.size + TRL)
    "LOSC".forEachIndexed { i, c -> out[i] = c.code.toByte() }
    out[4] = 1; out[5] = h.type.toByte(); out[6] = h.codec.toByte(); out[7] = h.flags.toByte()
    out.put32(8, h.seq); out.put32(12, h.boot); out.put32(16, h.monoMs); out.put64(20, h.rtcMs); out.put32(28, payload.size.toLong())
    payload.copyInto(out, HDR)
    out.put32(HDR + payload.size, crc32(out, 0, HDR + payload.size))
    for (i in 0 until 4) out[HDR + payload.size + 4 + i] = if (commit) COMMIT[i] else -1
    return out
}

fun gapPayload(first: Long, last: Long, reason: Int = 1) = ByteArray(12).also { it.put32(0, first); it.put32(4, last); it[8] = reason.toByte() }
