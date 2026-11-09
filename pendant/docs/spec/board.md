# 基板仕様: ピン割当・テストポイント・測定リンク・配置と ESD の規則・開発基板のレベル変換

対象: 基板の電気的な作り込み。XIAO 開発基板のピン割当（確認済み）、BC15M 基板のピン割当（確定）、SWD・テストポイント・電流測定リンク、基板に入れる回路の要件（privacy latch、電池電圧の測定経路、reservoir、逆流防止の footprint）、配置・ESD・EMI の規則、XIAO 開発基板だけのレベル変換。

他の仕様との境界:

- 電源の方式・しきい値・状態機械は [power.md](./power.md)。電池電圧の測定の方式は [power.md §4.6](./power.md)、NAND と Mic の load switch は [power.md §5](./power.md)、rail の電圧は [power.md §2](./power.md)。
- アンテナ・TX 電力・RF の試験は [rf.md](./rf.md)。
- マイクと AAD は [audio.md](./audio.md)、NAND の配線の方針は [storage.md §3](./storage.md)、RTC は [firmware.md §5](./firmware.md)、SWD の保護（APPROTECT）は [security.md §8](./security.md)、終端表示の回路は [terminal-indicator.md](./terminal-indicator.md)。
- 筐体の stack-up・防水・音響ポート・privacy latch の機構は [mechanical.md](./mechanical.md)。
- 試作の順序・使う基板・調達は [plan/prototype.md](../plan/prototype.md)。電流測定の治具は [docs/test/current-measurement-fixture.md](../test/current-measurement-fixture.md)。

確度の表記:

- **確定**: 設計の前提として動かさない。変えるときは設計変更として扱う。
- **方針**: 向きは決まっているが、数値や部品が決まっていない。
- **仮置き（実測で決める）**: 実測で置き換える前提の値。
- **確認済み**: データシート・回路図・ボードファイルで照合した事実。
- **未確定**: 決まっていない。

## §1 基板の種類と前提

| 基板 | 位置づけ | 電圧 | 確度 |
|---|---|---|---|
| Fanstel BC15M 基板（自宅でリフロー実装） | **dogfooding の本筋。** 11 月中旬の Premium は共通コアと並行して先行する | 1.8 V rail／GPIO。モジュール下限の表紙 1.7 V／ピン表 1.8 V は試作で確認。NAND・mic は同一 rail、**Premium の BQ25505 source 出力はレベル処理が必要** | 確定（VDD の下限は試作で確認） |
| XIAO nRF54L15（Sense） | **開発専用**（FW の開発と Phase 0 の測定）。製品には載せない | 3.3 V 系。電池から給電すると VDD は電池に追従する（§2.4） | 確定 |
| XIAO の Phase 1 キャリア | **作らない。** 電流測定用の 0 Ω と TP は BC15M 基板に載せる | — | 確定 |

BC15M モジュールの条件（確定。Fanstel データシート Ver 1.05）:

- 32 MHz と 32.768 kHz（20 ppm）の水晶、DC/DC のインダクタ、VDD のコンデンサは**モジュールに内蔵**。外付けの水晶とインダクタは無い。DECD・DCC・XL1・XL2 はパッドに出ない（P1.00 / P1.01 は内蔵の 32.768 kHz 水晶が使う）。
- 36 パッドの LGA（6 × 6、ピッチ 0.9 mm、Ø0.4）。外周 16 個は端まで出るが、B1〜E5 の 20 個はモジュールの下で、はんだは目視できない。組立後の電気試験で確かめる（[hw/standard/DESIGN.md §5](../../hw/standard/DESIGN.md)）。GND は E1・F1 の 2 つだけ。
- 書込みは SWD のパッドから行う（§4）。
- アンテナはモジュールの PCB アンテナ（[rf.md §3](./rf.md)）。

## §2 XIAO nRF54L15 開発のピン割当（確認済み）

nRF54L15 Product Specification v1.0（4503_018）と、Seeed の回路図（`nRF54L15_Sense_Schematic.pdf`）で確認した。XIAO のボードファイル（NCS v3.4.1、`zephyr/boards/seeed/xiao_nrf54l15/seeed_xiao_connector.dtsi`）の D0〜D15 の対応とも一致する。XIAO が載せているのは **CSP47（CAAA、WLCSP）**。

出典:
- Product Specification v1.0: https://files.seeedstudio.com/wiki/XIAO_nRF54L15/Getting_Start/Nordic_nRF54L15_Datasheet_v1.0.pdf（§8.8.3 Port capabilities、§10.1.1 Dedicated pins、§10.1.2 Clock pins、§10.1.6 CSP47 (CAAA) pin assignments、表 40・76・77・81）
- Seeed 回路図: https://files.seeedstudio.com/wiki/XIAO_nRF54L15/Getting_Start/nRF54L15_Sense_Schematic.pdf

### §2.1 データシートの規則（CSP47）

