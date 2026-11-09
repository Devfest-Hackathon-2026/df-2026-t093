package dev.lifeos.pendant

/** xorshift64: the same seed gives the same run on every target. */
class Rng(seed: Long) {
    private var s = if (seed == 0L) 0x9E3779B97F4A7C15uL.toLong() else seed
    fun next(): Long { s = s xor (s shl 13); s = s xor (s ushr 7); s = s xor (s shl 17); return s ushr 11 }
    fun int(n: Int) = (next() % n).toInt()
    fun chance(perMille: Int) = int(1000) < perMille
}

class FakeCfg(
    val lossPm: Int = 0, val dupPm: Int = 0, val corruptPm: Int = 0, val reorderPm: Int = 0,
    /** Per fragment: the link drops (out of range). */
    val disconnectPm: Int = 0,
    val ringBytes: Int = 1 shl 20, val recordEveryMs: Long = 500, val minPayload: Int = 100, val maxPayload: Int = 1000,
    val frag: Int = 244, val eventPm: Int = 30,
)

/**
 * Kotlin port of the pendant side: the ring (tail/next/acked, reclaim, overwrite) and fw/lib/sync's sync_run, one
 * fragment per step, with loss, duplication, corruption, reordering and disconnects injected on the way out.
 * Everything it writes is kept in [written] so tests can compare what the phone stored.
 */
class FakePendant(val id: String, val name: String, seed: Long, val cfg: FakeCfg, private val sched: Scheduler, var boot: Long = 1, startSeq: Long = 1, private val keepWritten: Boolean = true) {
    private val rng = Rng(seed)
    private val ring = ArrayList<ByteArray>() // ring[i] is seq tail + i
    private var ringBytes = 0
    var tail = startSeq; private set
    var next = startSeq; private set
    var acked = startSeq - 1; private set
    var recording = true; private set
    var clockSet = false; private set
    private var rtcOffset = 0L
    private val t0 = sched.nowMs()
    val written = HashMap<Long, ByteArray>()
    var syncs = 0; private set

    private class Cmd(val op: Int, val a: Long, val b: Long, val t: Long)
    private class S(var have: Long) { var restart = false; var abort = false; var err = false; val rs = ArrayList<Pair<Long, Long>>() }

    private val queue = ArrayDeque<Cmd>()
    private var job: Iterator<Unit>? = null
    private var ticking = false
    private var deliver: ((ByteArray) -> Unit)? = null
    private var dropped: (() -> Unit)? = null
    private var gen = 0

    fun monoMs() = sched.nowMs() - t0
    fun rtcNow() = if (clockSet) sched.nowMs() + rtcOffset else 0L
    val connected get() = deliver != null

    // ---------------- ring ----------------

    fun ringSeqs(): List<Long> = List(ring.size) { tail + it }
    fun ringRecord(seq: Long): ByteArray? = if (seq in tail until next) ring[(seq - tail).toInt()] else null

    private fun append(type: Int, payload: ByteArray): Long {
        val seq = next
        val h = Header(type, if (type == T_AUDIO) 1 else 0, if (clockSet) 0 else F_RTC_INVALID, seq, boot, monoMs(), rtcNow(), payload.size)
        val raw = encodeRecord(h, payload)
        while (ring.isNotEmpty() && ringBytes + raw.size > cfg.ringBytes) dropOldest()
        ring += raw; ringBytes += raw.size; next++
        if (keepWritten) written[seq] = raw
        return seq
    }

    private fun dropOldest() { ringBytes -= ring.removeAt(0).size; tail++ }
    fun reclaim() { while (ring.isNotEmpty() && tail <= acked) dropOldest() }
    private fun ringAck(v: Long) { if (v > acked && v < next) acked = v }

    fun recordOne() {
        if (rng.chance(cfg.eventPm)) {
            append(T_EVENT, byteArrayOf(1, 0, 0, 0, 0, 0, 0, 0).also { it.put32(4, monoMs()) })
            return
        }
        val n = (cfg.minPayload + rng.int((cfg.maxPayload - cfg.minPayload) / 4 + 1) * 4).coerceAtMost(cfg.maxPayload)
        append(T_AUDIO, ByteArray(n) { rng.int(256).toByte() })
    }

    fun startRecorder() {
        sched.after(cfg.recordEveryMs) { if (recording) { recordOne(); startRecorder() } }
    }

