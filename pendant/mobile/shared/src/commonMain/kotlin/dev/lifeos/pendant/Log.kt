package dev.lifeos.pendant

/** Writing or fsyncing failed: this RecordLog is dead, the session must stop and the next one reopens. */
class StoreFailure(cause: Throwable) : Exception("store failure: ${cause.message}", cause)

/** The log on disk does not follow the acceptance rules or has damage in the middle. Nothing was changed. */
class LogInconsistent(why: String) : Exception(why)

enum class Verdict { DUP, AHEAD, ACCEPT }

/** One row of the in-memory table per record in the log. No payload. [covFirst]..[covLast] is what it newly covered. */
class MetaRow(
    val hdr: Header, val offset: Long, val size: Int, val known: Boolean, val decoded: Decoded?,
    val covFirst: Long, val covLast: Long,
) {
    /** Same table whether built while appending or by reopening. */
    override fun equals(other: Any?) = other is MetaRow && hdr == other.hdr && offset == other.offset && size == other.size &&
        known == other.known && decoded == other.decoded && covFirst == other.covFirst && covLast == other.covLast
    override fun hashCode() = hdr.hashCode() * 31 + offset.hashCode()
}

/** chunk_rx_feed with D = [durableNext]. */
fun verdict(rec: Record, d: Long): Verdict = when {
    rec.last < d -> Verdict.DUP
    rec.first > d -> Verdict.AHEAD
    else -> Verdict.ACCEPT
}

private fun rowFor(rec: Record, offset: Long, d: Long) =
    MetaRow(rec.hdr, offset, rec.raw.size, rec.known, rec.decoded, maxOf(rec.first, d), rec.last)

class ScanOut(val rows: List<MetaRow>, val durableNext: Long, val p: Long, val problem: String?)

/**
 * Steps 1-3 of open, with no writes. [read] returns up to n bytes at an offset. P is the first position that does
 * not decode; everything before it must follow the acceptance rules; nothing complete may follow P.
 */
fun scanLog(size: Long, read: (Long, Int) -> ByteArray): ScanOut {
    val rows = ArrayList<MetaRow>()
    var pos = 0L
    var next = 1L
    var problem: String? = null
    while (pos < size) {
        val head = read(pos, minOf(HDR.toLong(), size - pos).toInt())
        val sz = recordSize(head) ?: break
        if (pos + sz > size) break
        val rec = (parseRecord(read(pos, sz)) as? Parsed.Ok)?.rec ?: break
        val v = if (rec.opaqueGap) null else verdict(rec, next)
        if (v != Verdict.ACCEPT) {
            problem = problem ?: "record at $pos (seq ${rec.hdr.seq}) is ${v ?: "an opaque gap"} at durableNext=$next"
        } else {
            rows += rowFor(rec, pos, next)
            next = rec.last + 1
        }
        pos += sz
    }
    // A torn tail never has a complete record behind it; if one does, the damage is in the middle.
    if (problem == null && pos < size) {
        val tail = read(pos, (size - pos).toInt())
        for (i in 1 until tail.size - HDR) {
            if (tail[i] == 'L'.code.toByte() && (parseRecord(tail, i) is Parsed.Ok)) {
                problem = "complete record at ${pos + i} behind damage at $pos"
                break
            }
        }
    }
    return ScanOut(rows, next, pos, problem)
}

/** Pure form for tests and tools. */
fun scanBytes(b: ByteArray) = scanLog(b.size.toLong()) { o, n -> b.copyOfRange(o.toInt(), minOf(b.size, o.toInt() + n)) }

/**
 * Append-only log of the records accepted from one device. The file is the only truth: no state file, no index.
 * durableNext is the next seq after the fsynced, contiguous prefix; ACK and SYNC may only carry durableNext - 1.
 */
class RecordLog private constructor(private val fs: Fs, private val dir: String, private val through: String, rows: List<MetaRow>, next: Long, private var size: Long) {
    private val path = "$dir/records.log"
    private val table = ArrayList(rows)
    private var dirSyncPending = size == 0L // an empty file may be a leftover whose directory entry was never fsynced
    val rows: List<MetaRow> get() = table
    var durableNext = next; private set
    var failed = false; private set

    companion object {
        /**
         * Opens (creating nothing) the log in [dir]. LogInconsistent leaves the file untouched; a torn tail is truncated only when consistent.
         * The entries leading to records.log are fsynced up to [through]: by default the parent of the root (dir is root/devices/<key>).
         */
        fun open(fs: Fs, dir: String, through: String = dir.substringBeforeLast('/', "").substringBeforeLast('/', "").substringBeforeLast('/', "")): RecordLog {
            val path = "$dir/records.log"
            val size = if (fs.exists(path)) fs.size(path) else 0L
            val out = scanLog(size) { o, n -> fs.readAt(path, o, n) }
            out.problem?.let { throw LogInconsistent(it) }
            // Readable is not durable: a record whose fsync failed earlier may still sit in the page cache. Make the whole
            // prefix (and the directory entries leading to it) durable before durableNext can reach a SYNC or ACK.
            if (size > 0) {
                try {
                    if (out.p < size) fs.truncate(path, out.p)
                    fs.sync(path); fs.syncUp(dir, through)
                } catch (e: Exception) { throw StoreFailure(e) }
            }
            return RecordLog(fs, dir, through, out.rows, out.durableNext, out.p)
        }
    }

    fun verdict(rec: Record) = verdict(rec, durableNext)

    /** Stores an ACCEPT record exactly as received, fsyncs, then moves durableNext. */
    fun append(rec: Record) {
        check(!failed) { "log is dead" }
        check(verdict(rec) == Verdict.ACCEPT && !rec.opaqueGap) { "not acceptable at $durableNext" }
        try {
            fs.mkdirsDurable(dir)
            fs.append(path, rec.raw)
            fs.sync(path)
            if (dirSyncPending) { fs.syncUp(dir, through); dirSyncPending = false }
        } catch (e: Exception) {
            failed = true
            throw StoreFailure(e)
        }
        table += rowFor(rec, size, durableNext)
        size += rec.raw.size
        durableNext = rec.last + 1
    }
}
