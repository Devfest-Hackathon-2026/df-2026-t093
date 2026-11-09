package dev.lifeos.pendant

import java.io.File
import java.nio.file.Files
import kotlin.test.*

/** The same RecordLog on real files (PlatformFs). Power-cut behaviour of the hardware itself is NOT tested. */
class RealFileTest {
    private val recs = listOf(audio(1), timefix(2, EPOCH), event(3), gap(4, 7), audio(8, len = 300), audio(9))

    @Test fun appendCloseReopen() {
        val dir = Files.createTempDirectory("rl").toString()
        val fs = PlatformFs()
        val l = RecordLog.open(fs, dir)
        recs.forEach { l.append(parse(it)) }
        val again = RecordLog.open(fs, dir)
        assertEquals(10L, again.durableNext)
        assertEquals(l.rows, again.rows)
        assertContentEquals(log(*recs.toTypedArray()), File("$dir/records.log").readBytes())
        checkCoverage(again.rows, 10)
    }

    @Test fun anyTruncationRecoversToTheLastCompleteRecord() {
        val full = log(*recs.toTypedArray())
        val ends = recs.runningFold(0) { a, r -> a + r.size }
        val fs = PlatformFs()
        for (cut in 0..full.size) {
            val dir = Files.createTempDirectory("rt").toString()
            File("$dir/records.log").writeBytes(full.copyOf(cut))
            val k = ends.indexOfLast { it <= cut }
            val l = RecordLog.open(fs, dir)
            assertEquals(k, l.rows.size, "cut $cut")
            assertEquals(ends[k].toLong(), File("$dir/records.log").length(), "cut $cut")
            assertEquals(if (k == 0) 1L else scanBytes(full).rows[k - 1].covLast + 1, l.durableNext)
        }
    }

    @Test fun nestedRootSyncsItsParentsOnRealFiles() {
        val root = Files.createTempDirectory("nr").toString() + "/AppSupport/pendant"
        val fs = PlatformFs()
        val s = DeviceStore(fs, root)
        s.register(Registration("AA:BB", "x"))
        val l = s.openLog("AA:BB")
        recs.forEach { l.append(parse(it)) }
        assertEquals(10L, s.openLog("AA:BB").durableNext)
    }

    @Test fun deviceStoreOnRealFiles() {
        val root = Files.createTempDirectory("ds").toString()
        val s = DeviceStore(PlatformFs(), root)
        assertNull(s.registration())
        s.register(Registration("AA:BB", "LifeOS-Pendant"))
        assertEquals("AA:BB", s.registration()!!.id)
        s.saveDevice(DeviceMeta("AA:BB", "LifeOS-Pendant", 7))
        assertEquals(7L, s.device("AA:BB")!!.maxBoot)
        s.forget()
        assertNull(s.registration())
        assertEquals(listOf("device"), PlatformFs().list(s.dir("AA:BB")))
        assertEquals(listOf("devices"), PlatformFs().list(root))
    }
}
