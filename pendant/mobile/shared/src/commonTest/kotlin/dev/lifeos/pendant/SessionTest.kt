package dev.lifeos.pendant

import kotlin.test.*

class SessionTest {
    private fun setup(info: ByteArray = infoBytes(next = 100), id: String = "aa"): Triple<Sess, ScriptLink, MemFs> {
        val clock = VirtualClock(); val fs = MemFs()
        val link = ScriptLink(clock, id, info)
        return Triple(Sess(fs, clock, link, Registration(id, "x")), link, fs)
    }

    private fun preload(s: Sess, fs: MemFs, vararg r: ByteArray) = fs.putLog(s.store.dir(s.reg.id), log(*r))
    private fun rows(s: Sess, fs: MemFs) = scanBytes(fs.bytes(s.logPath())!!).rows

    // ---------------- order of commands ----------------

    @Test fun commandOrder() {
        for (setTime in listOf(true, false)) {
            val w = World(1, FakeCfg(), Settings(setTime = setTime))
            repeat(3) { w.pendant.recordOne() }
            var bootSavedAtFirstCtrl: Long? = null
            w.link.onCtrl = { if (bootSavedAtFirstCtrl == null) bootSavedAtFirstCtrl = w.store.device(w.id)?.maxBoot }
            w.begin()
            assertTrue(w.clock.runUntil(60_000) { w.idleAt(w.pendant.next) })
            val t = w.link.trace
            val want = listOf("connect", "info", "enable") + (if (setTime) listOf("ctrl:3") else emptyList()) + listOf("ctrl:2:0")
            assertEquals(want, t.take(want.size))
            assertEquals("disconnect", t.last())
            assertTrue(t[t.size - 2].startsWith("ctrl:1:"), "the last ACK comes before the disconnect: $t")
            assertEquals(1L, bootSavedAtFirstCtrl)
        }
    }

    @Test fun holeAskedOnceAndLaterFragmentsDropped() {
        val (s, link, fs) = setup(infoBytes(next = 2))
        val raw = audio(1, len = 700)
        link.onSync = { val f = frags(1, raw); link.data(f[0]); link.data(f[2]); link.data(f[3]); link.end(2) }
        link.onResend = { seq, off -> link.send(raw, seq, off.toInt()) }
        assertIs<End.Done>(s.run())
        val rs = link.ctrls.filter { it[0].toInt() == 4 }
        assertEquals(1, rs.size)
        assertEquals(1L to 244L, rs[0].u32(1) to rs[0].u32(5))
        assertContentEquals(raw, fs.readAt(s.logPath(), 0, raw.size))
    }

    @Test fun silenceAbortsAfter20s() {
        val (s, link, _) = setup()
        assertIs<End.Incomplete>(s.run())
        assertEquals(DATA_TIMEOUT_MS, s.clock.nowMs() - link.ctrlAt.last())
    }

    @Test fun endMarkerRetriesEvery3sMax6() {
        val (s, link, _) = setup(infoBytes(next = 4))
        link.onSync = { link.send(audio(1)); link.send(audio(2)); link.end(4) }
        assertIs<End.Incomplete>(s.run())
        val at = link.ctrls.indices.filter { link.ctrls[it][0].toInt() == 4 }
        assertEquals(END_RETRIES, at.size)
        assertEquals(List(END_RETRIES - 1) { END_RETRY_MS }, at.map { link.ctrlAt[it] }.zipWithNext { a, b -> b - a })
        assertTrue(at.all { link.ctrls[it].u32(1) == 3L && link.ctrls[it].u32(5) == 0L })
        val markerAt = link.ctrlAt[link.ctrls.indexOfFirst { it[0].toInt() == 2 }] + 1 // onSync runs 1 ms after the SYNC write
        assertEquals(END_RETRY_MS, link.ctrlAt[at[0]] - markerAt)
    }

