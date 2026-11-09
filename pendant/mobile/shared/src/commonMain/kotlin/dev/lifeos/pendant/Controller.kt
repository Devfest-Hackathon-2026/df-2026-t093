package dev.lifeos.pendant

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

enum class Conn { UNREGISTERED, SCANNING, CONNECTING, SYNCING, IDLE, ERROR }

class Settings(val demo: Boolean = false, val periodSec: Int = 60, val setTime: Boolean = true) {
    fun copy(demo: Boolean = this.demo, periodSec: Int = this.periodSec, setTime: Boolean = this.setTime) = Settings(demo, periodSec, setTime)
}

class LastSync(val atMs: Long, val ok: Boolean, val text: String, val bytes: Long)

/** [id] (the offset in the log) is unique per row; [seq] is not (a gap may repeat the seq of a record before it). */
class RowView(val id: Long, val seq: Long, val kind: String, val utcMs: Long?, val detail: String)

class UiState(
    val settings: Settings = Settings(),
    val device: Registration? = null,
    val conn: Conn = Conn.UNREGISTERED,
    val accepted: Int = 0,
    val error: ErrKind? = null,
    val errorDetail: String = "",
    val info: Info? = null,
    /** durableNext - 1: the highest seq safely on this phone. */
    val durableMax: Long = 0,
    val lastSync: LastSync? = null,
    val rows: List<RowView> = emptyList(),
    val found: List<ScanItem> = emptyList(),
    val sharedVersion: String = SHARED_VERSION,
)

const val LIST_LIMIT = 200
const val ROWS_REFRESH_MS = 500L
const val SCAN_TIMEOUT_MS = 15_000L
const val BACKOFF_FIRST_MS = 2_000L
const val BACKOFF_MAX_MS = 60_000L

fun errorText(k: ErrKind) = when (k) {
    ErrKind.MISMATCH -> "不一致"; ErrKind.SAVE -> "保存エラー"; ErrKind.LOG_INCONSISTENT -> "ログ不整合"; ErrKind.UNSUPPORTED -> "未対応の形式"
}

/** The list rows, newest first, from the metadata table only. */
fun rowViews(rows: List<MetaRow>, limit: Int = LIST_LIMIT): List<RowView> =
    (rows.size - 1 downTo maxOf(0, rows.size - limit)).map { i ->
        val r = rows[i]
        val d = r.decoded
        val (kind, detail) = when {
            !r.known -> "?" to "未対応の形式（暗号化の可能性）"
            r.hdr.type == T_AUDIO -> "audio" to "${2 * r.hdr.len / 8000.0} s"
            r.hdr.type == T_TIMEFIX -> "timefix" to ""
            d is Decoded.Gap -> "gap" to "欠落 ${r.covFirst}..${d.last}"
            d is Decoded.Event -> "event" to when (d.kind) { 1 -> "button"; 2 -> "AAD wake"; 3 -> "AAD sleep"; 4 -> "reset"; else -> "event ${d.kind}" }
            else -> "?" to ""
        }
        RowView(r.offset, r.hdr.seq, kind, utcMs(rows, i), detail)
    }

/**
 * Screen state and the sync loop (foreground only): register, scan, sync every period, reconnect with a doubling
 * backoff, stop and wait for the user on an error. Single threaded: call it and feed it from the main thread.
 */
