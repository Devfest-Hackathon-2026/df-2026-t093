package dev.lifeos.pendant.android

import android.annotation.SuppressLint
import android.content.Context
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.speech.tts.TextToSpeech
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.Locale
import kotlin.concurrent.thread

private const val RATE = 16_000

/** 声を 16 kHz mono 16 bit で録音し、WAV にして返す（Gemini にそのまま渡す）。ファイルには書かない。 */
class Recorder {
    @Volatile private var rec: AudioRecord? = null
    private var reader: Thread? = null
    private val pcm = ByteArrayOutputStream()

    @SuppressLint("MissingPermission") // 呼ぶ前に RECORD_AUDIO を確認している
    fun start() {
        val n = AudioRecord.getMinBufferSize(RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT).coerceAtLeast(3200)
        val r = AudioRecord(MediaRecorder.AudioSource.VOICE_RECOGNITION, RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, n * 2)
        synchronized(pcm) { pcm.reset() }
        rec = r
        r.startRecording()
        reader = thread {
            val buf = ByteArray(n)
            while (rec === r) {
                val k = r.read(buf, 0, n)
                if (k > 0) synchronized(pcm) { pcm.write(buf, 0, k) }
            }
        }
    }

    fun stop(): ByteArray {
        val r = rec ?: return ByteArray(0)
        rec = null
        reader?.join()
        r.stop()
        r.release()
        return wav(synchronized(pcm) { pcm.toByteArray() })
    }

    private fun wav(p: ByteArray): ByteArray = ByteBuffer.allocate(44 + p.size).order(ByteOrder.LITTLE_ENDIAN).apply {
        put("RIFF".toByteArray()); putInt(36 + p.size); put("WAVE".toByteArray())
        put("fmt ".toByteArray()); putInt(16); putShort(1); putShort(1); putInt(RATE); putInt(RATE * 2); putShort(2); putShort(16)
        put("data".toByteArray()); putInt(p.size); put(p)
    }.array()
}

/** 返答の読み上げ（日本語）。 */
class Speaker(ctx: Context) {
    private val tts: TextToSpeech = TextToSpeech(ctx) { if (it == TextToSpeech.SUCCESS) ready() }
    private fun ready() { tts.language = Locale.JAPAN }
    fun say(s: String) { if (Live.readAloud) tts.speak(s, TextToSpeech.QUEUE_FLUSH, null, "anam") }
    fun stop() { tts.stop() }
    fun shutdown() { tts.shutdown() }
}
