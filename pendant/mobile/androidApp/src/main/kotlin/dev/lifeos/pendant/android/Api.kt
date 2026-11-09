package dev.lifeos.pendant.android

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.time.LocalDate
import java.time.format.TextStyle
import java.util.Locale

/**
 * pendant-api（origin/server の server/）の呼び出し。URL とトークンは pendant/mobile/local.properties の
 * pendant.api.base / pendant.api.token（Git に入れない）。未設定ならアプリは固定データ（ハリボテ）のまま動く。
 */
object Api {
    val enabled get() = BuildConfig.API_BASE.isNotEmpty() && BuildConfig.API_TOKEN.isNotEmpty()

    /** [wav] を渡すと JSON の代わりに音声（audio/wav）をそのまま本文に送る。 */
    suspend fun call(method: String, path: String, body: JSONObject? = null, wav: ByteArray? = null): String = withContext(Dispatchers.IO) {
        val c = URL(BuildConfig.API_BASE.trimEnd('/') + path).openConnection() as HttpURLConnection
        try {
            c.requestMethod = method
            c.connectTimeout = 10_000
            c.readTimeout = 30_000 // Gemini は数秒かかる。止まったら台本に切り替える
            c.setRequestProperty("Authorization", "Bearer ${BuildConfig.API_TOKEN}")
            val (bytes, type) = when {
                wav != null -> wav to "audio/wav"
                body != null -> body.toString().toByteArray() to "application/json"
                else -> null to null
            }
            if (bytes != null) {
                c.doOutput = true
                c.setRequestProperty("Content-Type", type)
                c.outputStream.use { it.write(bytes) }
            }
            val code = c.responseCode
            val text = (if (code < 400) c.inputStream else c.errorStream)?.bufferedReader()?.use { it.readText() }.orEmpty()
            if (code >= 400) error("HTTP $code ${runCatching { JSONObject(text).getString("detail") }.getOrDefault(text).take(150)}")
            text
        } finally {
            c.disconnect()
        }
    }
}

private fun JSONArray.objs() = (0 until length()).map { getJSONObject(it) }
private fun JSONObject.str(k: String) = if (isNull(k)) "" else optString(k)
private fun LocalDate.jp() = "$monthValue/$dayOfMonth（${dayOfWeek.getDisplayName(TextStyle.SHORT, Locale.JAPAN)}）"

private fun day(o: JSONObject): Day {
    val sec = o.optInt("audio_duration_sec")
    val audio = "${o.str("audio_codec")} ${o.optInt("audio_bitrate_kbps")} kbps ・ ${sec / 3600}時間${sec % 3600 / 60}分 ・ 約 ${o.optLong("audio_size_bytes") / 1_000_000} MB"
    val lines = o.getJSONArray("lines").objs().map { Line(it.str("time").take(5), it.str("who"), it.str("text")) }
    return Day(LocalDate.parse(o.getString("date")).jp(), o.str("summary").ifEmpty { "（要約はまだありません）" }, audio, lines)
}

/** 画面が読むデータ。サーバーから読めたら置き換え、読めない・未設定なら固定データのまま。単一スレッド（main）で使う。 */
object Live {
    var days by mutableStateOf(DAYS)
    var concerns by mutableStateOf(CONCERNS)
    var period by mutableStateOf("11/2〜11/9")
    var cloud by mutableStateOf(false)
    val memo = mutableStateListOf<String>()
    var error by mutableStateOf<String?>(null)
    var readAloud by mutableStateOf(true) // 返答を読み上げる（設定タブ）
    private var latest: LocalDate = LocalDate.of(2026, 11, 9)

    suspend fun load() {
        if (!Api.enabled) return
        runCatching {
            val ds = JSONArray(Api.call("GET", "/days")).objs().sortedByDescending { it.getString("date") }
            if (ds.isNotEmpty()) { days = ds.map(::day); latest = LocalDate.parse(ds[0].getString("date")) }
            cloud = JSONObject(Api.call("GET", "/settings")).getBoolean("cloud_enabled")
            setMemo(JSONArray(Api.call("GET", "/memos")))
            runCatching { clinic(JSONObject(Api.call("GET", "/clinic-summaries/latest"))) } // まだ無ければ 404
            error = null
        }.onFailure { error = "サーバーに接続できません。固定データを表示しています（${it.message}）" }
    }

    /** 要約タブの相談。返答を返し、メモはサーバーの一覧で置き換える。クラウドがオフならサーバーが台本で返す。 */
    suspend fun ask(q: String): String {
        val o = JSONObject(Api.call("POST", "/assistant/messages", JSONObject().put("text", q)))
        setMemo(o.getJSONArray("memos"))
        return o.getJSONObject("answer").getString("text")
    }

    /** 声で相談する。音声をそのまま Gemini に渡し、(聞き取った文, 返答) を返す。クラウドがオンのときだけ使える。 */
    suspend fun askVoice(wav: ByteArray): Pair<String, String> {
        val o = JSONObject(Api.call("POST", "/assistant/voice", wav = wav))
        setMemo(o.getJSONArray("memos"))
        return o.getJSONObject("question").getString("text") to o.getJSONObject("answer").getString("text")
    }

    suspend fun setCloud(on: Boolean) {
        cloud = JSONObject(Api.call("PATCH", "/settings", JSONObject().put("cloud_enabled", on))).getBoolean("cloud_enabled")
    }

    /** 直近 7 日の記録から Gemini で「気になる点」を作る（クラウドがオンのときだけ）。 */
    suspend fun generateClinic() {
        val body = JSONObject().put("patient_name", "山田 太郎").put("period_start", latest.minusDays(7).toString()).put("period_end", latest.toString())
        clinic(JSONObject(Api.call("POST", "/clinic-summaries/generate", body)))
    }

    private fun clinic(o: JSONObject) {
        val c = o.getJSONArray("concerns")
        concerns = (0 until c.length()).map { c.getString(it) }
        period = "${LocalDate.parse(o.getString("period_start")).let { "${it.monthValue}/${it.dayOfMonth}" }}〜${LocalDate.parse(o.getString("period_end")).let { "${it.monthValue}/${it.dayOfMonth}" }}"
    }

    private fun setMemo(a: JSONArray) { memo.clear(); memo += a.objs().map { it.getString("text") } }
}
