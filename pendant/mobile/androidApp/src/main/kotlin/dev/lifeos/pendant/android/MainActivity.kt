package dev.lifeos.pendant.android

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.provider.Settings as AndroidSettings
import androidx.activity.ComponentActivity
import androidx.activity.enableEdgeToEdge
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import dev.lifeos.pendant.*
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone

private val PERMS = arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)

class MainActivity : ComponentActivity() {
    private val granted = mutableStateOf(false)
    private var started = false
    private lateinit var ctl: Controller

    private fun hasPerms() = PERMS.all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

    /** Sync runs only in the foreground, and only when BLE is allowed or the demo is on. */
    private fun apply() { ctl.setForeground(started && (granted.value || ctl.state.value.settings.demo)) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge() // ステータスバーの文字色を背景に合わせる
        ctl =AppGraph.controller(this)
        setContent {
            MaterialTheme(colorScheme = PendantScheme) {
                val s by ctl.state.collectAsState()
                LaunchedEffect(s.settings.demo, granted.value) { apply() }
                App(s, granted.value, ctl, ::apply, { AppGraph.save(this, it); ctl.updateSettings(it) }, { onGranted -> granted.value = onGranted })
            }
        }
    }

    override fun onStart() { super.onStart(); started = true; granted.value = hasPerms(); apply() }
    override fun onStop() { super.onStop(); started = false; apply() }
}

internal fun utc(ms: Long?) = if (ms == null) "時刻不明" else SimpleDateFormat("yyyy-MM-dd HH:mm:ss'Z'", Locale.US).apply { timeZone = TimeZone.getTimeZone("UTC") }.format(Date(ms))

internal fun status(s: UiState) = when (s.conn) {
    Conn.UNREGISTERED -> "未登録"
    Conn.SCANNING -> "scan 中"
    Conn.CONNECTING -> "接続中"
    Conn.SYNCING -> "同期中（${s.accepted} 件受理）"
    Conn.IDLE -> "待機"
    Conn.ERROR -> "エラー: ${s.error?.let(::errorText) ?: ""}"
}

@Composable
private fun App(s: UiState, granted: Boolean, ctl: Controller, apply: () -> Unit, save: (Settings) -> Unit, setGranted: (Boolean) -> Unit) {
    var tab by remember { mutableIntStateOf(0) }
    Scaffold(
        topBar = { if (s.settings.demo) Surface(color = Color(0xFFB00020)) { Text("DEMO", Modifier.fillMaxWidth().padding(8.dp), color = Color.White, style = MaterialTheme.typography.titleMedium) } },
        bottomBar = {
            NavigationBar {
                listOf("ホーム", "ログ", "診療", "プライバシー", "開発").forEachIndexed { i, t -> NavigationBarItem(selected = tab == i, onClick = { tab = i }, icon = {}, label = { Text(t, maxLines = 1) }) }
            }
        },
    ) { pad ->
        Box(Modifier.padding(pad).padding(12.dp)) {
            when (tab) {
                0 -> HomeTab(s)
                1 -> LogsTab()
                2 -> ClinicTab()
                3 -> PrivacyTab()
                else -> DevTabs(s, granted, ctl, save, setGranted)
            }
        }
    }
}

/** 開発用の既存画面（BLE 同期・受信ログ・設定）。 */
@Composable
private fun DevTabs(s: UiState, granted: Boolean, ctl: Controller, save: (Settings) -> Unit, setGranted: (Boolean) -> Unit) {
    var sub by remember { mutableIntStateOf(0) }
    Column {
        PrimaryTabRow(selectedTabIndex = sub) { listOf("同期", "一覧", "設定").forEachIndexed { i, t -> Tab(selected = sub == i, onClick = { sub = i }, text = { Text(t) }) } }
        Spacer(Modifier.height(12.dp))
        when (sub) {
            0 -> SyncTab(s, granted, ctl, setGranted)
            1 -> ListTab(s)
            else -> SettingsTab(s, ctl, save)
        }
    }
}

