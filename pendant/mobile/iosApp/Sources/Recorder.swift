import AVFoundation

/// 声を 16 kHz mono 16 bit の WAV で録音する（Gemini にそのまま渡す）。読み出したら一時ファイルは消す。
final class Recorder {
    private var rec: AVAudioRecorder?
    private let url = FileManager.default.temporaryDirectory.appendingPathComponent("question.wav")

    func start() throws {
        let s = AVAudioSession.sharedInstance()
        try s.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker])
        try s.setActive(true)
        let r = try AVAudioRecorder(url: url, settings: [
            AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: 16_000, AVNumberOfChannelsKey: 1,
            AVLinearPCMBitDepthKey: 16, AVLinearPCMIsFloatKey: false, AVLinearPCMIsBigEndianKey: false,
        ])
        guard r.record() else { throw ApiError(message: "録音を開始できません") }
        rec = r
    }

    func stop() -> Data {
        guard let r = rec else { return Data() }
        r.stop()
        rec = nil
        defer { try? FileManager.default.removeItem(at: url) }
        return (try? Data(contentsOf: url)) ?? Data()
    }
}
