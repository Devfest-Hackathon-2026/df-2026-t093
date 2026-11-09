package dev.lifeos.pendant

import kotlin.test.*

/** Review findings F1 (directory entries made durable), F2 (demo sender follows its saved log), F3 (unique row identity), F9 (abort before GATT is ready),
 * F10 (Android disconnect cancels setup/reads now and drains only queued CTRL writes, bounded), F11 (FakeLink cancels a pending attach on disconnect). */
class ReviewFixTest {
    @Test fun f1_directoryEntriesAreDurableBeforeAnyAckAndSurviveAPowerCut() {
        val fs = MemFs().also { it.strictDirs = true }
        val w = World(3, FakeCfg(), Settings(setTime = false), fs = fs)
        repeat(6) { w.pendant.recordOne() }
        val prev = w.link.onCtrl
        w.link.onCtrl = { b ->
            prev?.invoke(b)
            if (b[0].toInt() in 1..2) {
                assertEquals(emptySet(), fs.unsyncedDirs, "ACK/SYNC sent while a directory entry is not fsynced")
                assertEquals(emptySet(), fs.unlinkedFiles, "ACK/SYNC sent while a file entry is not fsynced")
            }
        }
        w.begin()
        assertTrue(w.clock.runUntil(60_000) { w.idleAt(w.pendant.next) })
        w.verify()
        fs.crash()
        assertEquals(w.pendant.next, RecordLog.open(fs, w.store.dir(w.id)).durableNext, "the acknowledged log is gone after a power cut")
        assertEquals(w.id, w.store.registration()!!.id)
    }

    // The root was created earlier (iOS AppModel) inside a directory the OS provides: its entry must be fsynced before any ACK/SYNC.
    @Test fun f1_precreatedNestedRootIsDurableBeforeAck() {
        val fs = MemFs()
        fs.mkdirs("/c/home/AppSupport")       // made by the OS: durable
        fs.strictDirs = true
        fs.mkdirs("/c/home/AppSupport/pendant") // made by the app before the store: not fsynced
        val w = World(3, FakeCfg(), Settings(setTime = false), fs = fs, root = "/c/home/AppSupport/pendant")
        repeat(6) { w.pendant.recordOne() }
        val prev = w.link.onCtrl
        w.link.onCtrl = { b ->
            prev?.invoke(b)
            if (b[0].toInt() in 1..2) {
                assertEquals(emptySet(), fs.unsyncedDirs, "ACK/SYNC sent while a directory entry is not fsynced")
                assertEquals(emptySet(), fs.unlinkedFiles, "ACK/SYNC sent while a file entry is not fsynced")
            }
        }
        w.begin()
        assertTrue(w.clock.runUntil(60_000) { w.idleAt(w.pendant.next) })
        w.verify()
        fs.crash()
        assertEquals(w.pendant.next, RecordLog.open(fs, w.store.dir(w.id)).durableNext)
        assertEquals(w.id, w.store.registration()!!.id)
    }

    @Test fun f2_demoSenderResumesFromTheSavedLogAcrossRestartAndToggle() {
        val clock = VirtualClock(); val fs = MemFs()
        fun controller() = Controller(fs, "/real", "/demo", clock, { demo -> if (demo) Demo.link(clock, fs, "/demo") else FakeLink(FakePendant("AA", "x", 1, FakeCfg(), clock), clock) }, { EPOCH + clock.nowMs() }, Settings(demo = true))
        var ctl = controller()
        ctl.start(); ctl.scanForRegistration()
        clock.runUntil(10_000) { ctl.state.value.found.isNotEmpty() }
        ctl.register(ctl.state.value.found[0])
        clock.runFor(2 * 60_000)
        var saved = ctl.state.value.durableMax
        assertTrue(saved > 0 && ctl.state.value.error == null)

        fun synced(what: String) {
            val before = saved
            assertTrue(clock.runUntil(10 * 60_000) { ctl.state.value.durableMax > before }, "$what: no new records after the sender was recreated")
            assertNull(ctl.state.value.error, "$what: ${ctl.state.value.error}")
            saved = ctl.state.value.durableMax
        }
        ctl = controller(); ctl.start()                       // app restart: new controller, new simulated pendant
        synced("restart")
        ctl.updateSettings(Settings(demo = false)); ctl.updateSettings(Settings(demo = true)) // toggle off and on
        synced("toggle")
        ctl.forget(); clock.runFor(1_000)                     // forget and register the same demo pendant again
        ctl.scanForRegistration(); clock.runUntil(10_000) { ctl.state.value.found.isNotEmpty() }
        ctl.register(ctl.state.value.found[0])
        synced("re-register")
        val rows = scanBytes(fs.durableBytes("${DeviceStore(fs, "/demo").dir(Demo.ID)}/records.log")!!)
        assertNull(rows.problem); checkCoverage(rows.rows, rows.durableNext)
    }