    /** Stops recording, and with it every overwrite. */
    fun freeze() { recording = false }

    private fun setClock(t: Long) {
        val before = rtcNow()
        clockSet = true
        rtcOffset = t - sched.nowMs()
        if (recording) append(T_TIMEFIX, ByteArray(16).also { it.put64(0, before); it.put64(8, t) }) // frozen means no writes at all
    }

    fun info(): ByteArray = ByteArray(32).also {
        it[0] = 1; it[1] = if (recording) 1 else 0
        it.put32(4, boot); it.put32(8, tail); it.put32(12, next); it.put32(16, acked); it.put32(20, monoMs() / 1000); it.put64(24, rtcNow())
    }

    // ---------------- link ----------------

    fun attach(deliver: (ByteArray) -> Unit, dropped: () -> Unit): Boolean {
        if (this.deliver != null) return false
        this.deliver = deliver; this.dropped = dropped; gen++
        return true
    }

    /** Either side ends the link; the firmware answers with STOP and a reclaim, as ble.c does on disconnect. */
    fun disconnect(notifyPhone: Boolean = false) {
        val d = dropped
        deliver = null; dropped = null; gen++
        queue += Cmd(5, 0, 0, 0); queue += Cmd(0x80, 0, 0, 0)
        kick()
        if (notifyPhone) d?.invoke()
    }

    fun command(b: ByteArray) {
        if (!connected || b.isEmpty()) return
        val c = when (b[0].toInt()) {
            1, 2 -> Cmd(b[0].toInt(), b.u32(1), 0, 0)
            3 -> Cmd(3, 0, 0, b.u64(1))
            4 -> Cmd(4, b.u32(1), b.u32(5), 0)
            5 -> Cmd(5, 0, 0, 0)
            else -> return
        }
        queue += c
        kick()
    }

    private fun kick() {
        if (ticking) return
        ticking = true
        sched.after(2) { tick() }
    }

    private fun tick() {
        ticking = false
        step()
        if (job != null || queue.isNotEmpty()) kick()
    }

    private fun step() {
        job?.let { if (it.hasNext()) it.next() else job = null; return }
        val c = queue.removeFirstOrNull() ?: return
        when (c.op) {
            2 -> { syncs++; job = syncRun(c.a) }
            4 -> job = iterator { val s = S(0); yieldAll(resend(s, c.a, c.b)) }
            else -> apply(c, null)
        }
    }

    private fun apply(c: Cmd, s: S?) {
        when (c.op) {
            1 -> ringAck(c.a)
            3 -> setClock(c.t)
            0x80 -> reclaim()
            2 -> s?.let { it.have = c.a; it.restart = true }
            5 -> s?.abort = true
            4 -> s?.let {
                val i = it.rs.indexOfFirst { p -> p.first == c.a }
                if (i < 0) { if (it.rs.size < 8) it.rs += c.a to c.b } else it.rs[i] = c.a to minOf(it.rs[i].second, c.b)
            }
        }
    }

    private fun drain(s: S) { while (true) apply(queue.removeFirstOrNull() ?: break, s) }

    /** One notification out through the damaged radio. False when the link is gone. */
    private fun tx(pkt: ByteArray): Boolean {
        val d = deliver ?: return false
        if (rng.chance(cfg.disconnectPm)) { disconnect(notifyPhone = true); return false }
        if (rng.chance(cfg.lossPm)) return true
        var p = pkt
        if (rng.chance(cfg.corruptPm)) { p = pkt.copyOf(); val i = rng.int(p.size * 8); p[i / 8] = (p[i / 8].toInt() xor (1 shl (i % 8))).toByte() }
        val g = gen
        val late = if (rng.chance(cfg.reorderPm)) 5L + rng.int(30) else 0L
        sched.after(1 + late) { if (g == gen) d(p) }
        if (rng.chance(cfg.dupPm)) sched.after(1L + rng.int(20)) { if (g == gen) d(p) }
        return true
    }

    // ---------------- fw/lib/sync/sync.c ----------------

    private fun sendRecord(s: S, seq: Long, rec: ByteArray, off0: Int) = iterator<Unit> {
        var off = off0
        while (off < rec.size) {
            val n = minOf(cfg.frag, rec.size - off)
            val pkt = ByteArray(8 + n)
            pkt.put32(0, seq); pkt.put32(4, off.toLong())
            rec.copyInto(pkt, 8, off, off + n)
            if (!tx(pkt)) { s.err = true; return@iterator }
            off += cfg.frag
            yield(Unit)
        }
    }

