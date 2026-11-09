package dev.lifeos.pendant

/**
 * Demo transport: a FakePendant on a damaged link, so every flow runs without BLE. The ring is small enough that a few minutes away show gaps.
 * The simulated pendant is rebuilt on every app start and demo toggle, so it picks up where the saved demo log under [root] ends
 * (seq continues, boot counts up like a restart): otherwise its small next_seq would read as a shorter pendant log (不一致).
 */
object Demo {
    const val ID = "DEMO-0000-0001"
    fun link(sched: Scheduler, fs: Fs, root: String, seed: Long = 7): Link {
        val store = DeviceStore(fs, root)
        val next = try { store.openLog(ID).durableNext } catch (e: Exception) { 1L } // a broken log shows its own error on connect
        val boot = store.device(ID)?.let { it.maxBoot + 1 } ?: 1L
        val p = FakePendant(ID, "LifeOS-Pendant (demo)", seed, FakeCfg(lossPm = 20, dupPm = 10, corruptPm = 10, reorderPm = 10, disconnectPm = 2,
            ringBytes = 40_000, recordEveryMs = 1_000, minPayload = 1_000, maxPayload = 3_000), sched, boot, next, keepWritten = false) // a demo runs for days: no copy of everything it wrote
        p.startRecorder()
        return FakeLink(p, sched)
    }
}