| 規則 | 内容 | 出典 |
|---|---|---|
| ポートとドメイン | **P0 = 低電力ドメイン、P1 = 周辺ドメイン**（どちらも起床源、GPIOTE あり）、**P2 = MCU ドメイン**（高速 64 MHz、**起床不可、SENSE / DETECT なし、GPIOTE なし**） | 表 40 |
| 周辺は自分のドメインのピン | 同じ周辺は同じポートのピンだけを使う | §8.8.3 |
| P2 を使える例外 | **周辺ドメインの SPIM / SPIS / UARTE に限り**、一部の P2 ピンを使える。**Constant Latency サブ電力モードが必要**。ピン表に書かれた機能にだけ使える | §8.8.3、§10.1.1 |
| TWIM（I²C） | 例外に含まれない → **P1 のピンだけ**。SCL はクロックピン | 表 76、77 |
| PDM（XIAO の MSM261 用。製品は I2S20） | CLK はクロックピン、DIN は近くのピン。PDM20/21 は周辺ドメイン → **P1** | 表 77 |
| I2S20（製品の mic = T5848） | 周辺ドメイン → **P1**。マスタで SCK・LRCK を出す。製品は SCK = P1.12、LRCK = P1.03（どちらも P1 のクロックピン）、SD = P1.13 | PS §8.11、表 81 |
| SPIM00 | **MCU ドメインの周辺。P2 に専用ピンが 2 組**ある。SPIM00/20 の組 = SCK P2.01、SDO P2.02、SDI P2.04、CSN P2.05、DCX P2.00。SPIM00/21 の組 = SCK P2.06、SDO P2.08、SDI P2.09、CSN P2.10、DCX P2.07。**Constant Latency は不要**（要るのは SPIM20/21 を P2 で使うとき）。32 MHz には P2 の E0/E1（extra high drive）を設定する | 表 76、77 |
| CS | 専用ピンでなくてよい。Zephyr は CS を GPIO で駆動する | — |
| SAADC の入力 | アナログ入力は **P1.04〜P1.07 と P1.11〜P1.14**（AIN0〜7）だけ | ピン表 |

CSP47 のピン表（使うものだけ、表 81）:

| XIAO | ピン（ボール） | クロックピン | 専用機能 |
|---|---|:---:|---|
| D0 | P1.04（D6） | Yes | |
| D1 | P1.05（E6） | | |
| D2 | P1.06（D7） | | |
| D3 | P1.07（E7） | | |
| D4 | P1.10（C3） | | |
| D5 | P1.11（C4） | Yes | |
| D6 | P2.08（D4） | | SPIM SDO（SPIM00/21）、UARTE TXD |
| D7 | P2.07（E4） | | SPIM DCX（SPIM00/21）、UARTE RXD |
| D8 | P2.01（G7） | Yes | SPIM SCK（SPIM00/20） |
| D9 | P2.04（F5） | | SPIM SDI（SPIM00/20） |
| D10 | P2.02（F6） | | SPIM SDO（SPIM00/20） |
| D11 / D12 | P0.03 / P0.04 | | 裏面のテストパッド（IMU の I²C に抵抗経由。抵抗は未実装）。側面 14 接点に含まれない |
| D13 / D14 / D15 | P2.10 / P2.09 / P2.06 | | 裏面パッド |

### §2.2 側面 14 接点の割当（外部の周辺を付ける場合）

| 接点 | 信号 | ポート | 理由 |
|---|---|---|---|
| D0 | PDM CLK（外付けの PDM マイク） | P1.04 | クロックピン、PDM20 は P1 |
| D1 | PDM DIN | P1.05 | CLK の隣 |
| **D2** | **WAKE**（マイクの出力） | P1.06 | 割り込み・起床が要る → P1 |
| **D3** | **前面ボタン** | P1.07 | 割り込み・起床が要る → P1 |
| D4 | I²C SDA（RV-3032） | P1.10 | TWIM は P1 |
| D5 | I²C SCL | P1.11 | クロックピン必須 |
| **D6** | **THSEL**（マイクの入力） | P2.08 | 出力のみ（µs のパルス列）。P2 でよい |
| D7 | NAND CS | P2.07 | GPIO |
| D8 | NAND SCK | P2.01 | SPIM00/20 の SCK |
| D9 | NAND MISO | P2.04 | SPIM00/20 の SDI |
| D10 | NAND MOSI | P2.02 | SPIM00/20 の SDO |
| 5V | 電池（CR2477）の正極 | — | §2.4 |
| GND | — | | |
| 3V3 | 出力（周辺の給電・レベル変換 IC の高い側。§8） | | |

- 必要な GPIO は 11 本（PDM 2、WAKE、THSEL、I²C 2、ボタン、NAND 4）で、D0〜D10 の 11 本に**ちょうど収まる（余裕 0）**。
- WAKE とボタンは P1 でなければならない（P2 に GPIOTE も起床も無い）。P1 のヘッダピン D0〜D5 の 6 本は、PDM 2 + I²C 2 + WAKE + ボタンで使い切る。そのため THSEL は P2（D6）に置く。
- 余裕 0 なので、GPIO を増やす変更は入れない。**RTC の割込は P1 を使い切っているので置けない**（RTC の INT は未接続の方針: [firmware.md §5.3](./firmware.md)）。余らなくなったら THSEL を抵抗で固定して AAD を使わない（マイクの仕様確認が要る）。
- NAND の WP# / HOLD# の扱いは、採用する NAND のデータシートで確認する（[storage.md](./storage.md)）。
- 試験用のイベントマーカーは、D6 / D7 が周辺に使われるときは裏面パッドの D13〜D15（P2 の出力）へワイヤで出す。

### §2.3 FW が使っているピン（`fw/`、周辺を付けていない XIAO）

| 用途 | ピン | 場所 |
|---|---|---|
| オンボードのマイク（MSM261）の PDM CLK / DIN | P1.12 / P1.13（PDM20。どちらも P1 でクロックピン） | `fw/src/audio.c`、ボードの `pdm20` |
| マイク + IMU の電源（load switch） | enable = P0.01 | `pdm_imu_pwr`。`fw/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay` で boot-on を外し、録音中だけ有効にする |
| 電池の分圧器（XIAO 基板上） | enable = P1.15 | `vbat_pwr`。5V ピンから給電すると約 240 µA（4.8 V）流れるので、overlay で boot-on を外す |
| イベントマーカー（オシロ用、測定用ビルド） | M_REC = D6（P2.08）、M_STORE = D7（P2.07）、M_BLE = D15（P2.06、裏面パッド） | `fw/src/marker.c`、`fw/src/marker.h` |
| 基板の RF スイッチの制御 | P2.03（rfsw_pwr）、P2.05（rfsw_ctl）。boot-on のまま | ボードの `rfsw_pwr`、`rfsw_ctl`。**この 2 本は GPIO に使えない** |

