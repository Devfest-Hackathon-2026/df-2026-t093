# 試作計画（フェーズ・基板・調達・工具）

この文書が持つもの: 試作の順序と使う基板、Phase 0（XIAO）の測定方法、BC15M 基板の位置づけ、SWD プローブ、調達先と候補部品、既製ケースの問い合わせ、優先して実測するものと初号機の Go/No-Go。
各仕様の中身（電源・ストレージ・音声など）は [spec/](../spec/) が持つ。認証と販売の手続きは [compliance.md](./compliance.md) が持つ。

確度の表記:

- **確定**: 動かさない前提。
- **方針（数値未定）**: 向きは決まっているが、数値や部品が決まっていない。
- **仮置き（実測で決める）**: 実測で置き換える前提の値。
- **要検証**: 成立するかを確かめていない。

方針: **評価ボードを買い集めず、最初から本番に近い自作基板で測る。** 開発用の XIAO と、dogfooding の本筋である BC15M 基板の 2 本立てにする。

**優先順位の更新（2026-10-23）:** 11 月中旬のハッカソン向け Premium を Standard 完成待ちにしない。共通コアの検証と、Solar・HSL・非常用ボタン電池・外観を並行する。USB 給電は設けない。一次資料調査、未実装箇所、調達候補、日付つきの実行順は [Premium 試作計画](./premium-hackathon.md) を使う。Phase 番号は基板の分類として残す。

## §1 フェーズ構成

### §1.1 一覧（確定）

| Phase | 基板 | 目的 | 状態 |
|---|---|---|---|
| 0 | XIAO nRF54L15 Sense（無改造） | FW の縦串（録音 → ADPCM → ring → BLE burst → スマホ）を通す。電力は差分電流とイベント波形まで | **進行中** |
| 1 | Fanstel BC15M を載せた **Standard** 基板（自宅リフロー） | 電源・NAND・マイク・RTC の実測。dogfooding 用の Standard | 回路図 rev 0・2 層配線・DRC・Gerber/BOM まで完了（hw/standard/）。基板は未発注 |
| 2 | Fanstel BC15M を載せた **Premium** 基板（自宅リフロー） | Solar・BQ25505・120 F HSL・CR2032 mux・大容量 NAND と筐体の実測 | **11 月中旬向けに先行**。共通コア／電源部を並行。回路・PCB は未実装 |

- 他の仕様書の「Phase 1 基板」は、上の Phase 1（BC15M の Standard 基板）を指す。
- Standard と Premium は Bluetooth の構成を同じにする。差は電源・保存・筐体だけ（[compliance.md §3](./compliance.md)）。共通コア（MCU・マイク・RTC・BLE・FW）は Phase 0〜1 で先に固める。
- **XIAO を使った Phase 1 のキャリア基板は作らない**（確定）。XIAO は開発専用で、製品や配布物には載せない（理由は [compliance.md §6](./compliance.md)）。
- チップ直載せの基板（nRF54L15 を QFN で載せる小型 RF 基板）は今の計画に入れない。認証済みモジュールをそのまま使う（技適・FCC・CE の取得済みが条件。[compliance.md §4](./compliance.md)）。
- 予備のモジュールは u-blox NORA-B206。初版は BC15M だけで進め、NORA-B206 は後で試す（§3.6）。

### §1.2 今は買わないもの（確定）

| 品目 | 買わない理由 |
|---|---|
| nRF54L15 DK | XIAO で同じ SoC の FW を書ける。電流測定ヘッダは INA228 治具で足りる |
| Nordic PPK2 | INA228 の治具（[test/current-measurement-fixture.md](../test/current-measurement-fixture.md)）とシャント + オシロで代替する |
| マイクの評価ボード（T5838 用は EV_T5838-FX2。T5848 用は未確認） | T5838 用は高価。Phase 1 基板にマイクを載せれば足りる |
| BC15M 評価ボード（EV-BC15M。回路図と Gerber は公開） | 基板に載せた BC15M の RSSI を基準と比べる必要が出たときだけ買う（仮。§3.4） |
| NAND ブレイクアウト・SD NAND・SPI NAND モジュール | 生 NAND の bad block・ECC を FW で扱う必要があり、モジュールは中で隠すので試せない（[storage.md §2](../spec/storage.md)）。Phase 0 の ring 開発は内蔵 RRAM で足りる |
| 単独の 1 F キャパ評価系 | 今回は 120 F HSL と実負荷の評価を優先。**BQ25505・HSL・Solar・coin は今から調達対象** |

## §2 Phase 0 — XIAO nRF54L15 Sense（進行中）

### §2.1 構成（確定）

```text
XIAO nRF54L15 Sense
  ├ nRF54L15（BLE、1.5 MB NVM、256 KB RAM）
  ├ PDM mic MSM261DGT006（製品の T5848 とは別の部品で、出力も PDM）
  ├ SAMD11 CMSIS-DAP（USB 経由で書込・デバッグ）
  └ TPS628438 → VSYS 3.3 V
```