    @Test fun f3_rowIdsAreUniqueEvenWhenSeqRepeats() {
        val rows = scanBytes(log(*(1L..5L).map { audio(it) }.toTypedArray(), gap(3, 8))).rows
        val views = rowViews(rows)
        assertEquals(2, views.count { it.seq == 3L })
        assertEquals(views.size, views.map { it.id }.toSet().size)
        assertTrue(views.any { it.detail == "欠落 6..8" })
    }

    // Contract behind the iOS fix: an abort before the link is up still calls link.disconnect() once, and a late onConnected is ignored.
    @Test fun f9_abortBeforeGattReadyDisconnectsOnceAndIgnoresALateConnect() {
        val clock = VirtualClock(); val fs = MemFs()
        val link = ScriptLink(clock, "aa", infoBytes(next = 100))
        val s = Sess(fs, clock, link, Registration("aa", "x"))
        s.session.start()
        s.session.abort("stopped")
        clock.runFor(5_000)  // ScriptLink delivers onConnected after 10 ms even though it is closed
        assertEquals(listOf("connect", "disconnect"), link.trace)
        assertTrue(s.end is End.Incomplete && (s.end as End.Incomplete).why == "stopped")
        clock.runFor(60_000)
        assertEquals(listOf("connect", "disconnect"), link.trace)
        assertEquals("stopped", (s.end as End.Incomplete).why)
    }

    // ---- F10: GattQueue (Android's GATT queue and how disconnect cancels it) ----

    private class QRig {
        val clock = VirtualClock()
        var shuts = 0
        lateinit var q: GattQueue
        init { q = GattQueue(clock) { shuts++; q.reset() } } // AndroidBleLink.close() resets the queue
        val starts = HashMap<String, Int>(); val dones = HashMap<String, Int>()
        fun op(name: String, ctrl: Boolean = false) = GattQueue.Op(ctrl, { starts[name] = (starts[name] ?: 0) + 1; true }) { _, _ -> dones[name] = (dones[name] ?: 0) + 1 }
        fun s(n: String) = starts[n] ?: 0
        fun d(n: String) = dones[n] ?: 0
    }

    @Test fun f10_abortDuringMtuNegotiationClosesNow() {
        val r = QRig()
        r.q.enqueue(r.op("mtu")); r.q.enqueue(r.op("discovery"))
        r.q.disconnect()
        assertEquals(1, r.shuts); assertEquals(0, r.s("discovery"))
        r.q.complete(true)
        assertEquals(0, r.s("discovery")); assertEquals(0, r.d("discovery")); assertEquals(0, r.d("mtu"))
        r.clock.runFor(60_000)
        assertEquals(1, r.shuts)
    }

    @Test fun f10_abortDuringDiscoveryClosesNowAndIgnoresALateResult() {
        val r = QRig()
        r.q.enqueue(r.op("mtu")); r.q.enqueue(r.op("discovery"))
        r.q.complete(true)
        assertEquals(1, r.s("discovery"))
        r.q.disconnect()
        assertEquals(1, r.shuts)
        r.q.complete(true)
        assertEquals(0, r.d("discovery")); assertEquals(1, r.shuts)
    }

    @Test fun f10_abortDuringInfoReadAfterReadyClosesNow() {
        val r = QRig()
        r.q.ready = true
        r.q.enqueue(r.op("read"))
        r.q.disconnect()
        assertEquals(1, r.shuts)
        r.q.complete(true, byteArrayOf(1))
        assertEquals(0, r.d("read"))
        r.clock.runFor(60_000)
        assertEquals(1, r.shuts)
    }

