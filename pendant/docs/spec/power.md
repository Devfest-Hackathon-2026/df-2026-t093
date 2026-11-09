# 電源仕様（Standard / Premium）

対象: 電源パス、system rail、電圧しきい値と状態機械、低電圧停止と brownout の方針、電源ゲーティング、エネルギー収支、残量テレメトリ、OTA のエネルギーゲート。

終端表示（赤⇄黒の機構、last-gasp 回路）は [terminal-indicator.md](./terminal-indicator.md) が持つ。この文書は、その発動条件になる電圧の位置づけだけを書く。ストレージ側の電断対策は [storage.md](./storage.md)、watchdog・OTA の手順は [firmware.md](./firmware.md)、測定用の test point は [board.md](./board.md) を見る。

確度の表記:

- **確定**: 設計の前提として動かさない。変えるときは設計変更として扱う。
- **方針**: 向きは決まっているが、数値や部品が決まっていない。
- **仮置き**: 実測で置き換える前提の値。基板や仕様にそのまま使わない。

**Premium 試作は、実際に発電する Solar と外観、HSL 主蓄電＋非常用ボタン電池を必須とし、USB 給電は設けない。**以下の回路・数値は実装・実測済みという意味ではない。

## §1 SKU と電源構成

2 つの SKU は、Bluetooth の構成を共通にし、電源・保存・筐体だけが違う（確定）。

### §1.1 Standard（CR2477 1 個）

```text
CR2477 ──┬─ reservoir コンデンサ（220 µF 級、footprint は 100〜470 µF）
         │
         ▼
      TPS62840 ──→ 1.8 V system rail
```

- Solar、BQ25505、120 F HSL、CR2032、power mux は持たない（確定）。
- 電池はユーザーが交換する。充電作業は無い。電池交換は通常利用で約 1 年を目標にする（目標であり保証値ではない。§7.6）。
- 詳細は §4。

### §1.2 Premium（Solar + 120 F HSL + CR2032）

**11 月デモ回路の評価候補:** 2026-10-27 に小型化のため 30 F HSL・CR1632・1 枚 Solar と電源回路ドラフトを追加検討した。一次電池保護と切替は未成立で実セル接続・PCB 化前に検証が必要。以下の 120 F／CR2032 と無日照 72 h は製品基準として残す。回路と比較。

```text
Solar ─→ BQ25505 VIN_DC（MPPT + boost charger）
                 │
       120 F HSL ↔ VBAT_SEC ─ 内部 FET ─ VSTOR ─ back-to-back PMOS 2 素子 ─┐
                                                                          ├─ VOR → TPS62840 → 1.8 V
       CR2032 ─ 一次電池の逆流保護（要選定）─ back-to-back PMOS 2 素子 ────┘
                    BQ25505 の VB_PRI_ON / VB_SEC_ON が各ペアを制御
```

- 通常は Solar と 120 F で動き、残量低下で CR2032 に逃げ、その間も Solar が 120 F を充電し、回復後に自動で戻る構成を目指す（要求は確定。本試作での充電継続・切替は未検証）。
- ハッカソン試作は **BQ25505＋TPS62840 系**を採る方針。小型 TPS62840DLCR の入手難を受け、デモ回路では同系列の TPS62842DGRR を評価する。ピンと footprint は異なる。部品調達の記録。BQ25570 は内蔵 buck を持つが一次電池用 mux の置換にはならず、今回は切り替えない。
- Solar と HSL は 1 つの通常電源系、CR2032 は独立した非常用電源系とする。USB、CR2477 の並列追加、CR2032 から HSL への充電は行わない。
- 詳細は §3。

### §1.3 共通部分

| 項目 | 内容 | 確度 |
|---|---|---|
| system rail | TPS62840 で 1.8 V を作る（§2） | 確定（1.9 V 化は未決） |
| MCU | nRF54L15（Fanstel BC15M モジュール、VDD はデータシートの表紙 1.7〜3.6 V、ピン表 1.8〜3.6 V）。GPIO は VDD 基準。1.8 V の共通コアにレベル変換は不要だが、Premium の BQ25505 出力は別途レベル処理が必須（§3.4） | 確定（VDD の下限は試作で確認） |
| マイク | T5848（AAD で通常 ON） | 確定 |
| RTC | 外付け RTC（RV-3032-C7）は **footprint だけ**（載せるかは実測で決める、[firmware.md §5](./firmware.md)）。載せる場合は system rail だけから給電し、VBACKUP は 10 kΩ で GND、内蔵 backup switchover と trickle charger は無効。rail が消えたら時刻は保証しない | 確定（載せる場合の接続） |
| NAND | 1.8 V の生 SPI NAND（[storage.md](./storage.md)） | 確定 |
| 終端表示 | 両 SKU にある（[terminal-indicator.md](./terminal-indicator.md)） | 確定 |

XIAO nRF54L15 は開発専用で、電源の仕様は BC15M 基板を基準にする。XIAO は 3.3 V 系で動くので、開発基板でのレベル変換は [board.md](./board.md) が持つ。

## §2 1.8 V system rail

### §2.1 レギュレータ

TI TPS62840（https://www.ti.com/product/TPS62840）。

| 項目 | 値 |
|---|---|
| 入力 | 1.8〜6.5 V |
| 静止電流 | 60 nA typ（モデルでは 120 nA 相当の 0.3 µW を置く） |
| シャットダウン | 25 nA |
| 出力電流 | 750 mA |
| 100 % duty | 対応 |
| 出力精度 | ±1.5 %（PWM モード・IOUT = 0 mA・VOUT ≥ 1.8 V の条件） |
| UVLO | 上昇 1.72〜1.8 V、下降 1.45〜1.75 V |
| HS RDSON | typ 430 / max 600 mΩ（VIN 3.6 V、200 mA）。VIN が低いときの値はデータシートに無い |

CR2032（2.0〜3.0 V）、CR2477、120 F HSL（2.5〜3.70 V 公称）のどれからでも、buck だけで 1.8 V を作れる。

### §2.2 rail 電圧（1.8 V、1.9 V 化は未決）

| 部品 | 動作範囲 |
|---|---|
| nRF54L15（BC15M） | チップは 1.7〜3.6 V。電源投入時の POR は 1.75 V（Product Specification v1.0 §11.14.1）。BC15M のデータシートは、表紙が 1.7 V、ピン表が 1.8 V |
| NAND（例 W25N04LW） | 動作上限 1.95 V |
| T5848 | 1.62〜1.98 V |
| RV-3032 | 1.4〜5.5 V |

- 1.8 V 設定の下側は ±1.5 % で 1.773 V。POR 1.75 V への余裕は 23 mV。BC15M のピン表の下限 1.8 V は割る（判断: 試作で rail を 1.75 V 付近まで下げて動作を確かめる）。
- 1.9 V 設定の上側は 1.9285 V。NAND の上限 1.95 V への余裕は 21.5 mV に減る。
- ±1.5 % は上表の条件のときだけの保証。PFM、起動、負荷過渡、mux 切替、配線の電圧降下はこの範囲で保証されない。
- 判断: 1.8 V で必要な余裕を満たせば 1.8 V を維持する。満たせなければ、1.9 V、出力容量、運用モードのうち最小の変更で対応する（仮置き。実測で決める）。
- 実測するもの: MCU 端子の POR 最低電圧、全負荷時の最低電圧、NAND 端子の起動時・負荷解除時の最高電圧、CR2032 / CR2477 の低電圧時の buck 余裕、電圧変更による消費電力。

### §2.3 rail を保てる入力電圧の下限

- ノードの要求電圧は 1.85 V（TPS62840 の VIN 最小 1.8 V + 余裕 50 mV）を基準にする（仮置き）。
- 1.8 V の維持は VIN ≥ 約 1.81〜1.83 V が条件（20 mA、RDSON max 0.6 Ω + インダクタ DCR 0.12 Ω、VIN 3.6 V の値で見積り。低 VIN の RDSON は未確認）。
- システムの動作下限は nRF の POR（1.75 V）ではなく、TPS62840 が 1.8 V を保てる入力電圧で決まる。

## §3 Premium の電源パス（BQ25505 + HSL + CR2032）

### §3.1 部品