- **改造しない。** 3V3 ピンは `3V3_OUT` で、nRF の VDD は基板上の TPS628438 が作る 3.3 V。Phase 0 は 1.8 V 部品を外付けしない。
- 本番の 1.8 V 駆動・T5848（I²S）・NAND・RTC は Phase 1 で測る。XIAO の値は最終製品の絶対値に使わない。

### §2.2 実装する範囲

```text
PDM 録音 → IMA-ADPCM（32 kbps）→ 内蔵 NVM の ring → BLE burst sync → スマホ → ACK → reclaim
```

- 内蔵 1.5 MB からアプリを除くと約 1 MB。ring は専用の `ring_partition`（784 KB、32 kbps で約 3 分）。パーティション表は [firmware.md §2.1](../spec/firmware.md)。ring・chunk 形式・ACK・欠落記録・BLE プロトコルの開発には足りる。
- **内蔵 NVM（RRAM）は消去不要だが、NAND は消去が必要。** storage backend は最初から消去・書込・読出の形で切る。実装は Zephyr の `flash_area` API をそのまま使う（消去は `flash_area_flatten`: NAND では消去、RRAM では消去値で埋める）。生 SPI NAND は Zephyr 標準の flash API に載らないので、NAND ドライバは別に作る（[storage.md §6](../spec/storage.md)）。
- storage driver と power state machine だけを backend として差し替える構造を、ここで作る（[storage.md §5](../spec/storage.md)）。
- 測定用ビルドと BLE・ストレージの実装状況の issue: #4。

### §2.3 測定用ビルド（確定）

Phase 0 の電流測定は、USB を抜き、ヘッダの **5 V ピン（VBUS）** から給電して行う。

**注意: 5 V ピンに電池をつないだまま USB を挿してはならない。** USB の 5 V がショットキ D4 を通って VBUS へ入り、電池を充電する。電池を外してから USB を挿す。

```text
単 3 ×3（新品約 4.8 V）
  (+) ───────────────────── XIAO 5V
  (−) ── Rshunt ── XIAO GND
            ↑
     オシロ CH1: プローブ先端を XIAO GND 側、グラウンドクリップを電池(−)側
                （ローサイド。電池がフロートなので単端プローブで測れる。表示は反転）
     オシロ CH2: FW のイベントマーカー GPIO（§2.4）
```

回路図で確認した点:

- 5 V ピン = VBUS = TPS628438 の入力。充電 IC（SGM40567）と SAMD11 用 LDO は USB 端子側（TYPE-C_5V）にあり、5 V ピン給電では動かない。
- TPS628438 の VIN 上限は 5.5 V。新品の単 3 ×3（約 4.8 V）は範囲内。
- **Rshunt は、測定する state で入力電圧が十分残る範囲で選ぶ**（例: 100 Ω・10 mA で 1 V 降下しても入力 3.8 V）。想定外に mA 級へ起床すると降下で DUT の動作が変わるので、大きい Rshunt では起床させない。測れるのは入力側電流（降圧効率込み）で、相対比較には十分。
- **入力容量でシャント波形はなまる。** VBUS の C9（4.7 µF）と Rshunt が RC フィルタになり、時定数は 100 Ω で約 0.5 ms、1 kΩ で約 4.7 ms。BLE TX などの瞬時ピークは読めない。エネルギー（積分値）は保存されるので、C9 の再充電が終わるまで含めて長めの窓で積分する。
- **ボード定義の既定で常時 ON になる負荷を切る。** Zephyr の `xiao_nrf54l15` は `pdm_imu_pwr`（マイク・IMU のロードスイッチ）と `vbat_pwr`（電池分圧）を起動時に ON にする。5 V ピン給電では VBAT ≒ VBUS になり、分圧（10 k + 10 k）だけで約 240 µA 流れる。`fw/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay` で両方の `regulator-boot-on` を外してある。RF スイッチ（`rfsw-ctl` / `rfsw-pwr`）は BLE に必要なので残し、床電流の一部として扱う。
- USB を抜いている間はデバッガと RTT ログが使えない。測定用ビルドを書いてから USB を抜く。ログ・コンソール・UART・LED（P2.00）は切る。

測定用ビルド:

- `-DEXTRA_CONF_FILE=measure.conf` で作る（`fw/measure.conf`）。`CONFIG_PENDANT_MEASURE=y` でシナリオのループ（§2.4）を走らせ、`CONFIG_LOG`・`CONFIG_CONSOLE`・`CONFIG_UART_CONSOLE`・`CONFIG_SERIAL`・BLE sync サービス（`CONFIG_PENDANT_BLE`）・自動録音（`CONFIG_PENDANT_AUTO_RECORD`）を切る。BLE スタック自体（`CONFIG_BT`）は残り、区間 5 の advertising に使う。chunk 長は `CONFIG_PENDANT_CHUNK_BYTES=32000`。
- `fw/scenario-log.conf` は measure.conf にログを足したもの。UART でシナリオの順序だけを確かめる用で、電流測定には使わない。

