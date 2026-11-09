package dev.lifeos.pendant

import kotlin.test.assertEquals
import kotlin.test.assertTrue

const val EPOCH = 1_760_000_000_000L

class VirtualClock : Scheduler {
    private class Ev(val t: Long, val n: Long, val fn: () -> Unit) { var dead = false }
    private var now = 0L
    private var n = 0L
    private val q = ArrayList<Ev>() // sorted descending: the next event is last

    override fun nowMs() = now
    override fun after(ms: Long, fn: () -> Unit): () -> Unit {
        val e = Ev(now + ms, n++, fn)
        var lo = 0; var hi = q.size
        while (lo < hi) { val m = (lo + hi) / 2; if (q[m].t > e.t || (q[m].t == e.t && q[m].n > e.n)) lo = m + 1 else hi = m }
        q.add(lo, e)
        return { e.dead = true }
    }

    /** Runs events until [cond] holds (true) or [limitMs] of virtual time pass (false). */
    fun runUntil(limitMs: Long, cond: () -> Boolean): Boolean {
        val end = now + limitMs
        while (true) {
            if (cond()) return true
            val e = q.lastOrNull()
            if (e == null || e.t > end) { now = end; return cond() }
            q.removeAt(q.size - 1)
            now = e.t
            if (!e.dead) e.fn()
        }
    }

    fun runFor(ms: Long) { runUntil(ms) { false } }
}

class MemFs : Fs {
    class F(var durable: ByteArray = ByteArray(0), var vol: ByteArray = ByteArray(0))
    val files = HashMap<String, F>()
    val dirs = HashSet<String>()
    /** (op, path, bytes) -> throw. */
    var failHook: ((String, String, ByteArray?) -> Boolean)? = null
    var onFail: (() -> Unit)? = null
    var reads = 0; var writes = 0; var truncates = 0
    /** Directories and files whose entry in the parent has not been fsynced yet: a power cut loses them (and what is inside). */
    var strictDirs = false // off: directories and new files are durable at once (tests that lay out files by hand)
    val unsyncedDirs = HashSet<String>()
    val unlinkedFiles = HashSet<String>()

    private fun check(op: String, path: String, b: ByteArray? = null) {
        if (failHook?.invoke(op, path, b) == true) { onFail?.invoke(); throw RuntimeException("injected failure in $op $path") }
    }
    private fun parents(path: String) { var p = path.substringBeforeLast('/', ""); while (p.isNotEmpty()) { dirs += p; p = p.substringBeforeLast('/', "") } }

    override fun exists(path: String) = path in files || path in dirs
    override fun mkdirs(path: String) {
        var p = path
        while (p.isNotEmpty() && p !in dirs) { if (strictDirs && '/' in p.substring(1)) unsyncedDirs += p; p = p.substringBeforeLast('/', "") }
        dirs += path; parents(path)
    }
    override fun list(dir: String) = (files.keys + dirs).filter { it.substringBeforeLast('/', "") == dir && !it.endsWith(".tmp") }.map { it.substringAfterLast('/') }.distinct().sorted()
    override fun size(path: String) = files[path]!!.vol.size.toLong()
    override fun readAt(path: String, off: Long, len: Int): ByteArray {
        reads++
        val v = files[path]?.vol ?: throw RuntimeException("no file $path")
        return v.copyOfRange(minOf(off.toInt(), v.size), minOf(v.size, off.toInt() + len))
    }
    override fun append(path: String, bytes: ByteArray) { check("append", path, bytes); writes++; parents(path); if (strictDirs && path !in files) unlinkedFiles += path; val f = files.getOrPut(path) { F() }; f.vol += bytes }
    override fun sync(path: String) { check("sync", path); files[path]?.let { it.durable = it.vol } }
    override fun truncate(path: String, len: Long) { check("truncate", path); truncates++; files[path]!!.let { it.vol = it.vol.copyOf(len.toInt()) } }
    override fun syncDir(dir: String) {
        check("syncDir", dir)
        unsyncedDirs.removeAll { it.substringBeforeLast('/', "") == dir }
        unlinkedFiles.removeAll { it.substringBeforeLast('/', "") == dir }
    }
    override fun replace(path: String, bytes: ByteArray) { check("replace", path, bytes); writes++; parents(path); files[path] = F(bytes, bytes); unlinkedFiles -= path }
    override fun delete(path: String) { files.remove(path); dirs.remove(path) }

    fun durableBytes(path: String) = files[path]?.durable
    fun bytes(path: String) = files[path]?.vol

