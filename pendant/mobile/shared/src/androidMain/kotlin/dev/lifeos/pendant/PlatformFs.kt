package dev.lifeos.pendant

import java.io.File
import java.io.RandomAccessFile
import java.nio.channels.FileChannel
import java.nio.file.StandardOpenOption

actual class PlatformFs actual constructor() : Fs {
    override fun exists(path: String) = File(path).exists()
    override fun mkdirs(path: String) { File(path).let { if (!it.isDirectory && !it.mkdirs()) throw java.io.IOException("mkdirs $path") } }
    override fun list(dir: String) = File(dir).list()?.sorted() ?: emptyList()
    override fun size(path: String) = File(path).length()

    override fun readAt(path: String, off: Long, len: Int): ByteArray = RandomAccessFile(path, "r").use { f ->
        f.seek(off)
        val b = ByteArray(minOf(len.toLong(), maxOf(0L, f.length() - off)).toInt())
        f.readFully(b)
        b
    }

    override fun append(path: String, bytes: ByteArray) {
        RandomAccessFile(path, "rw").use { it.seek(it.length()); it.write(bytes) }
    }

    override fun sync(path: String) { RandomAccessFile(path, "rw").use { it.fd.sync() } }
    override fun truncate(path: String, len: Long) { RandomAccessFile(path, "rw").use { it.setLength(len) } }

    override fun syncDir(dir: String) {
        FileChannel.open(File(dir).toPath(), StandardOpenOption.READ).use { it.force(true) }
    }

    override fun replace(path: String, bytes: ByteArray) {
        val tmp = File("$path.tmp")
        RandomAccessFile(tmp, "rw").use { it.setLength(0); it.write(bytes); it.fd.sync() }
        if (!tmp.renameTo(File(path))) throw java.io.IOException("rename $path")
        syncDir(File(path).parent!!)
    }

    override fun delete(path: String) { if (!File(path).delete() && File(path).exists()) throw java.io.IOException("delete $path") }
}
