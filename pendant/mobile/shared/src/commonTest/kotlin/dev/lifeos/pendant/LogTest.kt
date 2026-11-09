package dev.lifeos.pendant

import kotlin.test.*

class LogTest {
    private val dir = "/real/devices/aa"
    private val path = "$dir/records.log"

    private fun fsWith(bytes: ByteArray) = MemFs().also { it.putLog(dir, bytes) }

    // ---------------- crash ----------------

    @Test fun crashAtEveryBytePosition() {
        val clean = listOf(audio(1), audio(2, len = 16), event(3), timefix(4, EPOCH), audio(5), gap(5, 8), audio(9))
        val full = log(*clean.toTypedArray())
        val whole = scanBytes(full)
        assertNull(whole.problem)
        val ends = clean.runningFold(0) { a, r -> a + r.size }
        for (cut in 0..full.size) {
            val clock = VirtualClock(); val fs = MemFs()
            val link = ScriptLink(clock, "aa", infoBytes(next = 10))
            val s = Sess(fs, clock, link, Registration("aa", "x"))
            val d = s.store.dir("aa")
            fs.mkdirs(d); fs.files[s.logPath()] = MemFs.F()
            fs.append(s.logPath(), full) // written, never fsynced
            fs.crash(keep = cut) // power cut after `cut` bytes reached the disk
            val k = ends.indexOfLast { it <= cut }
            val l = RecordLog.open(fs, d)
            assertEquals(ends[k].toLong(), fs.size(s.logPath()), "cut $cut")
            assertEquals(whole.rows.take(k), l.rows, "cut $cut")
            val durable = if (k == 0) 1L else whole.rows[k - 1].covLast + 1
            assertEquals(durable, l.durableNext)
            // resync from what the pendant still has: complete, nothing missing, nothing twice
            link.onSync = { clean.drop(k).forEach { r -> link.send(r) }; link.end(10) }
            val end = s.run()
            assertTrue(end is End.Done, "cut $cut: $end")
            val rows = RecordLog.open(fs, d).rows
            checkCoverage(rows, 10)
            assertEquals(clean.size, rows.size)
            assertEquals(listOf(durable - 1), link.syncs())
        }
    }

    @Test fun unsyncedWritesAreDropped() {
        val fs = MemFs(); fs.mkdirs(dir)
        val l = RecordLog.open(fs, dir)
        l.append(parse(audio(1)))
        fs.append(path, audio(2)) // written, not fsynced
        fs.crash()
        assertEquals(2L, RecordLog.open(fs, dir).durableNext)
    }

    // ---------------- F4: overlapping gap ----------------

    @Test fun overlappingGap() {
        val fs = MemFs(); fs.mkdirs(dir)
        val l = RecordLog.open(fs, dir)
        for (s in 1L..5) l.append(parse(audio(s)))
        val g = gap(3, 8)
        assertEquals(Verdict.ACCEPT, l.verdict(parse(g)))
        l.append(parse(g))
        assertEquals(9L, l.durableNext)
        assertEquals(6, l.rows.size)
        assertTrue(fs.readAt(path, l.rows[5].offset, l.rows[5].size).contentEquals(g))
        assertEquals(6L to 8L, l.rows[5].covFirst to l.rows[5].covLast)
        assertEquals("欠落 6..8", rowViews(l.rows).first().detail)
        val again = RecordLog.open(fs, dir)
        assertEquals(9L, again.durableNext)
        assertEquals(l.rows, again.rows)
        checkCoverage(again.rows, 9)
        // header.seq goes 1,2,3,4,5,3 and that is fine
        assertEquals(listOf(1L, 2L, 3L, 4L, 5L, 3L), again.rows.map { it.hdr.seq })
    }