シャントの使い分け:

| Rshunt | 用途 | 目安 |
|---:|---|---|
| 10 Ω | BLE 送受信・RRAM 書込など mA 級の波形 | 5 mA → 50 mV |
| 100 Ω | PDM 録音・ADPCM など数百 µA〜1 mA 級 | 500 µA → 50 mV |
| 1 kΩ + テスター | sleep など静的な µA 級 | 起床時に電圧が落ちるので、測定中は起床させないか短絡スイッチを併用 |

イベントエネルギーは `E = V_in × ∫(V_shunt / R) dt`。オシロの積分機能または CSV 出力で計算する。積分窓は CH2 のマーカー前後に、RC 時定数の数倍以上の余白を取る。

Phase 1 以降の電流測定は INA228 の治具に固定する（確定。[test/current-measurement-fixture.md](../test/current-measurement-fixture.md)）。Phase 0 の XIAO は上のシャント + オシロで測る。

### §2.4 イベントマーカーと測定シナリオ（確定）

オシロの CH2 に、FW が今何をしているかを GPIO で出す（`fw/src/marker.c`、`fw/src/marker.h`）。

| マーカー | 意味 | XIAO のピン | nRF54L15 |
|---|---|---|---|
| `M_REC` | 録音中 | ヘッダ D6 | P2.08 |
| `M_STORE` | ring への書込中 | ヘッダ D7 | P2.07 |
| `M_BLE` | BLE の区間、およびシナリオ区間の番号 | 裏面パッド D15 | P2.06 |

ピン割当の全体は [board.md](../spec/board.md) が持つ。

測定シナリオ（`fw/src/scenario.c`。`PENDANT_MEASURE` で有効）: 次の 5 区間を 20 s ずつ繰り返す。各区間の頭で、区間番号 n 個のパルス（2 ms High / 2 ms Low）を `M_BLE` に出して、オシロから区間を見つけられるようにする。その間は他のすべてを止める。

| 区間 | 状態 | 見るもの |
|---:|---|---|
| 1 | idle（System ON sleep、マイク・無線 OFF） | 床電流 |
| 2 | PDM + DMA のみ（`AUDIO_PDM_ONLY`） | 録音の電流 |
| 3 | PDM + ADPCM、書込なし（`AUDIO_ENCODE`） | 符号化の電流 |
| 4 | PDM + ADPCM + ring 書込（`AUDIO_FULL`。通常の録音） | 書込の電流 |
| 5 | BLE advertising（接続可能、未接続） | advertising の電流 |

- BLE 同期 1 回（接続・転送・切断）と、転送量あたりのエネルギーは、区間とは別にスマホ側から同期を開始して測る。
- 測るのは差分電流とイベント波形。

測らない:

- 最終製品の絶対値（µW）。XIAO は TPS628438・MSM261・3.3 V 動作で本番と違う。
- T5848 AAD の電流、NAND の書込エネルギー、CR2477 の寿命 → Phase 1。

### §2.5 Phase 0 で用意するもの

- XIAO nRF54L15 Sense 1 枚（約 ¥3,000）。無改造のまま使う。
- 抵抗 10 Ω / 100 Ω / 1 kΩ、単 3 電池ホルダと配線。
- オシロ（手持ちまたは手配）。
- 書込は pyOCD（[firmware.md §2.3](../spec/firmware.md)）。XIAO の SAMD11（CMSIS-DAP）をそのまま使える。

## §3 Phase 1 — Fanstel BC15M の Standard 基板

### §3.1 位置づけ

- dogfooding の本筋の基板。Fanstel BC15M（決定 X3）を自宅でリフロー実装する（確定）。
- 電流測定用の 0 Ω ジャンパとテストポイントをこの基板に載せる（確定）。測定は INA228 の治具（§7）。
- モジュールの VDD は 1 本で、GPIO のレベルは VDD。データシートの表紙は 1.7〜3.6 V、ピン表は 1.8〜3.6 V なので、**1.8 V の rail で直接動かす。** レベル変換は要らない。rail は 1.773 V まで下がるので、下限は試作で確かめる（§3.4）。外付け RTC を 3V3 側に置く例外も要らない（レベル変換と 3V3 側は XIAO の開発基板だけの話。[board.md](../spec/board.md)）。
- 32 MHz と 32.768 kHz の水晶、DC/DC のインダクタはモジュールに内蔵で、**外付けは無い**（確定）。DC/DC の有無（インダクタあり・なし）の比較はできない。DC/DC は FW で有効にする（[firmware.md §5.2](../spec/firmware.md)）。
- 書込み・デバッグは SWD のパッドから行う（§3.5）。利用者向けの更新は MCUboot + BLE DFU（[firmware.md §2.3](../spec/firmware.md)）。
- 認証: 技適 201-250527、FCC X8WBC15。詳細は [compliance.md §4](./compliance.md)。
- アンテナ・整合・keepout は [rf.md](../spec/rf.md)、筐体との収まりは [mechanical.md](../spec/mechanical.md)。

