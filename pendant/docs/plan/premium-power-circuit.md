# Premium デモ電源: 回路ドラフトと小型部品の選定

2026-10-28 更新。11 月デモの**評価候補**と、その成立に必要な確認を記録する。製品の無日照 72 時間目標や 120 F・CR2032 の既存基準を、ここで達成・変更したことにはしない。記録・BLE・NAND の実負荷も Solar の設置条件も未測定。今回のデモは公称 φ36 mm パネルを維持し、基板・電池の小型化を優先しない。非常用電池は **CR2032 × 1** を第一評価候補、電池運転 **24 時間**を目標とする。SMD は基板表面にまとめ、電池ホルダーと Vina 蓄電器を裏面に並べ、各スルーホール足を表面で手はんだ付けする。裏面の裸 TP も許容する。録音・保存・BLE の動作を実機負荷で確認し、勝手に省略した前提で 24 時間を達成したとは扱わない。

**秋葉原優先の追加検討:** 店頭調達では Vina 30 F（最大径 9 mm）を第一評価候補、薄さでは Eaton HSH 22 F（最大径 6.8 mm）を比較候補とする。部品の電圧下限・リードと footprint、実配置・実負荷・安全条件は別に確認する。[店頭・通販の調達状況](./premium-akihabara-parts.md)、[薄型案の構造と断面](../spec/mechanical.md#76a-11-月デモの薄型-mjf-案2026-10-27)。

[KiCad 回路ドラフト](../../hw/premium/premium_power.kicad_sch) には、電源に加えて BC15M／NAND／マイク／button／SWD と測定点を追加した。**一次電池・ハイブリッド蓄電器（Vina VPC と Eaton HSH/HSL を含む）の実物を接続できる回路ではない**。[評価用裸基板の発注データ](../../hw/premium/fab/README.md) は準備したが、D1 と R6 は実セル未承認の評価候補、bulk・端子・ADC・実 privacy スイッチ・reverse current・source 切替が未確定。[回路 README](../../hw/premium/README.md) に検証結果と制約を記録する。外部 USB 給電・充電は設けない。

### PCB の確定・製造に向けた設計ゲート（2026-10-27 更新）

1. **一次電池:** BAT41ZFILM と 1.2 kΩ は模擬源による評価候補。採用する CR2032 の逆充電・逆接・故障条件を確認し、D1 短絡・電池逆接を含む端子電圧、電流と累積逆電荷を評価する。C8 と VOR の容量は、BQ の VBAT_PRI 2 V 下限と buck 入力、NAND 実負荷・回復時間を含めて選ぶ。
2. **蓄電器・Solar:** Vina は 2.5 V 動作下限、Eaton HSH は使用条件に記載の下限をそれぞれ扱う。BQ の UV だけでは蓄電器の保管中の過放電を防げない。実パネルの Voc・I–V・厚さを測り、接続・保護・組立後の取扱いを決める。
3. **入出力と操作:** HSL／COIN の ADC は電源断時の逆給電対策を含めて未設計。VBAT_OK の level shift は VSTOR 無し／coin 使用時も評価。SW101 は solder jumper で、実操作できる privacy switch は未選定。
4. **実装:** BQ と CSD のパッドを評価用として追加した。CSD の F.Paste は未設定であり、そのままのステンシルには開口ができない。U2 の KiCad 汎用 DGR footprint は外周パッド幅 0.50 mm で、[TI の land 例](https://www.ti.com/lit/ds/symlink/tps62840.pdf)の 0.30 mm と異なる。EP の mask・paste も同一ではないため組立方法を確認する。L2 のランドは 2026-11-06 に Murata の推奨寸法へ変更した。NAND EP の扱い、MLCC の実効容量、各端子／holder、実装工程を確定する。

薄型 privacy switch の比較候補 [Alps SSAJ120100](https://tech.alpsalpine.com/j/products/detail/SSAJ120100/) は秋月掲載があり、約 0.7 mm 高の小型品。ただしメーカー最低定格 3 V／50 µA に対してマイクの AAD は約 1.8 V／20 µA なので、現時点ではその条件の接触信頼性を保証できない。共通端子と片側端子をマイク rail に直列とする方式は検討できるが、接点、操作力、筐体、OFF 中の信号からの回り込みを確かめてから採用する。SW101 の仮 jumper はそのまま。

KiCad 10 の ERC は LM66100 の未使用 ST をメーカー指示どおり GND に接続した箇所に 2 errors、0 warnings。電源・共通コアの主要接続は KiCad の netlist から別途チェックしたが、**実機の切替・漏れ・過渡・保護は未確認**。[直径 39 mm の PCB ドラフト](../../hw/premium/premium_power.kicad_pcb) は SMD を F 面、CH005 と Vina の仮スルーホール footprint を B 面に配置している。2026-11-06 の GND ビア追加後は未接続 0、PCB DRC エラー 0。ユーザーは同日に両部品の現物確認済みと回答した。警告、蓄電器経路と昇圧ループ、組立・パネル機構の確認は残る。詳細と検査結果は [PCB ドラフト記録](../../hw/premium/PCB_DRAFT.md) を参照。

## 1. デモ向けの第一評価候補

| 機能 | 候補と理由 | 未確定事項 |
|---|---|---|
| Solar | **uxcell φ36 mm の丸型、1 枚**。[Amazon の 2 枚セット](https://www.amazon.co.jp/dp/B0H5MJDDX8) は 2 V・40 mA・0.08 W の販売表示。測定条件、Voc、MPP と厚さは不明 | 現物の厚さ・端子と、実照度・角度・部分遮光での I–V、低温 Voc、筐体・スイッチの機構 |
| harvester | BQ25505RGR、22 µH。`VOC_SAMP = VSTOR` の内部 80% 設定は初回評価値 | 80% が uxcell の MPP に合うか、低温時 Voc、cold start、過電圧時と実 Solar での挙動 |
| 蓄電 | 秋葉原優先の評価候補 **Vina VEL08203R8306G、30 F**（最大 φ9.0 × 21.5 mm）。薄さ重視の比較候補は Eaton HSH0622-3R8226-R、22 F（最大 φ6.8 × 24 mm） | 横置き時のリードを含む平面配置、必要時間、各メーカーの使用下限、保護と過渡の確認 |
| 非常用 | **CR2032 × 1、交換式、24 時間を目標**。Panasonic の参考値は 3 V、225 mAh 公称、φ20 × 3.2 mm。秋月取扱いの Golden Power は別メーカーなので容量を混同しない | 実際の電池・ホルダー、保護素子の損失、動作負荷中の電圧と bulk、24 時間の成立は未確認 |
| 電源切替 | BQ25505 の gate driver、CSD75208W1015 × 2、LM66100 × 2 の評価回路 | 両方向の逆電荷、起動、遷移時の保持、全故障経路。LM66100 が一次電池の独立した保護 diode / resistor に代わるとは扱わない |
| 1.8 V | TPS62842DGRR、2.2 µH。VSET を GND に直結し、設定抵抗を省く。小型 DLC 品より大きいが入手性を優先 | 1.8 V 精度・過渡、実効容量、変換効率、実装と到着日 |
| source 状態 | SN74LVC1G17DBVR、100 nF。1.8 V rail で給電し BQ の `VBAT_OK` を 1.8 V 信号にする | rail OFF 時、信号入力と出力の漏れ・back-power を実測 |

出典: [Panasonic CR2032](https://energy.panasonic.com/dam/master/pdf/en/datasheet/lithium/CR2032_Datasheet_EN_240701.pdf)、[ANYSOLAR Rev. Jun. 2026](https://anysolar.biz/KXOB121K04TF-datasheet/)、[BQ25505 Rev. F](https://www.ti.com/lit/ds/symlink/bq25505.pdf)、[Eaton HSL](https://www.eaton.com/content/dam/eaton/products/electronic-components/resources/data-sheet/eaton-supercapacitor-hybrid-cylindrical-cells-data-sheet.pdf)、[CR1632](https://energy.panasonic.com/dam/master/pdf/en/datasheet/lithium/CR1632_Datasheet_EN.pdf)、[CR2032](https://energy.panasonic.com/dam/master/pdf/en/datasheet/lithium/CR2032_Datasheet_EN.pdf)、[TPS62840 Rev. D](https://www.ti.com/lit/ds/symlink/tps62840.pdf)、[SN74LVC1G17](https://www.ti.com/lit/ds/symlink/sn74lvc1g17.pdf)。

HSH の詳細は [Eaton HSH データシート](https://www.eaton.com/content/dam/eaton/products/electronic-components/resources/data-sheet/eaton-hsh-supercapacitors-hybrid-high-density-cells-data-sheet-elx1491-en.pdf)。22 F は ±20%、最大 DC ESR 1.0 Ω、漏れ最大 3.5 µA（25 °C・72 h 保持後）、本体 φ6.3 ±0.5 × 22 ±2 mm。25 F の HSH0814 は最大 φ8.5 × 15.5 mm、±20%、DC ESR 0.9 Ω、同条件の漏れ最大 2.5 µA。30 F HSL の最大径 10.5 mm に対し、22 F HSH の径は 3.7 mm 小さい一方、最大長は 6 mm 大きい。いずれも電圧定格は 3.8 V、minimum rated 2.5 V、minimum allowed 2.2 V。実回路で ESR、漏れ、リード、固定と端子を確認する。

22 F の利用可能エネルギーは、デモ回路ドラフトの公称値 3.67294 V から 2.78524 V、公称容量、無損失・無漏れを仮定した理想値で **約 17.52 mWh**。蓄電器側一定 1 mW なら理想約 17.5 h、5 mW なら約 3.5 h であり、変換・漏れ・容量公差・パルスで実際は短くなる。これは実機の持続時間ではなく、Solar 充電条件とデモ中の rail 消費電力を測って判断する。製品の 72 h 目標と区別する。

2026-10-27 の [DigiKey JP 22 F](https://www.digikey.jp/ja/products/detail/eaton-electronics-division/HSH0622-3R8226-R/26262349) は 649 個、1 個税別 539 円、[25 F](https://www.digikey.jp/ja/products/detail/eaton-electronics-division/HSH0814-3R8256-R/26262338) は 1,065 個、1 個税別 605 円と表示。メーカー標準リードタイムは両方 34 週表示。表示は変わるため購入時に在庫・送料・到着日を再確認する。購入はしていない。

MPPT の 80% は外付け 2 抵抗を省ける**初回評価設定**であり、uxcell の MPP に一致する根拠はまだない。以前の四角い ANYSOLAR の `2.23 / 2.76 = 80.8%` は別製品の STC 典型値で、この円形パネルには適用しない。現物の Voc と I–V を照明・角度ごとに測る。[BQ25505 §7.3.1](https://www.ti.com/lit/ds/symlink/bq25505.pdf)。

MOSFET を各枝 1 個にして独立した dual パッケージへ統合すれば部品を減らせる可能性はある。ただし OFF 時の body diode、BQ の `VBAT_PRI` への逆流、gate 電圧と起動時の挙動を満たす実型番・回路は未確認。CSD75208 の 2 素子は内部 source 共通なので、この独立 2 素子としては使えない。現ドラフトの LM66100 × 2 と back-to-back pair × 2 は成立確認用の仮構成で、最小部品数の確定ではない。[CSD75208](https://www.ti.com/lit/gpn/csd75208w1015)。

旧比較品の長方形 ANYSOLAR は対角約 33.97 mm。今回の uxcell は φ36 mm 表示の円形で、厚さ・寸法公差・端子を含む実寸は未確認。円形の透明ボタンの下に収める案は外観・CAD・現物を確認し、セルを押し曲げず、荷重を受ける透明 carrier と端子の応力逃がしを設ける。

**旧比較品の調達メモ:** 2026-10-27 に [DigiKey JP](https://www.digikey.jp/ja/products/detail/anysolar-ltd/KXOB121K04TF-TR/13999160) は KXOB121K04TF-TR を 52 個、1 個税別 1,045 円と表示。現在の前面候補は uxcell である。

BQ の推奨条件は `VBAT_OV > VIN_DC`。旧 ANYSOLAR の −20 °C における Voc 推定約 3.07 V は uxcell の値ではない。uxcell の Voc と低温特性は未確認。VIN_DC > VSTOR かつ VSTOR > OV の場合、BQ は VIN_DC を低抵抗で GND に引き込むため、その条件が起こり得る試験ではメーカーは入力源インピーダンス >20 Ω を要求している。電流制限を設けただけの低インピーダンス電源をそのまま接続しない。[BQ25505 §6.3・§7.3.3](https://www.ti.com/lit/ds/symlink/bq25505.pdf)。

### デモ回路の切替閾値（10/27 評価ドラフト）

Vina の 2.5 V 動作下限に対する余裕を広げるため、回路ドラフトの R4 を 5.62 MΩ から **6.34 MΩ** へ変更した。R3 = 787 kΩ、R5 = 4.87 MΩ、いずれも 0.1% のまま。6.34 MΩ は OV 側 R2 と共通化でき、抵抗種を増やさない。TI の式と VBIAS 1.21 V では、下降 **2.785236 V**、復帰 **2.980774 V**、抵抗合計 **11.997 MΩ**。0.1% 抵抗条件における TI の総合精度 ±2% は下降 2.7295〜2.8409 V、復帰 2.9212〜3.0404 V。両範囲の最小間隔は約 80 mV。[BQ25505 §6.5・§7.3.4](https://www.ti.com/lit/ds/symlink/bq25505.pdf)、[TI EVM guide](https://www.ti.com/lit/ug/sluuaa8a/sluuaa8a.pdf)。

下降側の静的下限と Vina 2.5 V の差は約 230 mV。ただし VSTOR 検知と蓄電器端子の差、ESR、負荷パルス、約 64 ms のサンプリング、切替遅れは含まない。放置中の自己放電も防げない。これは **実電池・蓄電器接続や PCB 製作の許可ではない**。旧 2.606345 V 案より、同一の 3.67294 V 上限から切替までに使える理想エネルギーは約 **14.4% 減**、電池への切替も早くなる。実波形とデモ時間で最終採否を決める。

## 2. 小型蓄電器と電池の影響

| HSL 型番 | 公称容量 | 最大外形 | DC ESR max @25 °C | HSL leakage max の測定条件付き値 |
|---|---:|---|---:|---:|
| HSL0814-3R8106-R | 10 F | φ8.5 × 15.5 mm | 1.5 Ω | 3 µA |
| HSL0820-3R8256-R | 25 F | φ8.5 × 22 mm | 0.65 Ω | 3.3 µA |
| HSL1016-3R8306-R | 30 F | φ10.5 × 18 mm | 0.55 Ω | 4 µA |
| HSL1020-3R8506-R | 50 F | φ10.5 × 22 mm | 0.45 Ω | 5 µA |
| HSL1225-3R8127-R | 120 F | φ12.9 × 27 mm | 0.20 Ω | 12 µA |

漏れは 25 °C で定格電圧に 72 時間保持した後の値であり、全温度・全期間の最大値ではない。Eaton は定格下限 2.5 V、許容下限 2.2 V、定格上限 3.8 V を示す。30 F の容量公差は Eaton 資料と販売ページの表記に食い違いがあり、確定前に照合する。寸法にはリード加工と固定の余裕を追加する。

同じ公称開始 3.67294 V・切替 2.78524 V で `E = ½C(V高²−V低²)` とし、蓄電側から一定 1 mW を取る理想比較では以下となる。

| 容量 | 理想エネルギー | 1 mW の理想時間 |
|---:|---:|---:|
| 10 F | 7.96 mWh | 8.0 h |
| 25 F | 19.91 mWh | 19.9 h |
| 30 F | 23.89 mWh | 23.9 h |
| 50 F | 39.81 mWh | 39.8 h |
| 120 F | 95.55 mWh | 95.5 h |

これは選択肢の規模比較であり、実動作時間ではない。電源変換・自己放電・温度・容量公差・電圧精度・実負荷・切替過渡は含まない。30 F は 120 F の公称エネルギーの 1/4 なので、製品の無日照 72 h 条件をデモ用候補へ引き継いだと扱わない。

CR1632 は CR2032 より電池本体の正面積が 64%、公称容量が約 62% で、厚さは同じ 3.2 mm。ホルダーの足跡、抜き差しの空間、使用時の容量とパルス電流は本体の比率だけから決められない。例として [MPD BU1632SM-JJ-GTR](https://www.digikey.jp/ja/products/detail/mpd-memory-protection-devices/BU1632SM-JJ-GTR/2079901) と [BU2032SM-JJ-GTR](https://www.memoryprotectiondevices.com/datasheets/BU2032SM-JJ-GTR/BU2032SM-JJ-GTR-datasheet.pdf) の公称外形矩形は約 27.7 × 18.4 mm と 31.7 × 22.4 mm で約 28% 減。製品の配置可能面積を保証する数値ではない。

2026-10-27 の [30 F 候補の DigiKey JP 表示](https://www.digikey.jp/ja/products/detail/eaton-electronics-division/HSL1016-3R8306-R/12179231) は在庫 1,197 個、1 個税別 702 円。メーカー標準リードタイムは 34 週表示。30 F の Eaton 公差表示は `0/+50%`、同販売ページは `±20%` で相違する。いずれも再確認し、現物納期を発注画面で確認する。

## 3. 一次電池の逆充電保護が最重要の未解決点

Panasonic の [一次電池の回路設計資料 p.12](https://industrial.panasonic.com/cdbs/www-data/pdf/AAA4000/AAA4000COL15.pdf) は、充電の可能性がある回路に低漏れ逆流防止 diode と直列保護抵抗を示す。BQ の `VBAT_PRI` sense を含むすべての分岐の手前に D1 と R6 を置いて検討する。LM66100 は応答時間・漏れ・故障があり、電池メーカーの独立保護を不要とする証拠ではない。

以下の CR1632 の計算は旧比較例で、現在の CR2032 採用値ではない。資料中の表は **2002 年時点**の CR1632 の異常充電電流 4 mA、CR2032 の 10 mA を示す。CR1632、D1 短絡、R の両端に最大 3.8 V がかかる単純仮定では `R > 3.8 / 0.004 = 950 Ω`。[RC0603FR-071K2L](https://www.yageogroup.com/component-documentation/download/specsheet/RC0603FR-071K2L) 1.2 kΩ ±1% は、温度係数を除く下限 1,188 Ω で 3.20 mA、抵抗消費 12.2 mW の**評価例**になる。最新の該当セル条件、最大故障電圧、抵抗の温度・パルス・故障、逆接続と全経路は未確定。4 mA は異常条件の比較値であり、通常の連続逆充電許可ではない。

同じ 1.2 kΩ では順方向も 0.1 mA で 0.12 V、0.5 mA で 0.60 V 落ち、さらに D1 と電池内部抵抗の損失が加わる。TPS の入力下限だけでなく、保護後の BQ `VBAT_PRI` に規定される 2 V 以上の動作条件も確認する。NAND／BLE のパルス電流を抵抗経由で直接供給できると考えず、D1・R6 **後**の bulk を検討する。逆流制限を bypass する位置には bulk を置かない。一次枝は 120 F／30 F HSL とは切り離されるため、それらを coin のパルス補助と見なさない。

例えば抵抗に使える電圧差を仮に 0.6 V とすれば、1.2 kΩ を介した平均補給は約 0.5 mA。C8 を仮に実効 1,000 µF としたとき抵抗による時定数だけで約 1.2 s となり、次の動作までの回復時間も必要になる。diode と電池の実際の電圧降下、負荷と低温条件を含まない計算例であり、1.2 kΩ／1,000 µF の採用根拠にはしない。COIN 中の録音・保存・BLE を含む実使用条件で電流と電圧を測り、24 時間を満たすか確認する。機能制限が必要と分かった場合は、デモに必要な動作を保てるか別途判断する。

さらに [F35UQA512M の資料](https://cdn.icstop.com/upload/pdfs/e7/de/e7decc6bb7e9f2384da286e27083f166.pdf) は erase 電流最大 25 mA、時間最大 10 ms を示す。この両最大が重なる保守的な仮定で、NAND 単体の 1.8 V 側に `25 mA × 10 ms × 1.8 V = 450 µJ` が必要。COIN からの補給をゼロ、C8 が 2.55→2.0 V で変換器へ供給、効率を仮に 85% 一定とすると、`C = 2 × 450 µJ / (0.85 × (2.55²−2.0²)) ≈ 423 µF` の**実効容量が NAND だけで必要な計算例**になる。1.8 V 側の容量、電池からの補給、電流波形、電池内部抵抗、ダイオード、MCU や効率の変化で結果は変わる。公称 100 µF を足すだけでは成立を示せない。実動作中の CAP→COIN 切替で既に発行した erase を完了できるかは、電源波形とデータの両方を確認する。

### 電圧測定の検討候補（回路図には未接続）

HSL と COIN ごとに TPS22917 で測定時だけ分圧器を有効にし、QOD で測定終了後の分圧器上端を落とす案を検討した。例えば 3.3 MΩ／820 kΩ、ADC 手前 100 kΩ、ADC 側 10 nF では 3.8 V 入力が公称約 0.756 V、抵抗による入力抵抗は約 757 kΩ。これは Nordic の SAADC 40 µs acquisition 時の 800 kΩ という条件との比較例で、誤差や ADC の動的負荷を含む保証ではない。**1V8 が急降下したとき ADC 側コンデンサの電荷が残る**ため、この案と 100 kΩ だけで MCU の電源断時の安全や誤動作防止を保証できない。保証された無電源時の入力特性・隔離を調べ、全電源状態を確認してから回路図へ接続する。[Nordic SAADC reference](https://docs.nordicsemi.com/r/bundle/ps_nrf54l15/page/saadc.html-padc_reference)、[TPS22917](https://www.ti.com/lit/ds/symlink/tps22917.pdf)。

低漏れ比較例の [Nexperia BAS116](https://assets.nexperia.com/documents/data-sheet/BAS116.pdf) は 25 °C、VR=75 V の試験で IR max 5 nA だが、同資料の VF max は 1 mA で 0.9 V。逆に [ST BAT54](https://www.st.com/resource/en/datasheet/bat54.pdf) は VF max 0.32 V @1 mA の一方で IR max 1 µA @30 V、25 °C。試験条件が異なり、どちらもこの回路での充電防止と電圧余裕を証明しない。実際の温度・逆電圧・電流・プリント基板表面を含めて選定する。

仮案の U3/U4 LM66100 は CE を自分の VOUT に接続する逆流検出方式。[TI §6.5・§8.3.2](https://www.ti.com/lit/ds/symlink/lm66100.pdf) の保証条件では ON には VOUT が VIN より最大 250 mV 低い状態が必要で、OFF 中も body diode を通る。小さい負荷では常に低抵抗で通ると仮定せず、保護 diode と抵抗の後に生じる追加降下、一次側起動と切替時の逆流電荷を測る。

同じ Panasonic 旧資料の生涯逆流電荷 3% を計算例に使うと、CR1632 の 140 mAh に対し 4.2 mAh。5 年間ずっと同じ逆電流が流れる仮定では平均約 96 nA になる。これは過渡電流の許容値ではない。LM66100 の最大逆漏れは測定条件で変化し、例えば VOUT−VIN ≤ 1 V、−40〜85 °C、CE 条件付きで 0.7 µA max。実回路の経路・時間・過渡を含めて、電池まで流れる電荷を測る。[TI LM66100 §6.5](https://www.ti.com/lit/ds/symlink/lm66100.pdf)。

bulk の例として、F35UQA512M の NAND 単独の最大値の積は program 25 mA × 0.75 ms、erase 25 mA × 10 ms。1.8 V でそれぞれ約 33.75 µJ／450 µJ。仮に bulk が 2.5→2.2 V の間にエネルギーを供給し、その間に入力からの補充がなく、変換損失も他の負荷もない場合、理想容量は約 48 µF／638 µF となる。実効容量・ESR・変換損失・MCU・無線・起動・電池の充電回復を含まないので採用値ではない。COIN の erase は予め CAP モードで済ませる運用も含めて検証する。

## 4. 小さい部品の候補と扱い

- BQ 用 22 µH: TI EVM 使用の **Coilcraft LPS4018-223MRC**。4 × 4 × 1.8 mm、メーカー表の Isat 0.74 A（10% L 低下）、Irms 0.65 A（20 °C 上昇）、DCR max 0.36 Ω。まず入手性とピーク電流を確認する。[Coilcraft](https://www.coilcraft.com/en-us/products/power/shielded-inductors/ferrite-drum/lps/lps4018/lps4018-223/)、[TI EVM](https://www.ti.com/lit/ug/sluuaa8a/sluuaa8a.pdf)。
- TPS 用 2.2 µH: TI DLC EVM 使用の **Murata DFE201612E-2R2M=P2**、2.0 × 1.6 × 最大 1.2 mm。[TI EVM](https://www.ti.com/lit/ug/sluuc18/sluuc18.pdf)、[Murata](https://www.murata.com/products/productdetail?partno=DFE201612E-2R2M%23)。
- BQ 入力と VSTOR の 4.7 µF は、指定条件での実効容量に注意する。例えば **YAGEO CC0805KRX5R7BB106** 10 µF、16 V、X5R、0805 を候補として DC bias・温度・公差の確認を行う。2026-10-27 の [DigiKey JP](https://www.digikey.jp/ja/products/detail/yageo/CC0805KRX5R7BB106/7164523) は在庫表示があるが到着日は要確認。[メーカー仕様](https://www.yageogroup.com/download/specsheet/CC0805KRX5R7BB106)。当初比較した Murata GRM21BR61A106KE19L は生産中止のため調達候補から外す。TPS の出力も実効 3 µF 以上など TI の条件を満たすことを検証し、公称値だけで 0402 を確定しない。
- PMOS・IC の小さい外形は基板面積や自宅実装性の保証ではない。パッド・配線・実装検査まで確認し、D1/R6 と bulk を含む実装点数を評価する。
- **footprint 照合の進捗:** [CSD75208W1015 の TI 図面 §7.1–7.2](https://www.ti.com/lit/gpn/csd75208w1015) で B1/B2 = 共通 source、C1/C2 = gate/drain 1、A2/A1 = gate/drain 2 を確認した。推奨ランドは 0.5 mm pitch・φ0.25 mm。KiCad 10 標準 `Texas_DSBGA-6_0.9x1.4mm_Layout2x3_P0.5mm` は pitch と並びは似るが銅 pad φ0.265 mm で、paste と本体寸法も同一ではない。BQ25505 の TI RGR 図面はランド例 0.6 × 0.24 mm・EP 2.05 × 2.05 mm、KiCad 標準 `QFN-20-1EP_3.5x3.5mm_P0.5mm_EP2x2mm` は 0.875 × 0.25 mm・EP 2.0 × 2.0 mm。標準 footprint の自動流用ではなく、両者に評価用 footprint を設定済み。Q1/Q2 の F.Paste は意図的に未設定で、いずれも製造先の要求・mask・paste・実装条件を照合する。

## 5. 基板化の前に閉じる検証

1. 最新セルの逆充電条件を確認し、D1・R6 の故障時電流と連続／過渡の逆流電荷を設計。実電池を使わず、電流制限・逆流を吸収できる模擬源で、source 挿入順と CAP↔COIN の切替を測る。
2. Solar simulator だけでなく uxcell の実 Solar を使い、明暗・傾きで VIN、VSTOR、HSL 相当端子、入力電力と充電エネルギーを測る。販売表示の 0.08 W は室内発電量としない。BQ の boost 入力条件と低温時の Voc を確認する。
3. 抵抗後 bulk の実効容量と実回路を使い、COIN・VOR・1V8・BQ gate／VBAT_OK・電流を同時捕捉。F35UQA512M の program／erase、BLE、source 切替、電圧低下・回復でリセットと保存結果を確認する。条件を満たさない COIN 状態では重い操作を禁止し、欠落を記録する。
4. 選んだ蓄電器の容量・ESR・電圧範囲、保管中の自己放電と運用期間を確認。BQ 内部 UV は約 1.95 V typ なので、Vina の 2.5 V 動作下限や Eaton HSH / HSL の 2.2 V 許容下限を単独では守れない。Vina に Eaton の 2.2 V 下限を流用せず、開始・保管・回収時の点検を決める。
5. 実装する各部品の番号・フットプリント・定格・実効容量、Solar とボタンの機構、寸法・配線・可搬性を確認してから PCB を確定する。

### 実物到着後の最初の測定

1. **パネル単体:** 未接続で極性・直径・厚さ・リードを確認し、屋外、会場に近い室内、遮光時の Voc を測る。同じ照明・角度で複数の抵抗負荷につなぎ、それぞれ端子電圧 V と抵抗 R を記録する。`I = V/R`、`P = V²/R` からおおよその最大電力点を比較する。抵抗の定格は測定した最大 Voc と最小抵抗で消費電力を計算して選ぶ。測定は BQ・電池・蓄電器から切り離した状態で行う。
2. **模擬電源:** 回路・部品レビュー後、逆方向の電流を安全に吸収できる電池模擬源と、出力電圧・入力インピーダンスを制限した Solar 模擬源を用意する。直流電源が sink できるとは仮定しない。実 coin と実 HSL／HSH はまだ使わない。
3. **切替・逆流:** 挿入順と CAP→COIN→CAP の各状態で、`VBAT_SEC`、`VSTOR`、`COIN_RAW`、`COIN`、`VOR`、`1V8`、gate、一次側電流を測る。電池方向へのピークだけでなく `∫I dt` と連続漏れ、電圧低下と回復を記録する。D1 短絡などの故障評価は模擬源で行い、許容条件は採用セルのメーカー仕様と照合する。
4. **負荷・保持:** 選択した C8 候補で起動・代表パルス・NAND／BLE 相当負荷を加え、COIN と 1V8 の最低電圧、bulk の回復時間、reset とデータの結果を記録する。測定前に容量値や稼働時間を保証しない。