- マーカーの D6 / D7 は、周辺を付けていない Phase 0 の測定でだけ使える。§2.2 の割当で THSEL と NAND CS を D6 / D7 に配線するときは、マーカーを D13〜D15 へ移す。
- 周辺（外付けのマイク、NAND）を外付けしたときのピンは、FW にまだ入っていない。

### §2.4 XIAO の電源（二重に危険な点）

回路図の電源ページ（`03 Power`、`02 Block Diagram`）で確認した。

- **BAT パッド**: 充電 IC SGM40567（4.2 V の Li-ion 用、充電電流 = 24000 / R4 = 200 mA）の出力。USB を挿すと 4.2 V で充電する。**CR2477（リチウム一次電池）を接続してはならない。**
- **5V ピン（VBUS）**: 降圧 U5（**TPS628438YKAR**）の入力。USB の 5 V が D4（ショットキ、USB 側から VBUS への一方向）経由で接続している。したがって **電池を 5V ピンに接続したまま USB を挿すと、USB の 5 V が D4 を通って電池へ流れ込む（充電される）。** USB を抜いてから電池を繋ぎ、電池を外してから USB を挿す。
- USB が無いときは、Q1（LP0404N3T5G）が VBAT を VBUS へ通す。USB 用の LDO（SGM2040、SAMD11 用）は USB を抜けば止まる。
- **3V3 ピン**は出力（`3V3_OUT`）。フォーラムの報告（非公式）では、3V3 ピンへ外部給電すると約 450 µA 流れた（逆流の可能性）。**3V3 ピンへ給電しない。**
- U5 は降圧のみで、CR2477（約 3.0 V → 終端 2 V 級）では常に dropout（100 % モード）。**nRF の VDD は電池に追従して下がる**（3.3 V の固定ではない）。U5: VIN 1.8〜5.5 V、100 % モードの HS の RDSON 170 mΩ typ / 260 mΩ max（VIN 3.6 V、300 mA）、UVLO 上昇 1.75〜1.8 V / 下降 1.65〜1.7 V。
- 電池を 5V ピンから入れると、USB の経路と並んで **電池 → 5V ピン → U5** が給電経路になる。電池と 5V ピンの間に直列のダイオード（またはイデアルダイオード）を入れるか、「USB を挿すときは電池を外す」運用にする（§6.4）。
- XIAO 基板上の部品（U5 の静止電流、Q1 の経路、UM3301 の変換 IC、RF スイッチのリーク）が待機電力にどれだけ足されるかは**未測定**（Phase 0 で測る）。
- XIAO を 1.8 V で動かす改造（L1 を外して 3V3 ピンに 1.8 V を入れる）はキャリアを作らないので行わない。

### §2.5 確認していないこと

- PDM の「DIN は CLK の近くのピン」の具体的な距離の規則（データシートに数値なし）。XIAO に外付けの PDM マイクをつないで確認する。
- 実機で、PDM を D0 / D1 に付け替えて録音できること。現状はオンボードのマイク（P1.12 / P1.13）で動作確認済み。

## §3 Fanstel BC15M 基板のピン割当

**割当は確定（rev 0、2026-10-16。マイクの 3 本は T5848 に合わせて本節が正。hw 側も T5848 版に反映済み）。表と理由は [hw/standard/DESIGN.md §4](../../hw/standard/DESIGN.md)、回路は `hw/standard/standard.kicad_sch`。** 要点: マイク（T5848、I2S20）の I²S SCK = P1.12（D1）、WS = P1.03（F4）、SD = P1.13（E2）、WAKE = P1.14、THSEL = P1.09、I²C = P1.10 / P1.11、電池 AIN0 = P1.04（EN = P1.08）、ボタン = P1.07、NAND = SPIM00 の P2.01 / P2.02 / P2.04 / P2.05（EN = P2.03）、Mic EN = P2.00、終端表示 RUN = P2.09、マーカー = P2.06〜P2.08、UART = P0.00 / P0.01（UARTE30）。各ネットのパッド（A1〜F6）は DESIGN.md §4 の表。GPIO の割当は XIAO と同じ規則（§2.1）で、モジュールを替えても変わらない。

**WS = P1.03 は NFC2 ピンと共用。** NFC のピンを GPIO として使う設定が要る。P1.02（NFC1）は使わない。NFC の 2 ピンがどちらも GPIO で、レベルが異なるときの漏れの注意が nRF54L15 にあるかは未確認（§未確定）。32 MHz と 32.768 kHz の水晶、DC/DC のインダクタはモジュール内蔵で、外付けは無い。以下は割当を決める前に置いた制約の表。

チップの規則（§2.1）はモジュールの中のチップにそのまま当てはまる（BC15M の中のチップは CSP47 で、§2.1 の表 81 と玉番号が一致する）。XIAO の「側面 14 接点」の制約（余裕 0）は BC15M 基板にはない。

