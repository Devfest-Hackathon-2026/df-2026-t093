import 'dart:math' as math;
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:speaker_verify/speaker_verifier.dart';

void main() {
  test('pcm16ToFloat', () {
    final b = ByteData(4)..setInt16(0, 16384, Endian.little)..setInt16(2, -32768, Endian.little);
    expect(AudioUtil.pcm16ToFloat(b.buffer.asUint8List()), [0.5, -1.0]);
  });

  test('trimSilence removes quiet frames', () {
    final x = Float32List(16000);
    for (var i = 3200; i < 12800; i++) {
      x[i] = 0.5 * math.sin(i / 10);
    }
    expect(AudioUtil.trimSilence(x).length, 9600); // 320サンプル単位
  });

  test('average is normalized and cosine is 1 for identical', () {
    final a = Float32List(SpeakerVerifier.embDim)..[0] = 1;
    final b = Float32List(SpeakerVerifier.embDim)..[1] = 1;
    final m = SpeakerVerifier.average([a, b]);
    expect(SpeakerVerifier.cosine(m, m), closeTo(1, 1e-6));
    expect(SpeakerVerifier.cosine(m, a), closeTo(math.sqrt1_2, 1e-6));
  });
}
