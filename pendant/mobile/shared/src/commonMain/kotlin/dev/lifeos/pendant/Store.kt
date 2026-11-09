package dev.lifeos.pendant

class Registration(val id: String, val name: String)

/** What devices/<key>/device holds: who it is and the highest boot counter seen (a lower INFO.boot means a different or reset pendant). */
class DeviceMeta(val id: String, val name: String, val maxBoot: Long)

/**
 * Layout under [root]: `registration` (the one registered device) and `devices/<key>/{records.log,device}`.
 * key = lower-case hex of the UTF-8 OS identifier. Forgetting a device deletes nothing but `registration`.
 */
class DeviceStore(private val fs: Fs, private val root: String) {
    private val regPath = "$root/registration"
    private val through = root.substringBeforeLast('/', "") // Application Support / filesDir: above it the OS answers for the entry

    fun key(id: String) = id.encodeToByteArray().joinToString("") { (it.toInt() and 0xFF).toString(16).padStart(2, '0') }
    fun dir(id: String) = "$root/devices/${key(id)}"

    fun registration(): Registration? {
        if (!fs.exists(regPath)) return null
        val l = fs.readAt(regPath, 0, fs.size(regPath).toInt()).decodeToString().split('\n')
        return if (l.size >= 2 && l[0].isNotEmpty()) Registration(l[0], l[1]) else null
    }

    fun register(r: Registration) {
        fs.mkdirsDurable(root)
        fs.replace(regPath, "${r.id}\n${r.name.replace('\n', ' ')}".encodeToByteArray())
        fs.syncUp(root, through) // root may have been made earlier (iOS AppModel) without anyone fsyncing its entry
    }

    fun forget() {
        if (fs.exists(regPath)) { fs.delete(regPath); fs.syncDir(root) }
    }

    fun device(id: String): DeviceMeta? {
        val p = "${dir(id)}/device"
        if (!fs.exists(p)) return null
        val l = fs.readAt(p, 0, fs.size(p).toInt()).decodeToString().split('\n')
        return if (l.size >= 3) DeviceMeta(l[0], l[1], l[2].toLongOrNull() ?: 0) else null
    }

    fun saveDevice(m: DeviceMeta) {
        fs.mkdirsDurable(dir(m.id))
        fs.replace("${dir(m.id)}/device", "${m.id}\n${m.name.replace('\n', ' ')}\n${m.maxBoot}".encodeToByteArray())
    }

    fun openLog(id: String) = RecordLog.open(fs, dir(id), through)
}