| 信号 | 向き | 置き場の制約 | SKU |
|---|---|---|---|
| I²S SCK（T5848） | 出力 | P1、クロックピン（P1.12） | 両方 |
| I²S WS / LRCK | 出力 | P1、クロックピン（P1.03。NFC2 と共用） | 両方 |
| I²S SD | 入力 | P1（P1.13） | 両方 |
| WAKE（T5848） | 入力 | P1（割り込み・起床） | 両方 |
| THSEL | 出力 | 任意（P2 でよい） | 両方 |
| 前面ボタン | 入力 | P1（起床） | 両方 |
| I²C SDA / SCL（RV-3032、footprint のみ） | 双方向 | P1、SCL はクロックピン | RTC を載せる場合 |
| NAND SCK / MOSI / MISO | 出力 / 出力 / 入力 | SPIM00 の P2 専用ピン（2 組のどちらか）、32 MHz は E0/E1 設定 | 両方 |
| NAND CS | 出力 | 任意の GPIO | 両方 |
| NAND の load switch の EN（実装パッドのみ） | 出力 | 任意 | 両方（[power.md §5.1](./power.md)） |
| Mic の load switch の EN | 出力 | 任意 | 両方（[power.md §5.1](./power.md)） |
| 電池電圧の測定（分圧器の出力 + スイッチの EN） | AIN + 出力 | AIN は P1.04〜P1.07 / P1.11〜P1.14 | 両方（§6.2） |
| Vcap の測定（分圧器の出力 + スイッチの EN）、レベル処理後 `VBAT_OK` の入力 | AIN + 出力、入力 | AIN は同上。VCAP は P1.05/AIN1 が予約候補。source pin は未確定。**VSTOR 基準の VBAT_OK を直結しない** | Premium（[power.md §3.4、§8.1](./power.md)） |
| 終端表示の RUN パルスの指令 | 出力 | 本数は [terminal-indicator.md §7](./terminal-indicator.md) | 両方 |
| 試験用のイベントマーカー（測定用ビルドを使う場合） | 出力 | P2 の出力で足りる | 試作 |
| SWD、UART の TX / RX パッド | — | §4 | 両方 |

未確認: モジュールの RF 端子の有無（[rf.md](./rf.md) の未確定）。FW 側では、内蔵の負荷容量（HFXO 17 pF、LFXO 7 pF）を devicetree で設定する（[firmware.md §5.2](./firmware.md)）。

## §4 SWD とデバッグ用パッド

**測れない基板は失敗**と考える。µA〜mA を跨ぐ電力の最適化が中心なので、製品版では消せるものも、最初の BC15M 基板では測定点を削らない（確定）。

- SWD は**開発・出荷前の書込み・救済**のために残す（[firmware.md §2.3](./firmware.md)）。必要な信号: SWDIO、SWCLK、RESET、VREF（target voltage）、GND。pogo で触れる形にする。大型のヘッダは不要。
  - Tag-Connect TC2030 と同じ配置（2 × 3、1.27 mm）の穴なしのポゴパッドに、ポゴピンを束ねた治具を当てる（決定 H6）。TC2030 の位置決め穴は付けない（裏面は電池ホルダが占める）。量産・最終筐体では専用 fixture で接触させる。
  - SWD の VREF は電圧の参照で、120 F の充電端子には使わない（§5.3）。
- **SWD の service / recovery 用パッドは、将来 APPROTECT を有効にしても物理的に残す**（[security.md §8](./security.md)）。
- 書込みプローブは、XIAO の SAMD11（CMSIS-DAP）を流用する案がある（方針）。**1.8 V のターゲットに合わせる方法と、XIAO の nRF54L15 と SWD の線を共有しない方法は未検証**（[plan/prototype.md](../plan/prototype.md)）。
- UART は必須ではない。USB / UART のコネクタを常設すると、ケースの穴、ESD 対策、面積、待機電力が増える。BLE + SWD で FW のデバッグはできる。**最初の基板には TX / RX のテストパッドだけ置く。**
- 最終筐体で pogo / SWD パッドが外から通常触れない位置にし、開発・製造 fixture からのアクセスを確保する。

## §5 テストポイントと電流測定リンク

### §5.1 電源の test point（最低限）

| TP | 内容 | SKU |
|---|---|---|
| TP_SOLAR | Solar 入力 | Premium |
| TP_CAP | 120 F の電圧（外部給電にも使う。§5.3） | Premium |
| TP_VSTOR | BQ25505 の VSTOR（内部 FET の後。TP_CAP と区別） | Premium |
| TP_CR2032 | CR2032 の電圧 | Premium |
| TP_BAT | CR2477 の電圧 | Standard |
| TP_MUX_OUT | power mux の出力 | Premium |
| TP_1V8_MAIN | 1.8 V の system rail（regulator 出力側と、MCU / NAND の近傍の両方で測れるようにする） | 両方 |
| TP_NAND_1V8 | NAND の rail（電流測定リンクの先） | 両方 |
| TP_MIC_1V8 | Mic の rail | 両方 |
| TP_GND | 複数置く | 両方 |
| reservoir の両端 | reservoir の電圧と充放電の観測 | Standard |

制御状態も出す（logic analyzer / オシロだけで source の遷移を追えるようにする）:

- `TP_VBAT_OK`（BQ25505 の出力）、`TP_PWR_SOURCE`（BQ25505 の power mux のゲート出力。primary / secondary）: Premium
- `TP_NAND_EN`、`TP_MIC_EN`: 両方

### §5.2 電流測定リンク

各 rail に **0 Ω のジャンパ、または短絡パッド**を設ける。0 Ω を外して電流計を直列に入れる。

```text
1V8_MAIN ── R0_MAIN ── system
                ↑
              外す
                ↓
          電流計（INA228 の治具）
```

対象:

- MAIN（`1V8_MAIN` の根元）、NAND、MIC、RTC / always-on 系（可能なら）
- 電池の入力側（Standard の TPS62840 の入力。R_BAT）
- Premium は cap 側の経路も電流を測れるリンクを設ける。PMOS の切替・充電経路を誤って短絡しない配置にする。

