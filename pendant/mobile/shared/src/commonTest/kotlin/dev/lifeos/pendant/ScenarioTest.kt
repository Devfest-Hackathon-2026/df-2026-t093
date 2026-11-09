package dev.lifeos.pendant

import kotlin.test.*

class ScenarioTest {
    private val seeds = (1L..20L)
    private fun cfgLossy(ring: Int = 1 shl 20, disc: Int = 0, corrupt: Int = 20) =
        FakeCfg(lossPm = 30, dupPm = 20, corruptPm = corrupt, reorderPm = 20, disconnectPm = disc, ringBytes = ring)

    /** Returns (resends asked, sessions, gap rows) summed over the seeds, to show the impairments really bit. */
    private fun run(name: String, cfg: (Long) -> FakeCfg, records: Int = 150): Triple<Int, Int, Int> {
        var resends = 0; var sessions = 0; var gaps = 0
        for (seed in seeds) {
            val w = World(seed, cfg(seed))
            w.begin()
            w.pendant.startRecorder()
            assertTrue(w.clock.runUntil(3_600_000) { w.pendant.next > records }, "$name seed $seed: recorder")
            w.pendant.freeze()
            assertTrue(w.clock.runUntil(2 * 3_600_000) { w.idleAt(w.pendant.next) }, "$name seed $seed: did not finish, state ${w.state().conn} ${w.state().durableMax}/${w.pendant.next - 1}")
            try { w.verify() } catch (e: Throwable) { throw AssertionError("$name seed $seed: ${e.message}", e) }
            assertEquals(null, w.state().error)
            resends += w.ctrls.count { it[0].toInt() == 4 }; sessions += w.link.trace.count { it == "connect" }
            gaps += scanBytes(w.fs.durableBytes(w.logPath())!!).rows.count { it.hdr.type == T_GAP }
        }
        println("$name: resends=$resends sessions=$sessions gaps=$gaps")
        return Triple(resends, sessions, gaps)
    }

    @Test fun clean() { assertEquals(0, run("clean", { FakeCfg() }).third) }
    @Test fun lossy() { assertTrue(run("lossy", { cfgLossy() }).first > 0) }
    @Test fun lossyWithDisconnects() { assertTrue(run("lossy+disconnects", { cfgLossy(disc = 8) }).second > 3 * 20) }
    @Test fun corruptHeavy() { assertTrue(run("corrupt-heavy", { cfgLossy(corrupt = 150) }).first > 100) }
    @Test fun overwrite() { assertTrue(run("overwrite", { cfgLossy(ring = 8_000) }, records = 400).third > 20) }

    @Test fun overwriteLeavesGaps() {
        val w = World(1, FakeCfg(ringBytes = 8_000)); w.begin(); w.pendant.startRecorder()
        w.clock.runUntil(400_000) { w.pendant.next > 300 }; w.pendant.freeze()
        w.clock.runUntil(3_600_000) { w.idleAt(w.pendant.next) }
        w.verify()
        assertTrue(w.state().rows.any { it.kind == "gap" })
    }
}