@Composable
private fun SyncTab(s: UiState, granted: Boolean, ctl: Controller, setGranted: (Boolean) -> Unit) {
    val ctx = androidx.compose.ui.platform.LocalContext.current
    val activity = ctx as ComponentActivity
    var denied by remember { mutableStateOf(false) }
    val ask = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { r ->
        setGranted(r.values.all { it }); denied = !r.values.all { it }
    }
    Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        if (!granted && !s.settings.demo) {
            Card { Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("ペンダントを探して接続するために「付近のデバイス」の権限が必要です。位置情報は使いません。")
                if (denied) Text("権限が拒否されました。もう一度要求するか、設定アプリで許可してください。")
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { ask.launch(PERMS) }) { Text("権限を要求") }
                    OutlinedButton(onClick = { ctx.startActivity(Intent(AndroidSettings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", ctx.packageName, null))) }) { Text("設定を開く") }
                }
            } }
        }
        val d = s.device
        Text("機器", style = MaterialTheme.typography.titleMedium)
        if (d == null) {
            Text("未登録")
            Button(onClick = { ctl.scanForRegistration() }, enabled = granted || s.settings.demo) { Text("機器を探す") }
            s.found.forEach { f -> OutlinedButton(onClick = { ctl.register(f) }) { Text("登録: ${f.name} (${f.id})") } }
        } else {
            Text("${d.name}\n${d.id}")
        }
        Text("状態: ${status(s)}", style = MaterialTheme.typography.titleMedium)
        if (s.error != null) Text(s.errorDetail, style = MaterialTheme.typography.bodySmall)
        s.info?.let { i ->
            Text("ペンダント: 録音中=${i.recording} boot=${i.boot} tail=${i.tail} next=${i.next} acked=${i.acked} uptime=${i.uptimeS}s RTC=${if (i.rtcMs == 0L) "未設定" else utc(i.rtcMs)}")
        }
        Text("端末に受理済みの最大 seq: ${s.durableMax}")
        s.lastSync?.let { Text("最後の同期: ${utc(it.atMs)} ${if (it.ok) "成功" else "失敗"} (${it.text}) ${it.bytes} B") }
        if (d != null) Button(onClick = { ctl.syncNow() }, enabled = granted || s.settings.demo) { Text("今すぐ同期") }
    }
}

@Composable
private fun ListTab(s: UiState) {
    if (s.device == null) { Text("機器が登録されていません"); return }
    LazyColumn(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        items(s.rows, key = { it.id }) { r ->
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                Text("#${r.seq}", Modifier.width(64.dp))
                Text(r.kind, Modifier.width(64.dp))
                Column { Text(utc(r.utcMs)); if (r.detail.isNotEmpty()) Text(r.detail, style = MaterialTheme.typography.bodySmall) }
            }
        }
    }
}

@Composable
private fun SettingsTab(s: UiState, ctl: Controller, save: (Settings) -> Unit) {
    val st = s.settings
    Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) { Text("demo モード（BLE を使わない）", Modifier.weight(1f)); Switch(st.demo, { save(st.copy(demo = it)) }) }
        Text("同期の周期")
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(30, 60, 300).forEach { p ->
                if (st.periodSec == p) Button(onClick = {}) { Text("$p s") } else OutlinedButton(onClick = { save(st.copy(periodSec = p)) }) { Text("$p s") }
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically) { Text("同期のたびに時刻を合わせる", Modifier.weight(1f)); Switch(st.setTime, { save(st.copy(setTime = it)) }) }
        var confirm by remember { mutableStateOf(false) }
        Button(onClick = { confirm = true }, enabled = s.device != null) { Text("登録した機器を忘れる") }
        Text("忘れても、その機器のログは端末に残ります。同じ機器をもう一度登録すると、続きから同期します。", style = MaterialTheme.typography.bodySmall)
        if (confirm) AlertDialog(
            onDismissRequest = { confirm = false },
            confirmButton = { TextButton(onClick = { confirm = false; ctl.forget() }) { Text("忘れる") } },
            dismissButton = { TextButton(onClick = { confirm = false }) { Text("やめる") } },
            text = { Text("登録を解除します。ログは残ります。") },
        )
        Text("OTA: 未対応（後続）")
        Text("アプリ ${androidx.compose.ui.platform.LocalContext.current.packageManager.getPackageInfo(androidx.compose.ui.platform.LocalContext.current.packageName, 0).versionName} / shared ${s.sharedVersion}")
    }
}
