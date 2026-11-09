package dev.lifeos.pendant

import kotlin.test.*

class ControllerTest {
    private class Refuse(private val clock: Scheduler, val id: String) : Link {
        val connects = ArrayList<Long>()
        override fun scan(onFound: (ScanItem) -> Unit) { clock.after(100) { onFound(ScanItem(id, "x")) } }
        override fun stopScan() {}
        override fun connect(id: String, listener: LinkListener) { connects += clock.nowMs(); clock.after(5) { listener.onDisconnected() } }
        override fun readInfo(done: (ByteArray?) -> Unit) {}
        override fun enableData(done: (Boolean) -> Unit) {}
        override fun writeCtrl(bytes: ByteArray) {}
        override fun disconnect() {}
    }

    @Test fun backoffDoublesUpTo60s() {
        val clock = VirtualClock(); val fs = MemFs()
        val link = Refuse(clock, "AA")
        DeviceStore(fs, "/real").register(Registration("AA", "x"))
        val ctl = Controller(fs, "/real", "/demo", clock, { link }, { EPOCH }, Settings())
        ctl.start()
        clock.runFor(400_000)
        val gaps = link.connects.zipWithNext { a, b -> b - a - 105 }
        assertEquals(listOf(2000L, 4000, 8000, 16000, 32000, 60000, 60000), gaps.take(7))
        assertEquals(Conn.IDLE, ctl.state.value.conn)
    }

    @Test fun syncsEveryPeriodWhileInTheForeground() {
        val w = World(1, FakeCfg(), Settings(periodSec = 60))
        repeat(5) { w.pendant.recordOne() }
        w.begin()
        w.clock.runFor(5 * 60_000)
        assertEquals(5, w.link.trace.count { it == "connect" }, w.link.trace.toString())
        w.ctl.setForeground(false)
        val n = w.link.trace.size
        w.clock.runFor(10 * 60_000)
        assertEquals(n, w.link.trace.size) // nothing in the background
        w.ctl.setForeground(true)
        w.clock.runFor(60_000)
        assertTrue(w.link.trace.size > n)
    }

    @Test fun errorsWaitForTheUser() {
        val w = World(1, FakeCfg()); repeat(3) { w.pendant.recordOne() }
        w.fs.putLog(w.store.dir(w.id), log(audio(1), audio(1)))
        w.begin(); w.clock.runFor(30 * 60_000)
        assertEquals(Conn.ERROR, w.state().conn)
        assertEquals(0, w.link.trace.count { it == "connect" })
        // mismatch at connect time: one attempt, then waiting
        val w2 = World(1, FakeCfg(), id = "A1")
        repeat(3) { w2.pendant.recordOne() }
        w2.store.saveDevice(DeviceMeta("A1", "x", 50))
        w2.begin(); w2.clock.runFor(30 * 60_000)
        assertEquals(ErrKind.MISMATCH, w2.state().error)
        assertEquals(1, w2.link.trace.count { it == "connect" })
    }

    @Test fun infoAndLastSyncAreShown() {
        val w = World(1, FakeCfg()); repeat(4) { w.pendant.recordOne() }
        w.begin()
        assertTrue(w.clock.runUntil(60_000) { w.idleAt(w.pendant.next) })
        val s = w.state()
        assertEquals("LifeOS-Pendant", s.device!!.name); assertEquals(w.id, s.device!!.id)
        assertEquals(1L, s.info!!.boot); assertEquals(1L, s.info!!.tail)
        assertTrue(s.lastSync!!.ok && s.lastSync!!.bytes > 0)
        assertEquals(w.pendant.next - 1, s.durableMax)
        assertEquals(s.accepted, w.pendant.next.toInt() - 1)
    }

    @Test fun demoTransportRunsTheWholeFlowWithoutBle() {
        val clock = VirtualClock(); val fs = MemFs()
        val links = ArrayList<Link>()
        val ctl = Controller(fs, "/real", "/demo", clock, { demo -> check(demo); Demo.link(clock, fs, "/demo").also { links += it } }, { EPOCH + clock.nowMs() }, Settings(demo = true))
        val seen = LinkedHashSet<Conn>()
        ctl.subscribe { seen += it.conn }
        ctl.start()
        assertEquals(Conn.UNREGISTERED, ctl.state.value.conn)
        ctl.scanForRegistration()
        clock.runUntil(10_000) { ctl.state.value.found.isNotEmpty() }
        assertEquals(Demo.ID, ctl.state.value.found[0].id)
        ctl.register(ctl.state.value.found[0])
        clock.runFor(10 * 60_000)
        assertEquals(setOf(Conn.UNREGISTERED, Conn.SCANNING, Conn.CONNECTING, Conn.SYNCING, Conn.IDLE), seen.toSet())
        val s = ctl.state.value
        assertTrue(s.settings.demo)
        assertTrue(s.rows.any { it.kind == "gap" }, "the small demo ring must show a gap")
        assertTrue((links[0] as FakeLink).trace.count { it == "connect" } > 3)
        assertNull(s.error)
        assertEquals(emptyList(), fs.list("/real"))
        assertEquals(listOf("devices", "registration"), fs.list("/demo"))
        val rows = scanBytes(fs.durableBytes("${DeviceStore(fs, "/demo").dir(Demo.ID)}/records.log")!!)
        assertNull(rows.problem); checkCoverage(rows.rows, rows.durableNext)
    }