- 理由: 全体だけ測ると、平均電流が 100 µA 高いときに、マイク、NAND の漏れ、GPIO の back-power、PMIC、BLE のどれかを切り分けられない。rail ごとに測れれば、データシートとの乖離をすぐ見つけられる。
- 測定は [INA228 の治具](../test/current-measurement-fixture.md)に固定している。測定の項目と順序は [power.md §10.3](./power.md) と [plan/prototype.md](../plan/prototype.md)。電源の品質はオシロで、1V8 rail の最小値、mux 切替の過渡、NAND の program パルス、BLE の TX パルスを見る（brownout の余裕の確認）。

### §5.3 外部給電（Premium の初期充電・cold start）

[power.md §3.8](./power.md) の方針に従い、**既存の TP_CAP と TP_GND を組立・評価専用の fixture に使えるようにする**。ハッカソン本体に USB 給電・USB 充電経路は設けず、本番の実証と事前充電は実 Solar で行う。SWD の VREF は参照入力で、プローブから本体へ給電しない。専用の CAP_PRECHARGE pogo は必要時だけ追加する。

- 外部電源は電圧と電流の両方を制限する。**3.70 V 公称**を基準にし、給電端で誤差・過渡込み **3.8 V を超えない**。
- 接続前に cap の電圧と極性を確認する。HSL を 2.2 V 未満へ放電すること、0 V 近傍からの通常の再使用は認めない。
- 外部給電が BQ25505 の OV 制御を迂回する場合、bench supply / fixture 側が上限を保証する。
- CR2032 への逆流・充電を防ぐ PMOS の向きと経路を確認する。初期充電の fixture では CR2032 を外すか隔離し、接続状態での動作は別に検証する。
- 外部電源を外したあとの cap → system の給電と、Solar の再充電を確認する。

### §5.4 信号の test point

- **NAND の SPI**: /CS、CLK、IO0、IO1 に、オシロのプローブを当てられる小パッド。Quad の IO2 / IO3 は必要なら追加。高速線に大きな stub を作らない。パッドは小さく、可能な限り線上に置く。
- **I²C**: RTC を載せる場合、SDA と SCL の小パッド（RTC が「時刻保持はするが I²C だけ死ぬ」問題の切り分け）。
- **UART**: TX / RX（§4）。
- **RF**: BC15M モジュールでは RF の test 経路は置かない。裸チップ・オプション構成は [rf.md §6](./rf.md)。

### §5.5 配置の規則

- 片面に集約する。fixture から一方向にアクセスできる。
- GND パッドを複数置く。電源の TP は識別しやすくする。
- pogo の滑りを防ぐランドの形状にする。
- 電池・Solar を組んだ状態でも触れる位置にする。**ケースを壊さないと debug できない設計は避ける。**
- **conformal coating との関係**: TP をコーティングすると接触できなくなる。test / debug のゾーンをマスクしてコーティングするか、コーティング前に測定する。防汗試験用の基板は別の fixture にする。最終製品に工場の test 用パッドを残す場合は、fixture の接触後に coating / 封止する工程を検討する（[mechanical.md](./mechanical.md)）。

## §6 基板に入れる回路の要件

### §6.1 privacy latch の配線（採る。機構は未決）

要求（確定）:

- スイッチが PRIVATE のとき、**FW が何をしても、マイクは音を拾えない**。
- スイッチの状態を GPIO で読まない（FW は latch の状態を知らない前提で設計する。[product.md §4.3](./product.md)）。
- 位置づけは「録音の一時停止」の表示（[security.md §9](./security.md)）。機構（スライドか磁気式か）は筐体と一緒に決める（[mechanical.md](./mechanical.md)）。

**「Mic の VDD を切るだけ」では足りない**（T5838 Rev 1.0 の Table 8 で確認した規定。T5848 の同じ規定は未確認で、同じ扱いにする）: デジタルピンの入力電圧は −0.3 V 〜 VDD + 0.3 V または 1.98 V の小さい方。VDD が 0 V のとき、SCK / WS / THSEL に 1.8 V を入れると絶対最大定格を超え、ESD ダイオード経由でマイクの VDD ノードが持ち上がる（部分給電）。FW が壊れて SCK を出し続ければ、スイッチを切ってもマイクが部分的に動くおそれがある。

**BC15M 基板（レベル変換 IC なし。nRF の 1.8 V で T5848 を駆動。SCK / WS / THSEL は Ioff バッファ SN74AUP3G34 経由、SD / WAKE は直結）**（rev 0 で実装）:

- latch は `MIC_1V8` の給電経路に直列に入れる。
- マイクの入力 3 本（**SCK、WS、THSEL**）の経路に、**電源を latch の先から取る 1.8 V のバッファ（Ioff 対応品）を 1 段入れる**か、アナログスイッチで切る。バッファは 3 ch 品（rev 0 は SN74AUP3G34。PDM なら CLK と THSEL の 2 ch）。latch を切ると、T5848 の VDD とバッファの電源が同時に落ち、SCK / WS / THSEL を駆動するものが無くなる。
- 抵抗を直列に入れるだけの案は、電流を制限するだけで絶対最大定格の逸脱が残るので採らない。
- SD は mic の出力（非選択時は tristate）。データシートの指示で **100 kΩ の pull-down** を付ける。`MIC_1V8` が切れた PRIVATE 中は、SD は pull-down で LOW になる。PRIVATE 中の SD / WAKE の状態は Phase 1 で見る。

**XIAO 開発基板で TXU0204 を使う場合**（§8）:

