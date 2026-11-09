package dev.lifeos.pendant

import org.junit.Assume.assumeTrue
import java.io.File
import java.lang.ProcessBuilder.Redirect
import kotlin.test.*

private fun hex(b: ByteArray) = b.joinToString("") { (it.toInt() and 0xFF).toString(16).padStart(2, '0') }
private fun unhex(s: String) = ByteArray(s.length / 2) { s.substring(2 * it, 2 * it + 2).toInt(16).toByte() }

/** SyncSession against the real C sender (fw/lib/{sync,ring,chunk}) through mobile/interop/harness.c. Needs `cc`. */
class InteropTest {
    private class Harness(exe: File, blocks: Int, seed: Long) : AutoCloseable {
        private val p = ProcessBuilder(exe.path).redirectError(Redirect.INHERIT).start()
        private val r = p.inputStream.bufferedReader()
        private val w = p.outputStream.bufferedWriter()
        fun send(s: String) { w.write(s); w.write("\n"); w.flush() }
        fun read(): String = r.readLine() ?: error("harness exited")
        fun ok(cmd: String) { send(cmd); assertEquals("OK", read()) }
        init { ok("INIT $blocks $seed") }
        fun state(): List<Long> { send("STATE"); return read().split(' ').drop(1).map { it.toLong() } }
        fun info(): ByteArray { send("INFO"); return unhex(read().substring(2)) }
        override fun close() { send("QUIT"); p.waitFor() }
    }

    private class Imp(val loss: Int = 0, val dup: Int = 0, val corrupt: Int = 0, val reorder: Int = 0, val disc: Int = 0)

    /** The BLE link, with the C sender at the other end. Everything runs synchronously inside the call that triggers it. */
    private class HarnessLink(
        val h: Harness, val sched: Scheduler, val id: String, val rng: Rng, val imp: Imp,
        val durableNext: () -> Long, val covered: (Long) -> Boolean,
    ) : Link {
        var l: LinkListener? = null
        var closed = true; var inLoop = false; var dropped = false; var fwDown = true
        val pending = ArrayList<ByteArray>()
        val problems = ArrayList<String>()
        var syncs = 0; var disconnects = 0

        override fun scan(onFound: (ScanItem) -> Unit) { sched.after(100) { onFound(ScanItem(id, "harness")) } }
        override fun stopScan() {}
        override fun connect(id: String, listener: LinkListener) {
            l = listener; closed = false; dropped = false; fwDown = false; pending.clear()
            sched.after(50) { if (!closed) listener.onConnected(this.id) }
        }
        override fun readInfo(done: (ByteArray?) -> Unit) { sched.after(10) { done(h.info()) } }
        override fun enableData(done: (Boolean) -> Unit) { sched.after(10) { done(true) } }

        override fun writeCtrl(bytes: ByteArray) {
            if (closed) return
            if (inLoop) { pending += bytes; return }
            when (bytes[0].toInt()) {
                2 -> sched.after(1) { if (!closed) runSync(bytes.u32(1)) }
                4 -> sched.after(1) { if (!closed) runResend(bytes.u32(1), bytes.u32(5)) }
                else -> h.ok("CMD ${hex(bytes)}")
            }
        }

        override fun disconnect() { if (!closed) { closed = true; sched.after(2) { fwDisconnect() } } }

        /** What ble.c does when the link goes: STOP and RECLAIM. Nothing may vanish that the phone has not fsynced. */
        private fun fwDisconnect() {
            if (fwDown) return
            fwDown = true; disconnects++
            val (t0, _, a0) = h.state()
            h.ok("DISC")
            val (t1, _, _) = h.state()
            if (a0 > durableNext() - 1) problems += "ring acked $a0 > phone durable ${durableNext() - 1}"
            for (s in t0 until t1) if (!covered(s)) problems += "seq $s was reclaimed but is not durable on the phone"
        }

        private fun deliver(p: ByteArray) { if (!closed && !dropped) l?.onData(p) }

        private fun onPacket(p0: ByteArray, held: Array<ByteArray?>) {
            if (closed || dropped) return
            if (rng.chance(imp.disc)) { dropped = true; closed = true; l?.onDisconnected(); return }
            if (rng.chance(imp.loss)) return
            var p = p0
            if (rng.chance(imp.corrupt)) { p = p0.copyOf(); val i = rng.int(p.size * 8); p[i / 8] = (p[i / 8].toInt() xor (1 shl (i % 8))).toByte() }
            if (rng.chance(imp.reorder)) { held[0]?.let { deliver(it) }; held[0] = p; return }
            deliver(p)
            if (rng.chance(imp.dup)) deliver(p)
            held[0]?.let { deliver(it); held[0] = null }
        }

        private fun exchange(cmd: String, recAtPoll: Boolean) {
            inLoop = true
            h.send(cmd)
            val held = arrayOfNulls<ByteArray>(1)
            var polls = 0
            while (true) {
                val line = h.read()
                when {
                    line.startsWith("D ") -> onPacket(unhex(line.substring(2)), held)
                    line == "POLL" -> {
                        polls++
                        pending.forEach { h.send("C ${hex(it)}") }; pending.clear()
                        if (dropped) { h.send("C 05"); h.send("C 80") }
                        if (recAtPoll && polls % 10 == 0) h.send("R 1") // the recorder keeps going while we send
                        h.send("END")
                    }
                    line.startsWith("DONE") -> break
                }
            }
            inLoop = false
            held[0]?.let { deliver(it) }
            val rest = pending.toList(); pending.clear() // written before the link went: the pendant still gets them
            rest.forEach { h.ok("CMD ${hex(it)}") }
            if (dropped) fwDisconnect()
        }

        private fun runSync(have: Long) { syncs++; exchange("SYNC $have", true) }
        private fun runResend(seq: Long, off: Long) = exchange("RESEND $seq $off", false)
    }

