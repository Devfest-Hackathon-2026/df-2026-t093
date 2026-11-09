package dev.lifeos.pendant.android

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import dev.lifeos.pendant.Controller
import dev.lifeos.pendant.Demo
import dev.lifeos.pendant.PlatformFs
import dev.lifeos.pendant.Scheduler
import dev.lifeos.pendant.Settings
import java.io.File

class MainScheduler : Scheduler {
    private val h = Handler(Looper.getMainLooper())
    override fun nowMs() = SystemClock.elapsedRealtime()
    override fun after(ms: Long, fn: () -> Unit): () -> Unit { val r = Runnable { fn() }; h.postDelayed(r, ms); return { h.removeCallbacks(r) } }
}

/** Settings live in SharedPreferences; the controller gets them at start and on every change. */
object AppGraph {
    private var ctl: Controller? = null

    fun controller(ctx: Context): Controller = ctl ?: run {
        val app = ctx.applicationContext
        val prefs = app.getSharedPreferences("settings", Context.MODE_PRIVATE)
        val settings = Settings(prefs.getBoolean("demo", false), prefs.getInt("periodSec", 60), prefs.getBoolean("setTime", true))
        val sched = MainScheduler()
        val root = app.filesDir // excluded from backup by data_extraction_rules.xml
        val fs = PlatformFs()
        val demoRoot = File(root, "pendant-demo").path
        Controller(
            fs, File(root, "pendant").path, demoRoot, sched,
            { demo -> if (demo) Demo.link(sched, fs, demoRoot) else AndroidBleLink(app, sched) }, { System.currentTimeMillis() }, settings,
        ).also { ctl = it; it.load() }
    }

    fun save(ctx: Context, s: Settings) {
        ctx.applicationContext.getSharedPreferences("settings", Context.MODE_PRIVATE).edit()
            .putBoolean("demo", s.demo).putInt("periodSec", s.periodSec).putBoolean("setTime", s.setTime).apply()
    }
}