| 部品 | 主な仕様 |
|---|---|
| TI BQ25505（VQFN-20、3.5 × 3.5 mm） | MPPT 付き昇圧チャージャ。Iq 325 nA typ。cold start は VIN 600 mV typ／700 mV max、その後は 100 mV 級から harvesting 継続（cold-start の試験条件は 100 µF で、120 F 本体の起動保証ではない）。一次電池用 mux の gate driver（VBAT_OK で切替、break-before-make）。OV／OK は抵抗設定。UV は内部固定（1.91 / 1.95 / 2.0 V、下降） |
| Eaton HSL1225-3R8127-R | 120 F、最大定格 3.8 V、**最低定格 2.5 V／最低許容 2.2 V**、DC ESR 最大 0.2 Ω（AC 1 kHz 最大 0.07 Ω）、漏れ ≤ 12 µA（25 °C・72 時間充電保持後）、φ12.5 × 25 mm、5.4 g、公称 576 J（3.8 → 2.2 V）= 160 mWh。初期容量 ±20 % |
| Panasonic CR2032 | 3 V、225 mAh（公称約 675 mWh）、φ20 × 3.2 mm、2.8 g、標準連続負荷 0.2 mA（許容最大電流ではない） |
| back-to-back PMOS 2 ペア | **4 素子（dual package なら 2 個）**。各ペアは common-source、両 gate をまとめて BQ25505 の対応する active-low 出力へ。VGS(th) だけで選ばず、最低駆動電圧での RDS(on)・漏れ・実装性を確認する |