    @Test fun overlappingGapThroughASession() {
        val clock = VirtualClock(); val fs = MemFs()
        val link = ScriptLink(clock, "aa", infoBytes(next = 9))
        val s = Sess(fs, clock, link, Registration("aa", "x"))
        fs.putLog(s.store.dir("aa"), log(*(1L..5).map { audio(it) }.toTypedArray()))
        link.onSync = { link.send(gap(3, 8)); link.end(9) }
        assertIs<End.Done>(s.run())
        assertEquals(listOf(8L), link.acks().distinct().filter { it > 5 })
        assertEquals(listOf(5L), link.syncs())
    }

    // ---------------- F7: inconsistent logs ----------------

    private val torn = audio(9, len = 40).let { it.copyOf(it.size - 11) }

    private fun bad(): Map<String, ByteArray> {
        val flipped = audio(2).also { it[40] = (it[40].toInt() xor 1).toByte() }
        return mapOf(
            "dup" to log(audio(1), audio(1)),
            "ahead" to log(audio(1), audio(3)),
            "opaqueGap" to log(audio(1), gap(2, 3, flags = 2)),
            "midDamage" to log(audio(1), flipped, audio(3)),
        )
    }

    @Test fun inconsistentLogsAreNeverTouched() {
        val all = bad().flatMap { (n, b) -> listOf(n to b, "$n+torn" to b + torn) }
        assertEquals(8, all.size)
        for ((name, bytes) in all) {
            val fs = fsWith(bytes)
            val before = fs.bytes(path)!!.copyOf()
            fs.writes = 0; fs.truncates = 0
            assertFailsWith<LogInconsistent>(name) { RecordLog.open(fs, dir) }
            assertContentEquals(before, fs.bytes(path), name)
            assertEquals(0, fs.writes + fs.truncates, name)
            // the controller says so on screen and sends nothing
            val w = World(1, FakeCfg(), fs = MemFs())
            w.fs.putLog(w.store.dir(w.id), bytes)
            val b2 = w.fs.bytes(w.logPath())!!.copyOf()
            w.fs.writes = 0
            w.begin()
            w.clock.runFor(600_000)
            assertEquals(Conn.ERROR, w.state().conn, name)
            assertEquals(ErrKind.LOG_INCONSISTENT, w.state().error)
            assertEquals("ログ不整合", errorText(w.state().error!!))
            assertEquals(emptyList(), w.ctrls.toList(), name)
            assertContentEquals(b2, w.fs.bytes(w.logPath()), name)
            assertEquals(0, w.fs.truncates + w.fs.writes - 1 /* the registration file */, name)
        }
    }

    @Test fun inconsistentLogInASessionSendsNothing() {
        val clock = VirtualClock(); val fs = MemFs()
        val link = ScriptLink(clock, "aa", infoBytes(next = 9))
        val s = Sess(fs, clock, link, Registration("aa", "x"))
        fs.putLog(s.store.dir("aa"), log(audio(1), audio(1)) + torn)
        val e = s.run()
        assertEquals(ErrKind.LOG_INCONSISTENT, (e as End.Error).kind)
        assertEquals(emptyList(), link.ctrls.toList())
    }

    @Test fun consistentLogWithTornTailIsTruncatedAndSyncStarts() {
        val body = log(audio(1), audio(2), audio(3))
        val clock = VirtualClock(); val fs = MemFs()
        val link = ScriptLink(clock, "aa", infoBytes(next = 5))
        val s = Sess(fs, clock, link, Registration("aa", "x"))
        fs.putLog(s.store.dir("aa"), body + torn)
        link.onSync = { link.send(audio(4)); link.end(5) }
        assertIs<End.Done>(s.run())
        assertEquals(listOf(3L), link.syncs())
        assertEquals(4, scanBytes(fs.bytes(s.logPath())!!).rows.size)
        assertContentEquals(body + audio(4), fs.bytes(s.logPath()))
        // alone: truncated to P right after seq 3
        val fs2 = fsWith(body + torn)
        assertEquals(4L, RecordLog.open(fs2, dir).durableNext)
        assertEquals(body.size.toLong(), fs2.size(path))
    }

    // ---------------- metadata table ----------------