### §3.2 構成（方針）

```text
CR2477 ホルダ ／ 外部電源入力（ジャンパで選択）
  │
  R_BAT（電池側シャント・0 Ω）
  │
  reservoir capacitor（footprint 100〜470 µF、第一候補 220 µF）
  │
TPS62840 → 1.8 V
  ├ R_MAIN  ─ Fanstel BC15M（VDD）
  ├ R_MIC   ─ T5848（SCK / WS / SD / WAKE / THSEL を MCU へ配線）
  ├ R_FLASH ─ 1.8 V SPI NAND（WSON-8 8×6 を共通の足跡）
  └ R_RTC   ─ RV-3032-C7（footprint のみ。載せるかは実測で決める）
```

基板に入れるもの:

| 項目 | 扱い | 詳細 |
|---|---|---|
| rail 別 0 Ω ジャンパとテストポイント | 確定 | 電池側の総電流（R_BAT）と各 rail のブロック別電流を測る。[board.md](../spec/board.md) |
| 電池電圧の測定経路 | 確定（Phase 1 基板に入れる） | [power.md §4.6](../spec/power.md) |
| 外付け RTC | footprint のみ（確定） | [firmware.md §5.3](../spec/firmware.md) |
| 逆流防止 | footprint のみ（Schottky / LM66100 / 0 Ω のどれか。載せるかは運用次第） | [power.md §3.5](../spec/power.md) |
| 終端表示（赤⇄黒） | Standard にも載せる。CR2477 用の last-gasp 回路を別に設計する | [terminal-indicator.md](../spec/terminal-indicator.md) |
| reservoir | 100〜470 µF の footprint | [power.md §4.4](../spec/power.md) |
| SWD | SWDIO / SWCLK / RESET / VTREF / GND を 2.54 mm ピッチのヘッダまたはパッドで出す（面積は無視してよい） | §3.5 |
| NAND の電源ゲート（実装パッドのみ、DNP 可） | 方針 | [storage.md §3](../spec/storage.md)、[power.md §5](../spec/power.md) |

- ピン割当は確定（決定 H2。BC15M のパッド名は [hw/standard/DESIGN.md §4](../../hw/standard/DESIGN.md)）。XIAO 基板のピン割当は [board.md](../spec/board.md)。

### §3.3 実装方法（確定）

- **自宅のリフロー炉で実装する。** JLCPCB の PCBA は使わない。
- **JLCPCB には裸基板とステンシルだけを発注する。**
- 部品は DigiKey・Mouser・LCSC などで買う。部品の選定は JLCPCB の在庫に縛られない。BC15M は DigiKey JP で買える（取扱い品）。調達の候補は §6。
- リフローで載せるもの: BC15M モジュール（LGA 36 パッド）、T5848（LGA、8 pad）、TPS62840（VSON-8、1.5×2 mm）、NAND（WSON-8 8×6 または LGA-8）、RV-3032（載せる場合）。ピンソケット・CR2477 ホルダ・0 Ω ジャンパは手はんだでもよい。
- BC15M のリフロー: MSL 3。プロファイルはデータシート p.23（LGA 用）、リフローは 2 回まで。B1〜E5 の 20 パッドはモジュールの下でパッドの間は 0.5 mm、はんだは目視できないので、組立後に全信号を動かす電気試験で確かめる（[hw/standard/DESIGN.md §5](../../hw/standard/DESIGN.md)）。
- 層数は 2 層（確定。BGA を避け、配線を 2 層に収めた。[storage.md §2.4](../spec/storage.md)）。

### §3.4 この基板で測るもの

測定の項目と判定は、各仕様の検証項目が持つ。ここでは順序だけを置く。