    /** Power cut: unsynced data is gone, except that the first [keep] unsynced bytes made it. */
    fun crash(keep: Int = 0) {
        for (d in unsyncedDirs) { dirs.removeAll { it == d || it.startsWith("$d/") }; files.keys.removeAll { it.startsWith("$d/") } }
        files.keys.removeAll(unlinkedFiles)
        unsyncedDirs.clear(); unlinkedFiles.clear()
        for (f in files.values) {
            val d = f.durable
            f.vol = if (f.vol.size > d.size && f.vol.copyOf(d.size).contentEquals(d)) f.vol.copyOf(d.size + minOf(keep, f.vol.size - d.size)) else d
            f.durable = f.vol
        }
    }

    fun putLog(dir: String, bytes: ByteArray) { mkdirs(dir); files["$dir/records.log"] = F(bytes, bytes) }
}

// ---------------- record builders ----------------

fun rec(type: Int, seq: Long, payload: ByteArray, codec: Int = if (type == T_AUDIO) 1 else 0, flags: Int = 0, boot: Long = 1, mono: Long = seq * 100, rtc: Long = 0) =
    encodeRecord(Header(type, codec, flags, seq, boot, mono, rtc, payload.size), payload)
fun audio(seq: Long, len: Int = 20, flags: Int = F_RTC_INVALID, boot: Long = 1, mono: Long = seq * 100, rtc: Long = 0, fill: Int = seq.toInt()) =
    rec(T_AUDIO, seq, ByteArray(len) { (fill + it).toByte() }, flags = flags, boot = boot, mono = mono, rtc = rtc)
fun timefix(seq: Long, after: Long, boot: Long = 1, mono: Long = seq * 100, flags: Int = 0, len: Int = 16) =
    rec(T_TIMEFIX, seq, ByteArray(len).also { if (len >= 16) it.put64(8, after) }, flags = flags, boot = boot, mono = mono)
fun gap(first: Long, last: Long, seq: Long = first, flags: Int = 0, len: Int = 12, boot: Long = 0) =
    rec(T_GAP, seq, gapPayload(first, last).copyOf(len), flags = flags, boot = boot)
fun event(seq: Long, kind: Int = 1) = rec(T_EVENT, seq, byteArrayOf(kind.toByte(), 0, 0, 0, 0, 0, 0, 0))
fun log(vararg r: ByteArray) = r.fold(ByteArray(0)) { a, b -> a + b }

fun parse(raw: ByteArray) = (parseRecord(raw) as Parsed.Ok).rec

fun checkCoverage(rows: List<MetaRow>, durableNext: Long) {
    val cnt = IntArray(durableNext.toInt() + 1)
    for (r in rows) for (s in r.covFirst..r.covLast) cnt[s.toInt()]++
    assertEquals(0, cnt[0])
    for (s in 1 until durableNext.toInt()) assertEquals(1, cnt[s], "seq $s covered ${cnt[s]} times")
    assertEquals(0, cnt[durableNext.toInt()])
}

fun frags(seq: Long, raw: ByteArray, from: Int = 0, frag: Int = 244): List<ByteArray> =
    (from until raw.size step frag).map { off -> ByteArray(8 + minOf(frag, raw.size - off)).also { it.put32(0, seq); it.put32(4, off.toLong()); raw.copyInto(it, 8, off, off + it.size - 8) } }

fun infoBytes(version: Int = 1, boot: Long = 1, tail: Long = 1, next: Long, acked: Long = 0) =
    ByteArray(32).also { it[0] = version.toByte(); it.put32(4, boot); it.put32(8, tail); it.put32(12, next); it.put32(16, acked) }

/** A link whose far end is a script, for tests that need exact fragment order. */
class ScriptLink(private val sched: Scheduler, val id: String, var info: ByteArray) : Link {
    val ctrls = ArrayList<ByteArray>()
    val ctrlAt = ArrayList<Long>()
    val trace = ArrayList<String>()
    var reportedId = id
    var onSync: (Long) -> Unit = {}
    var onResend: (Long, Long) -> Unit = { _, _ -> }
    var onCtrl: (ByteArray) -> Unit = {}
    lateinit var l: LinkListener
    var closed = false

    override fun scan(onFound: (ScanItem) -> Unit) { sched.after(100) { onFound(ScanItem(id, "pendant")) } }
    override fun stopScan() {}
    override fun connect(id: String, listener: LinkListener) { trace += "connect"; l = listener; closed = false; sched.after(10) { l.onConnected(reportedId) } }
    override fun readInfo(done: (ByteArray?) -> Unit) { trace += "info"; sched.after(5) { done(info) } }
    override fun enableData(done: (Boolean) -> Unit) { trace += "enable"; sched.after(5) { done(true) } }
    override fun writeCtrl(bytes: ByteArray) {
        ctrls += bytes; ctrlAt += sched.nowMs(); trace += "ctrl:${bytes[0]}"; onCtrl(bytes)
        when (bytes[0].toInt()) {
            2 -> sched.after(1) { onSync(bytes.u32(1)) }
            4 -> sched.after(1) { onResend(bytes.u32(1), bytes.u32(5)) }
        }
    }
    override fun disconnect() { trace += "disconnect"; closed = true }