    private val exe: File? by lazy {
        val out = File("build/interop").apply { mkdirs() }
        val exe = File(out, "harness")
        val cmd = listOf("cc", "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-I../../fw/lib/chunk", "-I../../fw/lib/ring", "-I../../fw/lib/sync",
            "-o", exe.path, "../interop/harness.c", "../../fw/lib/sync/sync.c", "../../fw/lib/ring/ring.c", "../../fw/lib/chunk/chunk.c")
        val p = try { ProcessBuilder(cmd).redirectErrorStream(true).start() } catch (e: java.io.IOException) { null }
        val log = p?.inputStream?.bufferedReader()?.readText()
        if (p == null) null else { assertEquals(0, p.waitFor(), "cc failed:\n$log"); exe }
    }

    private fun scenario(name: String, seed: Long, blocks: Int, imp: Imp, records: Int) {
        val exe = exe
        assumeTrue("no C compiler (cc)", exe != null)
        val clock = VirtualClock(); val fs = MemFs()
        Harness(exe!!, blocks, seed).use { h ->
            val store = DeviceStore(fs, "/real")
            val logPath = "${store.dir("harness-1")}/records.log"
            fun scan() = fs.durableBytes(logPath)?.let { scanBytes(it) }
            val link = HarnessLink(h, clock, "harness-1", Rng(seed * 31 + 7), imp, { scan()?.durableNext ?: 1L }, { s -> scan()?.rows?.any { s in it.covFirst..it.covLast } == true })
            val ctl = Controller(fs, "/real", "/demo", clock, { link }, { EPOCH + clock.nowMs() }, Settings(periodSec = 60))
            ctl.start(); ctl.register(ScanItem("harness-1", "harness"))
            var recorded = 0
            fun tick() { if (recorded < records) { h.ok("REC 1"); recorded++; clock.after(500) { tick() } } }
            clock.after(500) { tick() }
            assertTrue(clock.runUntil(3_600_000) { recorded >= records }, "$name seed $seed: recorder")
            assertTrue(clock.runUntil(2 * 3_600_000) { val s = h.state(); ctl.state.value.conn == Conn.IDLE && ctl.state.value.durableMax == s[1] - 1 },
                "$name seed $seed: did not finish, ${ctl.state.value.conn} ${ctl.state.value.durableMax} vs ${h.state()}")
            val out = scan()!!
            assertNull(out.problem)
            checkCoverage(out.rows, out.durableNext)
            for (r in out.rows) if (r.hdr.type == T_AUDIO) {
                val raw = fs.readAt(logPath, r.offset, r.size)
                for (i in 0 until r.hdr.len) assertEquals((r.hdr.seq * 31 + i * 7 + 5).toByte(), raw[HDR + i], "$name seed $seed: payload of ${r.hdr.seq}")
            }
            assertEquals(emptyList(), link.problems, "$name seed $seed")
            assertTrue(link.syncs >= 1)
            if (imp.disc > 0) assertTrue(link.disconnects > 1)
            if (blocks < 10) assertTrue(out.rows.any { it.hdr.type == T_GAP }, "$name seed $seed: small ring must overwrite")
        }
    }

    @Test fun clean() { for (s in 1L..5) scenario("clean", s, 40, Imp(), 120) }
    @Test fun lossy() { for (s in 1L..5) scenario("lossy", s, 40, Imp(30, 20, 20, 20), 120) }
    @Test fun disconnectsReconnectWithStopAndReclaim() { for (s in 1L..5) scenario("disconnects", s, 40, Imp(30, 20, 20, 20, disc = 8), 120) }
    @Test fun overwrite() { for (s in 1L..5) scenario("overwrite", s, 6, Imp(30, 20, 20, 20), 300) }
}
