package dev.lifeos.pendant

enum class ErrKind { MISMATCH, SAVE, LOG_INCONSISTENT, UNSUPPORTED }

sealed interface End {
    class Done(val accepted: Int, val bytes: Long) : End
    /** Dropped, timed out or gave up: reconnect later, nothing is wrong with the data. */
    class Incomplete(val why: String, val accepted: Int, val bytes: Long) : End
    /** Needs the user: no automatic reconnect. */
    class Error(val kind: ErrKind, val detail: String) : End
}

interface SessionListener {
    fun onInfo(info: Info)
    fun onProgress(accepted: Int)
    fun onEnd(end: End)
}

const val DATA_TIMEOUT_MS = 20_000L
const val END_RETRY_MS = 3_000L
const val END_RETRIES = 6
private const val BAD_RETRIES = 5

/**
 * One sync (formats.md §11): connect, check who answered, open that device's log, read INFO, save boot, enable DATA,
 * TIME, SYNC(durableNext-1), then store every record and ACK what is fsynced. ACK and SYNC carry only
 * log.durableNext - 1, because the pendant may erase everything up to that number.
 */
class SyncSession(
    private val link: Link, private val sched: Scheduler, private val store: DeviceStore, private val reg: Registration,
    private val setTime: Boolean, private val utcNow: () -> Long, private val listener: SessionListener,
) : LinkListener {
    var log: RecordLog? = null; private set
    var accepted = 0; private set
    private var bytes = 0L
    private var ended = false
    private var synced = false
    private var lastAck = 0L
    private var endNext: Long? = null
    private var retries = 0
    private var cancelTimer: (() -> Unit)? = null
    private val parts = HashMap<Long, ByteArray>()
    private val waiting = HashMap<Long, Long>() // seq -> offset a RESEND was asked for: other fragments of it are dropped
    private val badTries = HashMap<Long, Int>()
    private val ahead = HashMap<Long, Record>() // keyed by first; the earlier arrival wins

    fun start() {
        arm(30_000) // connecting, INFO and DATA enable must not hang
        link.connect(reg.id, this)
    }

    /** The user or the app leaves the foreground. */
    fun abort(why: String) = finish(End.Incomplete(why, accepted, bytes))

    private fun ctrl(b: ByteArray) = link.writeCtrl(b)

    private fun finish(e: End) {
        if (ended) return
        ended = true
        cancelTimer?.invoke()
        link.disconnect()
        listener.onEnd(e)
    }

    private fun fail(kind: ErrKind, d: String) = finish(End.Error(kind, d))

    override fun onDisconnected() = finish(End.Incomplete("disconnected", accepted, bytes))

    override fun onConnected(id: String) {
        if (ended) return
        if (id != reg.id) return fail(ErrKind.MISMATCH, "connected to $id, registered ${reg.id}")
        try {
            log = store.openLog(id)
        } catch (e: LogInconsistent) {
            return fail(ErrKind.LOG_INCONSISTENT, e.message ?: "")
        } catch (e: Exception) {
            return fail(ErrKind.SAVE, e.message ?: "")
        }
        link.readInfo { raw -> if (!ended) onInfoRead(raw) }
    }

    private fun onInfoRead(raw: ByteArray?) {
        val l = log!!
        if (raw == null) return finish(End.Incomplete("INFO read failed", accepted, bytes))
        val i = Info.parse(raw) ?: return fail(ErrKind.MISMATCH, "INFO is ${raw.size} bytes")
        val old: DeviceMeta?
        try { old = store.device(reg.id) } catch (e: Exception) { return fail(ErrKind.SAVE, e.message ?: "") }
        if (i.version != 1) return fail(ErrKind.MISMATCH, "INFO version ${i.version}")
        if (i.next - 1 < l.durableNext - 1) return fail(ErrKind.MISMATCH, "pendant next_seq ${i.next} is behind our durableNext ${l.durableNext}")
        if (i.boot < (old?.maxBoot ?: 0)) return fail(ErrKind.MISMATCH, "pendant boot ${i.boot} < seen ${old?.maxBoot}")
        listener.onInfo(i)
        try { store.saveDevice(DeviceMeta(reg.id, reg.name, maxOf(i.boot, old?.maxBoot ?: 0))) } catch (e: Exception) { return fail(ErrKind.SAVE, e.message ?: "") }
        link.enableData { ok ->
            if (ended) return@enableData
            if (!ok) return@enableData finish(End.Incomplete("enable DATA failed", accepted, bytes))
            if (setTime) ctrl(Ctrl.time(utcNow()))
            lastAck = l.durableNext - 1
            ctrl(Ctrl.sync(lastAck))
            synced = true
            arm(DATA_TIMEOUT_MS)
        }
    }

    private fun arm(ms: Long) {
        cancelTimer?.invoke()
        cancelTimer = sched.after(ms) { onTimeout() }
    }

    private fun onTimeout() {
        if (ended) return
        if (endNext == null) return finish(End.Incomplete("no data for ${DATA_TIMEOUT_MS / 1000} s", accepted, bytes))
        if (retries >= END_RETRIES) return finish(End.Incomplete("still missing after $END_RETRIES retries", accepted, bytes))
        retries++
        val d = log!!.durableNext
        askResend(d, parts[d]?.size?.toLong() ?: 0)
        arm(END_RETRY_MS)
    }

    private fun askResend(seq: Long, have: Long) {
        waiting[seq] = have
        ctrl(Ctrl.resend(seq, have))
    }

    override fun onData(bytes: ByteArray) {
        if (ended || !synced || bytes.size < 8) return
        if (endNext == null) arm(DATA_TIMEOUT_MS) // after the end marker only the first marker and progress re-arm: duplicates must not push the retry back
        val seq = bytes.u32(0)
        val off = bytes.u32(4)
        if (seq == 0L) {
            if (endNext == null) arm(END_RETRY_MS)
            endNext = off
            return checkDone()
        }
        this.bytes += bytes.size - 8
        val w = waiting[seq]
        if (w != null) {
            if (off == 0L) parts.remove(seq) else if (off != w) return // the rest of a part we know is lost
        }
        // fragments of an earlier record that never completed: a hole, ask once
        if (off == 0L) parts.filter { (q, p) -> q < seq && q !in waiting && p.size < (recordSize(p) ?: Int.MAX_VALUE) }
            .forEach { (q, p) -> askResend(q, p.size.toLong()) }
        val buf = parts[seq] ?: ByteArray(0)
        if (off > buf.size) return askResend(seq, buf.size.toLong())
        var cur = buf
        if (off < buf.size) { // a repeat of what we have, or a fresh start of a record (gap replacing it)
            val same = bytes.copyOfRange(8, bytes.size).let { d -> d.indices.all { k -> off + k >= buf.size || buf[(off + k).toInt()] == d[k] } }
            if (same || off != 0L) return
            cur = ByteArray(0)
        }
        waiting.remove(seq)
        cur += bytes.copyOfRange(8, bytes.size)
        parts[seq] = cur
        if (cur.size < HDR) return
        val size = recordSize(cur) ?: return bad(seq)
        if (cur.size < size) return
        parts.remove(seq)
        when (val p = parseRecord(cur.copyOf(size))) {
            is Parsed.Bad -> bad(seq)
            is Parsed.Ok -> ingest(p.rec)
        }
    }

    private fun bad(seq: Long) {
        parts.remove(seq)
        val n = (badTries[seq] ?: 0) + 1
        badTries[seq] = n
        if (n > BAD_RETRIES) return finish(End.Incomplete("record $seq keeps failing its CRC", accepted, bytes))
        askResend(seq, 0)
    }

    private fun ingest(rec: Record) {
        val l = log!!
        if (rec.opaqueGap) return fail(ErrKind.UNSUPPORTED, "gap record at seq ${rec.hdr.seq} in an unknown format")
        when (l.verdict(rec)) {
            Verdict.DUP -> {}
            Verdict.AHEAD -> ahead.getOrPut(rec.first) { rec }
            Verdict.ACCEPT -> if (!store(rec)) return
        }
        while (!ended) {
            val d = l.durableNext
            ahead.keys.filter { ahead[it]!!.last < d }.forEach { ahead.remove(it) }
            val c = ahead.values.filter { it.first <= d }.minByOrNull { it.first } ?: break
            ahead.remove(c.first)
            if (!store(c)) return
        }
        val v = l.durableNext - 1
        if (v > lastAck) { ctrl(Ctrl.ack(v)); lastAck = v; if (endNext != null) arm(END_RETRY_MS) }
        checkDone()
    }

    /** fsync first, count after: only then may anything be acknowledged. */
    private fun store(rec: Record): Boolean = try {
        log!!.append(rec)
        accepted++
        listener.onProgress(accepted)
        true
    } catch (e: StoreFailure) {
        fail(ErrKind.SAVE, e.message ?: "")
        false
    }

    private fun checkDone() {
        val e = endNext ?: return
        val l = log!!
        if (l.durableNext < e) return
        if (l.durableNext > 1) ctrl(Ctrl.ack(l.durableNext - 1))
        finish(End.Done(accepted, bytes))
    }
}