    fun data(b: ByteArray) { if (!closed) l.onData(b) }
    fun send(raw: ByteArray, seq: Long = raw.u32(8), from: Int = 0) = frags(seq, raw, from).forEach { data(it) }
    fun end(next: Long) = data(ByteArray(8).also { it.put32(4, next) })
    /** ACK / SYNC values in order, as (op, value). */
    fun acks() = ctrls.filter { it[0].toInt() == 1 }.map { it.u32(1) }
    fun syncs() = ctrls.filter { it[0].toInt() == 2 }.map { it.u32(1) }
}

class Sess(val fs: MemFs, val clock: VirtualClock, val link: Link, val reg: Registration, setTime: Boolean = false, root: String = "/real") {
    val store = DeviceStore(fs, root)
    var end: End? = null
    var info: Info? = null
    val session = SyncSession(link, clock, store, reg, setTime, { EPOCH + clock.nowMs() }, object : SessionListener {
        override fun onInfo(info: Info) { this@Sess.info = info }
        override fun onProgress(accepted: Int) {}
        override fun onEnd(end: End) { this@Sess.end = end }
    })
    fun run(limitMs: Long = 300_000): End { session.start(); assertTrue(clock.runUntil(limitMs) { end != null }, "session did not end"); return end!! }
    fun logPath() = "${store.dir(reg.id)}/records.log"
}

/** The whole app against one FakePendant: controller, virtual clock, memory file system. */
class World(
    seed: Long, cfg: FakeCfg, settings: Settings = Settings(), val id: String = "AA:BB:CC:DD:EE:01",
    val fs: MemFs = MemFs(), val clock: VirtualClock = VirtualClock(), pendantBoot: Long = 1, root: String = "/real",
) {
    val pendant = FakePendant(id, "LifeOS-Pendant", seed * 7919 + 13, cfg, clock, pendantBoot)
    val link = FakeLink(pendant, clock)
    val ctl = Controller(fs, root, "/demo", clock, { link }, { EPOCH + clock.nowMs() }, settings)
    val store = DeviceStore(fs, root)
    val ctrls = ArrayList<ByteArray>()
    val violations = ArrayList<String>()
    var failed = false

    init {
        link.onCtrl = { b ->
            ctrls += b
            val op = b[0].toInt()
            if (op == 1 || op == 2) {
                val d = durableNext()
                if (failed) violations += "op $op sent after the failure"
                if (b.u32(1) > d - 1) violations += "op $op value ${b.u32(1)} > durableNext-1 ${d - 1}"
            }
        }
    }

    fun logPath() = "${store.dir(id)}/records.log"
    /** durableNext as the fsynced bytes say, independent of any RecordLog instance. */
    fun durableNext(): Long = fs.durableBytes(logPath())?.let { scanBytes(it).durableNext } ?: 1L
    fun state() = ctl.state.value
    fun idleAt(next: Long) = state().conn == Conn.IDLE && state().durableMax == next - 1

    fun begin() { ctl.start(); ctl.register(ScanItem(id, "LifeOS-Pendant")) }

    /** Everything on disk against everything the pendant wrote. */
    fun verify() {
        val out = scanBytes(fs.durableBytes(logPath())!!)
        assertEquals(null, out.problem)
        checkCoverage(out.rows, out.durableNext)
        for (r in out.rows) if (r.hdr.type != T_GAP) {
            assertTrue(fs.readAt(logPath(), r.offset, r.size).contentEquals(pendant.written[r.hdr.seq]!!), "record ${r.hdr.seq} differs from what the pendant wrote")
        }
        assertTrue(pendant.acked <= out.durableNext - 1, "pendant acked ${pendant.acked} > durable ${out.durableNext - 1}")
        assertEquals(emptyList(), violations)
    }
}

class SwitchLink(var cur: Link) : Link {
    override fun scan(onFound: (ScanItem) -> Unit) = cur.scan(onFound)
    override fun stopScan() = cur.stopScan()
    override fun connect(id: String, listener: LinkListener) = cur.connect(id, listener)
    override fun readInfo(done: (ByteArray?) -> Unit) = cur.readInfo(done)
    override fun enableData(done: (Boolean) -> Unit) = cur.enableData(done)
    override fun writeCtrl(bytes: ByteArray) = cur.writeCtrl(bytes)
    override fun disconnect() = cur.disconnect()
}
