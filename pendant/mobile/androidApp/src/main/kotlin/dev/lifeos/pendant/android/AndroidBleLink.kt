package dev.lifeos.pendant.android

import android.annotation.SuppressLint
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import dev.lifeos.pendant.GattQueue
import dev.lifeos.pendant.Link
import dev.lifeos.pendant.LinkListener
import dev.lifeos.pendant.ScanItem
import dev.lifeos.pendant.Scheduler
import java.util.UUID

private val SVC = UUID.fromString("a3c1f000-2b6b-4d0c-9f6e-6c6966654f53")
private val DATA = UUID.fromString("a3c1f001-2b6b-4d0c-9f6e-6c6966654f53")
private val CTRL = UUID.fromString("a3c1f002-2b6b-4d0c-9f6e-6c6966654f53")
private val INFO = UUID.fromString("a3c1f003-2b6b-4d0c-9f6e-6c6966654f53")
private val CCC = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
private const val MTU = 247
private const val CONNECT_TIMEOUT_MS = 30_000L

/**
 * BluetoothGatt behind the shared Link. Development firmware: no bond (createBond is never called), the OS address
 * is the device identity. Not exercised on a phone yet. Every GATT operation goes through [ops] one at a time, because
 * Android drops a call made while another is outstanding; every callback is moved to the main thread first.
 */
@SuppressLint("MissingPermission") // the UI asks for BLUETOOTH_SCAN / BLUETOOTH_CONNECT before it starts the controller
class AndroidBleLink(private val ctx: Context, sched: Scheduler) : Link {
    private val main = Handler(Looper.getMainLooper())
    private val adapter = ctx.getSystemService(BluetoothManager::class.java)?.adapter
    private var scanCb: ScanCallback? = null
    private var gatt: BluetoothGatt? = null
    private var listener: LinkListener? = null
    private var data: BluetoothGattCharacteristic? = null
    private var ctrl: BluetoothGattCharacteristic? = null
    private var info: BluetoothGattCharacteristic? = null
    private var connectTimeout: Runnable? = null

    private val ops = GattQueue(sched) { close() }

    // ---------------- scan ----------------

    /** Revoked in Settings while the app lives: refuse BLE work instead of letting the call throw SecurityException. */
    private fun allowed() = listOf(android.Manifest.permission.BLUETOOTH_SCAN, android.Manifest.permission.BLUETOOTH_CONNECT)
        .all { ctx.checkSelfPermission(it) == android.content.pm.PackageManager.PERMISSION_GRANTED }

    override fun scan(onFound: (ScanItem) -> Unit) {
        if (!allowed()) return
        stopScan()
        val scanner = adapter?.bluetoothLeScanner ?: return
        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, r: ScanResult) {
                val item = ScanItem(r.device.address, r.scanRecord?.deviceName ?: "LifeOS-Pendant")
                main.post { if (scanCb === this) onFound(item) }
            }
        }
        scanCb = cb
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(SVC)).build() // the service UUID is in the advertisement
        scanner.startScan(listOf(filter), ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), cb)
    }

    override fun stopScan() {
        if (!allowed()) { scanCb = null; return }
        scanCb?.let { adapter?.bluetoothLeScanner?.stopScan(it) }
        scanCb = null
    }

    // ---------------- connection ----------------

    override fun connect(id: String, listener: LinkListener) {
        close()
        this.listener = listener
        if (!allowed()) return fail()
        val dev = adapter?.getRemoteDevice(id) ?: return fail()
        connectTimeout = Runnable { fail() }.also { main.postDelayed(it, CONNECT_TIMEOUT_MS) }
        gatt = dev.connectGatt(ctx, false, callback, android.bluetooth.BluetoothDevice.TRANSPORT_LE)
    }

    private fun fail() {
        val l = listener
        close()
        l?.onDisconnected()
    }

    private fun close() {
        connectTimeout?.let { main.removeCallbacks(it) }
        connectTimeout = null
        ops.reset()
        gatt?.let { it.disconnect(); it.close() }
        gatt = null; listener = null; data = null; ctrl = null; info = null
    }

    /** Cancels setup and reads now; only CTRL writes already queued (the final ACK) go out first, at most DRAIN_MS. Nothing reaches the listener after this. */
    override fun disconnect() {
        if (gatt == null) return
        listener = null
        ops.disconnect()
    }

    override fun readInfo(done: (ByteArray?) -> Unit) {
        val c = info ?: return done(null)
        ops.enqueue(GattQueue.Op(false, { gatt?.readCharacteristic(c) ?: false }) { ok, v -> done(if (ok) v else null) })
    }

    override fun enableData(done: (Boolean) -> Unit) {
        val c = data ?: return done(false)
        gatt?.setCharacteristicNotification(c, true)
        val ccc = c.getDescriptor(CCC) ?: return done(false)
        ops.enqueue(GattQueue.Op(false, {
            if (Build.VERSION.SDK_INT >= 33) gatt?.writeDescriptor(ccc, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == 0
            else @Suppress("DEPRECATION") run { ccc.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE; gatt?.writeDescriptor(ccc) ?: false }
        }) { ok, _ -> done(ok) })
    }

    override fun writeCtrl(bytes: ByteArray) {
        val c = ctrl ?: return
        ops.enqueue(GattQueue.Op(true, {
            if (Build.VERSION.SDK_INT >= 33) gatt?.writeCharacteristic(c, bytes, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) == 0
            else @Suppress("DEPRECATION") run { c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE; c.value = bytes; gatt?.writeCharacteristic(c) ?: false }
        }) { ok, _ -> if (!ok) fail() })
    }

    // ---------------- callbacks (binder thread: hop to main first) ----------------

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            main.post {
                if (g !== gatt) return@post
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    ops.enqueue(GattQueue.Op(false, { g.requestMtu(MTU) }) { _, _ -> }) // completes in onMtuChanged
                    ops.enqueue(GattQueue.Op(false, { g.discoverServices() }) { ok, _ -> if (ok) ready(g) else fail() })
                } else fail()
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) { main.post { if (g === gatt) ops.complete(true) } }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) { main.post { if (g === gatt) ops.complete(status == BluetoothGatt.GATT_SUCCESS) } }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) { main.post { if (g === gatt) ops.complete(status == BluetoothGatt.GATT_SUCCESS) } }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) { main.post { if (g === gatt) ops.complete(status == BluetoothGatt.GATT_SUCCESS) } }

        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
            main.post { if (g === gatt) ops.complete(status == BluetoothGatt.GATT_SUCCESS, value) }
        }

        @Deprecated("API < 33")
        @Suppress("DEPRECATION")
        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            if (Build.VERSION.SDK_INT < 33) { val v = c.value; main.post { if (g === gatt) ops.complete(status == BluetoothGatt.GATT_SUCCESS, v) } }
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) {
            main.post { if (g === gatt && c.uuid == DATA) listener?.onData(value) }
        }

        @Deprecated("API < 33")
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < 33) { val v = c.value; main.post { if (g === gatt && c.uuid == DATA) listener?.onData(v) } }
        }
    }

    private fun ready(g: BluetoothGatt) {
        val s = g.getService(SVC) ?: return fail()
        data = s.getCharacteristic(DATA); ctrl = s.getCharacteristic(CTRL); info = s.getCharacteristic(INFO)
        if (data == null || ctrl == null || info == null) return fail()
        connectTimeout?.let { main.removeCallbacks(it) }
        ops.ready = true
        listener?.onConnected(g.device.address) // the OS address is the device identity
    }
}
