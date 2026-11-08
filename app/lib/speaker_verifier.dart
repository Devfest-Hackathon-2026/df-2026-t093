import 'dart:math' as math;
import 'dart:typed_data';

import 'package:flutter/services.dart';
import 'package:onnxruntime/onnxruntime.dart';

/// ECAPA-TDNN (INT8 / ONNX) で話者埋め込みを計算する。
///
/// モデルは「16kHz の生波形 → L2 正規化済み 192 次元埋め込み」を1本で行う。
/// Fbank などの前処理はモデル内に含まれているので、Dart 側は PCM を float にするだけでよい。
class SpeakerVerifier {
  static const sampleRate = 16000;
  static const embDim = 192;

  OrtSession? _session;

  Future<void> load({String asset = 'assets/models/ecapa_int8.onnx'}) async {
    OrtEnv.instance.init();
    final raw = await rootBundle.load(asset);
    final options = OrtSessionOptions()
      ..setIntraOpNumThreads(4)
      ..setSessionGraphOptimizationLevel(GraphOptimizationLevel.ortEnableAll);
    _session = OrtSession.fromBuffer(raw.buffer.asUint8List(), options);
  }

  /// wav: 16kHz mono, -1..1 の float。戻り値は L2 正規化済み。
  Float32List embed(Float32List wav) {
    final session = _session;
    if (session == null) throw StateError('model not loaded');
    final input = OrtValueTensor.createTensorWithDataList(wav, [1, wav.length]);
    final runOptions = OrtRunOptions();
    final outputs = session.run(runOptions, {'wav': input});
    final emb = (outputs.first!.value as List).first as List;
    final result = Float32List.fromList(emb.cast<double>());
    input.release();
    runOptions.release();
    for (final o in outputs) {
      o?.release();
    }
    return result;
  }

  void dispose() {
    _session?.release();
    OrtEnv.instance.release();
  }

  /// 正規化済み同士なので内積 = コサイン類似度
  static double cosine(Float32List a, Float32List b) {
    var dot = 0.0, na = 0.0, nb = 0.0;
    for (var i = 0; i < a.length; i++) {
      dot += a[i] * b[i];
      na += a[i] * a[i];
      nb += b[i] * b[i];
    }
    return dot / (math.sqrt(na) * math.sqrt(nb) + 1e-12);
  }

  /// 複数回の登録発話を平均して1つの「声紋」にする
  static Float32List average(List<Float32List> embs) {
    final out = Float32List(embDim);
    for (final e in embs) {
      for (var i = 0; i < embDim; i++) {
        out[i] += e[i];
      }
    }
    var n = 0.0;
    for (final v in out) {
      n += v * v;
    }
    n = math.sqrt(n) + 1e-12;
    for (var i = 0; i < embDim; i++) {
      out[i] /= n;
    }
    return out;
  }
}

/// 録音データの前処理
class AudioUtil {
  /// PCM16 little-endian のバイト列 -> float(-1..1)
  static Float32List pcm16ToFloat(Uint8List bytes) {
    final bd = ByteData.sublistView(bytes);
    final n = bytes.length ~/ 2;
    final out = Float32List(n);
    for (var i = 0; i < n; i++) {
      out[i] = bd.getInt16(i * 2, Endian.little) / 32768.0;
    }
    return out;
  }

  /// 簡易エネルギー VAD: 最大フレームから [dbBelowPeak] dB 以上小さいフレームを無音として除去する。
  /// 無音が多いと埋め込みがブレるので、話者照合ではかなり効く。
  static Float32List trimSilence(Float32List x, {double dbBelowPeak = 35, int frame = 320}) {
    final nFrames = x.length ~/ frame;
    if (nFrames == 0) return x;
    final energies = List<double>.generate(nFrames, (f) {
      var e = 0.0;
      for (var i = f * frame; i < (f + 1) * frame; i++) {
        e += x[i] * x[i];
      }
      return 10 * math.log(e / frame + 1e-10) / math.ln10;
    });
    final peak = energies.reduce(math.max);
    final keep = <double>[];
    for (var f = 0; f < nFrames; f++) {
      if (energies[f] > peak - dbBelowPeak) {
        keep.addAll(x.sublist(f * frame, (f + 1) * frame));
      }
    }
    return Float32List.fromList(keep);
  }
}
