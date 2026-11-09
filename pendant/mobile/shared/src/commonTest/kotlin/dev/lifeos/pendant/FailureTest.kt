package dev.lifeos.pendant

import kotlin.test.*

/** Append / fsync / device-file failures: nothing past the last durable record may be acknowledged or lost. */
class FailureTest {
    private val rawSize = HDR + 100 + TRL

    private fun cfg(ringRecords: Int) = FakeCfg(minPayload = 100, maxPayload = 100, eventPm = 0, ringBytes = ringRecords * rawSize)

    private fun case(phoneHas: Int, records: Int, ringRecords: Int, hook: (MemFs) -> ((String, String, ByteArray?) -> Boolean)): Pair<Long, Long> {
        val w = World(5, cfg(ringRecords), Settings(setTime = false))
        repeat(records) { w.pendant.recordOne() }
        if (phoneHas > 0) w.fs.putLog(w.store.dir(w.id), log(*(1..phoneHas).map { w.pendant.written[it.toLong()]!! }.toTypedArray()))
        val dBefore = w.durableNext()
        assertEquals(phoneHas + 1L, dBefore)
        var d = -1L; var tailAtFail = -1L
        var snapshot = emptyMap<Long, ByteArray>()
        w.fs.failHook = hook(w.fs)
        w.fs.onFail = {
            w.failed = true
            d = w.durableNext(); tailAtFail = w.pendant.tail
            snapshot = w.pendant.ringSeqs().filter { it >= d }.associateWith { w.pendant.ringRecord(it)!! }
            w.pendant.freeze()
        }
        w.begin()
        assertTrue(w.clock.runUntil(600_000) { w.state().conn == Conn.ERROR }, "no error state")
        assertTrue(d > 0, "the failure never happened")
        // (c) the screen says save error; (a)+(b) nothing sent after, nothing above D-1 ever
        assertEquals(ErrKind.SAVE, w.state().error)
        assertEquals(emptyList(), w.violations)
        assertTrue(w.ctrls.filter { it[0].toInt() in 1..2 }.all { it.u32(1) <= d - 1 })
        assertTrue(w.pendant.acked <= d - 1)
        // (d) disconnect, STOP, reclaim: everything the ring held from D on is still there, unchanged
        w.pendant.command(Ctrl.STOP); w.pendant.disconnect(); w.clock.runFor(10_000)
        for ((s, b) in snapshot) assertTrue(w.pendant.ringRecord(s)?.contentEquals(b) == true, "seq $s lost or changed")
        assertEquals(emptyList(), w.violations)
        // (f) power cycle: reopen finds D by scanning; the next session starts from D-1
        w.fs.failHook = null; w.fs.crash(); w.failed = false
        assertEquals(d, RecordLog.open(w.fs, w.store.dir(w.id)).durableNext)
        val before = w.ctrls.size
        w.ctl.syncNow()
        assertTrue(w.clock.runUntil(600_000) { w.idleAt(w.pendant.next) }, "resync did not finish")
        assertEquals(d - 1, w.ctrls.drop(before).first { it[0].toInt() == 2 }.u32(1))
        w.verify()
        // (e) what was overwritten before the failure arrives again as a gap and is stored
        if (tailAtFail > d) assertTrue(scanBytes(w.fs.durableBytes(w.logPath())!!).rows.any { it.hdr.type == T_GAP && it.covFirst == d && it.covLast == tailAtFail - 1 })
        return d to tailAtFail
    }

    private fun isAudio(b: ByteArray?) = b != null && b.size > 5 && b[5].toInt() == T_AUDIO
    private fun isGap(b: ByteArray?) = b != null && b.size > 5 && b[5].toInt() == T_GAP
    private fun onLog(p: String) = p.endsWith("records.log")

    @Test fun case1_thirdAudioAppendOnEmptyLog() = assertEquals(3L to 1L, case(0, 10, 100) { var n = 0; { op, p, b -> op == "append" && onLog(p) && isAudio(b) && ++n == 3 } })

    @Test fun case2_firstAppendAfterSync99() = assertEquals(100L to 1L, case(99, 150, 200) { { op, p, _ -> op == "append" && onLog(p) } })

    @Test fun case3_gapAppendWithTailAhead() = assertEquals(100L to 120L, case(99, 139, 20) { { op, p, b -> op == "append" && onLog(p) && isGap(b) } })

    @Test fun case4_audioFsyncAfterGap() = assertEquals(8L to 8L, case(5, 27, 20) {
        var lastType = 0; var gapSeen = false
        { op, p, b ->
            if (!onLog(p)) false
            else if (op == "append") { lastType = b!![5].toInt(); false }
            else if (op == "sync" && lastType == T_GAP) { gapSeen = true; false }
            else op == "sync" && gapSeen && lastType == T_AUDIO
        }
    })