    @Test fun endMarkerRetriesAreNotPostponedByDuplicatesWithoutProgress() {
        val (s, link, _) = setup(infoBytes(next = 4))
        fun dup() { link.send(audio(2)); link.end(4); s.clock.after(2_000) { dup() } }
        link.onSync = { link.send(audio(1)); link.send(audio(2)); link.end(4); s.clock.after(2_000) { dup() } }
        assertIs<End.Incomplete>(s.run())
        val at = link.ctrls.indices.filter { link.ctrls[it][0].toInt() == 4 }
        assertEquals(END_RETRIES, at.size)
        assertEquals(List(END_RETRIES - 1) { END_RETRY_MS }, at.map { link.ctrlAt[it] }.zipWithNext { a, b -> b - a })
    }

    // ---------------- duplicates and reordering ----------------

    @Test fun duplicateIsNotStoredNorAcked() {
        val (s, link, fs) = setup(infoBytes(next = 4))
        var before = emptyList<Long>(); var afterDup = emptyList<Long>()
        link.onSync = { link.send(audio(1)); link.send(audio(2)); before = link.acks(); link.send(audio(2)); afterDup = link.acks(); link.send(audio(3)); link.end(4) }
        assertIs<End.Done>(s.run())
        assertEquals(listOf(1L, 2L), before); assertEquals(before, afterDup)
        assertEquals(listOf(1L, 2L, 3L), rows(s, fs).map { it.hdr.seq })
    }

    @Test fun aheadIsHeldUntilItsTurn() {
        val (s, link, fs) = setup(infoBytes(next = 4))
        var acks = emptyList<Long>()
        link.onSync = { link.send(audio(2)); link.send(audio(1)); acks = link.acks(); link.send(audio(3)); link.end(4) }
        assertIs<End.Done>(s.run())
        assertEquals(listOf(2L), acks) // one cumulative ACK, after both were fsynced
        assertEquals(listOf(1L, 2L, 3L), rows(s, fs).map { it.hdr.seq })
    }

    @Test fun firstCandidateWithTheSameFirstWins() {
        val (s, link, fs) = setup(infoBytes(next = 4))
        val a = audio(3, fill = 1); val b = audio(3, fill = 9)
        link.onSync = { link.send(a); link.send(b); link.send(audio(1)); link.send(audio(2)); link.end(4) }
        assertIs<End.Done>(s.run())
        val r = rows(s, fs)[2]
        assertContentEquals(a, fs.readAt(s.logPath(), r.offset, r.size))
    }

    @Test fun bufferedRecordBecomesDupWhenAGapCoversIt() {
        val (s, link, fs) = setup(infoBytes(next = 8))
        var ackAtGap = 0L
        link.onCtrl = { b -> if (b[0].toInt() == 1 && b.u32(1) == 6L) ackAtGap = scanBytes(fs.durableBytes(s.logPath())!!).durableNext }
        link.onSync = { link.send(audio(5)); link.send(gap(1, 6)); link.send(audio(7)); link.end(8) }
        assertIs<End.Done>(s.run())
        assertEquals(listOf(T_GAP, T_AUDIO), rows(s, fs).map { it.hdr.type })
        assertEquals(7L, ackAtGap) // the gap was fsynced before ACK 6 went out
        assertEquals(listOf(6L, 7L), link.acks().distinct())
    }

    // ---------------- unknown formats ----------------

