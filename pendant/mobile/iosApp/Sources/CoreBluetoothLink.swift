import CoreBluetooth
import Shared

/// CoreBluetooth behind the shared Link. Development firmware: no bonding, CBPeripheral.identifier is the device identity.
/// Everything runs on the main queue. The simulator has no BLE: this path is only built there, never exercised (use demo mode).
final class CoreBluetoothLink: NSObject, Shared.Link, CBCentralManagerDelegate, CBPeripheralDelegate {
    private static let svc = CBUUID(string: "a3c1f000-2b6b-4d0c-9f6e-6c6966654f53")
    private static let dataId = CBUUID(string: "a3c1f001-2b6b-4d0c-9f6e-6c6966654f53")
    private static let ctrlId = CBUUID(string: "a3c1f002-2b6b-4d0c-9f6e-6c6966654f53")
    private static let infoId = CBUUID(string: "a3c1f003-2b6b-4d0c-9f6e-6c6966654f53")

    private var central: CBCentralManager!
    private var onFound: ((ScanItem) -> Void)?
    private var seen: [UUID: CBPeripheral] = [:]
    private var peripheral: CBPeripheral?
    private var listener: LinkListener?
    private var data, ctrl, info: CBCharacteristic?
    private var readDone: ((KotlinByteArray?) -> Void)?
    private var enableDone: ((KotlinBoolean) -> Void)?
    private var outbox: [Data] = []
    private var closing = false
    private var timeout: DispatchWorkItem?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    // MARK: scan

    func scan(onFound: @escaping (ScanItem) -> Void) {
        self.onFound = onFound
        startScanIfReady()
    }

    func stopScan() {
        onFound = nil
        if central.state == .poweredOn { central.stopScan() }
    }

    private func startScanIfReady() {
        if onFound != nil && central.state == .poweredOn {
            central.scanForPeripherals(withServices: [Self.svc], options: nil) // the service UUID is in the advertisement
        }
    }

    func centralManagerDidUpdateState(_ c: CBCentralManager) { startScanIfReady() }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        seen[p.identifier] = p
        let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? p.name ?? "LifeOS-Pendant"
        onFound?(ScanItem(id: p.identifier.uuidString, name: name))
    }

    // MARK: connect

    func connect(id: String, listener: LinkListener) {
        reset()
        self.listener = listener
        guard let uuid = UUID(uuidString: id),
              let p = seen[uuid] ?? central.retrievePeripherals(withIdentifiers: [uuid]).first else { return fail() }
        peripheral = p
        p.delegate = self
        let t = DispatchWorkItem { [weak self] in self?.fail() }
        timeout = t
        DispatchQueue.main.asyncAfter(deadline: .now() + 30, execute: t)
        central.connect(p, options: nil)
    }

    private func reset() {
        timeout?.cancel(); timeout = nil
        if let p = peripheral { central.cancelPeripheralConnection(p) }
        peripheral = nil; listener = nil; data = nil; ctrl = nil; info = nil; readDone = nil; enableDone = nil; outbox = []; closing = false
    }

    private func fail() {
        let l = listener
        reset()
        l?.onDisconnected()
    }

    func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) { if p === peripheral { p.discoverServices([Self.svc]) } }
    func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) { if p === peripheral { fail() } }
    func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) { if p === peripheral { fail() } }

    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        guard p === peripheral else { return }
        guard let s = p.services?.first(where: { $0.uuid == Self.svc }) else { return fail() }
        p.discoverCharacteristics([Self.dataId, Self.ctrlId, Self.infoId], for: s)
    }

    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
        guard p === peripheral else { return }
        for c in s.characteristics ?? [] {
            if c.uuid == Self.dataId { data = c } else if c.uuid == Self.ctrlId { ctrl = c } else if c.uuid == Self.infoId { info = c }
        }
        guard data != nil, ctrl != nil, info != nil else { return fail() }
        timeout?.cancel()
        listener?.onConnected(id: p.identifier.uuidString) // the peripheral UUID is the device identity
    }

    // MARK: GATT

    func readInfo(done: @escaping (KotlinByteArray?) -> Void) {
        guard let p = peripheral, let c = info else { return done(nil) }
        readDone = done
        p.readValue(for: c)
    }

    func enableData(done: @escaping (KotlinBoolean) -> Void) {
        guard let p = peripheral, let c = data else { return done(KotlinBoolean(bool: false)) }
        enableDone = done
        p.setNotifyValue(true, for: c)
    }

    func peripheral(_ p: CBPeripheral, didUpdateNotificationStateFor c: CBCharacteristic, error: Error?) {
        guard p === peripheral else { return }
        enableDone?(KotlinBoolean(bool: error == nil)); enableDone = nil
    }

    func peripheral(_ p: CBPeripheral, didUpdateValueFor c: CBCharacteristic, error: Error?) {
        guard p === peripheral else { return }
        guard error == nil, let v = c.value else { if c.uuid == Self.infoId { readDone?(nil); readDone = nil }; return }
        if c.uuid == Self.infoId { readDone?(v.kotlin()); readDone = nil } else if c.uuid == Self.dataId { listener?.onData(bytes: v.kotlin()) }
    }

    func writeCtrl(bytes: KotlinByteArray) {
        outbox.append(bytes.data())
        flush()
    }

    /// write-without-response is flow controlled: send only while the peripheral can take it.
    private func flush() {
        guard let p = peripheral, let c = ctrl else { return }
        while let next = outbox.first, p.canSendWriteWithoutResponse {
            p.writeValue(next, for: c, type: .withoutResponse)
            outbox.removeFirst()
        }
        if outbox.isEmpty && closing { reset() }
    }

    func peripheralIsReady(toSendWriteWithoutResponse p: CBPeripheral) {
        guard p === peripheral else { return }
        flush()
    }

    /// Established: after the CTRL writes already queued, so the final ACK goes out first, but at most 2 s (DRAIN_MS in GattQueue.kt).
    /// Not GATT-ready: nothing to drain, cancel now.
    func disconnect() {
        guard ctrl != nil else { return reset() }
        listener = nil // nothing reaches the ended session while the drain runs
        closing = true
        flush()
        if closing {
            let t = DispatchWorkItem { [weak self] in self?.reset() } // reset() cancels it when the drain finishes first
            timeout = t
            DispatchQueue.main.asyncAfter(deadline: .now() + 2, execute: t)
        }
    }
}
