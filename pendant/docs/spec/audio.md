# 音声系: マイク・音響・AAD・codec・縮退モード・ASR 評価

録音の入口から、chunk に入れる前までを定める。chunk のバイト配置は [formats.md](./formats.md)、リングとバックログ連動ビットレートの制御は [storage.md](./storage.md)、電力収支は [power.md](./power.md)、防水・筐体は [mechanical.md](./mechanical.md) が持つ。

確度の凡例: **確定** = 決定済みで実装・設計の前提にしてよい / **方針（数値未定）** = 向きは決まっているが数値や選択肢が残る / **仮置き（実測で決める）** = 机上・代理実験の値で、実機や実録音で更新する。

## 1. 構成と要点

```text
T5848 (I²S) ─ I2S20 ─ 16 kHz / 24 bit host slot（LP 有効 16 bit）─ 間引き ─ 8 kHz / 16 bit mono PCM ─ IMA-ADPCM ─ record ─ ring ─ BLE 同期
   │  WAKE / THSEL
   └─ AAD Analog（SCK OFF のときだけ動く）→ 音を検出したら SCK ON → 録音 → 無音が続けば SCK OFF
```

| 項目 | 内容 | 確度 |
|---|---|---|
| マイク | TDK T5848（MMICT5848-00-012、I²S 出力）を 1 個。Standard と Premium で同じ（[product.md](./product.md)）。T5838 から替えた理由は [decisions.md H7](../decisions.md) | 確定（実物は未入手。実機の値は §7.5 で測る） |
| 運用 | 通常は AAD Analog（20 µA）で待ち、音を検出したら低電力 I²S（120 µA）で録音し、無音が続いたら AAD に戻る | 方針（連続低電力 I²S に対する ASR 劣化を §7.5 で確かめてから確定） |
| 録音形式 | LP は 16 kHz・有効 16 bit、48 SCK/stereo frame。FW は 24 bit host slot で受信し、8 kHz・16 bit・mono に間引いて IMA-ADPCM 4 bit。4000 B/s = 32 kbps | 方針は確定。有効 bit の位置・padding・実クロック・CPU コストは実機確認 |
| ビットレート上限 | 32 kbps。16 kHz が必要になっても 2 bit ADPCM などで 32 kbps 以内に収める | 確定 |
| 縮退モード | Premium 製品は空き容量が減ったら 16 kbps 級へ落とし、48 h オフラインを目指す。11 月デモ機は 32 kbps 固定で縮退を使わない | 製品方針（codec と下限レートは未定。§6）。デモ機への統合は未確認 |
| LC3 / Opus / G.726 | 採らない（演算量が節約を上回る）。G.726 は ASR 評価の参照にだけ使う | 確定（cycles/frame を実測して損益分岐を下回る実装が出れば再評価） |
| 語頭欠け | マイクが寝ている間の音は復元できないので、AAD 運用では避けられない。許容量は ASR で決める | 方針（§7.5） |
| 音響 | 上側面〜斜め上向きのポート、短い空洞、oleophobic な ePTFE vent、ボタン機構から離す | 方針（試作で確かめる） |
| 録音を始めてよい条件 | 初期設定（ペアリング）が終わるまで、マイクを動かさない。ペアリング前の録音は保存しない | 確定（手順は [firmware.md](./firmware.md)・[security.md](./security.md)） |

## 2. マイク T5848

### 2.1 仕様

- TDK T5848（MMICT5848-00-012）。bottom-port の I²S デジタル出力のマルチモードマイク。3.5 × 2.65 × 0.98 mm の LGA、8 pad。
- VDD 1.62〜1.98 V。1.8 V の rail に直結し、レベル変換は要らない。デジタル入出力のレベル（VIH / VIL / VOH）は未確認。XIAO の開発基板で 3.3 V 系の nRF とつなぐ場合の扱いは [board.md](./board.md)。
- 低電力モードの SNR 64 dBA、感度 −26 dBFS、AOP 119 dB SPL。感度 −26 dBFS は XIAO nRF54L15 Sense 上の MSM261 と同じ。高品質モードは SNR 68 dBA、感度 −37 dBFS、AOP 133 dB SPL。
- SD は mic の出力で、非選択時は tristate。データシートの指示で **SD に 100 kΩ の pull-down**、VDD に 100 nF（X7R）。
- LR は GND で左チャンネル。WS（LRCK）と SCK は nRF が出す（マスタ）。

| pad | 信号 | 向き（mic から見て） | 接続 |
|---|---|---|---|
| 1 | WS | 入力 | nRF の LRCK |
| 2 | LR | 入力 | GND（左） |
| 3 | GND | | GND（リング） |
| 4 | WAKE | 出力 | MCU の割り込み・起床 GPIO |
| 5 | THSEL | 入力 | AAD の設定（1 線書込み）。MCU の出力 |
| 6 | SCK | 入力 | nRF の SCK |
| 7 | VDD | | `MIC_1V8` |
| 8 | SD | 出力 | nRF の SDIN。100 kΩ pull-down |

