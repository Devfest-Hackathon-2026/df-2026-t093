package dev.lifeos.pendant.android

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import dev.lifeos.pendant.UiState

// 画面。データは Live（Api.kt）から読む。サーバー未設定・未接続のときは下の固定データ（ハリボテ）を表示する。
// 録音の有無・接続状態・最終同期は BLE の実データ（UiState）。

val PendantScheme = lightColorScheme(
    primary = Color(0xFF00796B), secondary = Color(0xFF455A64), tertiary = Color(0xFFB00020),
    secondaryContainer = Color(0xFFB2DFDB), background = Color(0xFFF6FBF9), surface = Color(0xFFF6FBF9),
    surfaceContainer = Color(0xFFE3F2EF), surfaceContainerHighest = Color(0xFFE0EEEB),
)

class Line(val time: String, val who: String, val text: String)
class Day(val date: String, val summary: String, val audio: String, val lines: List<Line>)

val DAYS = listOf(
    Day("11/9（月）", "朝食は完食。午後は膝の痛みを2回訴えた。夜は穏やかに家族と通話。",
        "ADPCM 32 kbps ・ 5時間12分 ・ 約 72 MB", listOf(
            Line("07:42", "ご本人", "今日は、ちょっと膝が痛いねえ。"),
            Line("07:43", "ヘルパー", "湿布を貼りましょうか。朝ごはんの後でいいですか？"),
            Line("14:10", "ご本人", "さっきから、また膝がずきずきする。"),
            Line("19:30", "家族", "お父さん、薬は飲んだ？"),
            Line("19:31", "ご本人", "飲んだよ。大丈夫。"))),
    Day("11/8（日）", "終日落ち着いて過ごした。夕方に物忘れの訴えが1回。",
        "ADPCM 32 kbps ・ 4時間40分 ・ 約 65 MB", listOf(
            Line("10:05", "ご本人", "今日は、何曜日だったかな。"),
            Line("10:06", "家族", "日曜日だよ。散歩に行こうか。"),
            Line("17:20", "ご本人", "鍵をどこに置いたか、忘れてしまった。"))),
)

val CONCERNS = listOf("膝の痛みの訴え: 週5日（朝・午後に多い）", "物忘れの訴え: 週2回（鍵・曜日）", "服薬の確認: 声かけに「飲んだ」と返答 6/7日")

private val TIERS = listOf("文字起こし", "要約", "生データ")

@Composable
fun HomeTab(s: UiState) {
    val rec = s.info?.recording ?: true
    LazyColumn(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Live.error?.let { e -> item { Text(e, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.tertiary) } }
        item {
            Card(Modifier.fillMaxWidth()) {
                Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    // 本体のラッチインジケーター（録音中は赤、停止は黒）と同じ見た目
                    Box(Modifier.size(56.dp).clip(CircleShape).background(if (rec) Color(0xFFD50000) else Color.Black))
                    Column {
                        Text(if (rec) "録音中" else "停止中", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                        Text(if (rec) "ペンダントの前面が赤く表示されています" else "ペンダントの前面は黒です")
                    }
                }
            }
        }
        item { Info("同期", status(s), s.lastSync?.let { "最後: ${utc(it.atMs)}" } ?: "次回: 約2時間ごと") }
        item { Info("電源", "ソーラー発電中", "蓄電 82 % ・ 非常用電池 満") }
        item { Info("端末に保存済み", "3日分のうち 1.2日", "古い順に自動で整理します") }
        item {
            Text("今日のハイライト", style = MaterialTheme.typography.titleMedium)
            Live.days[0].summary.split("。").filter { it.isNotBlank() }.forEach { Text("・$it") }
        }
    }
}

