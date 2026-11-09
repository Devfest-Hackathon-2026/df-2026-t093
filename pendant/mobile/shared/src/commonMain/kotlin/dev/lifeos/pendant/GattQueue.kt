package dev.lifeos.pendant

/** How long a disconnect may wait for CTRL writes already queued (the final ACK) before it closes anyway. */
const val DRAIN_MS = 2_000L

/**
 * Android's GATT operations one at a time (Android drops a call made while another is outstanding) and how a
 * disconnect cancels them. Plain Kotlin so commonTest can drive it; AndroidBleLink feeds it the GATT callbacks.
 */
class GattQueue(private val sched: Scheduler, private val shut: () -> Unit) {
    class Op(val ctrlWrite: Boolean, val start: () -> Boolean, val done: (Boolean, ByteArray?) -> Unit)
    /** GATT-ready (onConnected sent). Before that there is nothing worth draining. */
    var ready = false
    private val q = ArrayDeque<Op>()
    private var cur: Op? = null
    private var closing = false
    private var cancelDrain: (() -> Unit)? = null

    fun enqueue(op: Op) { q += op; pump() }

    /** The in-flight op's callback; ignored when nothing is in flight (late, after a reset). */
    fun complete(ok: Boolean, v: ByteArray? = null) { val op = cur ?: return; cur = null; op.done(ok, v); pump() }

    private fun pump() {
        if (cur != null) return
        val op = q.removeFirstOrNull() ?: return run { if (closing) shut() }
        cur = op
        if (!op.start()) { cur = null; op.done(false, null); pump() }
    }

    /** Not ready, or a setup/read/descriptor op in flight: close now. Otherwise finish only the queued CTRL writes, at most DRAIN_MS. */
    fun disconnect() {
        if (closing) return
        q.removeAll { !it.ctrlWrite }
        val c = cur
        if (!ready || c == null || !c.ctrlWrite) return shut()
        closing = true
        cancelDrain = sched.after(DRAIN_MS) { shut() }
    }

    /** Forgets queued and in-flight ops (their callbacks never run) and the drain timer. */
    fun reset() { cancelDrain?.invoke(); cancelDrain = null; q.clear(); cur = null; closing = false; ready = false }
}