- 出典: [TDK T5848 公式製品ページ](https://www.invensense.tdk.com/en-us/products/microphone/t5848) の DS-000479 **Rev 1.1（2026-09-15、2026-10-23 確認）**。電流 typ は保証上限ではない。旧 Rev 1.0／T5838 由来の THSEL 手順は、Rev 1.1 と照合後に実機適用する。
- 価格・在庫（2026-10-18）: DigiKey JP $2.88（在庫 15,115）、Mouser JP ¥475（在庫 40,281）。T5838（PDM、7 pad、ピン非互換）は在庫が戻れば代替になる（[decisions.md H7](../decisions.md)）。

### 2.2 モードとクロック（1.8 V）

| SCK | モード | 電流 | 備考 |
|---|---|---:|---|
| OFF | スリープ | 0.8 µA | **AAD Analog（20 µA）**、AAD D2（110 µA、クロックなし）が動く |
| 200 kHz 未満 | スリープ | 9 µA | 1 線の書き込み（§4.4。50 kHz 超）ができる |
| 768 kHz（1 frame 48 SCK、出力 16 kHz、有効 16 bit、host は 24 bit slot） | 低電力 | 120 µA typ | SNR 64 dBA、AOP 119 dB SPL、−26 dBFS。AAD D1（137 µA typ、I²S 動作中） |
| 2.0〜3.7 MHz | 高品質 | 310 µA | SNR 68 dBA、AOP 133 dB SPL、−37 dBFS |

- モード遷移: スリープ → 低電力のウェイクアップ 6 ms typ、スリープへの移行 1 ms typ。実機の最大遅延とは扱わない。
- WAKE: 音がしきい値を超えている間 HIGH、超えなくなると自動で LOW に戻る（ホストからのリセット不要）。AAD を有効にした直後、マイクが約 12 µs の HIGH パルスで応答する（§4.3）。
- THSEL: AAD の有効化と設定（1 線の書き込み。SCK が 50 kHz 超で動いている間）。AAD を使わないときの WAKE / THSEL の扱いは未確認（T5838 は GND か NC）。

### 2.3 電力（1.8 V）

```text
AAD Analog  20 µA  ≈  36 µW
低電力 I²S 120 µA  ≈ 216 µW
高品質 I²S 310 µA  ≈ 558 µW
```

- 本体平均の目標が 500 µW 級なので、高品質モードの常時動作は予算を超える。連続の低電力 I²S でも 216 µW を使う。
- AAD Analog と低電力 I²S の差は 100 µA（約 180 µW）で、本体平均目標の約 36 % に当たる。AAD を使えるかどうかは電力設計に大きく効く（[power.md](./power.md)）。
- **第一案は AAD Analog（20 µA typ）**。D1（137 µA typ）は LP より大きいが、D2（110 µA typ、SCK OFF）は LP（120 µA typ）より 10 µA 少なく、host clock を止められる可能性もある。D2 を省電力にならないと断定しない。検出品質・起床時の頭欠け・設定の追加検証が要るので、今回新たに導入する前提にはしない。

### 2.4 I2S20 のクロック設定

製品の mic は nRF54L15 の **I2S20** で受ける（Nordic PS v1.0 §8.11 で確認）。PDM 周辺は製品では使わない（XIAO の MSM261 用のみ）。

- I2S20 は周辺ドメインのインスタンスなので、ピンは **P1**。マスタモードで SCK・WS を出す。
- LRCK = MCK / RATIO、SCK = 2 × LRCK × SWIDTH。MCK ≥ SCK で、MCK / LRCK は 2 × SWIDTH の倍数。MCKFREQ は細かい分数で設定でき、動作中に変えられる。

| 用途 | SWIDTH | RATIO | MCK | LRCK | SCK |
|---|---|---|---|---|---|
| 低電力の録音 | 24 bit | 48X | 約 768 kHz | 16 kHz | 768 kHz |
| AAD に入る前の中間（スリープ、§4.5） | 24 bit | 48X | 約 192 kHz | 4 kHz | 192 kHz |

- 中間の 192 kHz は同じ設定で MCK だけ下げ、2 ms 以上保ってから STOP する。THSEL の 1 線書き込み（SCK が 50 kHz 超）も、I2S20 を動かしたままこの間に行える。
- MCKFREQ の設定値と実際の SCK の誤差は未確認（実機で SCK を測る。§未確定）。
- XIAO の MSM261 は PDM の 1〜3.5 MHz を使う（開発用）。ファームの `PENDANT_PDM_CLK_MIN/MAX` の既定値は MSM261 向け。

## 3. 音響設計

マイク IC より、衣服擦れ・風切り音・胸への衝突・ボタンのクリック・防水膜・ポート形状の方が録音品質を支配する見込み。マイクは筐体と一体の音響部品として設計する。

### 3.1 位置

- 推奨: ペンダントの上側面〜斜め上向き。人体から少し離れ、口の方向へ比較的開いた場所。
- 避ける場所: 背面（胸に密着して塞がれる）、Solar panel の真裏（ボタン押下の振動が直撃する）、下向き（衣服との擦れ・遮蔽が増える）、長い内部ダクトの先。

### 3.2 ポートと空洞

T5848 は bottom-port なので PCB に音響穴が要る。

```text
外   ePTFE vent → 短い空洞 → PCB の音響穴 → T5848 bottom port
```

- 音響経路は短くする。細長いトンネルは Helmholtz 共振や高域減衰を作り、ASR の帯域を歪める。
- gasket で mic port と筐体 port の間を seal し、空洞の容積を再現可能にする。
- 接着剤が port に流れ込まない治具を作る。

### 3.3 防水膜

- 候補は **GORE Acoustic Vent GAW334**。ePTFE、oleophobic、IP67 / IP68（2 m・1 h の記載）、1 kHz の透過損失は ID 1.6 mm で 2 dB 未満・ID 2.0 mm で 1.5 dB 未満、支持体の厚さ約 0.31 mm。https://www.gore.com/sites/g/files/ypyipe116/files/2017-01/GORE-Acoustic-Vent-GAW334-Datasheet-en.pdf
- 汗・皮脂があるので、単なる hydrophobic ではなく oleophobic を選ぶ。
- 防水・汗の筐体側の扱いは [mechanical.md](./mechanical.md)。

### 3.4 風切り音

- ePTFE 膜だけでは強風時の低周波の風雑音は消えない。
- 初号機は、port をケース表面から 0.5〜1 mm recess し、acoustic vent まで入れる。必要なら風試験の後に、port 前の小さな mesh、vent 内側の小さな空間、ソフトウェアの high-pass を足す。大きなスポンジは、サイズ・防水・経年劣化を増やすので入れない。

### 3.5 構造伝搬音（衣服擦れ・胸への打撃・ボタン）

- マイクを button plunger / hard-stop から離す。マイク周辺の PCB をケースの衝撃入力点から離す。mic port 側に硬い接触面を作らない。
- 背面の人体接触を軟質部材で少し decouple する案も試験する。
- ボタンの押下時刻はファームが event レコード（[formats.md §4.1](./formats.md)）に記録する。ASR の前処理で、押下から 100〜300 ms 程度をクリック雑音として扱える。クリックを機械的に完全に消すより、発生時刻を知っていることを使う。

### 3.6 マイクの数

- 1 個にする。2 個にすると消費電力がほぼ倍、port・防水部品・PCB 面積・DSP 負荷・較正が増える。第一目的は ASR 用の常時ログなので、1 個で成立性を見る。
- 将来の基板に 2 個目の footprint を DNP で置く余地はある。筐体の port まで最初から増やさない。

### 3.7 初号機の Go / No-Go（方針）

- 胸元装着で、本人の発話を高い確率で ASR できる。
- 1 m 程度の対面会話を実用の精度で認識できる。
- 通常の歩行で、handling noise により録音が飽和しない。
- AAD を使ったとき、連続の低電力 I²S に対して ASR の誤りが許容範囲に収まる（§7.5）。
- acoustic vent の装着の前後で、1 kHz の感度差を確認する。
- 雨滴・汗が付いた後に復帰する。

## 4. AAD（Acoustic Activity Detect）

実装は `fw/lib/t5838/`（THSEL の 1 線プロトコルとモード表）と `fw/lib/aad/`（起床の状態機械）。どちらも純 C でホスト試験済みだが、**現アプリから AAD／WAKE／THSEL の運用には未接続**。T5848 Rev 1.1 とレジスタ・アンロック・pulse timing を照合してから既存ロジックを接続する（ディレクトリ名は T5838 のまま）。連続 I²S の電力を AAD の duty で割り引かない。

### 4.1 方式

- 使うのは **AAD Analog（20 µA）だけ**。AAD D1 / D2 の設定は実装していない（§4.8）。
- **AAD Analog が動くのは SCK OFF（スリープ）のときだけ。** クロックを入れて低電力 I²S にすると WAKE は意味を持たない。低電力・高品質から AAD に入るときは、SCK を一度 50〜200 kHz にして 2 ms 以上保ってから OFF にする（§4.3）。録音をやめる判断は PCM のエネルギーで行う（§4.5）。
- 起床は 6 ms typ（スリープ → 低電力）。最大保証値ではない。WAKE が立つまでの検出遅延も §7.5 で測る。
- **マイクが寝ている間の音は復元できない。** 語頭の欠落は避けられない。AAD D1 は I²S を流し続けるため host 側で取得を続ける設計なら語頭を保持できるが、マイク自体は 137 µA typ で LP の 120 µA typ より大きい。

### 4.2 しきい値と限界

- AAD Analog の最小しきい値は **60 dB SPL（1 kHz）**。60〜97.5 dB SPL の 16 段階。静かな声（50〜55 dB SPL）は起こせない。
- 装着者本人の声は、胸元で 65〜75 dB SPL 程度の想定（未測定）。60 dB SPL 以下の発話は検出されない。

### 4.3 設定（Analog の基本手順を Rev 1.1 と照合、実出力は未検証）

下記の Analog 設定レジスタ・5 回のアンロック書込み・50〜200 kHz を 2 ms 以上経由する基本手順は、T5848 Rev 1.1 §5.4・§5.5 と照合した。T5838 由来の library 全体、THSEL の実パルス、起床時の遅延・D1／D2 は引き続き検証する。

| レジスタ | 値 |
|---|---|
| 0x35[2:0] AAD A_LPF | 1 = 4.4 kHz、2 = 2.0、3 = 1.9、4 = 1.8、5 = 1.6、6 = 1.3、7 = 1.1 kHz |
| 0x36[3:0] AAD A_TH | 0x0 = 60、0x2 = 65、0x4 = 70、0x6 = 75、0x8 = 80、0xA = 85、0xC = 90、0xE = 95、0xF = 97.5 dB SPL |
| 0x29 bit3 AAD A_EN | 1 = 有効 |

手順: VDD と SCK（50 kHz 超）を入れる → **アンロック**（0x5C, 0x3E, 0x6F, 0x3B, 0x4C にそれぞれ 0x00 を書く）→ LPF と TH を設定 → 0x29 = 0x08 → マイクが WAKE に約 12 µs の HIGH パルスを出して有効化を知らせる → SCK を 50〜200 kHz にして 2 ms 以上保つ（中間のスリープ。低電力・高品質で動かしていた場合は必須）→ SCK を OFF にして AAD Analog を動かす。設定は電源を入れるたびに書き直す（電源が続く間は有効）。

- 有効化の応答パルスは WAKE の立ち上がりに見えるので、設定中の WAKE の割り込みは無視する（方針）。

### 4.4 THSEL の 1 線書き込み（表 12・13）

`START/パイロット + 24 bit + STOP`。ペイロードはデバイスアドレス + RW（`1010011` + `0` = 0xA6）、レジスタアドレス、データ。パイロット幅 TP が全シンボルの基準。1 CLK = 1.3 µs（768 kHz）なので、パイロット幅を 16 CLK（20.8 µs）にして、ゼロ = 1 TP、ワン = 3 TP をタイマかビットバンで出す（192 kHz なら 1 CLK = 5.2 µs で幅が 4 倍になる）。

| シンボル | THSEL | 幅 |
|---|---|---|
| Start / Pilot | HIGH | 8〜20 CLK（typ 10 CLK） |
| Zero | HIGH | 1 × TP（max 1.5 × TP） |
| One | HIGH | 2〜3.5 × TP（typ 3） |
| Stop | HIGH | > 128 CLK（または LOW で > 8 × TP） |
| Space | LOW | 1〜2 × TP |

表の幅は T5838 Rev 1.0 の値。T5848 の同じ表は未確認（§未確定）。`t5838_write_pulses()` が（レベル, CLK サイクル数）の列を作り、`t5838_decode_pulses()` が表の範囲で厳密に検査して復号する。パルス列を実際の THSEL に出す部分（タイマ / PWM / ビットバン）は未実装（§未確定）。

### 4.5 起床の状態機械（`aad.c`）

```text
BOOT ──192 kHz + THSEL──→ SLEEP ──WAKE の立ち上がり──→ STARTING ──6 ms──→ ACTIVE
 (電源投入/60 s ごと)      (SCK OFF, AAD A)              (SCK 768 kHz)      (録音開始)
                                                                                │
                         SLEEP ←──192 kHz 2 ms 以上── STOPPING ←──無音 hang_ms──┘
```

- 順序は必ず **SCK ON → 録音開始 → 録音停止 → 中間（50〜200 kHz、2 ms 以上）→ SCK OFF**。クロックの無いマイクを録音しない。`aad.c` は BOOT/STARTING/ACTIVE/STOPPING を持ち、THSEL 書き込みは `ths` コールバックに渡す。`ths` が失敗したら SCK を止めて SLEEP に戻り、`reconfig_ms` 後に再試行する（fail-closed）。
- ACTIVE 中の WAKE の変化は無視する（低電力モードでは AAD Analog が動いていない）。
- **録音の終了は PCM のエネルギーで決める。** 100 ms ブロックの平均二乗が `loud_ms2` を超えない状態が `hang_ms`（初期値 2 s）続いたら止める。音が一度も大きくならなかった場合（クリックなどの誤起床）は `first_hang_ms`（0.3 s）で止め、I²S を無駄に回さない。`wakeup_ms` は 6。
- 試験（ホスト、場面を台本にして実行）: 起動時の THSEL 書き込みと 192 kHz 停止 / 無音で何も起きない / 3 秒の発話で録音が始まり 2 s のハングオーバーで止まる / 誤起床は約 0.4 秒しか録らない / 1 秒の間をあけた会話は途切れず 1 本 / 8 秒あけた 2 回の発話は 2 本 / 録音中の WAKE を無視 / ボタンは全状態で記録 / 60 s ごとの再設定 / SD 全 0 の privacy event。

### 4.6 イベントの記録

ボタンの押下と AAD の起床・睡眠、privacy の検出は event レコード（[formats.md §4.1](./formats.md)）に `mono_ms` つきで記録する（`CHUNK_EV_BUTTON` / `CHUNK_EV_AAD_WAKE` / `CHUNK_EV_AAD_SLEEP` / `CHUNK_EV_PRIVACY_ON`）。受信ツールは「button press at boot N +T ms」と表示する。ボタンの押下は、スリープ中なら録音を始める（用途は前面ボタンの役割と一緒に決める。[product.md](./product.md)）。

### 4.7 電力の見積り

```text
マイク平均電流 = 20 µA × (1 − d) + 120 µA × d      （d = 低電力 I²S の時間割合）
d = 12 % で 32 µA（連続 120 µA に対して約 −88 µA = 約 −160 µW）
```

d が高い環境（街中、カフェ）ほど差は小さい。d は実生活の録音で測る。保存デューティ p と同じ種類のデータで、収支への入れ方は [power.md](./power.md)。

### 4.8 注意

- **D1 / D2 の設定例は T5838 のデータシート Rev 1.0 の中で食い違っている。** §5.20 の AAD D1 の設定例は FLOOR / REL_TH を `0x2B` / `0x36` に書くが、表 19 のレジスタ定義は FLOOR = 0x2A/0x2B、REL_TH = 0x33、ABS_TH = 0x31/0x32 で、例と表が合わない（0x36 は AAD Analog のしきい値レジスタ）。D1 / D2 が必要になったら、T5848 のデータシートと TDK のアプリケーションノートで確認する（T5848 の D1 / D2 の設定は未確認）。
- AAD Analog がスリープ中に動く間、SCK と WS のピンを LOW に保つ必要がある（nRF の I2S20 を STOP したとき SCK / WS が浮かない・Hi-Z にならないこと）。未確認。WS（P1.03）は NFC2 ピンと共用で、NFC ピンの GPIO 化が要る（[board.md §3](./board.md)）。
- ピンの割り当て（WAKE は割り込み・起床が要る GPIO、THSEL は出力のみ）は [board.md](./board.md)。

## 5. codec

### 5.1 通常の codec（確定）

- **IMA-ADPCM 4 bit、8 kHz、mono。** 1 バイトに 2 サンプルを入れ、先のサンプルが下位ニブル（WAV IMA の順）。32 kbps = 4000 B/s。実装は `fw/lib/ima_adpcm/`（純 C）。
- **predictor と step index は record ごとに 0 に初期化する**（エンコーダとデコーダの両側）。record を途中から読んでも、codec 値を切り替えても復号が壊れない。header に初期状態は持たせない（[formats.md §2](./formats.md)）。
- 1 record の長さは約 8 秒（payload 32000 B）が基準。page 境界に揃える場合の長さは [formats.md §3](./formats.md)。
- エンコーダの CPU コスト（XIAO nRF54L15、128 MHz、-Os、RRAM 実行、実測）: **85.5 cycle/sample = 684k cycle/s = 171 cycle/出力 byte**。約 42 µW は別条件の CoreMark 由来約 60.7 pJ/cycle を掛けた推定で、実測電力ではない。I²S・HFXO・DMA・FIR・copy は別に測る（[Premium の電力再評価](../plan/premium-hackathon.md#5-エネルギーの再計算と成立条件)）。

### 5.2 損益分岐（方針）

旧比較の BLE 87 nJ/B は別の 3 V 条件、NAND 15 nJ/B は cap 側で境界が異なり、CPU コストも基準 IMA4 との差分ではなく総コストを引いていた。従来の 102 nJ/B／1,680 cycle/B の判定線と正味表は撤回する。

再評価は全項を同じ rail 境界で測り、`節約電力 = Δ保存byte/s × e_write + Δ同期byte/s × (e_read + e_BLE) − (P_codec_new − P_IMA4) − その他の増分` とする（e は J/B、結果は W）。I²S／clock／buffer の稼働割合が変わるならその差も含める。µW は cycle 数だけから実測扱いしない。[power.md §7](./power.md)。

- MCPS と pJ/cycle は仮定で、実際の固定小数点最適化・sample rate・frame 長・稼働割合で変わる。LC3／Opus が必ず電力で負けるとは結論しない。
- 今回は既存 IMA4 の音声・phone・電源統合を優先する。別 codec は容量／ASR／実測電力で必要性が出た場合に、この差分モデルで評価する。

### 5.3 採否

| codec | 採否 | 理由 |
|---|---|---|
| IMA-ADPCM 4 bit | 通常に使う | 軽く、ASR の劣化が小さい（§7.4） |
| IMA 型 2 bit / 3 bit ADPCM | 縮退側の候補（§6） | step table を引くだけで軽い |
| G.726 | 使わない。ASR 比較の参照のみ | 適応予測と対数量子化で IMA より一桁重くなりうる |
| LC3 / Opus | 今回は見送る | 既存 IMA4 の実機統合を優先。容量／ASR 上の必要性が出たら、§5.2 の同一境界・増分電力で再評価 |

## 6. 縮退モード

### 6.1 位置づけ

- Premium 製品の目標は、スマホ不在でも 48 h 分を保持すること（[product.md](./product.md)）。容量が足りなくなる前に、録音の解像度を下げてでも守る。これが縮退モード。11 月の 512 Mbit デモ機は対象外とし、32 kbps 固定で使う。
- いつ縮退するか（空き容量ルール、ヒステリシス、復帰条件、しきい値）は [storage.md](./storage.md) が持つ。このファイルは、縮退側の codec が満たす条件だけを定める。

### 6.2 縮退側 codec の条件（方針）

- 軽量な ADPCM 系にする。候補は IMA 型の 2 bit または 3 bit（step table 方式）。CPU は 50 cycle/sample 級以下（§5.2 の損益分岐のため）。G.726 は使わない。
- ビットレートの上限 32 kbps を守る。縮退側の下限は 16 kbps（2000 B/s）を仮置きにする（[storage.md](./storage.md) の制御はこの値を前提にしている。codec が決まれば値を差し替えるだけでよい）。24 kbps の中間段は音質の滑らかさのためで、48 h の保証には 2 段（32 / 16 kbps）で足りる。
- モードは record 単位で切り替える。record header の `codec` 値で示す（`codec` 0 = なし、1 = IMA-ADPCM 4 bit / 8 kHz / mono、2〜は縮退側に予約。[formats.md §2](./formats.md)）。record の先頭で predictor と step index を初期化するので、切替や欠落に強い。
- 受信側（スマホ）は、ペンダントが書きうる全ての `codec` 値を復号できなければならない。値と復号の仕様は決まった時点で formats.md に追加する。
- 縮退中は BLE と NAND のエネルギーも半減する。
- 縮退側の最低レートが ASR で使えない場合は、最低レートを上げる（容量の側が増える）。判断は §7.3・§7.4 の基準で行い、容量への影響は [storage.md](./storage.md)。

### 6.3 Standard

縮退モードは Premium の 48 h 保証が要求元。Standard（NAND 512 Mbit〜1 Gbit）で使うかは未確定（§未確定）。

## 7. ASR による評価方法

codec・縮退の下限レート・AAD を判断するための評価の手順。codec と容量の机上計算だけで 48 h の実用性を保証しない。

### 7.1 評価の順序（フル FW の完成を待たずに行う）

1. **PC で codec / ASR を予備選別する。** 胸元の日本語 PCM を 30 分程度集め、同じ音源で PCM 基準、8 kHz / 32 kbps の IMA-ADPCM、採用候補の 16 kbps ADPCM を、実際に encode / decode して ASR を比べる。「16 kbps 相当」の加工では代用しない。2 bit 版は 4 bit IMA と同一仕様とみなさず、アルゴリズム・実装の version・chunk の初期化条件を記録する。
2. **CER で比べる。** 人手の正解文、文字の正規化、ASR のモデル / version、言語・decode の設定を固定する。PCM 比の劣化と絶対 CER を併記する。8 kHz の入力は ASR の入力仕様に合わせて resample し、その条件も固定する。必要なら 16 kHz 系を同じ音源・同じ bitrate の制約で比べる。
3. **T5848 の実機で AAD を測る。** 会話の開始、衣擦れ、歩行、電車、屋外の風、無音のオフィス。検出から PCM 取得までの遅延、発話冒頭の欠損、誤検出、発話の見逃し、hangover を含む保存時間を記録する。PC の VAD 評価だけで T5848 AAD の検証を代替しない。
4. **代表環境で追試する。** 30 分は予備選別に限る。複数の話者・距離・騒音で ASR を追試し、日単位で保存デューティ p（発話以外の音・誤検出・hangover を含む）を測る。p の合否は固定の閾値ではなく、実 codec の電力・BLE の mJ/MB と合わせて収支（[power.md](./power.md)）を更新して判断する。
5. **確定判断。** 最低レートで ASR が不足すれば最低レートを上げる（容量の側が増える。[storage.md](./storage.md)）。AAD が不足すれば連続の低電力 I²S で電力・容量を再評価する。

### 7.2 比較条件（再現に必要な記録）

- ASR: モデル名と version、言語、temperature などの decode 設定。
- 正規化: NFKC のあと、文字・数字以外を除去する。
- CER: (1) 参照テキストとの絶対 CER、(2) PCM の認識結果に対する CER（= PCM 比の劣化量。漢字・かなの表記ゆれの影響を除く）。
- 入力: 8 kHz 16 bit mono。ASR 入力は 16 kHz に補間する。
- codec: 実装の version と、state の初期化条件（record ごとのリセット）。

### 7.3 判定基準（結果を見る前に固定した値。#19）

| 対象 | 条件 | 基準 |
|---|---|---|
| 32 kbps（IMA 4 bit） | 無雑音と SNR 20 dB | PCM 比の劣化 ≤ 2 ポイント |
| 16 kbps（2 bit 級） | 無雑音と SNR 20 dB | PCM 比の劣化 ≤ 5 ポイント |
| 16 kbps（2 bit 級） | SNR 10 dB | 絶対 CER ≤ 20 % |
| 全 codec | SNR 5 dB | 参考（基準に使わない） |

判定ルール: 参照の G.726 16 kbps（IMA 型の簡易 2 bit より高品質な 2 bit ADPCM）が 16 kbps の基準を満たさなければ、2 bit ADPCM 級は不採用にして 24 kbps（3 bit）か容量増へ進む。満たすなら、実装する簡易 2 bit（IMA 型）を同じ基準で別途測る。

### 7.4 代理実験（codec だけの影響）

- 方法: macOS `say` の日本語 3 声（Kyoko / Reed / Eddy）、12 文（約 330 文字）を 0.8 s の無音で連結し、8 kHz 16 bit mono にする。ピンクノイズ（固定シード）を SNR 20 / 10 / 5 dB で加算する（SNR は発話区間の RMS 基準）。無雑音を含め 4 水準。codec は PCM / IMA-ADPCM 4 bit（ファームの実装、8000 B ごとに状態リセット）/ G.726 24 kbps / G.726 16 kbps（ffmpeg）。ASR は Whisper large-v3-turbo（openai-whisper 20250625、ローカル）、言語 ja、temperature 0。3 声 × 4 SNR × 4 codec = 48 条件。
- 状態: 32 kbps の劣化は無雑音 0.7 / 20 dB 0.3 ポイント、G.726 16 kbps の劣化は無雑音 0.7 / 20 dB 0.3 ポイント、10 dB の絶対 CER は 2.9 % で、**§7.3 の基準を満たした**（#19）。事前の判定ルールにより 2 bit ADPCM 級は不採用にならない。
- 限界（**仮置き**）: Whisper large-v3-turbo は頑健で、TTS 音声は明瞭、雑音は合成なので、codec の差を見分けるには条件が甘い（差は 1 ポイント未満で誤差の範囲。1 文字 ≈ 0.3 ポイント）。実録音で再評価するまで「16 kbps で ASR が使える」とは言い切らない。ファームは 1 record = 32000 B ごとに状態をリセットするが、この実験は 8000 B（2 秒）ごとにリセットしている（FW より短い間隔なので、同じか不利な側の条件。実測で確かめてはいない）。
- 再現: `tools/asr_codec_eval.py`（要 `say`・ffmpeg・Whisper・`tools/ima_roundtrip.c` のビルド）、集計は `tools/asr_codec_report.py`。結果の全表は #19。

### 7.5 AAD の評価

比較: A = 連続の低電力 I²S（120 µA）／ B = AAD Analog → 低電力 I²S（§4.5 の状態機械）。

場面（7）: 1 静かな部屋、2 電車・駅、3 カフェ、4 屋外歩行、5 小声、6 遠距離会話（1〜2 m）、7 TV / BGM あり。各場面で A と B を同じ音源に対して比べる。B は A の録音に対して、起床遅れと検出しきい値を適用して机上で再現できる（AAD Analog の検出は「しきい値を超える音」なので、A の PCM から WAKE を作れる）。場面ごとに A の PCM を残し、`aad.c` に 100 ms のエネルギーを流して B を再現する。実機の評価は T5848 が届いてから。

判定基準（案）:

- 語頭の欠落: 発話の最初の 1 語が、A に対して B で欠けた割合（人手、または ASR の挿入・削除の差）。
- ASR の誤り率: B が A に対して、日本語の CER が悪化した量（絶対値と相対値）。許容範囲の数値は #19・#20 と一緒に決める。
- 検出率: 発話のうち B で録音された割合。60 dB SPL 以下は検出されない。
- 誤起床率: 音のない時間に B が起きた回数と合計時間（電力に効く）。
- 電力: 場面ごとの d（低電力 I²S の時間割合）と、その場面のマイク平均電流（§4.7）。

起床遅れ（WAKE から I²S 有効まで）の測り方（実機の測定項目）:

1. Phase 1 の基板で、AAD Analog を設定（LPF 0x1、TH 0x0〜0x4）し、SCK OFF にする（中間ステップを経由）。
2. 既知の音（スピーカからの 1 kHz のバースト、65〜75 dB SPL）を出し、音の立ち上がり（基準マイクまたは信号）から WAKE の立ち上がりまでをオシロで測る。これが検出遅延。
3. WAKE の立ち上がりから、SCK ON、I²S の最初の有効なデータ（感度が最終値の 0.5 dB 以内）までを測る。データシートのウェイクアップは 6 ms。nRF の I2S20 の起動時間を含めた実測値を取る。
4. 語頭の欠落量 = 音の立ち上がりから録音の最初のサンプルまでの時間（検出遅延 + SCK ON〜有効）。子音（無声破裂音など）の語頭が欠けるかを、7 場面の録音で ASR の誤りとして見る。
5. 結果を §3 の語頭欠落の議論と、p の見積りに戻す。

pre-roll（AAD の起床前の音を持つ）は、マイクが寝ている間の音を復元できないので、AAD Analog では作れない。RAM に溜める連続録音を前提にする別の構成（連続の低電力 I²S）が要る。pre-roll の長さ（0 / 0.3 / 0.5 s）ごとの保存量と RAM の増分は #20 で計算する。

## 8. 実装とツール

| 場所 | 内容 | 節 |
|---|---|---|
| `fw/lib/t5838/` | THSEL の 1 線プロトコル（パルス列の生成・復号）、AAD Analog の設定値、クロックとモードの表（T5838 由来。T5848 Rev 1.1 との全照合と実機出力は残る） | §2.2, §4.3, §4.4 |
| `fw/lib/aad/` | BOOT（THSEL 書き込み）→ SLEEP → STARTING → ACTIVE → STOPPING（192 kHz 2 ms）の状態機械、60 s ごとの AAD 再設定、SD 全 0 の privacy 判定。時刻とマイクは注入し、ホストで台本の場面を試験 | §4.5 |
| `fw/lib/ima_adpcm/` | IMA-ADPCM 4 bit のエンコーダ・デコーダ（純 C） | §5.1 |
| `fw/src/audio.c`, `fw/lib/audio_dsp/` | PDM（XIAO の MSM261）または I2S20（T5848）→ 100 ms ブロック → ADPCM → record。I2S は 24 bit/16 kHz を 8 kHz/16 bit mono に間引き、ADPCM 状態は record ごとにリセット。ビルドとホスト試験済み、実機の SCK/LRCK と CPU コストは未測定 | §2.4, §5.1 |
| `tools/asr_codec_eval.py`, `tools/asr_codec_report.py`, `tools/ima_roundtrip.c` | 代理実験の実行と集計 | §7.4 |

## 未確定

| 項目 | issue |
|---|---|
| 縮退側 codec（2 bit か 3 bit か）、下限レート（16 kbps か 24 kbps か）、`codec` 値の割り当て。簡易 2 bit（IMA 型）の実装と、§7.3 の基準での測定 | #19 |
| 胸元装着の実録音（30 分）での ASR 評価。実録音での許容 CER・語頭欠け・欠落の基準。16 kHz 系を同じ bitrate の制約で比べるか | #19 |
| 保存デューティ p の実測、AAD を残すか連続の低電力 I²S にするかの判断、pre-roll の要否 | #20 |
| T5848 実機の測定（WAKE の検出遅延、AAD Analog の実際のしきい値、誤起床率、起床から I²S 有効まで、有効化の応答パルス） | 新規 issue 候補: T5848 AAD の実機測定（Phase 1） |
| THSEL のパルス列を出す手段（タイマ / PWM / ビットバンのどれか）の実装、AAD 中の SCK / WS ピンが LOW に保たれることの確認 | 新規 issue 候補: THSEL パルス出力の実装と SCK / WS ピンの確認 |
| I2S20 の実機確認（SCK/LRCK、32 MDIV 由来の約 15.87 kHz からのずれ、192 kHz の中間ステップ、16 kHz → 8 kHz の CPU コスト）。`aad.c` への中間ステップの追加、`fw/lib/t5838` のクロック値の更新 | #35、新規 issue 候補: I2S20 の実機確認と AAD 入り手順 |
| T5848 Rev 1.1 は取得・主要電流と LP 有効 bit を確認済み。THSEL パルス表・レジスタ・起床手順の既存 library との全照合、D1 / D2 は使う場合の確認 | 新規 issue 候補: T5848 データシートの確認 |
| Standard が縮退モードを使うか（NAND 512 Mbit〜1 Gbit。要求元は Premium の 48 h 保証） | 新規 issue 候補: Standard の縮退モード要否 |
| 音響ポート・vent・風切り・衣服擦れ・ボタンのクリックの試作と評価（Go / No-Go は §3.7）。雨滴・汗の付着後の復帰 | 新規 issue 候補: 音響ポートと vent の試作評価 |