1. 起動・書込・BLE・録音・NAND の疎通（NAND の実機項目は [storage.md の未確定 #3](../spec/storage.md)、issue #27）。
2. rail ごとの電力（MAIN only / RTC only / Mic AAD / Mic LP I²S / NAND off・DPD・書込 / BLE sync）。T5848 AAD の実際の電流、NAND 書込のエネルギー、CR2477 で何か月持つか（[power.md §10.3](../spec/power.md)）。
3. Standard の電圧段階を決める測定: CR2477 の放電曲線、パルス降下、TPS62840 の dropout、BOR、電池交換直後の起動（[power.md §10.2](../spec/power.md)、issue #18）。
4. 送信出力 +4 dBm（アンテナ利得込み）以下での届く距離。BC15M の設定は +2.5 dBm 以下（胸元での RSSI と転送の安定性。基準が要るときは EV-BC15M と比べる。[rf.md](../spec/rf.md)）。
5. AAD と音声（[audio.md](../spec/audio.md)）。
6. VDD の下限: rail を 1.75 V 付近まで下げて動作（起動・BLE・NAND・録音）を確かめる（データシートは表紙 1.7 V、ピン表 1.8 V。[power.md §2.2](../spec/power.md)）。
7. 組立後の電気試験: 全信号を動かして、モジュールの下の 20 パッドのはんだを確かめる。

### §3.5 SWD プローブ（要検証）

- **XIAO（SAMD11 の CMSIS-DAP）をプローブとして流用する。** 書込みと消去は pyOCD（`-t nrf54l`、v0.38 以上）。外部のターゲットに使えるかは未検証。
- Phase 0 の間に検証する。確かめること:
  1. XIAO 上の nRF54L15 と SWD の線を共有しないようにする方法（裏面パッド TP1 = SWCLK、TP2 = SWDIO、TP3 = GND から取り出す場合を含む）。
  2. 1.8 V のターゲットに合わせる方法。XIAO は SAMD11 と nRF の間にレベル変換 IC（UM3301、1.2〜5.5 V）を持つ。ターゲット側の電圧をどう合わせるか。
  3. 外部ターゲットへの書込・消去・デバッグ（CTRL-AP による消去を含む）が通ること。
- 成功を BC15M 基板の発注の条件にする（方針）。
- 予備案:

| 案 | 内容 | 要確認 |
|---|---|---|
| A | Raspberry Pi Pico（debugprobe、CMSIS-DAP）+ pyOCD。基板側に SWD 用レベル変換 IC（B 側をプローブの 3.3 V、A 側を 1.8 V の rail）を載せる | 変換 IC の双方向 SWDIO での動作速度 |
| B | ST-LINK V3MINIE。ターゲット 1.65〜3.6 V 対応で変換 IC が要らない | ST 以外（nRF54L）への書込と、CTRL-AP による消去ができるか |

- 3.3 V 固定 I/O のプローブ（Raspberry Pi Pico、Raspberry Pi Debug Probe）は 1.8 V のターゲットに直結しない。
- OpenOCD + Pico debugprobe は、nRF54L に接続できても flash 書込で失敗する報告がある。pyOCD の手順を基準にする。

### §3.6 NORA-B206（後で試す）

- 初版は BC15M だけ。u-blox NORA-B206（技適 003-250204）は BC15M の次に試す予備のモジュール。
- 足跡の互換は未確認。試すときに別基板にするか、共通の足跡にするかを決める（[board.md](../spec/board.md)）。

## §4 Phase 2 — Premium 基板

- 11 月中旬の試作は Standard 完成を待たず先行する。BC15M と共通の音声・記録・BLE を使い、電源部を小さい別基板で評価してから本番筐体へ組み込む構成も認める。
- Premium 固有: 実 Solar、BQ25505＋back-to-back PMOS 2 ペア、一次電池逆流保護、120 F HSL、CR2032、NAND。仕様は [power.md §3](../spec/power.md)、[storage.md](../spec/storage.md)。**11 月デモ機は 512 Mbit F35UQA512M-WWR を設計採用**し、2 KiB page・power gate・保存／電源を実機で確認する。製品の 48 h 条件とは分ける。
- 共通コアの再利用は設計方針。**現 `store.c` は内蔵 RRAM、外付け NAND driver と実電圧・source policy は未統合**。AAD、phone 実機試験も含めた不足を [Premium 試作計画 §2・§7](./premium-hackathon.md) で管理する。製品の共通 NAND driver を SKU ごとに重複実装しない。

## §5 dogfooding（確定）

- **Standard と Premium の両方を作り、知人に好きな方を選んで使ってもらう。** 今回のハッカソン試作は上記の通り Premium を先行する。配布する製品の条件は下記と各仕様で別に確認する。
- 配布する個体は MCUboot を出荷前に書き、以降は BLE DFU で更新する（[firmware.md §2.3](../spec/firmware.md)）。ペアリングは LESC + OOB。**開発ビルド（GATT 無認証）は配布しない**（[security.md](../spec/security.md)）。
- 知人への無償配布は、Bluetooth SIG への問い合わせの範囲に入れない。配布の扱いは [compliance.md](./compliance.md) の書面確認の結果に従う（要書面確認）。

## §6 調達

### §6.1 調達先（確定）

| 対象 | 調達先 |
|---|---|
| 裸基板・ステンシル | JLCPCB（発注先は JLCPCB で確定） |
| Fanstel BC15M | DigiKey JP（取扱い品。マーケットプレイスではない） |
| 電子部品 | DigiKey、Mouser、LCSC |

- JLCPCB の PCBA・Extended 部品手数料は使わない。部品の調達と裸基板・ステンシルの見積は 2026-10-18 に取り、`hw/standard/fab/README.md` に置いた。発注は未着手で、issue はまだ無い（候補: BC15M 基板の発注）。
- 在庫と価格は動く。**発注直前に取り直す。** 日付つきの価格表と在庫表は `hw/standard/fab/README.md` に残し、発注 issue を作ったらそこにも記録する。

### §6.2 Phase 1 の候補部品（方針）

在庫と価格は書かない（§6.1）。「発注直前に確認」とあるのは、在庫が日々変わる品。

| 部品 | 型番 | パッケージ | 備考 |
|---|---|---|---|
| モジュール | Fanstel BC15M | LGA 36 パッド（6.0 × 9.6 × 1.9 mm） | DigiKey JP。技適 201-250527、FCC X8WBC15。ライブラリ（Eagle / Altium / PADS）と EV-BC15M の回路図・Gerber は https://www.fanstel.com/nrf54-70-document |
| マイク | TDK MMICT5848-00-012（T5848、I²S） | LGA 8 pad（3.5 × 2.65 × 0.98 mm） | DigiKey JP に在庫あり（15,115 個、2026-10-18）。Mouser は価格・在庫未確認。LCSC 番号は未確認（T5838 の C7230892 は別の部品なので使わない）。T5838（MMICT5838-00-012、PDM、7 pad）はピン非互換の代替（決定 H7）。`hw/standard/fab/README.md` |
| RTC（footprint のみ。載せる場合） | RV-3032-C7-32.768KHZ-2.5PPM-TA-QA | SMD3215-8P | LCSC C5366550。±2.5 ppm の品。在庫は発注時に再確認する（RTC を載せるかは #28） |
| 降圧 | TPS62840DLCR | VSON-8（1.5×2 mm） | LCSC C2071859 |
| NAND（Standard 候補） | 候補表は [storage.md §2.3](../spec/storage.md) | WSON-8 8×6 | 容量は 512 Mbit か 1 Gbit（未決） |

- RV-3032 の DPD 電流表記は型番ごとに説明が異なる品がある。待機電流の根拠に使う前に、型番ごとのデータシートで確認する（NAND は [storage.md §2.5](../spec/storage.md)）。

### §6.3 試作用 NAND（原理確認用、方針）

以前の原理確認案は下表に残す。ただし **11 月中旬の Premium は 1.8 V の実採用品 1 品へ集中し、3.3 V 品の追加評価・長納期待ちは必須手順にしない**。今回の型番固定と掲載在庫は [Premium 試作計画 §6.3](./premium-hackathon.md#63-調達の現状と優先順位) が持つ。48 h の保存要求と試作の実証範囲は分ける。

| 用途 | 型番 | 備考 |
|---|---|---|
| 旧原理確認案（今回の必須手順から外す） | SkyHigh S35ML04G300WHI000（3.3 V・4 Gbit・LGA-8） | 長納期のため今回の試作は待たない |
| 旧原理確認案（予備、任意） | SkyHigh S35ML02G300WHI000（3.3 V・2 Gbit・LGA-8） | 共通 driver の追加作業を伴う場合は今回増やさない |
| 1.8 V 側の確認（Standard 候補） | Lexar F35UQA512M-WWR（1.8 V・512 Mbit・WSON-8） | DigiKey |
| 今回の Premium デモ設計 | Lexar F35UQA512M-WWR（1.8 V・512 Mbit・2 KiB page・WSON-8） | 短時間デモのために設計採用。購入・実装・電力／保存の実測は未実施 |
| 旧 Premium 先行評価候補 | Alliance AS5F18G04SNDB-10LIN（1.8 V・8 Gbit B・4 KB page・LGA-8） | 大容量・高価格。デモでは F35UQA512M に置換 |
| Premium 試作の条件付き代替 | Alliance AS5F14G04SND-10LIN（1.8 V・4 Gbit・**2 KB page**・LGA-8） | 4 KB 条件からの例外。電力・geometry を再計算し、48 h を自動的に主張しない |

- 3.3 V 品は XIAO の 3.3 V 系に配線して使う。電力の実測値は最終品に流用しない。1.8 V 品は BC15M 基板（1.8 V rail）に載せる。
- 発注前に採用品のデータシートで ECC・page・待機と package の寸法を確認する。11 月デモの F35UQA512M は DPD／4 KiB page に関する製品条件の例外であり、power gate と 2 KiB 対応を検証する（[storage.md §2.4](../spec/storage.md)）。
- 価格・在庫・リードの日付つきの値は `hw/standard/fab/README.md`（発注 issue は未作成）。

### §6.4 既製ビーコンケースの問い合わせ

自社ケースを起こす前に、既製のビーコンケース（Minew / MOKO）を評価する。問い合わせの送付は担当者（issue #16）。

外形の目標: **Ø40〜42 × 15 mm 以内** を比較の基準にする（決めるのは担当者）。AirTag 互換の外形（Ø31.9 mm 外周・中央 10〜12 mm）を採るかは未決で、BC15M の寸法（6.0 × 9.6 × 1.9 mm）を入れた stack-up（[mechanical.md §7.4a](../spec/mechanical.md)）で決める。寸法と stack-up は [mechanical.md](../spec/mechanical.md)。

問い合わせ文面（Minew / MOKO 共通、英語。送付前の下書き）:

> We are developing a small BLE logger worn on the chest. Before tooling our own enclosure, we would like to evaluate your standard beacon housing. Could you tell us (1) whether the housing can be sold without the PCB (housing only), and the price for 20 / 100 pieces; (2) the internal PCB mounting points and keep-out dimensions (a 2D drawing or STEP file); (3) the gasket structure, ingress-protection rating and how it is tested; (4) whether the button mechanism and the battery cover can be reused with a custom PCB; (5) whether a small acoustic port with an ePTFE membrane can be added, and the MOQ and lead time for logo / colour changes; (6) the dimensions, weight and battery type (CR2477) of the housing.

回答の記録:

| 宛先 | 送付日 | 回答日 | 回答者 | housing only の可否・価格 | 内部寸法・STEP | 防水（等級と試験法） | 音孔の追加 | MOQ・納期 |
|---|---|---|---|---|---|---|---|---|
| Minew | | | | | | | | |
| MOKO | | | | | | | | |

- 既製ケースの防水等級は**ケース単体**の値。PCB・ボタン・音孔を足した製品の等級ではない（[mechanical.md](../spec/mechanical.md)）。
- 販売資料にメーカー名や AirTag などの語を出す場合の扱いは [compliance.md §8](./compliance.md)。

## §7 工具（確定）

| 工具 | 用途 |
|---|---|
| リフロー炉 | 自宅での表面実装（用意済み） |
| オシロ | Phase 0 のシャント測定とイベントマーカー（手持ちまたは手配） |
| INA228 の電流測定治具 | Phase 1 以降の電流測定。仕様は [test/current-measurement-fixture.md](../test/current-measurement-fixture.md) |
| XIAO（SAMD11 の CMSIS-DAP）+ pyOCD | SWD プローブ（§3.5。要検証） |

- PPK2、nRF54L15 DK は使わない（§1.2）。

## §8 優先して実測するもの

基板は §1.1 の Phase 1（Standard）と Phase 2（Premium）。各項目の判定は各仕様の検証項目が持つ。

### A. 電力（各 state の平均電力）

RTC only / Mic AAD / Mic LP I²S / NAND DPD または true off / NAND の書込（Premium の CAP は batch write、CR2032 運転は page 分散）/ BLE sync / CAP mode / CR2032 survival mode。完成基板の実測平均電力で採否を決める。→ [power.md §10.3](../spec/power.md)

### B. 電源の切替（Premium）

- [power.md §3.3](../spec/power.md) の製品向け評価候補は公称約 2.606／2.802 V。11 月デモの回路ドラフトは別設定で約 2.785／2.981 V。各設定で cap → coin／Solar 回復時の coin → cap を確認する。
- 一次電池模擬源で逆電流の定常値・peak・時間・電荷を確認してから実 coin へ進む。
- 1.8 V rail の brownout の有無。
- chatter が無いこと。

→ [power.md §10.1](../spec/power.md)

### C. Standard の電池と電圧

CR2477 の放電曲線とパルス降下、TPS62840 の dropout、BOR、電池交換直後の起動、reservoir の容量。→ [power.md §10.2](../spec/power.md)

### D. 音声

小型基板・NAND 容量の確定前に評価する。PC で実 codec の ASR を比較し、T5848 実機で AAD の頭欠けと誤反応を測る。30 分は予備選別とし、日単位の保存デューティは別に確認する。

- 胸元での日本語音声の ASR
- AAD の先頭音節の欠け
- 風
- 衣擦れ
- ボタンのクリック
- 音響メンブレンの差

→ [audio.md](../spec/audio.md)

### E. RF

- PCB アンテナを身に付けた状態での RSSI と転送速度。
- 実用上の最小 TX 出力（上限は +4 dBm、アンテナ利得込み）。
- ストラップのアンテナ試作との比較。

→ [rf.md](../spec/rf.md)

### F. 機構・環境

- Solar-button のサイクル
- バネ接点のサイクル
- 汗への暴露
- マイク通気口の濡れ・乾燥からの復帰
- ガスケットのシール

→ [mechanical.md](../spec/mechanical.md)

### G. FW の復旧

- SWD pogo（またはパッド）からの復旧
- BLE OTA の中断
- MCUboot の test / revert
- watchdog reset
- NAND 書込み中の brownout
- metadata commit 中のランダムな電源断

→ [firmware.md §3.5・§4.6](../spec/firmware.md)、[storage.md §8.5](../spec/storage.md)

## §9 初号機の Go/No-Go

下の最低条件がすべて成立すること。基本構成は試作の基準であり、成立は未検証。

| # | 条件 | 対象 | 詳細 |
|---|---|---|---|
| 1 | スクリーニングは rail 側の平均電力 0.8 mW 未満（目標 0.3〜0.5 mW）。最終判定は、容量下振れ・劣化・温度・しきい値誤差と実測の負荷・効率を含めて、CR2032 を使わない無日照 72 時間を守れること。想定する屋外活動で消費分を回収できることは別に確認する | Premium | [power.md §7.4](../spec/power.md) |
| 2 | cap と CR2032 の切替で reset しない | Premium | [power.md §10.1](../spec/power.md) |
| 3 | CR2032 へ逆充電しない | Premium | [power.md §3.5](../spec/power.md) |
| 4 | AAD または LP I²S で lifeOS 用の ASR が成立する | 共通 | [audio.md](../spec/audio.md) |
| 5 | 胸元で BLE 同期が安定する | 共通 | [rf.md](../spec/rf.md) |
| 6 | 汗・飛沫のあとで mic・ボタン・RF が復帰する。Premium は Solar も | 共通 | [mechanical.md](../spec/mechanical.md) |
| 7 | physical flag（終端で黒）が電源 0 V のあとも保持される | 共通（Standard にも載せる） | [terminal-indicator.md](../spec/terminal-indicator.md) |
| 8 | SWD から確実に復旧できる | 共通 | §3.5、[firmware.md §4.3](../spec/firmware.md) |
| 9 | OTA の中断で brick しない | 共通 | [firmware.md §3.5](../spec/firmware.md) |
| 10 | ランダムな電源断で NAND の既存ログを失わない | 共通 | [storage.md §8.5](../spec/storage.md) |
| 11 | Standard の電池寿命の目標（約 1 年）が成り立つ条件（ビットレート・保存デューティ p）が出る | Standard | [power.md §7.6](../spec/power.md)、#17 |

- 小型 PCB を確定する前に、音声・容量・rail・配置・筐体 stack-up を確認し、測定と復旧に必要なパッドを残す。option footprint の削除は検証結果に基づいて判断する。

## §10 リポジトリ内の置き場所

- `fw/`: Zephyr / nRF Connect SDK のアプリ（sysbuild で MCUboot + アプリ）。ビルドと書込は [firmware.md §2.2](../spec/firmware.md)。測定用ビルドは `fw/measure.conf`、`fw/scenario-log.conf`、`fw/src/scenario.c`、`fw/src/marker.c`（§2.3・§2.4）。
- `fw/lib/`: Zephyr 非依存の純 C ライブラリ（`ima_adpcm/`、`chunk/`、`ring/`、`powerstate/` など）。各ディレクトリの `test_*.c` の先頭にホストでの実行コマンドがある（[firmware.md §6](../spec/firmware.md)）。
- `tools/`: スマホ側受信ツールや計算スクリプト（[firmware.md §6](../spec/firmware.md)）。
- Standard 基板の KiCad プロジェクトは `hw/standard/`（設計根拠は [hw/standard/DESIGN.md](../../hw/standard/DESIGN.md)）。

## 未確定

| 項目 | 状態 | issue |
|---|---|---|
| 部品の調達と裸基板・ステンシルの発注 | 5 枚分の見積のみ（2026-10-18、hw/standard/fab/README.md）。10 枚分は未取得で、#31 は未クローズ（Raytac 前提のまま）。発注はこれから | 新規 issue 候補: BC15M 基板の発注。先に #31 を更新して閉じる |
| SWD プローブとして XIAO を流用できるか（§3.5） | 未検証 | 新規 issue 候補: XIAO を SWD プローブとして使う検証 |
| BC15M の VDD の下限（1.7 V か 1.8 V か）と、アンテナの置き方・隠れパッドの確認（§3.4） | 試作で確認 | [hw/standard/DESIGN.md §6](../../hw/standard/DESIGN.md) |
| NAND の原理確認の場所（3.3 V 品を XIAO に配線するか、1.8 V 品を BC15M 基板で直接試すか）と、LGA-8 を WSON-8 8×6 の足跡に載せられるか | 未決 | #27 |
| NAND の容量（Standard 512 Mbit か 1 Gbit） | 未決 | [storage.md](../spec/storage.md) の未確定 |
| 既製ケースの問い合わせ（送付と回答の記録）、AirTag 互換外形を採るか | 未送付 | #16 |
| NORA-B206 を試すときの基板の作り方 | 後で決める | 新規 issue 候補: NORA-B206 の試作 |
| Premium の harvester と電源パス | 今回は BQ25505＋TPS62840。source 信号変換、一次電池逆流保護、しきい値候補・実波形は要検証 | #25、[Premium 試作計画](./premium-hackathon.md) |
| 知人への無償配布の法的な扱い | 問い合わせに入れていない | #15、[compliance.md](./compliance.md)（要書面確認） |