class Controller(
    private val fs: Fs, private val realRoot: String, private val demoRoot: String, private val sched: Scheduler,
    private val makeLink: (demo: Boolean) -> Link, private val utcNow: () -> Long, settings: Settings = Settings(),
) {
    private val flow = MutableStateFlow(UiState(settings = settings))
    val state: StateFlow<UiState> get() = flow
    private val listeners = ArrayList<(UiState) -> Unit>()

    private var store = DeviceStore(fs, if (settings.demo) demoRoot else realRoot)
    private var link: Link = makeLink(settings.demo)
    private var session: SyncSession? = null
    private var foreground = false
    private var cancelTimer: (() -> Unit)? = null
    private var backoff = BACKOFF_FIRST_MS
    private var rowsAt = Long.MIN_VALUE / 2
    private var epoch = 0 // bumps when a session or scan is abandoned, so late callbacks are ignored

    /** Swift side: call [cb] now and after every change; the returned function unsubscribes. */
    fun subscribe(cb: (UiState) -> Unit): () -> Unit { listeners += cb; cb(flow.value); return { listeners -= cb } }

    private fun set(f: (UiState) -> UiState) { flow.value = f(flow.value); listeners.toList().forEach { it(flow.value) } }

    // ---------------- lifecycle ----------------

    /** Loads the registration and its log from disk without connecting (the BLE permission may still be missing). */
    fun load() = reload()

    fun start() {
        reload()
        setForeground(true)
    }

    fun setForeground(fg: Boolean) {
        foreground = fg
        if (!fg) { halt(); if (flow.value.conn != Conn.UNREGISTERED && flow.value.conn != Conn.ERROR) set { it.copy2(conn = Conn.IDLE) } }
        else if (flow.value.device != null && flow.value.conn == Conn.IDLE) cycle()
    }

    /** Loads the registration and the table of the registered device from disk. */
    private fun reload() {
        val reg = try { store.registration() } catch (e: Exception) { null }
        set { it.copy2(device = reg, conn = if (reg == null) Conn.UNREGISTERED else Conn.IDLE, error = null, rows = emptyList(), durableMax = 0, info = null, found = emptyList()) }
        if (reg == null) return
        try {
            val l = store.openLog(reg.id)
            set { it.copy2(rows = rowViews(l.rows), durableMax = l.durableNext - 1) }
        } catch (e: LogInconsistent) {
            fail(ErrKind.LOG_INCONSISTENT, e.message ?: "")
        } catch (e: Exception) {
            fail(ErrKind.SAVE, e.message ?: "")
        }
    }

    private fun halt() {
        epoch++
        cancelTimer?.invoke(); cancelTimer = null
        link.stopScan()
        session?.abort("stopped"); session = null
    }

    private fun fail(k: ErrKind, d: String) = set { it.copy2(conn = Conn.ERROR, error = k, errorDetail = d) }

    // ---------------- registration ----------------

    fun scanForRegistration() {
        if (!foreground) return // the platform withholds foreground while BLE is not allowed (and demo is off)
        halt()
        set { it.copy2(conn = Conn.SCANNING, found = emptyList()) }
        link.scan { item -> set { s -> if (s.found.any { it.id == item.id }) s else s.copy2(found = s.found + item) } }
    }

    fun register(item: ScanItem) {
        halt()
        // A storage failure stops here in 保存エラー: no sync runs until the user retries (register again / syncNow reload).
        try { store.register(Registration(item.id, item.name)) } catch (e: Exception) { return fail(ErrKind.SAVE, e.message ?: "") }
        reload()
        if (foreground && flow.value.conn == Conn.IDLE) cycle()
    }

    /** Drops the registration and everything held in memory; devices/<key> stays on disk. */
    fun forget() {
        halt()
        try { store.forget() } catch (e: Exception) { return fail(ErrKind.SAVE, e.message ?: "") }
        reload()
    }

    fun updateSettings(s: Settings) {
        val old = flow.value.settings
        set { it.copy2(settings = s) }
        if (s.demo != old.demo) {
            halt()
            (link as? FakeLink)?.run { pendant.freeze() } // the old simulated pendant must stop recording
            store = DeviceStore(fs, if (s.demo) demoRoot else realRoot)
            link = makeLink(s.demo)
            reload()
            if (foreground && flow.value.conn == Conn.IDLE) cycle()
        }
    }

    // ---------------- sync loop ----------------

    /** The user asks for a sync now (also the way out of an error). Ignored while not in the foreground / BLE not allowed. */
    fun syncNow() {
        if (flow.value.device == null || !foreground) return
        halt()
        backoff = BACKOFF_FIRST_MS
        reload()
        if (flow.value.conn == Conn.IDLE) cycle()
    }

    private fun later(ms: Long) {
        cancelTimer?.invoke()
        val e = epoch
        cancelTimer = sched.after(ms) { if (e == epoch && foreground) cycle() }
    }

    private fun cycle() {
        val reg = flow.value.device ?: return
        halt()
        val e = epoch
        set { it.copy2(conn = Conn.SCANNING, accepted = 0) }
        var found = false
        link.scan { item ->
            if (e != epoch || found || item.id != reg.id) return@scan
            found = true
            link.stopScan()
            cancelTimer?.invoke()
            connect(reg)
        }
        cancelTimer = sched.after(SCAN_TIMEOUT_MS) {
            if (e == epoch && !found) { link.stopScan(); ended(End.Incomplete("not found", 0, 0)) }
        }
    }

    private fun connect(reg: Registration) {
        val e = epoch
        set { it.copy2(conn = Conn.CONNECTING) }
        val s = SyncSession(link, sched, store, reg, flow.value.settings.setTime, utcNow, object : SessionListener {
            override fun onInfo(info: Info) { if (e == epoch) set { it.copy2(info = info, conn = Conn.SYNCING) } }
            override fun onProgress(accepted: Int) {
                if (e != epoch) return
                val l = session?.log ?: return
                val now = sched.nowMs()
                val refresh = now - rowsAt >= ROWS_REFRESH_MS // the list is rebuilt at most twice a second
                if (refresh) rowsAt = now
                set { it.copy2(accepted = accepted, rows = if (refresh) rowViews(l.rows) else it.rows, durableMax = l.durableNext - 1) }
            }
            override fun onEnd(end: End) { if (e == epoch) ended(end) }
        })
        session = s
        s.start()
    }

    private fun ended(end: End) {
        val l = session?.log
        session = null
        epoch++
        val t = utcNow()
        if (l != null) set { it.copy2(rows = rowViews(l.rows), durableMax = l.durableNext - 1) }
        when (end) {
            is End.Done -> {
                backoff = BACKOFF_FIRST_MS
                set { it.copy2(conn = Conn.IDLE, lastSync = LastSync(t, true, "${end.accepted} 件", end.bytes)) }
                later(flow.value.settings.periodSec * 1000L)
            }
            is End.Incomplete -> {
                set { it.copy2(conn = Conn.IDLE, lastSync = LastSync(t, false, end.why, end.bytes)) }
                later(backoff)
                backoff = minOf(backoff * 2, BACKOFF_MAX_MS)
            }
            is End.Error -> {
                set { it.copy2(conn = Conn.ERROR, error = end.kind, errorDetail = end.detail, lastSync = LastSync(t, false, errorText(end.kind), 0)) }
            }
        }
    }
}

private fun UiState.copy2(
    settings: Settings = this.settings, device: Registration? = this.device, conn: Conn = this.conn, accepted: Int = this.accepted,
    error: ErrKind? = if (conn == Conn.ERROR) this.error else null, errorDetail: String = if (conn == Conn.ERROR) this.errorDetail else "",
    info: Info? = this.info, durableMax: Long = this.durableMax, lastSync: LastSync? = this.lastSync, rows: List<RowView> = this.rows,
    found: List<ScanItem> = this.found,
) = UiState(settings, device, conn, accepted, error, errorDetail, info, durableMax, lastSync, rows, found, sharedVersion)
