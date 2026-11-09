package dev.lifeos.pendant.android

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import dev.lifeos.pendant.UiState

// ハリボテ: 見た目と画面遷移を決めるための固定データ。録音の有無・接続状態・最終同期だけ実データ（UiState）。
// ponytail: 保存・文字起こし・要約・共有は未接続。実装するときはここの固定データを差し替える。

val PendantScheme = lightColorScheme(
    primary = Color(0xFF00796B), secondary = Color(0xFF455A64), tertiary = Color(0xFFB00020),
    secondaryContainer = Color(0xFFB2DFDB), background = Color(0xFFF6FBF9), surface = Color(0xFFF6FBF9),
    surfaceContainer = Color(0xFFE3F2EF), surfaceContainerHighest = Color(0xFFE0EEEB),
)

private class Line(val time: String, val who: String, val text: String)
private class Day(val date: String, val summary: String, val audio: String, val lines: List<Line>)

private val DAYS = listOf(
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

private val TIERS = listOf("文字起こし", "要約", "生データ")

@Composable
fun HomeTab(s: UiState) {
    val rec = s.info?.recording ?: true
    LazyColumn(verticalArrangement = Arrangement.spacedBy(12.dp)) {
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
            DAYS[0].summary.split("。").filter { it.isNotBlank() }.forEach { Text("・$it") }
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
            items(DAYS) { d ->
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
fun ClinicTab() = LazyColumn(verticalArrangement = Arrangement.spacedBy(12.dp)) {
    item { Text("次回診療向けサマリー", style = MaterialTheme.typography.headlineSmall) }
    item { Text("対象期間: 11/2〜11/9（山田 太郎さん）", style = MaterialTheme.typography.bodySmall) }
    item {
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("気になる点", style = MaterialTheme.typography.titleMedium)
                Text("・膝の痛みの訴え: 週5日（朝・午後に多い）")
                Text("・物忘れの訴え: 週2回（鍵・曜日）")
                Text("・服薬の確認: 声かけに「飲んだ」と返答 6/7日")
            }
        }
    }
    item { Text("※ 会話ログからの自動要約です。診断ではありません。", style = MaterialTheme.typography.bodySmall) }
    item { Button(onClick = {}) { Text("医師に共有する") } }
}

@Composable
fun PrivacyTab() {
    var cloud by remember { mutableStateOf(false) }
    var confirm by remember { mutableStateOf(false) }
    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("プライバシー", style = MaterialTheme.typography.headlineSmall)
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) { Text("クラウドに送信する", style = MaterialTheme.typography.titleMedium); Text(if (cloud) "オン: 文字起こしテキストを Gemini API に送ります" else "オフ: すべてこの端末の中で処理します（初期値）", style = MaterialTheme.typography.bodySmall) }
                    Switch(cloud, { if (it) confirm = true else cloud = false })
                }
                if (cloud) Text("外部サーバーに会話の内容が渡ります。ご本人・ご家族の同意を確認してください。", color = MaterialTheme.colorScheme.tertiary)
            }
        }
        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp)) {
                Text("録音していることの表示", style = MaterialTheme.typography.titleMedium)
                Text("録音中はペンダント前面が赤になります。電源が切れると黒になり、周りの方にも分かります。", style = MaterialTheme.typography.bodySmall)
            }
        }
    }
    if (confirm) AlertDialog(
        onDismissRequest = { confirm = false },
        title = { Text("クラウド送信をオンにしますか？") },
        text = { Text("会話の文字起こしが外部のサーバーに送信されます。第三者の声が含まれる場合があります。") },
        confirmButton = { TextButton(onClick = { cloud = true; confirm = false }) { Text("理解してオンにする") } },
        dismissButton = { TextButton(onClick = { confirm = false }) { Text("やめる") } },
    )
}