    // F8: toggling demo off stops the old simulated recorder for good; a new demo pendant records on its own and keeps no copy.
    @Test fun f8_toggledOffDemoPendantStopsRecordingAndDemoKeepsNoCopy() {
        val clock = VirtualClock(); val fs = MemFs()
        val links = ArrayList<FakeLink>()
        val ctl = Controller(fs, "/real", "/demo", clock, { demo -> (if (demo) Demo.link(clock, fs, "/demo") else FakeLink(FakePendant("AA", "x", 1, FakeCfg(), clock), clock)).also { links += it as FakeLink } }, { EPOCH + clock.nowMs() }, Settings(demo = true))
        ctl.start(); ctl.scanForRegistration()
        clock.runUntil(10_000) { ctl.state.value.found.isNotEmpty() }
        ctl.register(ctl.state.value.found[0])
        clock.runFor(2 * 60_000)
        val old = links.last { it.pendant.id == Demo.ID }.pendant
        ctl.updateSettings(Settings(demo = false))
        val n = old.next
        clock.runFor(60_000)
        assertEquals(n, old.next)
        ctl.updateSettings(Settings(demo = true))
        val fresh = links.last { it.pendant.id == Demo.ID }.pendant
        assertNotSame(old, fresh)
        val m = fresh.next
        clock.runFor(60_000)
        assertTrue(fresh.next > m)
        assertEquals(n, old.next)
        assertTrue(fresh.written.isEmpty())
    }

    // F6: a storage failure while registering or forgetting shows 保存エラー, sends nothing, and a retry recovers.
    @Test fun registerAndForgetStorageFailuresShowSaveError() {
        val w = World(1, FakeCfg()); repeat(3) { w.pendant.recordOne() }
        w.ctl.start()
        w.fs.failHook = { op, p, _ -> op == "replace" && p == "/real/registration" }
        val item = ScanItem(w.id, "LifeOS-Pendant")
        w.ctl.register(item) // must not throw
        assertEquals(Conn.ERROR, w.state().conn); assertEquals(ErrKind.SAVE, w.state().error)
        w.clock.runFor(10 * 60_000)
        assertEquals(0, w.ctrls.size); assertEquals(0, w.link.trace.count { it == "connect" })
        w.fs.failHook = null
        w.ctl.register(item)
        assertTrue(w.clock.runUntil(60_000) { w.idleAt(w.pendant.next) })
        w.fs.failHook = { op, p, _ -> op == "syncDir" && p == "/real" }
        w.ctl.forget() // delete done, directory fsync fails
        assertEquals(ErrKind.SAVE, w.state().error)
        val n = w.ctrls.size
        w.clock.runFor(10 * 60_000)
        assertEquals(n, w.ctrls.size)
        w.fs.failHook = null
        w.ctl.forget()
        assertEquals(Conn.UNREGISTERED, w.state().conn); assertNull(w.state().error)
    }

    @Test fun togglingDemoSwitchesRootAndLink() {
        val clock = VirtualClock(); val fs = MemFs()
        val ctl = Controller(fs, "/real", "/demo", clock, { demo -> if (demo) Demo.link(clock, fs, "/demo") else Refuse(clock, "AA") }, { EPOCH }, Settings())
        ctl.start()
        ctl.updateSettings(Settings(demo = true))
        ctl.scanForRegistration(); clock.runUntil(10_000) { ctl.state.value.found.isNotEmpty() }
        ctl.register(ctl.state.value.found[0])
        assertEquals("/demo/registration", "/demo/registration".takeIf { fs.exists(it) })
        assertFalse(fs.exists("/real/registration"))
        ctl.updateSettings(Settings(demo = false))
        assertEquals(Conn.UNREGISTERED, ctl.state.value.conn)
    }

    // F5: a registration is saved but Bluetooth permission is denied (the platform keeps foreground false): no scan, no connect.
    @Test fun manualSyncAndScanDoNothingWithoutForeground() {
        val clock = VirtualClock(); val fs = MemFs()
        var scans = 0
        val link = object : Link {
            override fun scan(onFound: (ScanItem) -> Unit) { scans++ }
            override fun stopScan() {}
            override fun connect(id: String, listener: LinkListener) { scans += 100 }
            override fun readInfo(done: (ByteArray?) -> Unit) {}
            override fun enableData(done: (Boolean) -> Unit) {}
            override fun writeCtrl(bytes: ByteArray) {}
            override fun disconnect() {}
        }
        DeviceStore(fs, "/real").register(Registration("AA", "x"))
        val ctl = Controller(fs, "/real", "/demo", clock, { link }, { EPOCH }, Settings())
        ctl.load(); ctl.setForeground(false)
        ctl.syncNow(); ctl.scanForRegistration()
        clock.runFor(120_000)
        assertEquals(0, scans)
        assertEquals(Conn.IDLE, ctl.state.value.conn)
        ctl.setForeground(true)                                   // permission granted: it works again
        ctl.syncNow()
        assertTrue(scans > 0)
    }
}