    @Test fun tableMatchesReopenAndHasNoPayload() {
        val fs = MemFs(); fs.mkdirs(dir)
        val l = RecordLog.open(fs, dir)
        listOf(audio(1), timefix(2, EPOCH), event(3), gap(4, 6), rec(7, 7, ByteArray(4), codec = 0), audio(8)).forEach { l.append(parse(it)) }
        assertEquals(l.rows, RecordLog.open(fs, dir).rows)
        assertTrue(l.rows.filter { it.hdr.type == T_AUDIO }.all { it.decoded == null })
        assertEquals(Decoded.Event(1, 0, 0), l.rows[2].decoded)
        assertEquals(Decoded.Gap(4, 6, 1), l.rows[3].decoded)
        assertEquals(Decoded.Timefix(0, EPOCH), l.rows[1].decoded)
    }

    @Test fun listAndUtcDoNotTouchTheFile() {
        val fs = fsWith(log(audio(1), timefix(2, EPOCH), audio(3, flags = 0, rtc = EPOCH + 5)))
        val l = RecordLog.open(fs, dir)
        val reads = fs.reads
        val v = rowViews(l.rows)
        assertEquals(listOf(3L, 2L, 1L), v.map { it.seq })
        assertEquals(EPOCH - 100 + 0, v.last().utcMs) // audio 1 at mono 100, timefix at mono 200
        assertEquals(reads, fs.reads)
    }

    @Test fun noIndexFileOnlyRegistrationAndDevices() {
        val w = World(1, FakeCfg())
        repeat(5) { w.pendant.recordOne() }
        w.begin(); w.clock.runUntil(600_000) { w.idleAt(w.pendant.next) }
        assertEquals(listOf("devices", "registration"), w.fs.list("/real"))
        assertEquals(listOf("device", "records.log"), w.fs.list(w.store.dir(w.id)))
        assertEquals(emptyList(), w.fs.list("/demo"))
    }

    // ---------------- UTC rule ----------------

    @Test fun utcRule() {
        val rows = scanBytes(log(
            audio(1, flags = 0, rtc = EPOCH + 7),          // own clock
            audio(2, flags = F_RTC_INVALID, mono = 1000),  // from the next timefix
            audio(3, flags = 0, rtc = 0, mono = 1500),     // rtc_ms = 0: also from the timefix
            timefix(4, EPOCH + 9999, mono = 2000),
            audio(5, flags = F_RTC_INVALID, boot = 2, mono = 10), // no timefix in its boot
            rec(T_AUDIO, 6, ByteArray(8), flags = 2, rtc = EPOCH), // opaque: unknown even though rtc is set
        )).rows
        assertEquals(EPOCH + 7, utcMs(rows, 0))
        assertEquals(EPOCH + 9999 - 1000, utcMs(rows, 1))
        assertEquals(EPOCH + 9999 - 500, utcMs(rows, 2))
        assertNull(utcMs(rows, 4))
        assertNull(utcMs(rows, 5))
    }

    @Test fun opaqueRowsAreNotTimeSources() {
        val rows = scanBytes(log(
            audio(1, flags = F_RTC_INVALID, mono = 100),
            rec(T_TIMEFIX, 2, ByteArray(16).also { it.put64(8, EPOCH) }, flags = 0x05, mono = 200), // unknown bit set
        )).rows
        assertFalse(rows[1].known)
        assertNull(utcMs(rows, 0))
        assertEquals("未対応の形式（暗号化の可能性）", rowViews(rows).first().detail)
        assertNull(rowViews(rows).first().utcMs)
    }

    @Test fun listIsNewestFirstAndLimited() {
        val rows = scanBytes(log(*(1L..300).map { audio(it) }.toTypedArray())).rows
        val v = rowViews(rows)
        assertEquals(LIST_LIMIT, v.size)
        assertEquals(300L, v.first().seq)
        assertEquals("${2 * 20 / 8000.0} s", v.first().detail)
    }
}