@Composable
private fun Info(title: String, main: String, sub: String) = Card(Modifier.fillMaxWidth()) {
    Column(Modifier.padding(16.dp)) {
        Text(title, style = MaterialTheme.typography.labelLarge)
        Text(main, style = MaterialTheme.typography.titleLarge)
        Text(sub, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
fun LogsTab() {
    var tier by remember { mutableIntStateOf(0) }
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TIERS.forEachIndexed { i, t -> FilterChip(selected = tier == i, onClick = { tier = i }, label = { Text(t) }) }
        }
        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(Live.days) { d ->
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text(d.date, style = MaterialTheme.typography.titleMedium)
                        when (tier) {
                            0 -> d.lines.forEach { Text("${it.time}  ${it.who}：${it.text}") }
                            1 -> Text(d.summary)
                            else -> Text(d.audio)
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun ClinicTab() {
    val scope = rememberCoroutineScope()
    var busy by remember { mutableStateOf(false) }
    var err by remember { mutableStateOf<String?>(null) }
    LazyColumn(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item { Text("次回診療向けサマリー", style = MaterialTheme.typography.headlineSmall) }
        item { Text("対象期間: ${Live.period}（山田 太郎さん）", style = MaterialTheme.typography.bodySmall) }
        item {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("気になる点", style = MaterialTheme.typography.titleMedium)
                    Live.concerns.forEach { Text("・$it") }
                }
            }
        }
        item { Text("※ 会話ログからの自動要約です。診断ではありません。", style = MaterialTheme.typography.bodySmall) }
        if (Api.enabled) item {
            Button(enabled = !busy && Live.cloud, onClick = {
                scope.launch { busy = true; err = runCatching { Live.generateClinic() }.exceptionOrNull()?.message; busy = false }
            }) { Text(if (busy) "Gemini で作成中…" else "記録から作り直す（Gemini）") }
            if (!Live.cloud) Text("設定でクラウド送信をオンにすると使えます", style = MaterialTheme.typography.bodySmall)
            err?.let { Text(it, color = MaterialTheme.colorScheme.tertiary, style = MaterialTheme.typography.bodySmall) }
        }
    }
}

@Composable
fun PrivacyTab(onDev: () -> Unit) {
    val cloud = Live.cloud
    val scope = rememberCoroutineScope()
    var err by remember { mutableStateOf<String?>(null) }
    // サーバーにつながっていればその設定を切り替える（Gemini に送るかどうかはサーバー側でも判定する）
    fun set(on: Boolean) {
        if (!Api.enabled) { Live.cloud = on; return }
        scope.launch { err = runCatching { Live.setCloud(on) }.exceptionOrNull()?.message }
    }
    var confirm by remember { mutableStateOf(false) }
    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("プライバシー", style = MaterialTheme.typography.headlineSmall)
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) { Text("クラウドに送信する", style = MaterialTheme.typography.titleMedium); Text(if (cloud) "オン: 文字起こしテキストを Gemini API に送ります" else "オフ: すべてこの端末の中で処理します（初期値）", style = MaterialTheme.typography.bodySmall) }
                    Switch(cloud, { if (it) confirm = true else set(false) })
                }
                if (cloud) Text("外部サーバーに会話の内容が渡ります。ご本人・ご家族の同意を確認してください。", color = MaterialTheme.colorScheme.tertiary)
                err?.let { Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.tertiary) }
            }
        }
        Card(Modifier.fillMaxWidth()) {
            Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) { Text("返答を読み上げる", style = MaterialTheme.typography.titleMedium); Text("要約タブの返答を声でお伝えします", style = MaterialTheme.typography.bodySmall) }
                Switch(Live.readAloud, { Live.readAloud = it })
            }
        }
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp)) {
                Text("録音していることの表示", style = MaterialTheme.typography.titleMedium)
                Text("録音中はペンダント前面が赤になります。電源が切れると黒になり、周りの方にも分かります。", style = MaterialTheme.typography.bodySmall)
            }
        }
        TextButton(onClick = onDev) { Text("開発者向け画面（BLE 同期・受信ログ）") }
    }
    if (confirm) AlertDialog(
        onDismissRequest = { confirm = false },
        title = { Text("クラウド送信をオンにしますか？") },
        text = { Text("会話の文字起こしが外部のサーバーに送信されます。第三者の声が含まれる場合があります。") },
        confirmButton = { TextButton(onClick = { set(true); confirm = false }) { Text("理解してオンにする") } },
        dismissButton = { TextButton(onClick = { confirm = false }) { Text("やめる") } },
    )
}