- マイクの信号は TXU0204（CLK、THSEL / DIN、WAKE）、NAND は TXU0304（SCK、MOSI、CS / MISO）に分ける。
- **TXU0204 の VCCB を latch の先（`MIC_1V8`）に繋ぐ。** TXU は、どちらかの電源が 100 mV 未満または浮きのとき、全出力が Hi-Z（Ioff-float、データシート §9.3.5）。latch を切るとマイクの VDD と TXU0204 の B 側が同時に落ち、CLK / THSEL を駆動するものが無くなる。部品も GPIO も増えない。
- VCCB が浮いたときの ICCA は max 1.5 µA（25 ℃）/ 7 µA（85 ℃）/ 15 µA（125 ℃）。PRIVATE 中は録音しないので平均電力に効かない。
- Hi-Z になった出力（DIN → nRF、WAKE → nRF）は nRF 側で浮く。**nRF 側の入力**に 1 MΩ 級の pull-down（または nRF 内蔵の pull-down）を置く。
- TXU は「入力は電源を浮かせる前に Low にしておくこと」と規定している。latch を切る瞬間に CLK が High だと一瞬流れる。A 側の入力の内蔵 pull-down と、Ioff-float の上限（±1.5〜2.5 µA）で許容できるかを実機で確認する。

**試験（Phase 1。追加部品は不要）**: `R_MIC`（0 Ω）を、スイッチまたは 2 ピンのジャンパにできる footprint にする。R_MIC を開放し、SCK を回したまま、**マイクの VDD ノード（TP_MIC）の電圧と SD の動き**を見る。これで SN74AUP3G34 の Ioff で部分給電が抑えられるかの確認になる（バッファの要否は決まっている）。

**FW への影響**（方針）:

- T5848 の AAD の設定（THSEL の 1 線書込み）は、電源を入れるたびにアンロックから必要（[audio.md §4.3](./audio.md)）。latch を戻した瞬間にマイクは初期状態（AAD 無効）に戻るが、**FW は latch の状態を読めない**。
- 対策案: (1) FW が一定の周期（例: 1 分）でマイクを起こして AAD を設定し直す。1 回 10 ms × 120 µA × 1.8 V を 60 s ごとなら約 0.04 µW で無視できる。(2) I²S のデータが定数（SD の pull-down により全 0）のとき「マイク無電源」とみなし、event レコードに「privacy ON」を記録する（GPIO 不要、要実機。event の kind の追加が要る: [formats.md §4.1](./formats.md)）。
- latch を切っている間の欠落は、既存の欠落レコードに載せる（[formats.md §5](./formats.md)）。

機構（未決）: latch の電流は 20〜120 µA、1.8 V と小さい。乾式接点（金メッキ）の小型スライドスイッチが候補。ただし、防水の前面全面ボタン・膜防水に対し、可動の外部スイッチは封止の穴になる。磁気式（リードスイッチなど）で筐体の中に閉じ込める案は、ゼロ電力で封止も保てる。どちらも未検討（[mechanical.md](./mechanical.md)）。状態が一目で分かる表示（スライドの色など）を機構側で作る。

### §6.2 電池電圧の測定経路（Phase 1 基板に入れる。方式は [power.md §4.6](./power.md)）

電池電圧は、Standard の電圧段階（[power.md §4.2](./power.md)）と書込み・消去の電圧ゲート（[power.md §4.5](./power.md)）が依存する。**測れなければ状態機械は動かせない。**

**BC15M 基板（rail が TPS62840 で 1.8 V に固定される）:**

- SAADC の VDD 入力では電池電圧にならない。**測定時だけ通電するスイッチ付き分圧器（1 MΩ 級以上）を置き、出力を AIN 対応のピンに入れる。** 必要なピンは AIN 1 本 + スイッチの EN 用の GPIO 1 本（§3）。
- 分圧器の電流（なぜ常時分圧にしないか）: 100 kΩ + 100 kΩ の常時接続は 3 V で 15 µA = 45 µW（平均約 363 µW の 12 %。1 年で約 0.39 Wh で、3 Wh の約 13 %）で採らない。数 MΩ の常時分圧は 0.3〜0.5 µA 程度だが、SAADC のサンプリング時間と入力容量の条件を確認していない（未確認）。
- 校正: 実基板で 2 点以上（[power.md §4.6](./power.md)）。
- Premium の Vcap の測定も同じ作りで、AIN をもう 1 本使う（[power.md §8.1](./power.md)）。

**XIAO 開発基板（電池から給電すると VDD が電池に追従する）:**

- SAADC の内部入力 `Vdd`（`CH[n].PSELP.INTERNAL = 2`、Zephyr では `NRF_SAADC_VDD`）で電池電圧を測れる。**分圧器もピンも要らない。** VDD ≒ 電池 − 負荷 × 約 0.4 Ω で、Standard のしきい値（2.60 / 2.40 / 2.25 V）の判定には足りる。
- gain は 1/4 を使う（DevZone の投稿では VDD を gain 1 で測ると 0.9 V 付近になる。0.9 V ÷ 0.25 = 3.6 V が満点）。**実機で、gain 1/4 の値が電池電圧と合うかを確認する**（未確認）。
- XIAO の側面 14 接点に空きの AIN は無い（D0〜D3 と D5 が使用済み）。足りなければ、分圧器の出力を D3（ボタン）とジャンパで共有する案がある。

### §6.3 reservoir の footprint

- 容量: **220 µF 級を第一候補**にし、footprint は 100〜470 µF を載せられる形にする（[power.md §4.4](./power.md)）。
- 漏れ電流（エンジニアリング判断で、データシート未確認）: 220〜470 µF のタンタル / 導電性高分子は、漏れ電流が µA〜数十 µA に達しうる。平均 363〜551 µW（約 120〜180 µA）に対し、10 µA は 2〜8 %。**漏れの小さい MLCC（100 µF 級。DC バイアスで実効容量が下がる）を第一候補にし、タンタルは漏れを実測してから**にする。
- Phase 1 の測定項目に「reservoir を交換したときの rail 電流の差」を入れる。reservoir の両端に TP を置く（§5.1）。

### §6.4 逆流防止の footprint（Schottky / LM66100 / 0 Ω。実装は運用次第）

