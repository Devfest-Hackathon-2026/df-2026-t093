package dev.lifeos.pendant

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import platform.Foundation.NSFileManager
import platform.Foundation.NSFileSize
import platform.posix.*

@OptIn(ExperimentalForeignApi::class)
actual class PlatformFs actual constructor() : Fs {
    private val fm = NSFileManager.defaultManager

    private fun fail(what: String): Nothing = throw RuntimeException("$what: errno $errno")

    private inline fun <T> withFd(path: String, flags: Int, f: (Int) -> T): T {
        val fd = open(path, flags, 0x1A4) // 0644
        if (fd < 0) fail("open $path")
        try { return f(fd) } finally { close(fd) }
    }

    /** F_FULLFSYNC reaches the disk platter/flash; plain fsync on iOS only reaches the drive cache. */
    private fun fullSync(fd: Int) {
        if (fcntl(fd, 51 /* F_FULLFSYNC */) == -1 && fsync(fd) != 0) fail("fsync")
    }

    override fun exists(path: String) = fm.fileExistsAtPath(path)
    override fun mkdirs(path: String) {
        if (!fm.createDirectoryAtPath(path, true, null, null)) fail("mkdirs $path")
    }
    override fun list(dir: String): List<String> = (fm.contentsOfDirectoryAtPath(dir, null) ?: emptyList<Any?>()).map { it.toString() }.sorted()
    override fun size(path: String): Long = (fm.attributesOfItemAtPath(path, null)?.get(NSFileSize) as? Number)?.toLong() ?: fail("size $path")

    override fun readAt(path: String, off: Long, len: Int): ByteArray = withFd(path, O_RDONLY) { fd ->
        val b = ByteArray(len)
        var got = 0
        b.usePinned { p ->
            while (got < len) {
                val n = pread(fd, p.addressOf(got), (len - got).toULong(), off + got)
                if (n < 0) fail("pread")
                if (n == 0L) break
                got += n.toInt()
            }
        }
        if (got == len) b else b.copyOf(got)
    }

    private fun writeAll(fd: Int, bytes: ByteArray) {
        var done = 0
        bytes.usePinned { p ->
            while (done < bytes.size) {
                val n = write(fd, p.addressOf(done), (bytes.size - done).toULong())
                if (n < 0) fail("write")
                done += n.toInt()
            }
        }
    }

    override fun append(path: String, bytes: ByteArray) = withFd(path, O_WRONLY or O_CREAT or O_APPEND) { writeAll(it, bytes) }
    override fun sync(path: String) = withFd(path, O_RDONLY) { fullSync(it) }
    override fun truncate(path: String, len: Long) = withFd(path, O_WRONLY) { if (ftruncate(it, len) != 0) fail("ftruncate") }
    override fun syncDir(dir: String) = withFd(dir, O_RDONLY) { fullSync(it) }

    override fun replace(path: String, bytes: ByteArray) {
        val tmp = "$path.tmp"
        withFd(tmp, O_WRONLY or O_CREAT or O_TRUNC) { writeAll(it, bytes); fullSync(it) }
        if (rename(tmp, path) != 0) fail("rename $path")
        syncDir(path.substringBeforeLast('/'))
    }

    override fun delete(path: String) { if (!fm.removeItemAtPath(path, null) && fm.fileExistsAtPath(path)) fail("delete $path") }
}