// 「先生に伝えたいこと」をAIと相談するアシスタント。サーバーにつながっていればサーバーが返す
// （クラウドがオンなら Gemini、オフなら同じ台本）。未設定・失敗時はアプリ内の台本で返す。
private class Script(val chip: String, val answer: String, val memo: String)

private val SCRIPTS = listOf(
    Script("何を伝えるべき？", "膝の痛みが今週5日あります。痛む時間帯（朝・午後）と、湿布で和らいだかを伝えると診察が早く進みます。物忘れ（鍵・曜日）も週2回ありました。", "膝の痛み: 週5日・朝と午後・湿布で対応"),
    Script("膝の痛みを詳しく", "朝食前後と14時ごろに訴えがあり、「ずきずき」という表現でした。痛みの強さを10段階で本人に聞いておくと、先生に伝えやすくなります。", "痛みの表現: 「ずきずき」（強さは未確認）"),
    Script("服薬は大丈夫？", "19:31に「飲んだよ」と返答があり、今週は6/7日で確認できました。確認できなかった1日は、先生にそのまま伝えましょう。", "服薬: 6/7日確認（1日不明）"),
)

@Composable
fun SummaryTab() {
    val msgs = remember { mutableStateListOf(false to "下の大きなボタンを押して、先生に伝えたいことや気になることを話してください。話し終わったら、もう一度押します。") }
    val memo = Live.memo
    var input by remember { mutableStateOf("") }
    var report by remember { mutableStateOf(false) }
    var busy by remember { mutableStateOf(false) }
    var typing by remember { mutableStateOf(false) }
    var recording by remember { mutableStateOf(false) }
    var secs by remember { mutableIntStateOf(0) }
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    val list = rememberLazyListState()
    val recorder = remember { Recorder() }
    val speaker = remember { Speaker(ctx) }
    DisposableEffect(Unit) { onDispose { recorder.stop(); speaker.shutdown() } }
    LaunchedEffect(msgs.size, busy) { list.animateScrollToItem(msgs.size) }

    fun reply(a: String) { msgs += false to a; speaker.say(a) }
    fun local(a: String, m: String) { reply(a); if (m !in memo) memo += m }
    fun ask(q: String, a: String, m: String) {
        msgs += true to q
        if (!Api.enabled) return local(a, m)
        scope.launch {
            busy = true
            runCatching { Live.ask(q) }.onSuccess { reply(it) }.onFailure { local(a + "（サーバーに接続できないため台本で返答）", m) }
            busy = false
        }
    }
    fun stopAndSend() {
        if (!recording) return
        recording = false
        val wav = recorder.stop()
        if (wav.size < 44 + RATE_BYTES / 2) return reply("短くて聞き取れませんでした。もう一度、ボタンを押してお話しください。")
        scope.launch {
            busy = true
            runCatching { Live.askVoice(wav) }
                .onSuccess { (q, a) -> msgs += true to q; reply(a) }
                .onFailure { reply("うまく届きませんでした。もう一度お話しください。（${it.message}）") }
            busy = false
        }
    }
    fun start() {
        if (!Api.enabled || !Live.cloud) return reply("声で相談するには、設定で「クラウドに送信する」をオンにしてください。")
        speaker.stop()
        recorder.start(); secs = 0; recording = true
    }
    val mic = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { ok ->
        if (ok) start() else reply("マイクの使用が許可されていません。設定アプリで許可してください。")
    }
    LaunchedEffect(recording) {
        while (recording) { delay(1000); secs++; if (secs >= 60) stopAndSend() } // 1 回は最大 60 秒
    }

    Column(Modifier.fillMaxSize().imePadding(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                Text("本日の要約  ${Live.days[0].date}", style = MaterialTheme.typography.titleMedium)
                Text(Live.days[0].summary)
            }
        }
        LazyColumn(Modifier.weight(1f), state = list, verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(msgs) { (mine, text) ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = if (mine) Arrangement.End else Arrangement.Start) {
                    Surface(shape = RoundedCornerShape(16.dp), color = if (mine) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.secondaryContainer) {
                        Text(text, Modifier.padding(12.dp).widthIn(max = 300.dp), color = if (mine) Color.White else Color.Unspecified, fontSize = 18.sp, lineHeight = 26.sp)
                    }
                }
            }
            if (busy) item { Text("聞いています…", Modifier.padding(start = 10.dp), style = MaterialTheme.typography.bodyMedium) }
            if (memo.isNotEmpty()) item {
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp)) {
                        Text("診察で伝えるメモ", style = MaterialTheme.typography.titleMedium)
                        memo.forEach { Text("・$it") }
                    }
                }
            }
        }
        if (report) ReportDialog(reportText(memo)) { report = false }
        // 録音中はペンダントの表示と同じ赤にする
        Button(
            enabled = !busy,
            onClick = {
                when {
                    recording -> stopAndSend()
                    ctx.checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED -> start()
                    else -> mic.launch(Manifest.permission.RECORD_AUDIO)
                }
            },
            modifier = Modifier.fillMaxWidth().height(84.dp),
            shape = RoundedCornerShape(42.dp),
            colors = if (recording) ButtonDefaults.buttonColors(containerColor = Color(0xFFD50000)) else ButtonDefaults.buttonColors(),
        ) {
            Text(if (recording) "話し終わったら押す（${secs}秒）" else if (busy) "考えています…" else "押して話す", fontSize = 22.sp)
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            TextButton(onClick = { typing = !typing }) { Text(if (typing) "文字入力を閉じる" else "文字で入力する") }
            TextButton(onClick = { report = true }) { Text("医師向け報告書にまとめる") }
        }
        if (typing) {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SCRIPTS.forEach { sc -> AssistChip(enabled = !busy, onClick = { ask(sc.chip, sc.answer, sc.memo) }, label = { Text(sc.chip) }) }
            }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(input, { input = it }, Modifier.weight(1f), placeholder = { Text("先生に伝えたいことを相談") })
                Button(enabled = input.isNotBlank() && !busy, onClick = { ask(input, "「$input」を診察で伝えるメモに追加しました。", input); input = "" }) { Text("送信") }
            }
        }
    }
}

