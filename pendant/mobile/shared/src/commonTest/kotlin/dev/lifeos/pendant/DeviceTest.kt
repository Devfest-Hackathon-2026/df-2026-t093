package dev.lifeos.pendant

import kotlin.test.*

class DeviceTest {
    private val cfg = FakeCfg(minPayload = 100, maxPayload = 100, eventPm = 0)

    private class Rig(val clock: VirtualClock = VirtualClock(), val fs: MemFs = MemFs()) {
        val sw = SwitchLink(FakeLink(FakePendant("x", "x", 1, FakeCfg(), clock), clock))
        val store = DeviceStore(fs, "/real")
        val violations = ArrayList<String>()
        val firstSync = HashMap<String, Long>()
        fun controller() = Controller(fs, "/real", "/demo", clock, { sw }, { EPOCH + clock.nowMs() }, Settings(setTime = false))
        /** A pendant with [n] records, on a link that checks every ACK / SYNC against that device's own durable log. */
        fun pendant(id: String, seed: Long, n: Int, cfg: FakeCfg, boot: Long = 3): Pair<FakePendant, FakeLink> {
            val p = FakePendant(id, "P-$id", seed, cfg, clock, boot)
            repeat(n) { p.recordOne() }
            val l = FakeLink(p, clock)
            l.onCtrl = { b ->
                val op = b[0].toInt()
                if (op == 1 || op == 2) {
                    val d = fs.durableBytes("${store.dir(id)}/records.log")?.let { scanBytes(it).durableNext } ?: 1L
                    if (b.u32(1) > d - 1) violations += "$id: op $op ${b.u32(1)} > ${d - 1}"
                    if (op == 2) firstSync.getOrPut("$id#${l.trace.count { it == "connect" }}") { b.u32(1) }
                }
            }
            return p to l
        }
        fun logOf(id: String) = "${store.dir(id)}/records.log"
        fun rows(id: String) = scanBytes(fs.bytes(logOf(id))!!).rows
    }

    private fun run(restart: Boolean) {
        val r = Rig()
        var ctl = r.controller().also { it.start() }
        fun phase() { if (restart) ctl = r.controller().also { it.start() } }
        fun settle(p: FakePendant) = assertTrue(r.clock.runUntil(600_000) { ctl.state.value.conn == Conn.IDLE && ctl.state.value.durableMax == p.next - 1 })

        // 1. A, seq 1..99 only
        val (a, la) = r.pendant("AAAA", 1, 99, cfg)
        r.sw.cur = la
        ctl.register(ScanItem("AAAA", "A")); settle(a)
        assertEquals(100L, scanBytes(r.fs.durableBytes(r.logOf("AAAA"))!!).durableNext)
        // 2. forget: the directory and the log stay
        val aBytes = r.fs.bytes(r.logOf("AAAA"))!!.copyOf()
        ctl.forget()
        assertEquals(Conn.UNREGISTERED, ctl.state.value.conn)
        assertEquals(emptyList(), ctl.state.value.rows)
        assertTrue(r.fs.exists(r.store.dir("AAAA")))
        assertContentEquals(aBytes, r.fs.bytes(r.logOf("AAAA")))
        // 3. B: same boot, same next_seq, other contents
        phase()
        val (b, lb) = r.pendant("BBBB", 2, 150, cfg)
        r.sw.cur = lb
        ctl.register(ScanItem("BBBB", "B")); settle(b)
        assertEquals(0L, r.firstSync["BBBB#1"])
        assertTrue(b.acked <= scanBytes(r.fs.durableBytes(r.logOf("BBBB"))!!).durableNext - 1)
        assertEquals(150, ctl.state.value.rows.size) // the list is B's log only
        assertEquals(b.next - 1, ctl.state.value.durableMax)
        assertContentEquals(aBytes, r.fs.bytes(r.logOf("AAAA")))
        // 4. back to A, which has recorded more in the meantime
        ctl.forget(); phase()
        repeat(51) { a.recordOne() }
        r.sw.cur = la
        ctl.register(ScanItem("AAAA", "A")); settle(a)
        assertEquals(99L, r.firstSync["AAAA#2"])
        assertEquals(150, r.rows("AAAA").size)
        assertContentEquals(aBytes, r.fs.bytes(r.logOf("AAAA"))!!.copyOf(aBytes.size))
        // nobody's records in the other's log
        for ((id, p) in listOf("AAAA" to a, "BBBB" to b)) {
            val own = r.rows(id)
            checkCoverage(own, 151)
            for (row in own) assertTrue(r.fs.readAt(r.logOf(id), row.offset, row.size).contentEquals(p.written[row.hdr.seq]!!), "$id seq ${row.hdr.seq}")
        }
        assertEquals(emptyList(), r.violations)
        assertEquals(listOf("AAAA", "BBBB").map { r.store.key(it) }, r.fs.list("/real/devices").sorted().let { l -> listOf("AAAA", "BBBB").map { r.store.key(it) }.filter { it in l } })
    }

    @Test fun separateDevices() = run(restart = false)
    @Test fun separateDevicesAcrossRestarts() = run(restart = true)

    @Test fun otherDeviceAnsweringIsRefused() {
        val clock = VirtualClock(); val fs = MemFs()
        val p = FakePendant("AAAA", "A", 1, cfg, clock); repeat(5) { p.recordOne() }
        val l = FakeLink(p, clock, reportedId = "OTHER")
        val ctl = Controller(fs, "/real", "/demo", clock, { l }, { EPOCH }, Settings())
        ctl.start(); ctl.register(ScanItem("AAAA", "A"))
        clock.runFor(600_000)
        assertEquals(ErrKind.MISMATCH, ctl.state.value.error)
        assertEquals("不一致", errorText(ErrKind.MISMATCH))
        assertEquals(listOf("connect", "disconnect"), l.trace) // no INFO, SYNC, ACK or TIME; and no automatic retry
    }

    @Test fun smallerBootOnANewDeviceIsFine() {
        val r = Rig()
        val ctl = r.controller().also { it.start() }
        val (a, la) = r.pendant("AAAA", 1, 120, cfg, boot = 9)
        r.sw.cur = la; ctl.register(ScanItem("AAAA", "A"))
        assertTrue(r.clock.runUntil(600_000) { ctl.state.value.durableMax == 120L && ctl.state.value.conn == Conn.IDLE })
        ctl.forget()
        val (b, lb) = r.pendant("BBBB", 2, 10, cfg, boot = 1)
        r.sw.cur = lb; ctl.register(ScanItem("BBBB", "B"))
        assertTrue(r.clock.runUntil(600_000) { ctl.state.value.durableMax == 10L && ctl.state.value.conn == Conn.IDLE })
        assertNull(ctl.state.value.error)
        assertEquals(1L, r.store.device("BBBB")!!.maxBoot)
        assertEquals(9L, r.store.device("AAAA")!!.maxBoot)
        // and A again, with its old boot, is not a mismatch either
        ctl.forget(); r.sw.cur = la; ctl.register(ScanItem("AAAA", "A"))
        assertTrue(r.clock.runUntil(600_000) { ctl.state.value.conn == Conn.IDLE && ctl.state.value.lastSync?.ok == true })
        assertNull(ctl.state.value.error)
    }
}
