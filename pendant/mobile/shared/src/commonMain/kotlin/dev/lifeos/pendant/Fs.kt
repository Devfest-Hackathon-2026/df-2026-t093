package dev.lifeos.pendant

/**
 * The only file API the durable store uses. Paths are "/"-joined strings. Every method may throw; the store
 * turns a failure of a write-side method into a dead RecordLog (see RecordLog.append).
 */
interface Fs {
    fun exists(path: String): Boolean
    fun mkdirs(path: String)
    fun list(dir: String): List<String>
    fun size(path: String): Long
    /** Up to [len] bytes from [off]; fewer at end of file. */
    fun readAt(path: String, off: Long, len: Int): ByteArray
    fun append(path: String, bytes: ByteArray)
    /** fsync the file (Android: FileDescriptor.sync, iOS: F_FULLFSYNC then fsync). */
    fun sync(path: String)
    fun truncate(path: String, len: Long)
    fun syncDir(dir: String)
    /** Write a temp file, fsync it, rename it over [path], fsync the directory. */
    fun replace(path: String, bytes: ByteArray)
    fun delete(path: String)
}

/**
 * mkdirs that is also durable: every directory it creates is made reachable by an fsync of its parent, so a file
 * fsynced inside it is still found after a power cut. A path with no parent component (a test's "/real") has nothing to sync.
 */
fun Fs.mkdirsDurable(path: String) {
    if (exists(path)) return
    val parent = path.substringBeforeLast('/', "")
    if (parent.isNotEmpty()) mkdirsDurable(parent)
    mkdirs(path)
    if (parent.isNotEmpty()) syncDir(parent)
}

/**
 * fsync [from] and each parent up to and including [through] ("" = up to the top component): the directory entries that lead
 * to a file under [from]. Whatever lies above [through] (Android dataDir, iOS Library) is the OS's.
 */
fun Fs.syncUp(from: String, through: String) {
    require(through.isEmpty() || from == through || from.startsWith("$through/")) { "$from is not under $through" }
    var d = from
    while (d.isNotEmpty()) {
        syncDir(d)
        if (d == through) break
        d = d.substringBeforeLast('/', "")
    }
}

/** The device's Fs: java.io + FileChannel.force on Android, posix on iOS. */
expect class PlatformFs() : Fs