private const val RATE_BYTES = 32_000 // 16 kHz × 16 bit = 1 秒あたりのバイト数

/** 医師に渡す報告書。チャットではなく、相談で整理したメモと記録の所見だけを1枚にまとめる。 */
private fun reportText(memo: List<String>) = buildString {
    appendLine("【医師向け報告書】")
    appendLine("患者: 山田 太郎さん")
    appendLine("期間: ${Live.period}")
    appendLine()
    appendLine("【伝えたいこと】")
    if (memo.isEmpty()) appendLine("・（相談で整理した内容はまだありません）") else memo.forEach { appendLine("・$it") }
    appendLine()
    appendLine("【記録からの所見】")
    Live.concerns.forEach { appendLine("・$it") }
    appendLine()
    appendLine("【本日の様子 ${Live.days[0].date}】")
    appendLine(Live.days[0].summary)
    appendLine()
    append("※ ペンダントの会話ログからの自動要約です。診断ではありません。")
}

@Composable
private fun ReportDialog(text: String, onClose: () -> Unit) {
    val ctx = LocalContext.current
    Dialog(onDismissRequest = onClose, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Surface(Modifier.fillMaxSize()) {
            Column(Modifier.systemBarsPadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Column(Modifier.weight(1f).verticalScroll(rememberScrollState())) { Text(text) }
                Button(onClick = { ctx.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text), null)) }, Modifier.fillMaxWidth()) { Text("医師に共有する") }
                OutlinedButton(onClick = onClose, Modifier.fillMaxWidth()) { Text("閉じる") }
            }
        }
    }
}