**footprint だけ置く**（確定）。載せるかは運用次第で、USB を挿せない構造なら実装しない選択肢を残す。

- 経路: XIAO 開発では、電池と 5V ピンの間（USB を挿すと D4 経由で電池が充電される: §2.4）。BC15M 基板には USB が無いので、充電経路の逆流は無い。BC15M 基板での要否は回路図で決める（rev 0 は 0 Ω、LM66100 は DNP）。
- Schottky の場合、降下ぶんだけ電池の使用下限が上がる。U5 の VIN 最小 1.8 V に対し、電池の内部抵抗 40 Ω・8 mA のパルスで必要な電池電圧は、ダイオード無しで 2.12 V、Schottky（降下 0.25 V と仮置き）で 2.37 V。周辺 rail 側の下限 2.18 V より 0.19 V 高く、nRF 側がシステムの下限を決める。この 0.19 V の間に残る電池容量の割合は未確認。
- イデアルダイオードの候補: **TI LM66100**（VIN 1.5〜5.5 V、IQ 150 nA typ、RON 141 mΩ typ @ 1.8 V、逆流を遮断）。降下はほぼ 0。逆流時の漏れ、価格、JLCPCB の在庫は未確認。
- 逆流防止を「必須」とは扱わない。USB-C を筐体の外へ出さず、電池を付けたまま USB を挿す手順を作らないので、保守作業での事故に対する保険。

## §7 配置・ESD・EMI の規則

小型 PCB の設計条件。XIAO を使う Phase 0 の開始条件ではない。大規模な解析や保護素子の一律の追加は行わず、侵入経路・実波形・測定結果で必要な対策を決める。

### §7.1 RF・機構

- アンテナの置き方・keep-out は [rf.md §3](./rf.md)。keep-out は機構の CAD にも反映する。
- φ40〜42 mm の筐体の stack-up に、横置きの cap、CR2032 / CR2477、PCB、Solar-button の移動量・hard stop、音響ポート、pogo のアクセスを入れて、干渉と keep-out を確認する（[mechanical.md](./mechanical.md)）。部品単体の外形だけで収まったと判定しない。
- 裸チップの場合の reference layout と matching: [rf.md §6](./rf.md)。

### §7.2 電源・マイク・デジタル配線

- BQ25505 / TPS62840 の入力・出力コンデンサ、inductor、switching loop は各データシートの配置に従う。switch node の面積を小さくし、mic・音響ポート・RF から離す。
  - https://www.ti.com/lit/ds/symlink/bq25505.pdf 、https://www.ti.com/lit/ds/symlink/tps62840.pdf
- I²S の SCK・WS / SPI CLK は、mic のポートや RF との近接並走を避け、連続した GND 参照を保つ。GND plane の分割を跨ぐ配線を作らない。
- NAND SPI の直列抵抗の footprint と近傍のデカップリングを置く（配線の方針と値の範囲は [storage.md §3](./storage.md)）。抵抗値は周波数・配線・信号の方向に合わせてオシロで決め、22〜47 Ω を一律に実装しない。
- NAND の /CS はゲート後の VCC へ pull-up する。電源 OFF 時の SPI の back-power と、mic を含む I/O の電圧整合を確認する（[power.md §5.3](./power.md)）。
- system rail は 1.8 V を評価の基準にする。1.9 V は、NAND の上下限と、POR・PFM・負荷過渡・mux 切替の実波形で決める（[power.md §2.2](./power.md)）。

### §7.3 ESD の侵入経路と帰路

- Solar-button の接点、筐体の隙間、mic の開口、strap の feed、pogo / test パッドを機構と一緒に点検し、外から触れる導体と結合しうる経路を特定する。
- **Solar 入力の保護**は、コネクタ / 接点の近くで短い帰路へ逃がす構成を検討する。保護素子は、panel の最大開放電圧、BQ25505 の許容電圧、clamp 特性、漏れ電流を照合して選ぶ。**型番・定数は未決定。** 常時の保護素子の漏れを電力収支に入れる（[power.md §7](./power.md)）。
- RF の ESD: BC15M モジュールでは RF 経路に素子を置かない。strap を使う場合は低容量の素子が必須（[rf.md §4、§6.4](./rf.md)）。
- 放電電流の帰路が、mic / RF / MCU の細い GND 配線を通る配置を避ける。**IC 単体の HBM / CDM の仕様だけで、システムの ESD 耐性を保証しない。**
- pogo は、最終筐体の外から通常触れない位置にし、開発・製造 fixture からのアクセスを確保する（§4、§5.5）。

### §7.4 確認の時期

