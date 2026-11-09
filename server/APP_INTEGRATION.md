# アプリ（pendant/mobile）をサーバーにつなぐ手順

Android アプリの固定データ（ハリボテ）を、Cloud Run 上の API（[API.md](./API.md)）につなぎ替える手順。

- 通信処理は KMP の `shared/` に 1 つだけ書き、Android と iOS で共有する。
- **この手順書のコードは、まだ一度もビルドしていない。** コンパイルエラーが出たら、その場で直すこと。ライブラリのバージョンも例なので、新しいものに上げてかまわない。

## 0. 準備

- JDK 17 以上、Android SDK（compileSdk 36）、USB デバッグを有効にした Android 実機。
- 作業ブランチ:
  ```bash
  git fetch origin
  git switch -c app-api origin/server   # ui の最新 + server/ を含む
  ```
- 先にサーバーが動くことを確かめておく（[API.md の curl 例](./API.md#curl-での確認例)）。
- API_TOKEN はチームの人から受け取る。**git にはコミットしない。**

## 1. 方針の変更: 通信を許可する

このアプリは「データを端末から出さない」前提で作られている。サーバーにつなぐと、この前提が変わる。**チームで合意してから進めること。**

### 1-1. `pendant/mobile/androidApp/src/main/AndroidManifest.xml`

```xml
<!-- Scan only for the pendant's service, never for location. -->
<!-- INTERNET: 要約・ログ・診療を pendant-api（server/）から取得する。AI 処理は設定でクラウド送信をオンにしたときだけ。 -->
<uses-permission android:name="android.permission.INTERNET" />
```

もとのコメントにある `no network permission is declared: nothing leaves the phone.` は消す。

### 1-2. `pendant/mobile/verify.sh`

35〜40 行目に「merged manifest に INTERNET が無いこと」の確認（関数 `no_internet` と `step "3b …"`）がある。これを消すか、INTERNET が**ある**ことの確認に反転させる。消す場合の例:

```bash
# step "3b merged manifest has no INTERNET" no_internet   ← この行をコメントアウト
```

`pendant/mobile/README.md` の 10 行目（verify.sh の説明）と、72 行目（検証の行列）の INTERNET に関する記述も、あわせて直す。

## 2. 接続先とトークンを設定する（コミットしない）

### 2-1. `pendant/mobile/local.properties`

このファイルは `.gitignore` に入っている。無ければ作る。

```properties
pendant.apiBaseUrl=https://pendant-api-756406909128.asia-northeast1.run.app
pendant.apiToken=<API_TOKEN>
```

### 2-2. `pendant/mobile/androidApp/build.gradle.kts`

```kotlin
import java.util.Properties

val localProps = Properties().apply {
    rootProject.file("local.properties").takeIf { it.exists() }?.inputStream()?.use { load(it) }
}

android {
    // …既存の設定…
    defaultConfig {
        // …既存の設定…
        buildConfigField("String", "API_BASE_URL", "\"${localProps.getProperty("pendant.apiBaseUrl", "")}\"")
        buildConfigField("String", "API_TOKEN", "\"${localProps.getProperty("pendant.apiToken", "")}\"")
    }
    buildFeatures { compose = true; buildConfig = true }   // buildConfig = true を追加
}
```

> APK に入ったトークンは、取り出そうと思えば取り出せる。デモ用の共有トークンとして割り切る。本番では Firebase Auth による利用者ごとのログインに置き換える。

## 3. `shared/` に API クライアントを追加する

### 3-1. プラグインとライブラリ

`pendant/mobile/build.gradle.kts`（ルート）の plugins に追加:

```kotlin
id("org.jetbrains.kotlin.plugin.serialization") version "2.3.21" apply false
```

`pendant/mobile/shared/build.gradle.kts`:

```kotlin
plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("com.android.library")
    id("org.jetbrains.kotlin.plugin.serialization")   // 追加
}

kotlin {
    // …
    sourceSets {
        commonMain.dependencies {
            implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.2")
            implementation("io.ktor:ktor-client-core:3.3.1")
            implementation("io.ktor:ktor-client-content-negotiation:3.3.1")
            implementation("io.ktor:ktor-serialization-kotlinx-json:3.3.1")
        }
        androidMain.dependencies { implementation("io.ktor:ktor-client-okhttp:3.3.1") }
        iosMain.dependencies { implementation("io.ktor:ktor-client-darwin:3.3.1") }
        commonTest.dependencies { implementation(kotlin("test")) }
    }
}
```

### 3-2. `shared/src/commonMain/kotlin/dev/lifeos/pendant/api/PendantApi.kt`（新規）

```kotlin
package dev.lifeos.pendant.api

import io.ktor.client.HttpClient
import io.ktor.client.call.body
import io.ktor.client.plugins.HttpTimeout
import io.ktor.client.plugins.contentnegotiation.ContentNegotiation
import io.ktor.client.plugins.defaultRequest
import io.ktor.client.request.*
import io.ktor.client.statement.HttpResponse
import io.ktor.client.statement.bodyAsText
import io.ktor.http.ContentType
import io.ktor.http.contentType
import io.ktor.http.isSuccess
import io.ktor.serialization.kotlinx.json.json
import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonNamingStrategy

// ---- モデル（API.md と同じ。キーは snake_case ⇔ camelCase を自動変換） ----

@Serializable data class DeviceStatus(
    val recording: Boolean, val lastSyncAt: String? = null, val powerSource: String? = null,
    val batteryPercent: Int? = null, val backupBattery: String? = null,
    val storedDays: Double? = null, val capacityDays: Double? = null, val updatedAt: String,
)

@Serializable data class Settings(val cloudEnabled: Boolean, val cloudConsentedAt: String? = null)
@Serializable private data class SettingsUpdate(val cloudEnabled: Boolean)

@Serializable data class Line(val id: Int, val time: String, val who: String, val text: String)

@Serializable data class Day(
    val date: String, val summary: String? = null,
    val audioCodec: String? = null, val audioBitrateKbps: Int? = null,
    val audioDurationSec: Int? = null, val audioSizeBytes: Long? = null,
    val updatedAt: String, val lines: List<Line> = emptyList(),
)

@Serializable data class Message(val id: Int, val mine: Boolean, val text: String, val createdAt: String? = null)
@Serializable data class Memo(val id: Int, val text: String, val createdAt: String)
@Serializable data class AskResponse(val question: Message, val answer: Message, val memos: List<Memo>)
@Serializable private data class TextBody(val text: String)

@Serializable data class ClinicSummary(
    val id: Int, val patientName: String, val periodStart: String, val periodEnd: String,
    val concerns: List<String>, val createdAt: String,
)
@Serializable private data class ClinicRequest(val patientName: String, val periodStart: String, val periodEnd: String)

/** status: HTTP ステータス（401 / 404 / 409 / 422 / 502 …）。message: サーバーの detail を含む本文。 */
class ApiException(val status: Int, message: String) : Exception(message)

private suspend inline fun <reified T> HttpResponse.ok(): T {
    if (!status.isSuccess()) throw ApiException(status.value, bodyAsText())
    return body()
}

private suspend fun HttpResponse.okEmpty() {
    if (!status.isSuccess()) throw ApiException(status.value, bodyAsText())
}

class PendantApi(baseUrl: String, token: String) {
    @OptIn(ExperimentalSerializationApi::class)
    private val client = HttpClient {
        expectSuccess = false
        install(ContentNegotiation) {
            json(Json { ignoreUnknownKeys = true; namingStrategy = JsonNamingStrategy.SnakeCase })
        }
        install(HttpTimeout) { requestTimeoutMillis = 60_000 }   // AI 生成は数秒かかる
        defaultRequest {
            url(baseUrl.trimEnd('/') + "/")   // 以下のパスは先頭に / を付けない
            bearerAuth(token)
            contentType(ContentType.Application.Json)
        }
    }

    // ホーム
    @Throws(Exception::class) suspend fun status(): DeviceStatus = client.get("status").ok()

    // 設定
    @Throws(Exception::class) suspend fun settings(): Settings = client.get("settings").ok()
    @Throws(Exception::class) suspend fun setCloud(enabled: Boolean): Settings =
        client.patch("settings") { setBody(SettingsUpdate(enabled)) }.ok()

    // ログ
    @Throws(Exception::class) suspend fun days(limit: Int = 14): List<Day> =
        client.get("days") { parameter("limit", limit) }.ok()
    @Throws(Exception::class) suspend fun summarize(date: String): Day = client.post("days/$date/summarize").ok()

    // 要約タブ
    @Throws(Exception::class) suspend fun suggestions(): List<String> = client.get("assistant/suggestions").ok()
    @Throws(Exception::class) suspend fun messages(): List<Message> = client.get("assistant/messages").ok()
    @Throws(Exception::class) suspend fun ask(text: String): AskResponse =
        client.post("assistant/messages") { setBody(TextBody(text)) }.ok()
    @Throws(Exception::class) suspend fun resetChat() = client.delete("assistant/messages").okEmpty()
    @Throws(Exception::class) suspend fun memos(): List<Memo> = client.get("memos").ok()
    @Throws(Exception::class) suspend fun deleteMemo(id: Int) = client.delete("memos/$id").okEmpty()

    // 診療
    @Throws(Exception::class) suspend fun latestClinic(): ClinicSummary = client.get("clinic-summaries/latest").ok()
    @Throws(Exception::class) suspend fun generateClinic(patientName: String, start: String, end: String): ClinicSummary =
        client.post("clinic-summaries/generate") { setBody(ClinicRequest(patientName, start, end)) }.ok()
}
```

`@Throws` は iOS（Swift）から呼ぶときに必要。付けないと、例外が起きたときにアプリが落ちる。

## 4. Android の画面をつなぐ

### 4-1. API のインスタンス

`androidApp/src/main/kotlin/dev/lifeos/pendant/android/Api.kt`（新規）:

```kotlin
package dev.lifeos.pendant.android

import dev.lifeos.pendant.api.Day
import dev.lifeos.pendant.api.PendantApi
import java.time.LocalDate

val api by lazy { PendantApi(BuildConfig.API_BASE_URL, BuildConfig.API_TOKEN) }

/** "2026-11-09" → "11/9（月）" */
fun Day.label(): String {
    val d = LocalDate.parse(date)
    return "${d.monthValue}/${d.dayOfMonth}（${"月火水木金土日"[d.dayOfWeek.value - 1]}）"
}

/** → "ADPCM 32 kbps ・ 5時間12分 ・ 約 72 MB"（値がなければ "録音情報なし"） */
fun Day.audioLabel(): String {
    val sec = audioDurationSec ?: return "録音情報なし"
    val mb = (audioSizeBytes ?: 0) / 1_000_000
    return "${audioCodec ?: "?"} ${audioBitrateKbps ?: "?"} kbps ・ ${sec / 3600}時間${sec % 3600 / 60}分 ・ 約 $mb MB"
}
```

### 4-2. `ProductUi.kt` の差し替え

固定データ `DAYS` と `SCRIPTS` を、API の値に置き換える。以下はタブごとの書き方の例。必要な import は、`kotlinx.coroutines.launch` と `dev.lifeos.pendant.api.*`。

**要約タブ（`SummaryTab`）**: 一番見栄えに効くので、ここから始める。

```kotlin
@Composable
fun SummaryTab() {
    val scope = rememberCoroutineScope()
    val msgs = remember { mutableStateListOf<Pair<Boolean, String>>() }
    val memo = remember { mutableStateListOf<String>() }
    var chips by remember { mutableStateOf(emptyList<String>()) }
    var today by remember { mutableStateOf<Day?>(null) }
    var busy by remember { mutableStateOf(false) }
    var input by remember { mutableStateOf("") }

    LaunchedEffect(Unit) {
        runCatching {
            chips = api.suggestions()
            msgs += api.messages().map { it.mine to it.text }
            memo += api.memos().map { it.text }
            today = api.days(limit = 1).firstOrNull()
        }.onFailure { msgs += false to "サーバーに接続できません: ${it.message}" }
    }

    fun ask(q: String) = scope.launch {
        busy = true
        msgs += true to q
        runCatching { api.ask(q) }
            .onSuccess { r -> msgs += false to r.answer.text; memo.clear(); memo += r.memos.map { it.text } }
            .onFailure { msgs += false to "エラー: ${it.message}" }
        busy = false
    }

    // 以下のレイアウトは既存のものをそのまま使う。変えるのは次の点だけ:
    // - 「本日の要約」のカード: DAYS[0] → today?.label() / today?.summary
    // - チップ: SCRIPTS.forEach → chips.forEach { c -> AssistChip(onClick = { ask(c) }, enabled = !busy, label = { Text(c) }) }
    // - 送信ボタン: onClick = { ask(input); input = "" }, enabled = input.isNotBlank() && !busy
    // - busy の間はリストの最後に「考え中…」を出すとよい
}
```

**ログタブ（`LogsTab`）**:

```kotlin
var days by remember { mutableStateOf(emptyList<Day>()) }
LaunchedEffect(Unit) { runCatching { days = api.days() } }
// items(DAYS) → items(days)
// d.date → d.label()、d.audio → d.audioLabel()
// 文字起こし: "${it.time.take(5)}  ${it.who}：${it.text}"（"07:42:00" → "07:42"）
// 要約: d.summary ?: "（要約なし）"
```

**ホームタブ（`HomeTab`）**: 今日のハイライトと電源の表示。

```kotlin
var today by remember { mutableStateOf<Day?>(null) }
var st by remember { mutableStateOf<DeviceStatus?>(null) }
LaunchedEffect(Unit) { runCatching { today = api.days(limit = 1).firstOrNull(); st = api.status() } }
// DAYS[0].summary → today?.summary.orEmpty()
// 電源: "蓄電 ${st?.batteryPercent ?: "-"} %"
// 録音中かどうか・同期の状態は、これまでどおり BLE の UiState を使う（ペンダントから直接取れる方が新しい）
```

**診療タブ（`ClinicTab`）**:

```kotlin
var cs by remember { mutableStateOf<ClinicSummary?>(null) }
var busy by remember { mutableStateOf(false) }
val scope = rememberCoroutineScope()
LaunchedEffect(Unit) { runCatching { cs = api.latestClinic() } }
// 「対象期間: ${cs.periodStart}〜${cs.periodEnd}（${cs.patientName}さん）」
// 気になる点: cs.concerns.forEach { Text("・$it") }
// ボタンを 1 つ追加: 「記録からAIで作り直す」
//   onClick = { scope.launch { busy = true; runCatching { api.generateClinic(cs!!.patientName, cs!!.periodStart, cs!!.periodEnd) }.onSuccess { cs = it }; busy = false } }
//   409 が返ったら「設定でクラウド送信をオンにしてください」と表示する
```

**設定タブ（`PrivacyTab`）**: クラウド送信のスイッチをサーバーと同期する。

```kotlin
val scope = rememberCoroutineScope()
LaunchedEffect(Unit) { runCatching { cloud = api.settings().cloudEnabled } }
// オフにする:                  Switch の onCheckedChange で false → scope.launch { cloud = api.setCloud(false).cloudEnabled }
// 確認ダイアログの「理解してオンにする」: scope.launch { cloud = api.setCloud(true).cloudEnabled; confirm = false }
```

### 4-3. エラー表示の目安

| `ApiException.status` | 画面に出す内容 |
|---|---|
| 401 | 「接続設定を確認してください」（トークンの間違い） |
| 409 | 「設定でクラウド送信をオンにしてください」 |
| 502 / 503 | 「AI の応答に失敗しました。時間をおいて再度お試しください」 |
| 通信例外（ApiException 以外） | 「サーバーに接続できません」 |

## 5. ビルドと確認

```bash
cd pendant/mobile
export ANDROID_HOME=~/Android/Sdk          # SDK の場所に合わせる
./gradlew :shared:testDebugUnitTest :androidApp:installDebug
```

`verify.sh` は macOS（Homebrew）のパスを前提にしている。Linux では上のように gradlew を直接使う。

確認の順番:

1. **ログタブ**: 11/9・11/8 の 2 日分が出る（サーバーのデモデータ）。
2. **要約タブ**: チップを押すと台本の答えが返り、メモが増える（クラウド送信オフの動き）。
3. **設定タブ**: クラウド送信をオンにする。
4. **要約タブ**: 自由入力で質問すると、Gemini の答えが数秒で返る。
5. **診療タブ**: 「AIで作り直す」で、気になる点が書き換わる。

うまくいかないとき:

- **すべて失敗する**: `local.properties` の URL とトークンを確認する。`curl` で同じ URL とトークンが通るか試す。
- **最初の 1 回だけ遅い**: Cloud Run のコールドスタート。正常な動き。
- **クラウド送信がいつの間にかオフになっている / データが戻っている**: サーバーが再起動した。デモ構成の仕様（[API.md](./API.md#サーバーのデータについての注意デモ構成)）。

## 6. iOS（Mac で作業する人へ）

- 手順 3 の `shared` は、そのまま iOS でも使える。Swift からは `PendantApi(baseUrl:token:)` を作り、`try await api.ask(text: "…")` のように呼ぶ。`@Throws` を付けた suspend 関数は、Swift では async throws になる。
- URL とトークンは、git に入れない `Secrets.xcconfig` に書き、`Info.plist` 経由で読む（`Bundle.main.object(forInfoDictionaryKey:)`）。
- 画面（`ProductViews.swift`）の差し替え方は、手順 4-2 と同じ考え方。
- ATS（App Transport Security）は HTTPS なので、追加の設定はいらない。

## 7. 終わったら

- `app-api` ブランチを push して PR を出す。**`local.properties` が含まれていないことを確認する。**
- README の「INTERNET 権限なし」に関する記述を更新する。