    private fun sendGap(s: S, first: Long, last: Long) =
        sendRecord(s, first, encodeRecord(Header(T_GAP, 0, 0, first, 0, monoMs(), rtcNow(), 12), gapPayload(first, last)), 0)

    private fun resend(s: S, seq: Long, off: Long) = iterator<Unit> {
        val rec = ringRecord(seq)
        if (rec != null) yieldAll(sendRecord(s, seq, rec, off.toInt()))
        else if (tail > seq) yieldAll(sendGap(s, seq, tail - 1))
    }

    private fun syncRun(have: Long) = iterator<Unit> {
        val s = S(have)
        while (true) {
            s.restart = false
            ringAck(s.have)
            val tl = tail
            var nx = next
            if (tl > s.have + 1) { yieldAll(sendGap(s, s.have + 1, tl - 1)); if (s.err) return@iterator }
            var seq = maxOf(tl, s.have + 1)
            while (seq < nx && !s.restart && !s.abort) {
                val rec = ringRecord(seq)
                if (seq + 1 == nx) nx = next
                drain(s)
                if (s.restart || s.abort) break
                if (rec == null) { // overwritten while we were sending: say so, or the phone waits for it forever
                    val t = tail
                    if (t > seq) { yieldAll(sendGap(s, seq, t - 1)); if (s.err) return@iterator }
                    seq = maxOf(seq, t - 1) + 1
                    continue
                }
                yieldAll(sendRecord(s, seq, rec, 0)); if (s.err) return@iterator
                drain(s)
                while (s.rs.isNotEmpty() && !s.restart && !s.abort) {
                    val (rs, ro) = s.rs.removeAt(0)
                    yieldAll(resend(s, rs, ro)); if (s.err) return@iterator
                }
                seq++
            }
            if (s.restart) continue
            if (s.abort) return@iterator
            tx(ByteArray(8).also { it.put32(4, next) })
            return@iterator
        }
    }
}

/** A Link to a FakePendant. [reportedId] lets a test pretend a different device answered. */
class FakeLink(val pendant: FakePendant, private val sched: Scheduler, private val reportedId: String = pendant.id) : Link {
    /** What the phone did, in order: connect, info, enable, ctrl:<op>:<a>, disconnect. */
    val trace = ArrayList<String>()
    /** Every CTRL write, for tests (checked at the moment of the write). */
    var onCtrl: ((ByteArray) -> Unit)? = null
    private var cancelScan: (() -> Unit)? = null
    private var closed = true
    private var gen = 0 // bumped by connect and disconnect: a callback from an older connection is dropped
    private var cancelAttach: (() -> Unit)? = null

    override fun scan(onFound: (ScanItem) -> Unit) {
        cancelScan = sched.after(100) { onFound(ScanItem(pendant.id, pendant.name)) }
    }

    override fun stopScan() { cancelScan?.invoke() }

    override fun connect(id: String, listener: LinkListener) {
        trace += "connect"
        cancelAttach?.invoke()
        val g = ++gen
        cancelAttach = sched.after(50) {
            if (g != gen) return@after
            val ok = pendant.attach({ b -> if (g == gen && !closed) listener.onData(b) }, { if (g == gen && !closed) { closed = true; listener.onDisconnected() } })
            if (ok) { closed = false; listener.onConnected(reportedId) } else listener.onDisconnected()
        }
    }

    override fun readInfo(done: (ByteArray?) -> Unit) {
        trace += "info"
        val g = gen
        sched.after(10) { if (g == gen) done(if (closed) null else pendant.info()) }
    }

    override fun enableData(done: (Boolean) -> Unit) {
        trace += "enable"
        val g = gen
        sched.after(10) { if (g == gen) done(!closed) }
    }

    override fun writeCtrl(bytes: ByteArray) {
        trace += "ctrl:${bytes[0]}" + if (bytes.size == 5) ":${bytes.u32(1)}" else ""
        onCtrl?.invoke(bytes)
        sched.after(1) { pendant.command(bytes) }
    }

    override fun disconnect() {
        trace += "disconnect"
        cancelAttach?.invoke(); cancelAttach = null; gen++
        if (!closed) { closed = true; sched.after(2) { pendant.disconnect() } } // after the CTRL writes already in flight
    }
}