- source 選択に別の mux IC は追加しない（方針）。ただし一次電池の逆流保護は別問題であり、§3.5 の確認で必要なら追加する。BQ25505 が内蔵するのは **mux の制御／gate driver** で、外付けの電流経路は省略できない。
- BQ25505 は buck を持たないので、system rail は TPS62840 で作る。
- HSL は `VBAT_SEC`、通常負荷への経路は **`VSTOR` から**取る。HSL から外付け PMOS へ直結して内部 UV 切断を迂回しない。[TI BQ25505 データシート §7.3.2・Fig.16](https://www.ti.com/lit/ds/symlink/bq25505.pdf)、[EVM SLUUAA8A Fig.1](https://www.ti.com.cn/cn/lit/pdf/sluuaa8)。
- 出典: https://www.ti.com/product/BQ25505、https://www.ti.com/lit/ds/symlink/bq25505.pdf、https://www.eaton.com/us/en-us/skuPage.HSL1225-3R8127-R.html、https://energy.panasonic.com/jp/business/products/lithium/coin-cr-standard/models/CR2032

### §3.2 切替電圧（従来値と今回の推奨評価候補）

| 動作 | 電圧（`VBAT_OK`） |
|---|---|
| 120 F から CR2032 へ切り替える | 従来は約 2.5 V。実抵抗の公称 **2.5097 V**。今回の推奨評価候補は **2.6063 V**（下降） |
| 120 F へ戻る | 従来は約 2.7 V。実抵抗の公称 **2.7006 V**。今回の推奨評価候補は **2.8019 V**（上昇） |

- 下降／上昇の間は現在の source を維持する（ヒステリシス）。切替点が 1 つだと負荷の着脱で行き来し、brownout・reset・NAND 中断を招く。
- **最低定格 2.5 V と最低許容 2.2 V は別。** 従来の下降しきい値は −2 % で 2.4595 V になり、最低許容以上でも定格範囲からは外れる。今回の候補は公称 2.6063 V、−2 % で 2.5542 V とし、通常動作を定格範囲に置く余裕を取る。ピーク時の端子降下・検出遅延も含めて確認するまでは **未採用の評価候補** とする。下記の 2.5／2.7 V の説明は従来設計の理由であり、実装値は §3.3 で区別する。
- 2.5 V にする理由: HSL の下限 2.2 V を割ると劣化する。CR2032 運転中も HSL は自己放電（max 12 µA）するので、日照が戻らない間に 2.2 V を割らない余裕が要る。2.5 V 切替なら 2.5 → 2.2 V は 120 F・一定 12 µA の単純計算で約 35 日（2.30 V 切替なら約 12 日）。保管の保証ではない（§3.8）。
- 2.7 V まで待つ理由: 2.7 → 2.5 V に 17.3 mWh あり、0.5 mW 負荷で理想値 約 35 時間分。少し回復しただけで戻らず、約 1.5 日分を貯めてから通常運転に戻る。
- BQ25505 の UV は内部固定で、2.5 V で負荷を切り替えても自己放電は止まらない。UV に頼って HSL の 2.2 V 下限を無期限に守れるとは扱わない。

従来の設計理由を示す、丸めた 3.70／2.5／2.7 V の説明用エネルギー（現抵抗の正確な値・今回の評価候補は §3.3／§7.4。E = ½C(V₁² − V₂²)、C = 120 F）:

| 区間 | エネルギー | 用途 |
|---|---|---|
| 3.70 → 2.5 V | 446.4 J = **124.0 mWh** | 通常運転 |
| 2.7 → 2.5 V | 62.4 J = 17.3 mWh | ヒステリシス幅 |
| 2.5 → 2.2 V | 84.6 J = 23.5 mWh | 使わない（自己放電への余裕） |

2.5 → 2.2 V は負荷に使う last-gasp の予算ではない。shutdown・commit・終端表示は、実際に使える電源経路のエネルギーで成立させる。

### §3.3 充電上限（VBAT_OV）と VBAT_OK の抵抗（BQ25505 を採った場合）

式（BQ25505 データシート SLUSBJ3F §7.3.3、§7.3.4）:

| 項目 | 式 |
|---|---|
| VBAT_OV（上昇時） | `1.5 × VBIAS × (1 + ROV2/ROV1)`。下降時は VBAT_OV − 24〜45 mV |
| VBAT_OK 下降（OK が落ちる） | `VBIAS × (1 + ROK2/ROK1)` |
| VBAT_OK 上昇（OK が戻る） | `VBIAS × (1 + (ROK2 + ROK3)/ROK1)` |
| 条件 | VBAT_UV < VBAT_OK 下降 ≤ VBAT_OK 上昇 − 50 mV < VBAT_OV。VBAT_OV ≥ VBAT_OK 上昇 |

- VBIAS は 1.205 / 1.21 / 1.217 V（min / typ / max）。
- ROV1 + ROV2、ROK1 + ROK2 + ROK3 は、どちらも 11 / 13 / 15 MΩ（min / typ / max）。
- 分圧器は 64 ms ごとに短時間だけ通電するので、連続電流ではない。
- しきい値の総合精度は **±2 %**（0.1 % 抵抗のとき）。**0.1 % の抵抗を使う**（確定）。1 % 抵抗への代替はこの総合精度の保証前提から外れる。VBIAS と抵抗だけの corner 計算を、1 % 使用時の総合最大値の保証に置き換えない。

OV の既決定値と、従来の OK 設定（実機検証前）:

| しきい値 | 抵抗 | 公称 | 総合（±2 %） |
|---|---|---:|---|
| OV | ROV1 = 6.34 MΩ、ROV2 = 6.49 MΩ | **3.672942 V** | 3.599483〜3.746400 V |
| OK 下降（従来） | ROK1 = 6.34 MΩ、ROK2 = 6.81 MΩ、ROK3 = 1.00 MΩ（ΣR 14.15 MΩ） | 2.5097 V | 2.4595〜2.5599 V |
| OK 上昇（従来） | 同上 | 2.700552 V | 2.646541〜2.754563 V |

- OV の設計基準は 3.70 V 以下に寄せる。理由: ±2 % の上側が HSL の最大定格 3.8 V を超えないようにするため。E96 の最近傍（ROV1 = 6.19 MΩ、ROV2 = 6.49 MΩ）は公称 3.718 V で、上側 3.792 V となり余裕が 8 mV しかない。3.673 V の組は上側 3.746 V で余裕 54 mV。
- OV の正確な公称値は **3.6729416 V**。従来の OK 下降公称 2.5097003 V と組み合わせると、120 F の使用可能量は **119.865 mWh**（2.5 V に丸めた計算とは区別する）。
- 抵抗値の丸め、温度、基板の漏れ、充電停止時の波形まで含めて 3.8 V 以下を実測で確認する。120 F という容量だけでリップルや過渡を無視しない。

今回の推奨評価候補（**抵抗・回路・PCB を変更済みという意味ではない**）:

| 項目 | 候補 | 公称 | ±2 % |
|---|---|---:|---|
| OK 下降 | ROK1 = 4.87 MΩ、ROK2 = 5.62 MΩ、ROK3 = 787 kΩ（ΣR = 11.277 MΩ） | 2.606345 V | 2.554218〜2.658472 V |
| OK 上昇 | 同上 | 2.801883 V | 2.745845〜2.857921 V |

OV は既決定の 6.34／6.49 MΩ を維持する。分圧器の総抵抗は、電気特性表の 11〜15 MΩ に加え、本文の推奨 13 MΩ 以下も満たす。最悪の上昇下限 − 下降上限は約 87 mV で、50 mV の分離条件を満たす。実しきい値と HSL 端子波形で採否を決める。

**11 月デモの回路ドラフトは別の評価設定:** Vina 30 F に対する静的余裕を増やすため、下降／復帰の公称を 2.785／2.981 V とした。製品向け上記候補の採用・評価を変更した意味ではなく、デモでも過渡と保管中の下限は未保証。デモ回路の計算と制約。

再現ツール: [bq25505_resistors.py](../../tools/bq25505_resistors.py)（従来値／評価候補、実公称値を中心にした corner とエネルギーを出力）。

### §3.4 起動直後と既定状態（方針）

- `VBAT_OK` LOW で primary、HIGH で secondary を選ぶ。無発電・HSL 低下・CR2032 のみ・CR2032 なしの各起動状態と gate の既定レベルを実機で確認する。全電源を失った状態から MCU の操作で切替を成立させる設計にしない。
- **`VBAT_OK` は VSTOR 基準の push-pull 出力であり、1.8 V GPIO へ直結しない。** BQ25505 の pin 表と Nordic の入力定格を照合し、電圧変換・rail OFF 時の back-power 防止を含めて接続する。固定抵抗分圧だけで全範囲の VIH と入力上限を両立できるかも確認する。
- `VB_PRI_ON`／`VB_SEC_ON` の高電圧側信号も MCU へ直結しない。MCU が観測するのはレベル処理後の source と Vcap／coin 電圧。BQ25505 が UV で VSTOR と VBAT_SEC を切り離す状態では、`VBAT_OK` だけから HSL 電圧を推定しない。

### §3.5 逆流の禁止（確定）

| 経路 | 可否 |
|---|---|
| Solar → 120 F | 可 |
| CR2032 → system | 可 |
| CR2032 → 120 F | **禁止** |
| 120 F → CR2032 | **禁止** |

- CR2032 で 120 F を充電すると、非常用電池を短期間で使い切り、power path も複雑になる。
- OFF 時の双方向遮断は、各 source の **common-source back-to-back PMOS ペア**で行う。単一 PMOS の body diode に頼らない。TI Fig.16／EVM Fig.1 の source・drain・gate 接続を照合する。
- **ON 中の PMOS channel は双方向に流れる。** 消耗した CR2032 より VOR／bulk が高いまま primary を ON にすると逆電流があり得る。break-before-make は source の同時接続を避ける機能であり、一次電池への逆充電禁止を単独で保証しない。
- 一次電池には順方向降下・漏れ・過渡を評価した逆流保護を設ける方針。低漏れ diode と理想 diode は候補で、品番・配置・電圧余裕は未決定。まず電池模擬電源で VOR > coin の遷移を試し、定常漏れだけでなく **逆電流の peak・時間・積算電荷**を記録して電池メーカーの制限と照合する。測定器の分解能以下を数学的なゼロとは呼ばない。
- 逆方向の **COIN → CAP 復帰**も、VOR の bulk が回復直後の VSTOR より高い場合、secondary の ON により残留電荷が HSL 側へ移る可能性がある。secondary 電流と電荷も測定し、継続的な coin 充電経路と有限の残留電荷移動を分ける。「CR2032 → HSL 禁止」を満たす保護・許容条件は PCB 前の未決事項とし、必要なら secondary 側の逆流保護も評価する。一次枝の保護だけで両方向を検証済みとはしない。

### §3.6 CR2032 運転中の動作（方針）

CR2032 は通常運転を完全に再現する電源ではなく、survival mode 用。

| 項目 | CAP mode | CR2032 mode |
|---|---|---|
| 録音・AAD | 通常 | 優先する |
| NAND 書込み | 64〜128 KB のバッチ program | フルページを 1 枚ずつ program し、BUSY 解除・status 確認のあとも電源回復を待つ。連続バッチ program は禁止 |
| BLE 同期 | 周期的に行う | 停止する |
| 大電流の処理 | 通常 | 延期する |
| Solar | 120 F を充電 | 120 F を独立に充電し続ける（実機で確認） |

- CR2032 選択中は PMOS（secondary）が OFF なので、**120 F は system 負荷のピーク補助にならない**。逆流防止を崩して 120 F を並列接続しない。
- RAM の蓄積量や chunk 単位と、program の連続性は別に扱う。
- CAP mode で消去済みブロックを準備しておく。CR2032 中に使い切った場合は検証済みの条件での消去、または保存制限と欠落記録に移る（[storage.md](./storage.md)）。未検証の erase や無期限の録音継続を前提にしない。
- CAP → CR2032 の切替時は、進行中の program **および erase** を安全に完了できる電源余裕を検証し、以降の各 command を制御する。割込みを受けてからの FW だけでは発行済み command を安全化できない。回復待ちで RAM が溢れる場合の制限と欠落記録も定義する。
- pre-erase は data block だけでなく metadata・起動時の更新・reclaim・BBM 更新まで確認する。現行 `ring` の metadata commit は erase を伴い、data block の事前消去だけで CR2032 中の erase が無くなるわけではない。未検証なら新規 program／erase を閉じ、残エネルギーでの停止と復帰時の欠落報告を行う。常時保存できる survival mode は検証後に許可する。
- 4 KB / 32 kbps の payload は 1 ページあたり約 1.024 秒。11 月デモ機の F35UQA512M は **2 KiB/page** なので payload だけなら約 0.512 秒/page。ヘッダ・metadata・gate 復帰・最終ページのサイズを含めて、必要な平均保存速度と回復待ち時間を両立させる。連続の 32 KiB record は 16 page に当たる。
- bulk コンデンサの位置（buck 入力側 / 出力側）、容量、ESR、実効容量、漏れは未確定。**100 µF・1 秒間隔での成立は未検証**。bulk だけで CR2032 の電流が低く制限されるとは仮定しない。入力側は `ΔE = C(Vhigh² − Vlow²)/2` と buck 効率で評価し、出力側の `ΔV = Q/C` と混同しない。必要なら最小限の電流制限などを追加し、ハード追加が不要とは断定しない。
- 規模感（一定容量・補給なしの概算。降下予測や容量の決定値ではない）: program だけの 20 mA × 440 µs = 8.8 µC は 100 µF で 88 mV。SPI・MCU を含めると約 19.3 µC で約 193 mV。連続 32 ページの program だけで 281.6 µC。W25N08LW は program 440 µs typ / 800 µs max、erase 3 ms typ / 10 ms max で、20 mA × 3 ms だけでも 60 µC。ページ分散で erase の負荷は消えない。
- 11 月デモ機の F35UQA512M は参照 DS Rev 1.1 で program / erase 電流 各 15 mA typ / 25 mA max、ECC 有効 program 380 µs typ / 750 µs max、erase 2 ms typ / 10 ms max。各最大値を掛けた NAND 単独の保守的な規模感は program 18.75 µC、erase 250 µC。MCU・SPI・変換損失・電池特性を含まないため、COIN での実行を保証する値ではない。2 KiB page の書込み回数・電力は 4 KiB 基準モデルを流用せず測る。[F35UQA512M DS Rev 1.1](https://cdn.icstop.com/upload/pdfs/e7/de/e7decc6bb7e9f2384da286e27083f166.pdf)。
- 標準連続負荷 0.2 mA を、電池の許容最大電流とは解釈しない。

### §3.7 CR2032 の役割（確定）

CR2032 は通常運転の主エネルギー源ではない。日照不足、長期間の屋内、cap 残量不足のあいだ動き続けるための bridge。公称 3 V × 225 mAh ≈ 675 mWh は銘板上の理想値で、低電圧 cutoff、負荷電流、温度、内部抵抗を含めた実使用時間は別に評価する。

### §3.8 初期充電・「空」・長期保管（方針）

- 「空」は、通常運転分を使い切って 2.5 V 付近になった状態を指す。**HSL を 0 V まで放電してよいという意味ではない**（メーカーは 2.2 V 未満への放電を禁止している）。
- CR2032 が生きていれば system が起動・継続し、その間に Solar → BQ25505 → 120 F が独立に充電し、§3.3 の上昇しきい値で cap へ戻る構成を目指す。CR2032 の挿入順・低温・消耗状態と起動負荷、充電継続は本試作で検証する。
- ハッカソン試作品に **USB 給電・USB 充電経路は作らない**。発電・補充は実パネルで行い、本番前に Solar だけで必要な開始電圧へ回復できる時間を確認する。TP_CAP/GND の電流制限付き fixture は組立・評価用に限り、デモの代替電源にはしない（[board.md §5.3](./board.md)）。
- **0 V 近傍の HSL を通常の手順で再充電・再使用しない。** 許容範囲外まで放電した個体は、メーカーに再使用の可否を確認する。BQ25505 が 0 V の storage から cold start できることと、HSL がその状態を許容することは別の問題。
- 長期保管では 2.5 → 2.2 V の余裕は有限。120 F・一定 12 µA で約 34.7 日、容量 67.2 F なら約 19.4 日（25 °C の漏れ仕様による単純計算）。回路・分圧器・保護部品の漏れと温度を含めた放置試験で、保管期限と点検・再充電の条件を決める。負荷を切り離すだけでは cap 自身の自己放電は止まらない。

### §3.9 Premium の電源状態機械（方針）

```text
CAP_NORMAL ── VBAT_OK LOW ──→ CR2032_SURVIVAL
    ▲                                │
    └──────── VBAT_OK HIGH ─────────┤（Solar は CR2032 運転中も 120 F を充電）
                                     │
                          CR2032 が終端電圧に到達
                                     ▼
                                 LAST_GASP ──→ 機械フラグ BLACK ──→ OFF
```

- CAP_NORMAL ⇄ CR2032_SURVIVAL の切替は BQ25505 の `VBAT_OK`（§3.2）がハードで行う。FW は `VBAT_OK` の GPIO と Vcap の ADC で状態を観測し、telemetry と運用ポリシー（§3.6）に使う。
- LAST_GASP は、MCU を介さない独立の supervisor と last-gasp 回路が動かす（[terminal-indicator.md](./terminal-indicator.md)）。CR2032 の終端しきい値は 2.325 V を基準にする（terminal-indicator.md §5）。

## §4 Standard（CR2477）の電源

### §4.1 電池

| 項目 | 値 |
|---|---|
| Jauch CR2477J（252937） | 公称 3 V、1000 mAh、標準負荷 4.7 kΩ（0.6 mA）、最大パルス電流 25 mA、動作 −30〜+85 °C。内部抵抗の記載なし |
| Renata CR2477N | 950 mAh（Panasonic CR2477 の資料は未取得） |

CR2477 の内部抵抗はメーカー資料に数値が無い（Jauch のパルス放電曲線から 30〜40 Ω と読んだ。§4.4）。実測で置き換える。

### §4.2 電圧段階と状態機械（段階の形は方針、数値はすべて仮置き）

実装は `fw/lib/powerstate/`（純 C。電池電圧を入力にしてホストで試験する）。

| 段階 | 入る条件（仮置き） | erase / program | 録音 | 縮退レート | BLE 同期 | OTA |
|---|---|---|---|---|---|---|
| NORMAL | — | 可 | 可 | 通常（32 kbps） | 可 | 可 |
| LOW | 電池 < 2.60 V が 3 サンプル | 可 | 可 | 16 kbps 側へ | 可 | 不可 |
| CRITICAL | 電池 < 2.40 V が 3 サンプル | **新規禁止** | **停止** | — | 不可 | 不可 |
| TERMINAL | 電池 < 2.25 V が 2 サンプル | 不可 | 不可 | — | 不可 | 不可 |

遷移の規則（方針）:

- **サンプルは、無線とフラッシュが静かなときに取る。** BLE TX や program 中の電圧降下を電池の残量と取り違えないため。
- 悪い方向への遷移は 1 段ずつ。TERMINAL への最後の 1 歩だけ速い（2 サンプル。MCU が brownout するまでの時間が少ない）。
- 良い方向への復帰は、しきい値 + 100 mV（ヒステリシス）を 5 サンプル連続で超えたとき。境界付近で電圧が揺れても行き来しない（ノイズ ±60 mV の 5,000 サンプルのホスト試験で遷移は 1 回以下）。
- **TERMINAL は再起動までラッチ**する。電池が一時的に回復しても戻らない。
- **起動の門**: 電池が 2.70 V（仮置き）に届かなければ、起動せず電源を切ったまま待つ。平らな電池に交換した直後に、起動 → 即 TERMINAL → 再起動のループを作らないため。
- LOW では、バックログ連動のビットレート制御（[storage.md](./storage.md)）とは別に、電池を理由に 16 kbps 側へ落とす。

既定値（`fw/lib/powerstate/powerstate.c` の `ps_default`、すべて仮置き）: low 2600 mV、crit 2400 mV、term 2250 mV、hyst 100 mV、restart 2700 mV、サンプル数 3 / 5 / 2。

TERMINAL の位置づけ: 状態機械の TERMINAL は system rail を安全に落とす段。終端表示（BLACK への反転）は MCU を介さない別のハード経路で、Standard 用の last-gasp 回路を別に設計する（[terminal-indicator.md](./terminal-indicator.md) §13）。

### §4.3 仮置きの値の根拠

- CR2477 は公称 3 V。放電の大半は 2.8〜2.9 V 付近で、終端に向けて下がる（Jauch の曲線）。
- 1.8 V rail は TPS62840 の入力が約 1.81〜1.83 V を割ると保てない（§2.3）。nRF の POR は 1.75 V。
- 終端付近は電池の内部抵抗が上がる。BLE TX 5 mA 級のパルスで 0.25 V 程度下がると仮定し、TERMINAL の 2.25 V は「パルスで 1.9 V 近くまで下がっても動き続けられる」余裕を持たせた置き方。この仮定は未検証。
- 閾値は §10.2 の測定のあとで決める。期限は Phase 1 基板の発注前。

### §4.4 パルス降下と reservoir（確定は「reservoir を置く」こと、容量は仮置き）

モデル: 開路電圧 Voc → 内部抵抗 R → ノード（reservoir 容量 C）→ 負荷。電流 I の定電流パルス（長さ T）で `V(T) = Voc − I·R·(1 − exp(−T/(R·C)))`。C = 0 なら `Voc − I·R`。再現ツールは `tools/cr2477_pulse.py`。

仮定（実測で置き換える）:

| 項目 | 値 |
|---|---|
| 電池の内部抵抗 R | 15 Ω（ms 級）/ 40 Ω（Jauch の 5 s 曲線）/ 80 Ω（低温、40 Ω の 2 倍） |
| BLE パルス | nRF 側 8 mA（無線 + CPU、3 V）+ 1.8 V 側 0.3 mA、3 ms |
| 1.8 V 降圧の効率 | 0.9 |
| ノードの要求電圧 | 1.85 V |

結果（ノード ≥ 1.85 V を全パルスで保つのに必要な最小の Voc、reservoir なし）:

| パルス | R = 15 Ω | R = 40 Ω | R = 80 Ω |
|---|---:|---:|---:|
| BLE（8 mA、3 ms） | 1.97 V | **2.18 V** | 2.51 V |
| 20 mA 級の program（5 ms）| 2.17 V | 2.70 V | 3.54 V |
| program + BLE（0.8 ms） | 2.14 V | 2.62 V | 3.38 V |

- BLE のパルスは、R = 40 Ω で Voc が約 2.2 V になるまで reservoir なしで 1.85 V の rail を保てる。R = 40 Ω では Voc = 2.0 V で BLE すら 1.67 V に落ちるので、reservoir なしで電池終端まで使うことはできない。
- ページ program（0.8〜5 ms）は、100〜220 µF の reservoir で Voc = 2.2〜2.5 V まで救える（R = 40 Ω）。20 mA・5 ms の program は Voc = 2.5 V で 86 µF、Voc = 2.0 V で 757 µF が要る。
- 固定容量での最小ノード電圧（R = 40 Ω、Voc = 2.2 V）: BLE は 100 µF で 2.02 V、220 µF で 2.10 V。20 mA・5 ms の program は 220 µF で 1.78 V、470 µF で 1.98 V。
- 長いパルス（τ = R·C = 40 Ω × 470 µF = 19 ms より十分長いもの）は reservoir では橋渡しできない。電池の電圧ゲート（§4.5）が唯一の対策。
- 低温（R が 2 倍）では、重い program や erase は Voc 3.0 V 付近の新品でないと耐えない。
- NAND の program / erase（例 W25N08LW: program 440 µs typ / 800 µs max、erase 3 ms typ / 10 ms max、20 mA 級）は、上の 20 mA・5 ms の行を近似として使い、NAND の定数で再計算する（#18）。

reservoir の仕様（方針）: **220 µF 級を第一候補**にし、footprint は 100〜470 µF を載せられる形にする（0805 / 1206 のセラミックなら 100 µF 級、タンタルまたは導電性高分子で 220〜470 µF）。DC バイアスによる実効容量の低下は未確認で、部品選定で確認する。

### §4.5 書込み・消去の電圧ゲート（方針）

- NAND の erase / program は、**電池の開放電圧が閾値以上のときだけ許す**。長いパルスは reservoir で救えないので、電池電圧のゲートで防ぐ。
- 状態機械では、CRITICAL（2.40 V 仮置き）で新規の erase / program を禁止し、LOW（2.60 V 仮置き）までは許す。閾値の具体値は、Phase 1 の実測（§10.2）で決める。
- 低温では R が増えるので、同じゲートが先に働き、低温だけの別の制限は設けない（確定）。
- erase の開始時にノード電圧を測れない構成の場合は、電池の使用範囲を 2.6 V 以上に限る。

### §4.6 電池電圧の測定（方針。Phase 1 基板に入れる）

- 電池ノードに、**測定時だけ通電するスイッチ付き分圧器**（1 MΩ 級以上）を置き、SAADC で読む。測定前に数 ms の settling を取り、one-shot で読み、必要なら oversampling する。測定後に分圧器を切る。常時分圧は、3 V / 10 MΩ でも 0.3 µA 流れ、µA 級のシステムでは無視できない。
- Standard の rail は TPS62840 で 1.8 V に固定されるので、SAADC で VDD を読んでも電池電圧にならない。分圧器が要る（VDD が電池に追従する XIAO 開発基板では VDD 読みも使える）。
- ADC には DC 誤差があるので、実基板で 2 点以上の校正を行う。
- サンプルを取るタイミングは §4.2 の規則に従う。

### §4.7 電池寿命

§7.6 を見る。

## §5 電源ゲーティング

「OFF にしたつもりの部品が数 µA 流れ続ける」問題を潰す。

### §5.1 load switch（TI TPS22917DBVR）

自宅リフローと検査のため、Standard の現回路と同じ **TPS22917DBVR（SOT-23-6）** を共通コアの基準にする。VIN 1.0〜5.5 V、2 A、RON **80 mΩ @ 5 V／120 mΩ @ 1.8 V typ**、ON 時 Iq 0.5 µA typ、OFF 時 10 nA typ。CT で rise time、QOD の外付け接続で出力放電を調整する。TPS22916 の WCSP 外形・RON・variant の説明を転用しない。[TI TPS22917 SLVSDW8B](https://www.ti.com/lit/ds/symlink/tps22917.pdf)、[Standard の設計根拠](../../hw/standard/DESIGN.md)。

RCB の検出電流 −500 mA typ は一次電池の微小逆充電を防ぐ保証ではない。§3.5 の保護にそのまま流用しない。NAND の DPD は採用品ごとに確認し、未確認なら power gate を確保する。旧 Alliance 候補の自動 Sleep を、規定電流の分かった DPD と同等とは扱わない。

- **DPD 対応 NAND は DPD（Deep Power-down）優先**（製品方針）。対応品はバースト間の DPD と hard gate を比較する。11 月デモ機の F35UQA512M は参照版に DPD 命令が見当たらず、power gate を検証する。
- DPD の無い品（例 XTX XT26Q18D は待機 max 120 µA = 216 µW）では、ゲートが必須。
- W25N08LW の例: 待機 約 20 µA typ、DPD 約 2 µA typ（max 30 µA = 54 µW）、page program 約 20 mA typ。2 µA × 1.8 V = 3.6 µW は 0.5 mW の約 0.72 %。typ なら小さいが、max や個体差を考えると真の power-off にできる価値は残る。DPD の実測（10〜20 個）で、ゲートを常用するかを決める。
- F35UQA512M の standby は 10 µA typ / 50 µA max（1.8 V rail で 18 / 90 µW）。他の小容量候補の DPD・電流と、gate 後の消費は採用品ごとに確認する（#27）。
- 毎ページ OFF は自動的な既定にしない。wake-up／reset／設定／突入の energy と待ち時間、standby／Sleep の実測を比較して周期を決める。COIN で必要な平均書込速度と RAM を満たし、BUSY 中の遮断を起こさないことを条件にする。
- **Mic は通常運転では gate しない。** T5848 の AAD は、マイク自身が音を監視して wake を出すので電源が要る。ただし独立の gate を持つ。使う場面: 明示的な power-off、出荷・保管、電池が最後の非常領域まで下がったとき、FW の recovery。
- NAND と Mic は別の gate（別 rail）にする（共有すると、NAND を切ると AAD まで死ぬ）。

```text
1.8V MAIN ─┬─ TPS22917 → NAND_1V8
           └─ TPS22917 → MIC_1V8（通常 ON）
```

- QOD は、OFF 時に出力容量を放電して rail を確実に 0 V へ落とす一方、頻繁に ON/OFF すると毎回エネルギーを捨てる。NAND rail は rail 容量が小さく ghost powering を確実に消したいので QOD も合理的。QOD あり / なしの両方を試し、消費と起動安定性で選ぶ。

### §5.2 NAND の電源 OFF / ON の順序（方針）

OFF:

1. pending chunk を commit する
2. NAND の BUSY 解除を確認する
3. /CS = HIGH
4. SPI のピンを安全な状態にする（Hi-Z / input、またはデータシートが許す LOW）
5. load switch を OFF
6. rail 電圧が落ちる

ON:

1. load switch を ON
2. power-up の安定化を待つ（NAND のデータシートの power-up timing を満たす）
3. NAND を reset する
4. status を確認する
5. 必要な feature / config を設定する
6. append を再開する

### §5.3 GPIO back-power（試験を必須にする）

NAND の VCC が 0 V でも、MCU の SPI の信号線が 1.8 V の HIGH のままだと、NAND の入力保護ダイオードなどを通って電流が流れる。結果として、数 µA〜mA の謎のリーク、rail が中途半端な電圧で浮く、latch-up、起動失敗が起こりうる。**電源を切るだけでは不十分で、信号線の状態まで定義する。** まず GPIO の Hi-Z で成立するかを実測し、足りなければ 22〜100 Ω の直列抵抗や bus isolation を検討する。

### §5.4 分圧器

Vcap と電池電圧の分圧器は常時接続しない（アナログスイッチまたは MOSFET で測定時だけ通電）。

```text
V ── MOSFET / analog switch ── 抵抗分圧 ── ADC
```

### §5.5 「OFF」の定義

| 状態 | RTC | Mic | MCU | NAND | BLE |
|---|---|---|---|---|---|
| Normal low-power | ON | AAD ON | sleep | OFF（DPD またはゲート） | OFF |
| User power-off / storage（system rail を維持する場合） | ON | OFF | system OFF または最小の wake ロジック | OFF | OFF |
| Dead indicator state | rail 消失時は保持を保証しない | OFF | OFF | OFF | OFF |

Dead indicator state では、機械フラグ（終端後は BLACK）だけが受動で状態を保持する（[terminal-indicator.md](./terminal-indicator.md)）。regulator も止める保管では、RTC の時刻保持を保証しない（[firmware.md](./firmware.md)）。

## §6 低電圧停止と brownout

watchdog の方針は [firmware.md](./firmware.md) が持つ。ここは電圧の側だけを書く。

- **BOR（brownout reset）は最後の安全網**（方針）。通常は BOR に突入する前に、電源管理が低電圧を検出して次の順で止める。
  1. 新規の NAND erase / program を禁止する
  2. metadata の commit を終える
  3. 録音を停止または縮退する
  4. system rail を安全に shutdown する
- 通常の UV しきい値は、必ず BOR のしきい値より上に置く（`normal UV threshold > BOR threshold`）。BOR を通常の shutdown の手段にしない。graceful UV shutdown は必須。
- Standard の対応: 手順 1・2 は CRITICAL、3 は LOW で縮退・CRITICAL で停止、4 は TERMINAL（§4.2）。metadata の commit は CRITICAL に入った時点で 1 回行い、進行中の追記の完了を待ってから書く。TERMINAL のしきい値 `term_mv` は、実機の BOR のしきい値より十分上にあることが条件（実測、§10.2）。
- Premium の対応: 120 F は §3.2 の下降しきい値で CR2032 に切り替わる。CR2032 が終端に達すると、ハードの last-gasp が動く方針。FW の graceful shutdown とともに実装・実測が必要。
- **restart chatter の対策**: 低電圧で shutdown したあと、電圧が少し戻っただけでは起動しない。十分高い restart しきい値まで充電してから起動する（起動電流で電圧が再び落ちる reset ループを防ぐ）。Standard は起動の門（§4.2）、Premium は §3.2 の上昇しきい値と coin 側の起動条件を確認する。
- 起動時は、reset reason を読み、RTC を確認し、電源 source と電圧を確認してから NAND の復旧へ進む（手順の全体は [firmware.md](./firmware.md)、NAND の復旧は [storage.md](./storage.md)）。
- reset reason は RTC の timestamp と一緒に小さな persistent event log に残し、スマホにも通知する（§8）。

## §7 エネルギー収支

### §7.1 規約

- 電力は **rail 側**（1.8 V rail の負荷）と **cap 側 / 電池側**を区別する。変換損失を二重に計上しない。
- 全負荷を rail 側で測った場合、Premium の簡易モデルは `P_CAP = P_LOAD / 0.90 + 0.037 mW`（cap 漏れ 36 µW + BQ25505 などの固定 約 1 µW）。部品別の値を足す場合、**既に cap 側へ換算済みの NAND の mJ/MB は再度 0.90 で割らない**（[storage.md §4.1](./storage.md)）。
- η_1V8 = 0.90 と η_harvester = 0.85 は**仮置き**。0.1〜1 mW 級の負荷での TPS62840 の効率と、実 Solar 動作点での BQ25505 の効率を実測する。
- I²S を動かす割合 `a`、保存する割合 `p`、実際に BLE で送る量を別に測る。連続受信して FW で音声を捨てる場合は `a = 1` のままで、マイク・clock・DMA の電力を `p` 倍にしない。未同期中も録音の負荷は続き、再同期時は backlog の読出し・転送が加わる。

### §7.2 固定の床とデータ系の項

| 項目 | 値 | 確度 |
|---|---|---|
| T5848 AAD Analog | 20 µA → 1.8 V で 36 µW（cap 側で約 40 µW） | データシート Rev 1.1 の typ。保証上限ではない |
| T5848 低電力 I²S | 120 µA → 216 µW。AAD Analog との差は +180 µW | 同上。AAD D2 は 110 µA typ、D1 は 137 µA typ |
| RV-3032-C7 | 搭載する場合のみ約 0.3 µW @ 1.8 V を仮置き | 現行基板は DNP。GRTC の床と二重計上しない |
| BQ25505 | Iq 325 nA typ（3 V 級の VSTOR で約 1 µW） | cap 側の固定分 |
| TPS62840 | Iq 60 nA typ | 入力側。実測効率に含めた場合は再加算しない |
| HSL の漏れ | 12 µA × 3 V ≈ 36 µW を暫定の固定損失とする。12 µA は 25 °C・72 時間充電保持後の仕様で、全電圧・全温度・経年を包含する上限ではない | 仮置き |
| nRF54L15 の床 | System ON IDLE + GRTC + 256 KB RAM = 2.9 µA は 3 V・25 °C・DC/DC の typ。1.8 V に掛けた 5.2 µW は移植の仮置き | BC15M の 1.8 V 実機で測る |
| nRF の I2S20 + 間引き + codec + memory copy | 従来の 65 µW は未検証。ADPCM の実測は 85.5 cycle/sample で、約 42 µW は別条件の CoreMark 由来 pJ/cycle からの推定 | HFXO・I2S・DMA・FIR を含む連続受信の実電力を測る。現 FW は AAD 未接続 |
| NAND の書込み + 読出し + 消去 | 約 15 mJ/MB は **cap 側**。書込＋消去 9.9、読出し 5.1 mJ/MB（32 kbps 常時保存＋同量同期で約 60 µW） | typ と仮定を混ぜた旧モデル。最終型番・geometry・metadata 込みで再計測 |
| BLE 同期 | 約 87 mJ/MB は 3 V・約 100 kB/s・平均 2.91 mA の別条件からの仮説。1.8 V rail の測定値ではない | 再送・再接続・スマホ側の速度・backlog を含む J/実受信 byte で測る |

- データ系の電力はほぼ BLE で決まる（BLE は NAND の約 6 倍）。32 kbps = 345.6 MB/日 × 87 mJ/MB ÷ 86,400 s ≒ 348 µW。
- I²S 受信 + 間引き + codec + memory copy + BLE burst の平均エネルギーは、データシートの値だけでは出ない。**実機での実測が必須**。

### §7.3 部品別モデルの扱い（32 kbps）

旧表の 270〜810 µW は現行 I²S FW の予測として使わない。受信 duty と保存 duty の混同、NAND の変換損失の二重計上、根拠不明の BLE 約 6 µW の差分があった。

録音中の MCU 増分 `A` と BLE の測定境界が未確定な間は、§7.4 の **基板全体の rail 電力別の感度表**を使い、部品 typ の足し算から成立済みと結論しない。NAND をゲートできない場合の standby、逆流保護、ADC、source 変換回路の損失も追加する。

### §7.4 Premium の無日照運転（確定は式と目標、数値は仮置き）

```text
E_mWh    = C × (Vhigh² − Vlow²) / 7.2
P_CAP_mW = P_LOAD_mW / 0.90 + 0.037
runtime_h = E_mWh / P_CAP_mW
```

cap 側の合計から計算する場合は `E / P_CAP` を使い、効率と固定損失を再加算しない。

今回の推奨評価候補（§3.3）: 公称容量 120 F、OV 3.6729416 V、下降 2.6063450 V、**111.624 mWh**。従来値との比較も再現ツールに残す。

| rail 側負荷 | cap 側予算 | 無日照運転 |
|---:|---:|---:|
| 0.3 mW | 0.370 mW | 301.4 h |
| 0.5 mW | 0.593 mW | 188.4 h |
| 0.8 mW | 0.926 mW | 120.6 h |
| 1.0 mW | 1.148 mW | 97.2 h |
| 3.0 mW | 3.370 mW | 33.1 h |
| 5.0 mW | 5.593 mW | 20.0 h |
| 10.0 mW | 11.148 mW | 10.0 h |
| 20.0 mW | 22.259 mW | 5.0 h |

容量の下振れ・劣化・しきい値の誤差を重ねた設計 corner: HSL の初期容量は ±20 %、メーカーの寿命試験（70 °C・3.8 V・1000 時間）で容量変化 ≤ 30 %、AC ESR ≤ 初期最大値の 200 %。保守的に `C = 120 × 0.8 × 0.7 = 67.2 F` を置く（使用年数を保証する劣化モデルではない）。

| 条件 | C | 充電停止 / 切替 | 使用可能 | rail 0.5 mW | rail 0.8 mW | 72 h を満たす rail 側上限 |
|---|---:|---|---:|---:|---:|---:|
| 従来抵抗の公称 | 120 F | 3.672942 / 2.509700 V | 119.865 mWh | 202.3 h | 129.5 h | 1.465 mW |
| 従来抵抗の保守 corner | 67.2 F | 3.599483 / 2.559894 V | 59.763 mWh | 100.9 h | 64.6 h | **0.714 mW** |
| 今回の候補・公称 | 120 F | 3.672942 / 2.606345 V | 111.624 mWh | 188.4 h | 120.6 h | 1.362 mW |
| 今回の候補・保守 corner | 67.2 F | 3.599483 / 2.658472 V | 54.962 mWh | 92.8 h | 59.4 h | **0.654 mW** |

- corner は OV の **実公称値 × 0.98** と下降しきい値の **実公称値 × 1.02** を使う。目標 3.70／2.50 V に誤差を掛けてから流用しない。
- **rail 側 0.8 mW 未満というだけでは、無日照 72 時間を保証しない。** 今回の候補では 0.5 mW で約 92.8 h、0.8 mW で約 59.4 h。これも η = 0.90・固定 37 µW などの仮定の下での計算であり、温度を含む実機最悪値の保証ではない。
- これらは、温度による容量・ESR・漏れ、実効率、BLE・NAND のピークによる rail の落ち込みでの早期切替、実際の初期充電状態を含まない。無日照試験では開始電圧を記録し、想定運用でその状態まで充電できるかも確認する。
- 目標（Premium）:
  - 平均電力の最終目標は rail 側 0.3〜0.5 mW。
  - Phase 1 以降のスクリーニングは rail 側 0.8 mW 未満（3 日の保証とは別）。
  - 最終の電源 Go/No-Go は、**CR2032 を使わず無日照 72 時間を守れること**（容量下振れ・劣化・温度・しきい値誤差・実測の負荷と効率を含む）と、想定する屋外活動で消費分を回収できることを別々に確認する。机上の公称日数や 1 回の平均電力の測定だけで Go としない。
  - 0.5 mW を超えた場合も評価は続けるが、3 日周期の運用の可否は最悪条件のエネルギー収支で判断する。

### §7.5 Premium の Solar 収支（仮置き）

- 前面に実発電する Solar を必ず置く。従来の φ36 mm / 2 V / 40 mA / 80 mW は **寸法・電力の目安であり、メーカー型番・Voc・納期は未固定**。選定と外観を同時に確認する。
- BQ25505 は boost 型。panel の MPP 電圧だけでなく、低温・強照射時の Voc、VSTOR との大小、入力定格、過充電停止時の挙動を確認する。USB 5 V や 5〜6 V panel をそのまま代替入力にしない。
- `P_CAP_IN = 0.85 × P_SOLAR_REAL`、`P_NET = 0.85 × P_SOLAR_REAL − P_CAP_LOAD`。

rail 側負荷 0.5 mW（cap 側約 0.593 mW）の 3 日分は 約 42.7 mWh。この分を屋外で回収する時間（外にいる間ずっとその平均出力が得られた場合）:

| panel 端子の実効平均出力 | harvester 後 | 負荷を引いた充電電力 | 3 日分の回収時間 |
|---:|---:|---:|---:|
| 5 mW | 4.25 mW | 3.66 mW | 約 11.7 h |
| 10 mW | 8.5 mW | 7.91 mW | 約 5.4 h |
| 20 mW | 17 mW | 16.41 mW | 約 2.6 h |

Solar の成否は回路効率よりも、**胸元で φ36 mm の panel から平均 5 / 10 / 20 mW のどこまで取れるか**で決まる（未測定）。冬季の日照低下と、温度による容量・ESR・漏れは別々に扱う。

### §7.6 Standard の電池寿命（仮置き）

モデル（電池側の平均電力）: `P = Σ（1.8 V rail の各項）/ 0.9 + 自己放電`。32 kbps = 4,000 B/s、保存割合 p。再現ツールは `tools/standard_power.py`。

| 項 | 式（rail 側 µW） | 出典・確度 |
|---|---|---|
| mic | 36 (1 − p) + 216 p | T5848 データシート |
| MCU の音声処理 | 65 p | 旧モデルの仮置き。ADPCM は cycle 実測から約 42 µW と代理換算。I²S を連続受信する場合はこの p 比例では扱わない |
| MCU の床 | 5.2 | Nordic PS |
| NAND の書込み + 読出し | cap 側の約 15 nJ/B を rail 側へ戻すなら 13.5 nJ/B × 4,000 B/s × p | 仮置き。待機（DPD の有無と電流）は未確認（#27）。η = .9 を使うときの境界変換 |
| BLE | 87 mJ/MB × 4 kB/s × p = 348 p | 設計仮説（60〜100） |
| RTC | 0.3 | RV-3032 |
| 降圧の Iq | 0.3 | TPS62840 |
| 自己放電 | 1 %/年 | 仮置き |

- `tools/standard_power.py` の既定値は旧 NOR モデルのまま。NAND の境界を上表へ合わせて比較する場合は `--nor 13.5` が必要だが、待機・I²S・AAD 未実測のため最終寿命には使わない。下の寿命・p 上限も旧概算として残す。Premium の再計算には [BQ25505 の再現ツール](../../tools/bq25505_resistors.py) と §7.4 を使う。
- 容量: Jauch CR2477J 1000 mAh、Renata CR2477N 950 mAh。平均電圧 2.9 V は Jauch の放電曲線（20 °C、15 kΩ〜4.7 kΩ）から読んだ粗い値で、2.0 V 終端なら 1000 mAh で 2.9 Wh、950 mAh で 2.76 Wh。UV 段 2（2.40 V）までしか使えない場合は容量の 95 %（仮置き）。
- 32 kbps、NAND の書込み 約 15 nJ/B のモデルの電池側の平均電力は、p = 0.25 / 0.4 / 0.6 / 1.0 で 230 / 339 / 484 / 774 µW。p = 0.4 で 2.9 Wh なら約 356 日。
- **365 日を満たす p の上限（32 kbps、BLE 87 mJ/MB、950〜1000 mAh・終端 2.0 V）は 0.36〜0.38。** BLE の mJ/MB は p の上限にほとんど効かない（±0.02）。効くのはバイト数。
- ストレージの待機が DPD の無い 10〜20 µA なら、電池側で +18〜36 µW になり、上の上限は下がる。
- 目標（約 1 年）は、NAND 前提で成り立つ条件（ビットレート、p）を上のモデルで出し直して確定する（方針。#17）。

## §8 残量テレメトリ

スマホ接続時に、電源の状態が見えるようにする。

### §8.1 Premium（cap 残量）

- **測定**: nRF54L15 の SAADC（one-shot、内部 0.9 V reference、oversampling が使える。最大 8 入力）。Vcap の最大は 3.8 V なので、抵抗分圧で ADC の入力範囲に合わせる。分圧器は測定時だけ有効にする（§5.4）。1 MΩ 級以上、測定前に数 ms settling、one-shot、必要なら oversampling、測定後に切断。実基板で 2 点以上の校正を行う。出典: https://docs.nordicsemi.com/r/bundle/ps_nrf54l15/page/saadc.html
- **SOC は energy 基準**（電圧に線形な % 表示は禁止）:

```text
SOC_energy = (Vcap² − Vmin²) / (Vmax² − Vmin²)    0〜100 % に clamp
Vmin = 使用する下降しきい値の実測値（従来公称 2.5097 V、今回の候補 2.606345 V）
Vmax = VBAT_OV の公称（3.673 V。実測で校正できたらその値）
```

- **稼働時間**: `E_remaining = ½ C (Vcap² − Vmin²)`、`T_remaining = E_remaining / P_avg`。P_avg は cap 側の負荷・損失を含む値とし、rail 側の測定値は変換損失・漏れを加えてから使う。FW が過去数時間〜数日の実測平均を EMA で持つ。C は公称値と実測 / 保守的推定値を区別する。容量の下振れや劣化は % に現れないので、稼働時間の計算に別途反映する。
- **Solar の充放電**: 専用の current sensor は付けない（損失、静止電流、BOM、面積の理由）。cap 電圧の時間変化から net energy flow を推定する（`P_net ≒ ΔEcap / Δt`）。これは負荷を差し引いた net 値で、panel の gross 出力ではない。数十秒〜数分間隔の `dV/dt` で charging / discharging / stable を判定し、net mW の推定を出す。

### §8.2 Standard（電池）

電池電圧（§4.6）と、電源状態（NORMAL / LOW / CRITICAL / TERMINAL。§4.2）を出す。電池の残量 % は、放電曲線の実測（§10.2）のあとに定義する（未定）。

### §8.3 BLE で送る項目（意味。バイト列は [formats.md](./formats.md)）

| 項目 | SKU | 内容 |
|---|---|---|
| `cap_voltage_mV` | Premium | cap 電圧 |
| `cap_energy_percent_x100` | Premium | energy 基準の残量（%×100） |
| `estimated_runtime_minutes` | Premium | 推定稼働時間 |
| `source_state` | Premium | CAP / COIN（`VBAT_OK` から） |
| `solar_state` | Premium | charging / discharging / stable、net mW 推定 |
| `emergency_cell_state` | Premium | CR2032 の状態（使用中か、`v_coin_mV` など） |
| `last_emergency_entry_timestamp` | Premium | CR2032 運転に入った最後の時刻 |
| `battery_mv`、`power_state` | Standard | 電池電圧と状態機械の段階 |
| `reset_reason` | 共通 | 直近の reset の理由（§6） |

表示の例（Premium）: Energy reserve 73 %、Cap voltage 3.31 V、Estimated runtime 6.8 days、Power source capacitor、Solar charging、Backup cell unused。

Standard は、TERMINAL の前（LOW 段）に、電池残量の低下をスマホへ通知できるようにする（終端表示の補助。[terminal-indicator.md](./terminal-indicator.md) §12）。

## §9 OTA のエネルギーゲート

ファームウェア更新中の電源喪失による brick の確率を下げる。OTA の手順と MCUboot は [firmware.md](./firmware.md) が持つ。

### §9.1 条件

- **Standard**: 電池の段階が **NORMAL（2.60 V 以上、仮置き）のときだけ** OTA を許す（確定。閾値の数値は §4.2 の実測で動く）。
- **Premium**: 次のどちらかなら OTA を拒否する。
  - 電源 source が CR2032 のみ（CR2032 運転中は原則禁止）。
  - cap の残エネルギーが閾値未満。
- Solar の入力を必須条件にはしない。

### §9.2 Premium の閾値（方針。数値は実測で決める）

固定の電圧は先に決めない。実機で OTA 1 回に必要なエネルギーを測り、次のようにする。

```text
E_required = E_upload + E_verify + E_swap + E_boot + margin     margin は最低 2〜3 倍
E_cap = ½ C (Vcap² − Vmin²)                                       C は保守的な値
```

OTA の許可は、電圧ではなく energy の閾値として実装する（dynamic threshold）。

### §9.3 update 中の挙動（方針）

- 録音は一時停止してよい。NAND の batch write を終えて clean な状態にする。update 中の録音継続は初号機では優先しない。
- BLE の update 接続を優先する。cap の電圧を途中で監視し、brownout の近くまで来たら新規の update 開始を禁止する。
- image の swap 中のソフトウェアの判断に頼りすぎない。bootloader 側の recovery（MCUboot の primary / secondary slot の test / revert）を前提にする。

### §9.4 スマホへの表示

理由を明示する。例: 「Firmware update unavailable. Stored energy is too low. Cap: 18 %. Power source: backup cell. Move the pendant into bright light and retry later.」（Standard は電池電圧が低いことを示す。）

## §10 検証項目

成立の条件。実施の順序と使う基板は plan/prototype.md が持つ。

### §10.1 Premium

1. cap の実充電上限（公称 3.672942 V）からの放電で、§3.3 の選択した下降設定で CR2032 に切り替わる。今回の候補では通常端子電圧 2.5 V 以上を確認し、全試験で測定誤差を含め 2.2 V より手前で止める。Solar あり / なしの両方。
2. CR2032 使用中に実 Solar で cap が充電され、選択した上昇設定で cap に戻る。USB・外部給電を本番の動作証拠に使わない。
3. しきい値付近で source の chatter が起きない。BLE・NAND のパルスで誤切替しない。
4. 電池模擬源で VOR > coin／coin > VSTOR の切替を先に確認。定常逆漏れ、一次枝への逆電流と COIN→CAP 復帰時の secondary／HSL 電流について、peak・時間・積算電荷・端子過渡を記録する。両方向の保護・許容条件を決めてから実 coin で確認する。
5. source 切替時に 1.8 V rail が brownout しない（break-before-make 中の落ち込み）。
6. BQ25505 が止まっているときの mux の既定側。
7. CR2032 運転中の、電池電流（peak / 平均）、buck 入力の最低電圧、MCU・NAND 端子の最低電圧、回復時間、連続録音時の RAM 占有率（新品 / 消耗・低温の CR2032、1.8 / 1.9 V rail、CAP 切替直後）。ページ分散・metadata・erase・進行中の program での source 切替を含む。
8. VBAT_OK の実しきい値（§3.3 の採用設定。製品評価は 10 台程度、0 / 25 / 40 °C）、OV の実値と HSL の端子電圧（3.8 V 以下）、分圧器の実リーク、BQ25505 の Iq、低温・高温での VBIAS のずれ。1.8 V 側 source 入力の電圧と rail OFF 時の back-power も確認する。
9. 120 F の漏れ（full charge 後に十分 soak）。CR2032 運転中・保管中の cap の電圧低下速度（2.2 V に達する前に試験を止める）。
10. app 表示の cap SOC と、実測のエネルギーの整合。Vcap から推定したエネルギーと実放電時間の一致。
11. 無日照 72 時間の試験（開始電圧を記録）。

### §10.2 Standard

1. CR2477 の放電曲線（実負荷、新品〜終端、25 °C）→ 各段階の閾値、約 1 年の見積り。
2. BLE TX・NAND の program・erase 時の、電池端子と rail の電圧降下（新品・中盤・終端、低温）→ LOW / CRITICAL の閾値、reservoir の容量。CR2477 の内部抵抗（最重要。新品 / 半分消費 / 終端近傍で、10 mA / 20 mA のパルスを 1 ms / 10 ms / 200 ms かけ、ms 級と 200 ms 級の R が同じかを見る）。
3. TPS62840 の入力電圧を下げたときの 1.8 V rail（dropout）。負荷は録音 + BLE のピーク。TPS62840 の RDSON（VIN 2.0 V 付近）。
4. BOR の閾値（実機）→ `term_mv` が BOR より十分上にあること。
5. 電池電圧の測定経路（分圧と ADC）と、測定自体の電流。
6. 電池交換直後の起動（新品・半分使った電池）と、restart しきい値付近の chatter → `restart_mv`、起動の門。
7. 複数メーカーの CR2477 の比較（Renata、Panasonic の資料は未取得）。

### §10.3 共通

1. rail ごとの電力: MAIN only / RTC only / Mic AAD / Mic LP I²S / NAND off / NAND DPD / NAND true gated / NAND batch write / BLE sync / 電池または CR2032 backup mode。**datasheet typ の足し算ではなく、完成基板の実測平均電力で採否を決める。**
2. TPS22917 の QOD 接続・未接続、CT による突入・復帰の比較。GPIO back-power の試験（必須）。
3. watchdog hang、NAND の timeout injection、BLE task hang、電圧をゆっくり下げる brownout 試験、restart しきい値付近の chatter、reset reason が正しく記録されるか（[firmware.md](./firmware.md)）。

## 未確定

| 項目 | 状態 |
|---|---|
| Standard の電圧しきい値（NORMAL / LOW / CRITICAL / TERMINAL、起動の門）、電池電圧の測定経路の実装 | 仮置き。#12 |
| Standard の終端表示（CR2477 用の last-gasp 回路）の電圧と構成 | #12（[terminal-indicator.md](./terminal-indicator.md) §13） |
| CR2477 の内部抵抗、reservoir の容量（220 µF 級を第一候補）、NAND の program / erase での降下、書込み・消去の電圧ゲートの閾値 | #18 |
| Standard の電池寿命（約 1 年）を、NAND 前提のビットレートと p で出し直す。NAND の待機（DPD）を入れる | #17、#27 |
| 小容量 NAND の DPD の有無と電流、NAND ゲートの要否 | #27 |
| OTA の energy 閾値（E_upload などの実測） | #11 |
| BQ25505 の抵抗値の実測（VBAT_OK、OV、VBIAS の温度変化）、無日照 72 時間の試験 | #25 |
| Premium の電源 IC | 今回の試作は BQ25505＋TPS62842DGRR 評価候補 |
| Premium の OK 設定・一次電池逆流保護・source 信号変換 | 約 2.606／2.802 V、LM66100 等、SN74LVC1G17 は評価候補。回路・実測の採否は未決 |
| Premium の電源パスの実機検証（PMOS の品番と向き、逆流、切替時の rail 波形、CR2032 運転中の cap 充電、mux の既定側、UV 固定値と HSL の下限の整合） | 新規 issue 候補: Premium 電源パス（BQ25505 mux）の実機検証 |
| CR2032 運転中の bulk（位置・容量・ESR）、ページ分散の間隔、電池電流の制限の要否 | 新規 issue 候補: Premium の CR2032 運転中の bulk と電池電流 |
| system rail を 1.8 V にするか 1.9 V にするか（POR 下限と NAND 上限の実測） | 新規 issue 候補: system rail 電圧（1.8 V / 1.9 V）の判断 |
| 胸元での Solar の実発電量（panel 端子で 5 / 10 / 20 mW のどこか）、BQ25505 の実効率 | 新規 issue 候補: Premium の Solar 実発電と harvester 効率の実測（Phase 2） |
| HSL の保管期限（2.5 → 2.2 V の余裕）と点検・再充電の条件 | 新規 issue 候補: HSL の放置試験と保管期限 |