- **PCB の発注前**: 回路図・配置・機構の keep-out・stack-up・保護経路をレビューする。保護部品の許容値と漏れ、外部給電の上限制御（§5.3）を確認する。
- **最初の基板**: NAND の書込・BLE の送信・負荷解除・mux 切替での rail の上下限、録音へのノイズの混入、実筐体の RF、想定する外部接触点への ESD のあとの録音・保存・通信・再起動を確認する。**ESD 試験のレベルと方法は、試験の前に定義する**（未定）。
- 基準の資料: [Nordic nRF54L15 reference layout](https://www.nordicsemi.com/Products/nRF54L15/Reference-layout)、上の TI のデータシート（Layout の節）。

## §8 レベル変換（XIAO 開発基板だけ）

**BC15M 基板は 1.8 V の rail で直接動かすので、レベル変換は要らない。** この節は、XIAO（nRF の VDD = 電池に追従する 3.3 V 系）に 1.8 V の周辺（外付けのマイク、NAND）を付けて開発する場合だけの仕様。キャリアは作らないので、必要になるまで実装しない（方針）。

- **push-pull の 8 本は TXU0204 + TXU0304 の 2 個**を第一候補にする（5 本 = nRF → 周辺、3 本 = 周辺 → nRF。0204 は 2 + 2、0304 は 3 + 1 の固定方向）。0204 = マイク、0304 = NAND の分担（§6.1）。DIR 端子が無く、OE を電源へ固定するので GPIO を使わない。
  - ICC（データシートの max、両電源合計、1 個あたり）: 2.5 µA（25 ℃）/ 3 µA（85 ℃）/ 6 µA（125 ℃）。typ は未公表。
  - SPI の上限は 10 MHz 前後と仮置き（Schmitt 入力で遅い見込み。実測で決める）。
  - BOM 増は約 $1.71 / 台（JLCPCB の 1 個価格、2026-10-14、PW 型: 0304 が $0.7276、0204 が $0.9837）。
- **I²C は変換しない。** RV-3032（VDD 1.4〜5.5 V、160 nA）を XIAO の 3V3 側に置く（[firmware.md §5.3](./firmware.md)）。XIAO の U5 は nRF が System OFF でも動くので、時計は動き続ける。
- **TXS0102 は採らない**（B 側 2.3 V 以上で、電池終端 2.0 V と衝突）。SN74AXC4T245 は予備（0.65〜3.6 V、ICC の max は 1 個あたり 20 µA（85 ℃）/ 40 µA（125 ℃）、typ 未公表。3 個要る）。
- 変換 IC の高い側の電源は、3V3 ピン（電池に追従）につなぐ。固定 3.3 V を仮定しない。電池終端での動作下限: TXU 1.1 V、RV-3032 1.4 V、nRF 1.7 V、U5 の UVLO 下降 1.65〜1.7 V。
- nRF → 周辺の信号の入力は、TXU に内蔵の約 5 MΩ の pull-down がある。pull が要るのは、TXU の出力が Hi-Z になったときに浮く nRF 側の入力（§6.1）。
- 変換 IC は OE を固定するので止められず、ICC は常に載る（nRF が System OFF のときも）。

## §9 基板で確認する項目

| # | 項目 | 方法 |
|---|---|---|
| 1 | privacy latch で、R_MIC を開放して SCK を回したときの TP_MIC の電圧と SD の動き（部分給電の有無）。SN74AUP3G34 の Ioff の効果 | §6.1 |
| 2 | latch を切る瞬間に CLK が High のときの流入（TXU を使う場合） | §6.1 |
| 3 | 電池電圧の測定: 分圧器の精度・settling・測定自体の電流。XIAO では SAADC の `Vdd`（gain 1/4）と電池電圧の一致 | §6.2、[power.md §10.2](./power.md) |
| 4 | reservoir を交換したときの rail 電流の差（漏れ） | §6.3 |
| 5 | GPIO の back-power（NAND / Mic を切った状態で、信号線が HIGH のときの電流） | [power.md §5.3](./power.md) |
| 6 | PDM を D0 / D1 に付け替えた録音、DIN と CLK の距離の規則（XIAO） | §2.5 |
| 7 | XIAO の 3V3 ピンの電池追従と、負荷（変換 IC・RTC）への耐性。U5 の待機電力への寄与 | §2.4、§8 |
| 8 | 書込みプローブ（XIAO の SAMD11）を 1.8 V のターゲットに使えるか | §4 |
| 9 | ESD 試験のあとの録音・保存・通信・再起動と RF の変化 | §7.4、[rf.md §4](./rf.md) |

## 未確定

| 項目 | 状態 |
|---|---|
| BC15M の隠れパッド（B1〜E5 の 20 個）のはんだの確認（組立後の電気試験）、VDD の下限（rail を 1.75 V 付近まで下げる） | 試作で確認（[hw/standard/DESIGN.md §5・§6](../../hw/standard/DESIGN.md)） |
| 電池電圧の測定経路の実装（分圧器の値、スイッチ、ピン）。Standard の電圧しきい値 | #12 |
| privacy latch の機構（スライドか磁気式か）、FW の再設定（AAD の再アンロック）と event の kind | 配線は SN74AUP3G34 で実装済み（rev 0）。新規 issue 候補: privacy latch の機構 |
| SWD プローブ（XIAO の SAMD11 の流用）を 1.8 V のターゲットで使う方法 | 新規 issue 候補: SWD プローブ（XIAO の SAMD11 流用）の 1.8 V ターゲット検証（[plan/prototype.md](../plan/prototype.md)） |
| 逆流防止の footprint（R2 0 Ω、U1 LM66100 は DNP）を載せるか、LM66100 の漏れ・価格・在庫 | rev 0 に footprint は実装済み。載せるかは運用次第（新規 issue 不要） |
| Solar 入力の ESD 保護素子の型番・定数、ESD 試験のレベルと方法 | 新規 issue 候補: Solar 入力の ESD 保護と ESD 試験の定義 |
| XIAO 開発基板のレベル変換: TXU の tpd と SPI の上限 MHz、BQA 型の在庫、3V3 ピンの耐性 | #22（XIAO 開発専用になったので、必要になるまで保留） |
| 筐体の stack-up（XIAO は実測 3.33 mm、USB-C 部 4.44 mm。BC15M 基板での再計算） | #24、[mechanical.md](./mechanical.md) |
| PDM の DIN と CLK の距離の規則、実機での PDM の付け替え（XIAO） | XIAO で確認（BC15M 基板は I2S20 なので不要） |
| WS = P1.03（NFC2 ピン）を使うときの NFC ピンの設定（GPIO 化）と、NFC の 2 ピンが GPIO で異なるレベルのときの漏れの注意が nRF54L15 にあるか。I2S20 の SCK・LRCK を P1 のクロックピンに置く必要があるか | 新規 issue 候補: I2S20 のピンと NFC ピンの確認 |