    @Test fun f10_finalAckDrainsQueuedCtrlWritesOnlyThenCloses() {
        val r = QRig()
        r.q.ready = true
        r.q.enqueue(r.op("write1", true)); r.q.enqueue(r.op("read")); r.q.enqueue(r.op("write2", true))
        r.q.disconnect()
        assertEquals(0, r.shuts)
        r.q.complete(true)
        assertEquals(1, r.s("write2")); assertEquals(0, r.s("read")); assertEquals(0, r.shuts)
        r.q.complete(true)
        assertEquals(1, r.shuts); assertEquals(1, r.d("write1")); assertEquals(1, r.d("write2"))
        r.clock.runFor(DRAIN_MS * 2)
        assertEquals(1, r.shuts)
    }

    @Test fun f10_drainIsBoundedWhenTheWriteCallbackNeverComes() {
        val r = QRig()
        r.q.ready = true
        r.q.enqueue(r.op("write", true))
        r.q.disconnect()
        r.clock.runFor(DRAIN_MS - 1)
        assertEquals(0, r.shuts)
        r.clock.runFor(1)
        assertEquals(1, r.shuts)
        r.q.complete(true)
        assertEquals(0, r.d("write")); assertEquals(1, r.shuts)
    }

    @Test fun f10_staleDrainTimerDoesNotCloseTheNextConnection() {
        val r = QRig()
        r.q.ready = true
        r.q.enqueue(r.op("old", true))
        r.q.disconnect()
        r.q.reset()                                   // reconnect
        r.q.ready = true
        r.q.enqueue(r.op("new", true))
        r.clock.runFor(DRAIN_MS * 2)
        assertEquals(0, r.shuts)
    }

    // ---- F11: FakeLink.disconnect() cancels a pending attach ----

    private class Rec : LinkListener {
        val events = ArrayList<String>()
        override fun onConnected(id: String) { events += "connected" }
        override fun onData(bytes: ByteArray) { events += "data" }
        override fun onDisconnected() { events += "disconnected" }
    }

    @Test fun f11_fakeLinkDisconnectBeforeAttachCancelsIt() {
        val clock = VirtualClock(); val p = FakePendant("AA", "x", 1, FakeCfg(), clock); val link = FakeLink(p, clock)
        val l = Rec()
        link.connect("AA", l); clock.runFor(10)
        link.disconnect(); clock.runFor(1_000)
        assertFalse(p.connected); assertEquals(emptyList(), l.events)
        link.connect("AA", l); clock.runFor(1_000)
        assertEquals(listOf("connected"), l.events); assertTrue(p.connected)
    }

    @Test fun f11_backgroundBeforeAttachThenNextForegroundSyncSucceeds() {
        val w = World(5, FakeCfg(), Settings(setTime = false))
        repeat(4) { w.pendant.recordOne() }
        w.begin()
        assertTrue(w.clock.runUntil(10_000) { w.link.trace.lastOrNull() == "connect" })
        w.ctl.setForeground(false)
        w.clock.runFor(1_000)
        assertFalse(w.pendant.connected)
        assertNull(w.state().lastSync)
        w.ctl.setForeground(true)
        assertTrue(w.clock.runUntil(60_000) { w.state().lastSync != null })
        val ls = w.state().lastSync!!
        assertTrue(ls.ok, "first sync after background: ${ls.text}"); assertNull(w.state().error)
    }

    @Test fun f11_demoSwitchBeforeAttachReleasesTheDemoPendant() {
        val clock = VirtualClock(); val fs = MemFs()
        var demo: FakeLink? = null
        val ctl = Controller(fs, "/real", "/demo", clock, { d -> if (d) (Demo.link(clock, fs, "/demo") as FakeLink).also { demo = it } else FakeLink(FakePendant("AA", "x", 1, FakeCfg(), clock), clock) }, { EPOCH + clock.nowMs() }, Settings(demo = true))
        ctl.start(); ctl.register(ScanItem(Demo.ID, "demo"))
        assertTrue(clock.runUntil(10_000) { demo!!.trace.lastOrNull() == "connect" })
        val d = demo!!
        ctl.updateSettings(Settings(demo = false))
        clock.runFor(1_000)
        assertFalse(d.pendant.connected)
    }
}