    @Test fun opaqueRecordsAreStoredRawAndAckedOneAtATime() {
        val opaque = listOf(
            rec(T_AUDIO, 2, ByteArray(8), flags = 2), rec(T_TIMEFIX, 3, ByteArray(16), flags = 4), rec(T_EVENT, 4, ByteArray(8), flags = 8),
            rec(T_AUDIO, 5, ByteArray(8), codec = 2), timefix(6, 5, len = 15), rec(T_EVENT, 7, ByteArray(9)), rec(5, 8, ByteArray(8), codec = 0),
        )
        val (s, link, fs) = setup(infoBytes(next = 9))
        link.onSync = { link.send(audio(1)); opaque.forEach { link.send(it) }; link.end(9) }
        assertIs<End.Done>(s.run())
        assertEquals((1L..8).toList(), link.acks().distinct())
        val r = rows(s, fs)
        checkCoverage(r, 9)
        assertEquals(listOf(true) + List(7) { false }, r.map { it.known })
        assertTrue(r.drop(1).all { it.decoded == null })
        opaque.forEachIndexed { i, raw -> assertContentEquals(raw, fs.readAt(s.logPath(), r[i + 1].offset, r[i + 1].size)) }
        assertTrue(rowViews(r).dropLast(1).all { it.detail == "未対応の形式（暗号化の可能性）" })
    }

    @Test fun opaqueGapStopsTheSessionAtAnyPosition() {
        // phone has 1..5; ACK 6 goes out for audio(6); durableNext is then 7
        val kinds = listOf<(Long) -> ByteArray>(
            { p -> gap(p, p + 1, flags = 2) }, { p -> gap(p, p + 1, len = 16) }, { p -> gap(p + 1, p + 2, seq = p) }, { p -> gap(p, p - 1) },
        )
        for (pos in listOf(3L, 7L, 12L)) for ((k, mk) in kinds.withIndex()) {
            val (s, link, fs) = setup(infoBytes(next = 20))
            preload(s, fs, *(1L..5).map { audio(it) }.toTypedArray())
            link.onSync = { link.send(audio(6)); link.send(mk(pos)); link.send(audio(7)); link.end(20) }
            val e = s.run()
            assertEquals(ErrKind.UNSUPPORTED, (e as End.Error).kind, "pos $pos kind $k")
            assertEquals(listOf(6L), link.acks(), "pos $pos kind $k")
            assertEquals(listOf(5L), link.syncs())
            assertEquals(6, rows(s, fs).size)
        }
    }

    @Test fun logWithOpaqueNonGapReopensAndResyncs() {
        val (s, link, fs) = setup(infoBytes(next = 6))
        val op = rec(T_AUDIO, 2, ByteArray(8), flags = 2)
        preload(s, fs, audio(1), op, audio(3))
        val l = RecordLog.open(fs, s.store.dir("aa"))
        assertEquals(4L, l.durableNext)
        assertEquals(l.rows, RecordLog.open(fs, s.store.dir("aa")).rows)
        link.onSync = { link.send(audio(4)); link.send(audio(5)); link.end(6) }
        assertIs<End.Done>(s.run())
        assertEquals(listOf(3L), link.syncs())
        checkCoverage(rows(s, fs), 6)
    }

    // ---------------- mismatch ----------------

    private fun mismatch(info: ByteArray, prep: (Sess, MemFs) -> Unit = { _, _ -> }) {
        val (s, link, fs) = setup(info)
        prep(s, fs)
        val e = s.run()
        assertEquals(ErrKind.MISMATCH, (e as End.Error).kind)
        assertEquals(emptyList(), link.ctrls.toList())
    }

    @Test fun versionMismatch() = mismatch(infoBytes(version = 2, next = 100))
    @Test fun pendantLogShorterThanOurs() = mismatch(infoBytes(next = 3)) { s, fs -> preload(s, fs, *(1L..5).map { audio(it) }.toTypedArray()) }
    @Test fun bootWentBackwards() = mismatch(infoBytes(boot = 4, next = 100)) { s, _ -> s.store.saveDevice(DeviceMeta("aa", "x", 5)) }
    @Test fun shortInfo() = mismatch(ByteArray(10))

    @Test fun bootIsSavedAsTheMaximum() {
        val (s, link, fs) = setup(infoBytes(boot = 7, next = 1))
        link.onSync = { link.end(1) }
        assertIs<End.Done>(s.run())
        assertEquals(7L, s.store.device("aa")!!.maxBoot)
        assertEquals(listOf(0L), link.syncs())
        assertEquals(emptyList(), link.acks())
    }
}
