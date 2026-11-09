package dev.lifeos.pendant

/** UTC (unix ms) of row [i], the same rule as pendant_rx.utc_ms, from the table only. Null = unknown. */
fun utcMs(rows: List<MetaRow>, i: Int): Long? {
    val r = rows[i]
    if (!r.known) return null
    if (r.hdr.flags and F_RTC_INVALID == 0 && r.hdr.rtcMs != 0L) return r.hdr.rtcMs
    for (j in i + 1 until rows.size) {
        val t = rows[j]
        val d = t.decoded
        if (t.known && d is Decoded.Timefix && t.hdr.boot == r.hdr.boot) return d.afterMs - (t.hdr.monoMs - r.hdr.monoMs)
    }
    return null
}

/** The 32-byte INFO characteristic (fw/src/ble.c info_read). */
class Info(val version: Int, val recording: Boolean, val boot: Long, val tail: Long, val next: Long, val acked: Long, val uptimeS: Long, val rtcMs: Long) {
    companion object {
        fun parse(b: ByteArray?): Info? = if (b == null || b.size < 32) null else
            Info(b.u8(0), b.u8(1) != 0, b.u32(4), b.u32(8), b.u32(12), b.u32(16), b.u32(20), b.u64(24))
    }
}

/** CTRL commands (fw/src/ble.h). */
object Ctrl {
    fun ack(v: Long) = byteArrayOf(1, 0, 0, 0, 0).also { it.put32(1, v) }
    fun sync(have: Long) = byteArrayOf(2, 0, 0, 0, 0).also { it.put32(1, have) }
    fun time(ms: Long) = ByteArray(9).also { it[0] = 3; it.put64(1, ms) }
    fun resend(seq: Long, off: Long) = ByteArray(9).also { it[0] = 4; it.put32(1, seq); it.put32(5, off) }
    val STOP = byteArrayOf(5)
}

/** Every Link callback and every timer runs on one thread (the platform's main thread; a virtual clock in tests). */
interface Scheduler {
    fun nowMs(): Long
    /** Runs [fn] after [ms]; the returned function cancels it. */
    fun after(ms: Long, fn: () -> Unit): () -> Unit
}

class ScanItem(val id: String, val name: String)

interface LinkListener {
    /** GATT is ready (services found, MTU set). [id] is the OS identifier of the device that answered. */
    fun onConnected(id: String)
    fun onData(bytes: ByteArray)
    fun onDisconnected()
}

/** The BLE side, callback style so Swift can implement it without a coroutine bridge. */
interface Link {
    /** Scans for the pendant service; reports each device until [stopScan]. */
    fun scan(onFound: (ScanItem) -> Unit)
    fun stopScan()
    fun connect(id: String, listener: LinkListener)
    /** The 32-byte INFO, or null on failure. */
    fun readInfo(done: (ByteArray?) -> Unit)
    fun enableData(done: (Boolean) -> Unit)
    /** One CTRL write; a failure shows up as onDisconnected. */
    fun writeCtrl(bytes: ByteArray)
    fun disconnect()
}