    @Test fun case5_bootSaveFails() {
        val w = World(5, cfg(100), Settings(setTime = false))
        repeat(10) { w.pendant.recordOne() }
        w.fs.failHook = { op, p, _ -> op == "replace" && p.endsWith("/device") }
        w.begin()
        assertTrue(w.clock.runUntil(60_000) { w.state().conn == Conn.ERROR })
        assertEquals(ErrKind.SAVE, w.state().error)
        assertEquals("保存エラー", errorText(w.state().error!!))
        assertEquals(emptyList(), w.ctrls.map { it[0].toInt() }) // no SYNC, no ACK, no TIME
        assertEquals(listOf("connect", "info", "disconnect"), w.link.trace)
    }

    // F4: the log fsync failed but the record stayed readable; reopening without a power cut must not turn it into a frontier before it is fsynced.
    @Test fun f4_retryAfterLogFsyncFailureWithoutPowerCut() {
        val w = World(5, cfg(100), Settings(setTime = false))
        repeat(10) { w.pendant.recordOne() }
        var n = 0
        w.fs.failHook = { op, p, _ -> op == "sync" && onLog(p) && ++n == 1 }
        w.fs.onFail = { w.failed = true; w.pendant.freeze() }
        w.begin()
        assertTrue(w.clock.runUntil(60_000) { w.state().conn == Conn.ERROR })
        assertEquals(ErrKind.SAVE, w.state().error)
        assertEquals(0, w.fs.durableBytes(w.logPath())!!.size) // readable (vol) but not durable
        assertTrue(w.fs.bytes(w.logPath())!!.isNotEmpty())
        w.fs.failHook = null; w.failed = false
        val before = w.ctrls.size
        w.ctl.syncNow()                                          // reload() reopens the log; no MemFs.crash()
        assertTrue(w.clock.runUntil(600_000) { w.idleAt(w.pendant.next) }, "retry did not finish")
        assertTrue(w.ctrls.drop(before).first { it[0].toInt() == 2 }.u32(1) >= 1)
        w.verify() // violations: every ACK/SYNC <= what the fsynced bytes cover at that moment
    }

    @Test fun f4_openFailsWhenTheFsyncOfAReadablePrefixFails() {
        val fs = MemFs()
        fs.files["/real/devices/k/records.log"] = MemFs.F(ByteArray(0), log(audio(1), audio(2)))
        fs.dirs += "/real/devices/k"
        fs.failHook = { op, _, _ -> op == "sync" }
        assertFailsWith<StoreFailure> { RecordLog.open(fs, "/real/devices/k") }
        fs.failHook = null
        assertEquals(3L, RecordLog.open(fs, "/real/devices/k").durableNext)
        assertEquals(fs.bytes("/real/devices/k/records.log")!!.size, fs.durableBytes("/real/devices/k/records.log")!!.size)
    }

    // F1: the fsync of the root's parent fails: 保存エラー, and no ACK above 0 was sent.
    @Test fun f1_rootParentSyncFailureIsASaveErrorWithoutAck() {
        val w = World(5, cfg(100), Settings(setTime = false), root = "/c/home/AppSupport/pendant")
        repeat(10) { w.pendant.recordOne() }
        w.begin()
        w.fs.failHook = { op, p, _ -> op == "syncDir" && p == "/c/home/AppSupport" }
        w.fs.onFail = { w.failed = true }
        assertTrue(w.clock.runUntil(60_000) { w.state().conn == Conn.ERROR })
        assertEquals(ErrKind.SAVE, w.state().error)
        assertTrue(w.ctrls.none { it[0].toInt() in 1..2 && it.u32(1) > 0 })
        assertEquals(emptyList(), w.violations)
    }

    // F1: the directory fsync failed after mkdirs; the retry finds the directory already there and must still sync devices/ before any ACK.
    @Test fun f1_retryAfterParentDirFsyncFailure() {
        val fs = MemFs().also { it.strictDirs = true }
        val w = World(5, cfg(100), Settings(setTime = false), fs = fs)
        repeat(10) { w.pendant.recordOne() }
        fs.failHook = { op, p, _ -> op == "syncDir" && p == "/real/devices" }
        fs.onFail = { w.failed = true }
        w.begin()
        assertTrue(w.clock.runUntil(60_000) { w.state().conn == Conn.ERROR })
        fs.failHook = null; w.failed = false
        w.link.onCtrl = { b ->
            if (b[0].toInt() in 1..2 && b.u32(1) > 0) assertEquals(emptySet(), fs.unsyncedDirs, "ACK/SYNC with an unsynced directory entry")
        }
        w.ctl.syncNow()
        assertTrue(w.clock.runUntil(600_000) { w.idleAt(w.pendant.next) })
        fs.crash()
        assertEquals(w.pendant.next, RecordLog.open(fs, w.store.dir(w.id)).durableNext)
    }
}
