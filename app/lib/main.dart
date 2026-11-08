import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:record/record.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'speaker_verifier.dart';

void main() => runApp(const App());

class App extends StatelessWidget {
  const App({super.key});

  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'Speaker Verify PoC',
        theme: ThemeData(colorSchemeSeed: Colors.indigo, useMaterial3: true),
        home: const HomePage(),
      );
}

enum Mode { idle, enrolling, verifying }

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  static const _prefsKey = 'enroll_embeddings';
  static const _minVoicedSec = 1.5;
  static const _enrollTarget = 3;

  final _verifier = SpeakerVerifier();
  final _recorder = AudioRecorder();
  final _chunks = BytesBuilder(copy: false);
  StreamSubscription<Uint8List>? _sub;

  bool _ready = false;
  Mode _recording = Mode.idle;
  List<Float32List> _enrolled = [];
  double _threshold = 0.33; // LibriSpeech test-clean の EER 閾値。実環境で要調整
  String _message = 'モデルを読み込み中…';
  double? _lastScore;
  bool? _accepted;

  @override
  void initState() {
    super.initState();
    _init();
  }

  Future<void> _init() async {
    final sw = Stopwatch()..start();
    await _verifier.load();
    final prefs = await SharedPreferences.getInstance();
    final saved = prefs.getStringList(_prefsKey) ?? [];
    _enrolled = saved
        .map((s) => Float32List.fromList((jsonDecode(s) as List).cast<num>().map((e) => e.toDouble()).toList()))
        .toList();
    setState(() {
      _ready = true;
      _message = 'モデル読み込み完了 (${sw.elapsedMilliseconds} ms)';
    });
  }

  Future<void> _saveEnrollment() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setStringList(_prefsKey, _enrolled.map((e) => jsonEncode(e.toList())).toList());
  }

  Future<void> _start(Mode mode) async {
    if (!await _recorder.hasPermission()) {
      setState(() => _message = 'マイクの権限がありません');
      return;
    }
    _chunks.clear();
    final stream = await _recorder.startStream(const RecordConfig(
      encoder: AudioEncoder.pcm16bits,
      sampleRate: SpeakerVerifier.sampleRate,
      numChannels: 1,
      // OS 側のノイズ抑圧/AGC は声質を変えるので、登録と照合で条件を揃えるため切っておく
      noiseSuppress: false,
      autoGain: false,
      echoCancel: false,
    ));
    _sub = stream.listen(_chunks.add);
    setState(() {
      _recording = mode;
      _message = mode == Mode.enrolling ? '登録用の発話を録音中… (3〜5秒ほど話してください)' : '照合用の発話を録音中…';
    });
  }

  Future<void> _stop() async {
    final mode = _recording;
    await _recorder.stop();
    await _sub?.cancel();
    setState(() => _recording = Mode.idle);

    final raw = AudioUtil.pcm16ToFloat(_chunks.takeBytes());
    final voiced = AudioUtil.trimSilence(raw);
    final sec = voiced.length / SpeakerVerifier.sampleRate;
    if (sec < _minVoicedSec) {
      setState(() => _message = '発話が短すぎます (有声 ${sec.toStringAsFixed(1)}s)。もう少し長く話してください');
      return;
    }

    final sw = Stopwatch()..start();
    final emb = _verifier.embed(voiced);
    final ms = sw.elapsedMilliseconds;
    final info = '有声 ${sec.toStringAsFixed(1)}s / 推論 $ms ms';

    if (mode == Mode.enrolling) {
      // 2回目以降は、既存の登録と大きく食い違う発話（別人・ノイズ）を弾く
      if (_enrolled.isNotEmpty) {
        final s = SpeakerVerifier.cosine(SpeakerVerifier.average(_enrolled), emb);
        if (s < _threshold) {
          setState(() => _message = '既存の登録音声と一致しません (score ${s.toStringAsFixed(3)})。もう一度録音してください');
          return;
        }
      }
      _enrolled.add(emb);
      await _saveEnrollment();
      setState(() => _message = '登録 ${_enrolled.length}/$_enrollTarget 完了 ($info)');
    } else {
      final score = SpeakerVerifier.cosine(SpeakerVerifier.average(_enrolled), emb);
      setState(() {
        _lastScore = score;
        _accepted = score >= _threshold;
        _message = info;
      });
    }
  }

  Future<void> _reset() async {
    _enrolled = [];
    await _saveEnrollment();
    setState(() {
      _lastScore = null;
      _accepted = null;
      _message = '登録をリセットしました';
    });
  }

  @override
  void dispose() {
    _sub?.cancel();
    _recorder.dispose();
    _verifier.dispose();
    super.dispose();
  }

  Widget _recButton(Mode mode, String label, IconData icon, {bool enabled = true}) {
    final isThis = _recording == mode;
    final busy = _recording != Mode.idle && !isThis;
    return FilledButton.icon(
      onPressed: !_ready || busy || !enabled ? null : () => isThis ? _stop() : _start(mode),
      icon: Icon(isThis ? Icons.stop : icon),
      label: Text(isThis ? '停止して処理' : label),
      style: FilledButton.styleFrom(
        minimumSize: const Size.fromHeight(56),
        backgroundColor: isThis ? Colors.red : null,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final enrolledDone = _enrolled.length >= _enrollTarget;
    final theme = Theme.of(context);
    return Scaffold(
      appBar: AppBar(title: const Text('話者照合 PoC (ECAPA-TDNN INT8)')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: ListTile(
              leading: Icon(enrolledDone ? Icons.verified_user : Icons.person_add),
              title: Text('登録済み発話: ${_enrolled.length} / $_enrollTarget'),
              subtitle: const Text('静かな場所で、毎回違う文を 3〜5 秒話すと精度が上がります'),
              trailing: IconButton(
                icon: const Icon(Icons.delete_outline),
                tooltip: '登録をリセット',
                onPressed: _enrolled.isEmpty || _recording != Mode.idle ? null : _reset,
              ),
            ),
          ),
          const SizedBox(height: 12),
          _recButton(Mode.enrolling, '登録用に録音', Icons.mic),
          const SizedBox(height: 12),
          _recButton(Mode.verifying, '照合する', Icons.record_voice_over, enabled: _enrolled.isNotEmpty),
          const SizedBox(height: 24),
          Text('閾値: ${_threshold.toStringAsFixed(2)}', style: theme.textTheme.titleSmall),
          Slider(
            value: _threshold,
            min: 0.0,
            max: 0.8,
            divisions: 80,
            onChanged: (v) => setState(() {
              _threshold = v;
              if (_lastScore != null) _accepted = _lastScore! >= v;
            }),
          ),
          const SizedBox(height: 8),
          if (_lastScore != null)
            Card(
              color: _accepted! ? Colors.green.shade100 : Colors.red.shade100,
              child: Padding(
                padding: const EdgeInsets.all(20),
                child: Column(children: [
                  Text(_accepted! ? '本人と判定' : '別人と判定',
                      style: theme.textTheme.headlineMedium?.copyWith(color: Colors.black87)),
                  const SizedBox(height: 8),
                  Text('コサイン類似度: ${_lastScore!.toStringAsFixed(3)}',
                      style: theme.textTheme.titleMedium?.copyWith(color: Colors.black87)),
                ]),
              ),
            ),
          const SizedBox(height: 12),
          Text(_message, style: theme.textTheme.bodyMedium),
        ],
      ),
    );
  }
}
