package dev.lifeos.pendant

import java.io.File
import kotlin.test.*

/** The same fixtures and expectations as tools/test_rx.py, read from the repo. */
class FixtureTest {
    private fun load(name: String): List<MetaRow> {
        val b = File("../../tools/fixtures/$name").readBytes()
        val out = scanBytes(b)
        assertEquals(b.size.toLong(), out.p)
        return out.rows
    }

    private fun row(r: MetaRow) = listOf(r.hdr.seq, r.hdr.type.toLong(), r.hdr.boot, r.hdr.monoMs, r.hdr.rtcMs, r.hdr.len.toLong())

    @Test fun sample() {
        val rows = load("sample.rec")
        assertTrue(rows.all { it.known })
        assertEquals(
            listOf(
                listOf(1L, 1L, 3L, 1000L, 0L, 400L), listOf(2L, 1L, 3L, 1100L, 0L, 400L), listOf(3L, 2L, 3L, 1300L, 1760000000000L, 16L),
                listOf(4L, 3L, 3L, 1300L, 1760000000000L, 12L), listOf(6L, 1L, 3L, 1700L, 1760000000400L, 400L),
            ),
            rows.map(::row),
        )
        assertEquals(listOf(1L, 1L, 0L, 0L, 0L).map { it.toInt() }, rows.map { it.hdr.flags })
        // record 1 started 300 ms before the timefix at mono 1300 whose "after" is 1760000000000
        assertEquals(1760000000000L - 300, utcMs(rows, 0))
        assertEquals(1760000000000L - 200, utcMs(rows, 1))
    }

    @Test fun backwardsClockKeepsSeqOrder() {
        val rows = load("backwards.rec")
        assertEquals(listOf(1L, 2L, 3L, 4L), rows.map { it.hdr.seq })
        assertEquals(listOf(1L, 1L, 1L, 2L), rows.map { it.hdr.type.toLong() })
        assertEquals(listOf(5L, 5L, 5L, 5L), rows.map { it.hdr.boot })
        assertEquals(listOf(1000L, 1100L, 1200L, 1300L), rows.map { it.hdr.monoMs })
        assertEquals(listOf(400, 400, 400, 16), rows.map { it.hdr.len })
        assertTrue(rows[1].hdr.rtcMs < rows[0].hdr.rtcMs)
        assertTrue(rows.all { it.known })
    }
}
