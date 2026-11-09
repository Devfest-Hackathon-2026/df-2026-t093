# Premium: 2026 年 11 月中旬の試作に向けた電源調査・実行計画

更新: 2026-10-23。ユーザーの今回の要求を受け、電源回路・エネルギー収支・現行 FW の統合状況を別々の観点から Astra で調査し、一次資料と現ファイルを照合した。これは **設計文書と試作計画**であり、Premium の KiCad 回路・PCB ドラフトはあるが、実機の回路・PCB・FW を実装／実測した記録ではない。

**2026-10-28 電源回路・PCB 更新:** [評価用 KiCad 回路ドラフト](../../hw/premium/premium_power.kicad_sch) と [小型部品・安全条件の検討](./premium-power-circuit.md) を追加した。Vina 30 F、CR2032、1 枚の Solar はデモ用の評価候補であり、非常用電池で 24 時間の動作を目標とする。これは本文の 120 F／CR2032 の製品基準や無日照 72 h 目標を変更・達成したものではない。SMD は表面にまとめ、電池ホルダーと Vina 蓄電器を裏面に並べて足を表面で手はんだ付けする方針。裏面の裸 TP は許容する。現 PCB には両者の仮 footprint を配置し、TP を仮外形から移動した。実物の向き・極性・組立は未確認。一次電池の逆流保護、bulk、電源切替と個々の部品は未確定で、現状の回路図は実セル接続・PCB 発注に使えない。

**筐体方針:** デモは丸型 PA12 MJF を前提に、前面の受け・ガイド・孔を造形する。Vina 30 F と CH005 電池ホルダーを同じ裏面側に並べることで両者の高さは加算せず、完成厚 10 mm を目標にする。PCB・表面部品・Solar・ケースを含む実際の最大積層と平面配置は未確認。HSH 22 F は薄さ重視の比較候補とする。電源成立も未確認。[寸法比較と製造条件](../spec/mechanical.md#76a-11-月デモの薄型-mjf-案2026-10-27)。

**同日、調達方針を追加:** できるだけ秋月など秋葉原実店舗で小売りされる部品を優先する。Vina 30 F は秋月の店頭表示を確認できたが、22 F 案より厚くなる。Solar や電源 IC の店頭入手は別途調査し、通販在庫と区別する。[調達表と評価条件](./premium-akihabara-parts.md)。

**2026-10-27 日程更新:** 参加予定は [DevFest GDGoC Japan Hackathon 2026](https://df-hack26.gdgs.jp/) と確認した。11/1 にテーマ・ルールを発表、応募締切は 11/4、オンラインの開発・提出期間は 11/5〜10、Regional Day は 11/14、選出された Top 10 の Demo Day は 11/27。提出の具体的な時刻と形式は未確認のため、11/10 までに提出物を用意する計画とし、正式な案内を確認する。紹介された別の [GDGoC イベントページ](https://gdg.community.dev/events/details/google-gdg-on-campus-university-of-aizu-fukushima-japan-presents-gdgoc-japan-hackathon/cohost-gdg-on-campus-tokyo-metropolitan-university-tokyo-japan/) は 2026/2/22〜3/29 の大会を記載している。

現在の仕様は [power.md](../spec/power.md)、音声は [audio.md](../spec/audio.md)、保存は [storage.md](../spec/storage.md)。この文書は今回の期限・調査根拠・未解決点・実装順を持つ。型番・価格・在庫の記録はこの調査日のスナップショットとして読む。

## 1. 今回の必須条件と結論

- **2026 年 11 月半ばまでに現物の試作品が必要。** 11/14 の Regional Day に向け、11/5〜10 の開発・提出期間と 11/11〜13 のリハーサルを区別する。提出の時刻・形式は正式案内で確認する。
- **見た目を含む Solar が必須。** 前面のパネルは実際に HSL を充電する。装飾と外部電源だけで代用しない。
- 電源は **Solar＋HSL の通常系／非常用ボタン電池の二段構え**。USB 給電・USB 充電経路は作らない。
- 既存の BC15M、T5848、1.8 V 生 SPI NAND、IMA-ADPCM 32 kbps、記録形式、BLE 同期を使う。Standard 完成を待たず、共通コアの検証と Premium の電源・機構を並行する。
- **2026-10-27 の変更:** ユーザーの短時間・低コスト優先を受け、NAND 容量を削り、F35UQA512M-WWR（512 Mbit）を設計採用する。デモ機は IMA-ADPCM 32 kbps 固定とし、48 h 用の縮退制御は適用しない。48 h 保存は製品目標とし、デモ機の対象外とする。

**デモの評価回路は BQ25505＋小型蓄電器＋TPS62842DGRR を先行する。** 1.8 V buck は TPS62840 系列で、元の小型 DLC 品の入手難に対する評価候補。蓄電器の第一評価候補は秋葉原での入手を優先した Vina 30 F、比較候補は薄さを重視した Eaton HSH 22 F。120 F は製品目標の検討用として残す。初号機は、共通コアと小型電源基板を短い配線で接続する構成も認める。最後は実パネル・実電池・本番筐体で動かす。Standard の CR2477 は接続せず、電源入力の二重接続を作らない。

ただし「今の Standard 基板へ Solar を追加すれば完成」ではない。下記の回路修正、外付け NAND backend、source 別の power policy と実機試験が必要。**48 h オフライン保存／CR2032 なしの無日照 72 h は製品目標のまま残し、ハッカソンの実証範囲と混同しない。** 会場で必要な動作時間は未指定なので、測定後に開始電圧・負荷・持続時間を明示する。

## 2. 現在どこまでできているか

監査基点は `23868c8` と 2026-10-23 の作業ツリー。以下はコードの存在と接続状況の確認であり、本試作の動作確認ではない。既存の未コミット FW／Standard PCB はこの調査では変更しない。

| 領域 | 現在の実体 | 今回残ること |
|---|---|---|
| 共通データ経路 | IMA4、record、ring、BLE fragment／ACK、mobile 保存の実装あり | 本番 phone と本番基板で録音→保存→同期→読戻しを通す |
| T5848 I²S | `fw/src/audio.c` に受信・間引き処理、BC15M board 設定あり | 実機 SCK/LRCK、データの bit 位置、消費電力・録音時間の照合 |
| AAD | `fw/lib/aad`／`t5838` は純 C。アプリから AAD／THSEL／WAKE の運用へ未接続 | 待機中に本当に I²S／HFXO を止め、起床時に録音へ移る統合 |
| NAND | 仕様はあるが NAND driver は未実装。`store.c` は内蔵 `ring_partition`／flash_area | F35UQA512M 用の SPI backend、2 KiB page／128 KiB block、ECC、bad block、status、timeout |
| source／電圧 | Standard 用 `powerstate` は純 C。アプリの実電圧入力・Premium policy なし | Vcap、coin、レベル処理後 VBAT_OK、起動判定・runtime 許可へ接続 |
| OTA | callback gate はあるが `ota_allowed()` は Phase 0 の test flag 以外では許可 | 本体電源 policy に接続。今回実演しないなら試作構成で拒否する |
| 時刻 | uptime＋phone TIME。外付け RTC は DNP 方針 | 電源断後の時刻無効と再同期を確認。RTC を急いで追加しない |
| 基板 | 評価用電源と共通コアの回路図、φ39 mm の部分配線 PCB ドラフトあり | 電源ループと残り配線、一次電池保護、容量・端子、Vcap／coin 測定、実スイッチ、実装条件を確定 |
| mobile | core 実装あり。README の実端末 BLE 試験は未確認 | 第 1 週に実 phone で手動同期・切断復旧を確認。background 同期を前提にしない |

コード上の入口: [CMakeLists](../../fw/CMakeLists.txt)、[store](../../fw/src/store.c)、[audio](../../fw/src/audio.c)、[main](../../fw/src/main.c)、[OTA](../../fw/src/ota.c)、[BC15M DTS](../../fw/boards/pendant/bc15m/bc15m_nrf54l15_cpuapp.dts)、[mobile README](../../mobile/README.md)。

## 3. 電源構成と PMIC の判断

```text
前面 Solar → BQ25505 VIN_DC
                   │
  HSL+ ↔ VBAT_SEC ↔ 内部 battery FET ↔ VSTOR → secondary PMOS pair ─┐
                                                                  ├→ VOR → reservoir → TPS62842DGRR → 1.8 V 共通コア
  CR2032+ → 逆流保護の評価回路 → primary PMOS pair ─────────────────┘
      └→ VBAT_PRI sense（保護素子・gate 基準との整合を回路図で確認）
```

各 pair は **common-source の back-to-back P-channel 2 素子**。外側 drain が source 入力と VOR、共通 source はペア内の中点。両 gate を結び、`VB_SEC_ON`／`VB_PRI_ON` で制御する。**計 4 素子／dual package 2 個**である。HSL から負荷へ直接取ると BQ の内部切断を迂回するので、secondary の起点は VSTOR とする。[TI BQ25505 Fig.16・§7.3.2](https://www.ti.com/lit/ds/symlink/bq25505.pdf)、[EVM SLUUAA8A Fig.1](https://www.ti.com.cn/cn/lit/pdf/sluuaa8)。

| 案 | 要求との適合 | 今回の判断 |
|---|---|---|
| BQ25505＋外付けペア＋TPS62840 系 | primary／secondary の gate driver と MPPT がある | **先行する構成**。デモ回路は調達のため TPS62842DGRR を評価。電源選択と一次電池保護は別に設計する |
| BQ25570 | buck 内蔵だが BQ25505 と同じ primary 用 gate driver はない | CR2032 選択を追加設計する必要があるため今回変更しない |
| 新しい harvester へ全面変更 | 別の入力条件・制御・調達・基板検証が増える | BQ25505 に実測上の不成立が出た場合に再評価 |

BQ25570 の buck 統合だけで BOM が減るとは判断できない。[BQ25570 SLUSBH2G の pin／構成](https://www.ti.com/lit/ds/symlink/bq25570.pdf)。BQ25505 の EVM は比較試験の参考だが、**既定 OV が 4.2 V なので抵抗変更とダミー負荷での確認前に HSL を接続しない**。[EVM guide p1](https://www.ti.com.cn/cn/lit/pdf/sluuaa8)。

PMOS の第一評価候補は **CSD75208W1015 ×2**。TI の [TIDA-00488](https://www.ti.com/tool/TIDA-00488) が BQ25505／同 PMOS／CR2032 を組み合わせている。dual common-source、1 × 1.5 mm WLP、ペア RDS(on) max 285 mΩ @ −1.8 V／150 mΩ @ −2.5 V。ただし IDSS max 1 µA の規定があり「nA 保証」ではない。WLP の実装性、実電圧での漏れ、gate 波形と調達を確認して採用する。[CSD75208W1015 datasheet](https://www.ti.com/lit/gpn/CSD75208W1015)。

## 4. PCB を出す前に閉じる電気的な論点

### 4.1 source 信号は 1.8 V GPIO へ直結しない

`VBAT_OK` は **VSTOR 基準の push-pull 出力**で、約 3.75 V に達し得る。1.8 V 共通コアに NAND／マイクの変換が不要でも、この信号は別。単純分圧は全 VSTOR 範囲の VIH と rail OFF 時の back-power まで確認する必要がある。[BQ25505 pin 表](https://www.ti.com/lit/ds/symlink/bq25505.pdf)。

初号機の具体的評価案は **SN74LVC1G17DBVR** を 1.8 V で給電し、`VBAT_OK→A`、`Y→MCU`、直近に 100 nF。入力 5.5 V 許容、Ioff 対応、Schmitt 入力、SOT-23-5。ただし ICC max 10 µA／入力漏れ max ±5 µA を損失へ計上する。無電源時の入出力・プル抵抗・極性は回路レビューと実測で確認する。これは採用済み部品ではない。[TI SN74LVC1G17 §5.3・§5.5](https://www.ti.com/lit/ds/symlink/sn74lvc1g17.pdf)。

### 4.2 一次電池の逆流は OFF 時だけを見ない

back-to-back PMOS は OFF 時の双方向遮断に有効だが、**ON 中は双方向に通る**。例えば VOR の bulk が 2.6 V、消耗 coin が 2.3 V のとき primary を選ぶと、bulk の電荷が coin へ戻り得る。break-before-make は同時接続防止であり、逆充電禁止の証明ではない。

一次枝に逆流保護を用意する方針。低漏れ diode は単純だが Vf と低温時の buck 入力余裕が課題。**LM66100 を coin 枝に置き、CE をその VOUT に接続する案**は比較候補である。1.5〜5.5 V、Iq 150 nA typ／300 nA max、SC-70。ただし応答時間・判定差電圧・温度別の逆漏れがあり、逆流を数学的なゼロとはできない。[LM66100 §5・§6.5・§8](https://www.ti.com/lit/ds/symlink/lm66100.pdf)。

COIN→CAP 復帰時も、例えば VOR 約 3 V／回復 HSL 約 2.8 V なら、bulk の残留電荷が secondary 側へ移り得る。secondary の電流・積算電荷も測り、必要ならこちらの逆流保護も設ける。継続的な coin 充電経路と有限の電荷移動を区別するが、許容条件が未定な間は既存の「coin→HSL 禁止」を満たしたと扱わない。

Standard の VOR 後段にある逆流保護 footprint だけでは、VOR bulk→coin の経路を遮断できない。まず電池模擬源で切替時の `I_coin`、VOR、1V8、`∫I_reverse dt` を測り、**実セルへ接続する前に Panasonic の許容条件と照合する**。定常 µA の測定だけで合格にしない。[Panasonic CR2032](https://energy.panasonic.com/jp/business/products/lithium/coin-cr-standard/models/CR2032)。

### 4.3 HSL の 2.5 V と 2.2 V を区別する

Eaton の **minimum rated 2.5 V、minimum allowed 2.2 V、maximum rated 3.8 V**は別の値。自己放電は負荷切断でも止まらない。BQ 内部 UV は約 2.0 V なので、HSL の最低許容を守る保護として単独では足りない。[Eaton Technical Data 11043 p2](https://www.eaton.com/content/dam/eaton/products/electronic-components/resources/data-sheet/eaton-supercapacitor-hybrid-cylindrical-cells-data-sheet.pdf)。

| 設定 | OV 抵抗 MΩ | OK 抵抗 MΩ | 公称 OV／下降／上昇 V | 位置づけ |
|---|---|---|---|---|
| 従来 | 6.34／6.49 | 6.34／6.81／1.00 | 3.672942／2.509700／2.700552 | 下降 −2 % = 2.459506 V。従来の試作基準として履歴を残す |
| 製品向けの評価候補 | 同上 | 4.87／5.62／0.787 | 3.672942／2.606345／2.801883 | 下降 −2 % = 2.554218 V。旧ドラフトの設定 |
| 11 月デモの回路ドラフト | 同上 | 4.87／6.34／0.787 | 3.672942／2.785236／2.980774 | 下降 −2 % = 2.729531 V。Vina の下限に対する静的余裕を拡大。実測・保護は未完了 |

全て 0.1 % 抵抗、±2 % は BQ の総合精度として **実抵抗から求めた公称値に適用**する。VBIAS corner と同じ総合誤差を重複加算しない。デモ案の ΣROK は 11.997 MΩ、最悪の上昇下限−下降上限は約 80.2 mV。回路図ドラフトは変更したが、PCB・実機は未検証。実しきい値・蓄電器端子過渡で採否を決める。[計算と制約](./premium-power-circuit.md)。

監督下の 11 月試作では使用前後の Vcap と Solar 回復を確認し、終了後の切離し・点検期限を決める。無監督の長期保管／製品運用を保証する段階では、外付け負荷の低電圧切断・漏れ・保管条件を追加評価する。Eaton の 2.2 V 未満へ落ちた個体は通常手順で再使用しない。Vina はメーカーの動作下限 2.5 V を使い、Eaton の許容下限を流用しない。

### 4.4 coin の bulk と切替途中の NAND

CR2032 選択中、120 F の secondary 枝は切断されるため **120 F は NAND／BLE のピーク源にならない**。bulk は VOR／buck 入力・出力のどこに置くかで計算が異なる。

```text
出力側の電圧低下: ΔV = Q / C
入力側の利用エネルギー: ΔE = C (Vhigh² − Vlow²) / 2
不足エネルギー: ∫ max(Pload − Psource, 0) dt
```

W25N04LW／08LW の参照 Rev A では chip 単独の上限例が program 25 mA × 0.8 ms = **20 µC**、erase 25 mA × 10 ms = **250 µC**。後者は 100 µF なら補給なしの計算で 2.5 V の降下に相当し、「100 µF＋1 秒ごとの program」では erase や MCU／SPI／metadata の成立を説明できない。typ の 8.8／60 µC を最大設計値として使わない。最終型番・revision で再確認する。[Winbond W25N08LW Rev A §10](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/8921/256_W25N08LWxxxx.pdf)。

最初は **CAP で通常動作、COIN で BLE／OTA を止め、未検証の新規 NAND command は拒否**する。COIN 中の録音・保存継続は、その型番でページ速度・電源回復・RAM・metadata まで成立後に許可する。既に発行した program／erase の完了分はハードのエネルギー余裕が必要。保存を止めた区間は復帰時に正しく報告し、無期限に保存できると表示しない。

## 5. エネルギーの再計算と成立条件

### 5.1 実測したものと推定を分ける

- T5848 Rev 1.1 の Analog AAD **20 µA**、LP **120 µA**、D1 **137 µA**、D2 **110 µA**は typ。D2 は LP より 10 µA 少なく、待機中の host clock を止められる可能性もある。Analog の代替としての音声検出性能は別途比較する。
- ADPCM の **85.5 cycle/sample は実測**だが、**42 µW は CoreMark 条件の pJ/cycle を使った推定**。I²S／clock／DMA／FIR を含む 65 µW が実測されたわけではない。
- 現 BC15M DTS は `PCLK32M_HFXO` を指定し、NCS の I²S driver は受信中 HF clock を要求する。録音中の電力を CPU の実行 cycle だけから出さない。現行の 63 tap FIR・copy・受信 DMA の費用も測る。
- [storage.md §4.1](../spec/storage.md) の NAND **15 mJ/MB は cap 側換算済み**。他の rail 項と足してもう一度効率で割る計算を修正した。
- BLE **87 mJ/MB**は 3 V・特定 throughput の参照点からの仮説。1.8 V の完成基板と本番 phone で測った値ではない。再送・接続・idle を含めて測定し直す。
- Nordic の 2.9 µA idle も元の測定条件は 3 V／25 °C／DC/DC。`5.2 µW = 2.9 µA×1.8 V` は移植の仮置きで、BC15M 実機の床電力の保証ではない。

出典: [TDK T5848 現行製品ページ](https://www.invensense.tdk.com/en-us/products/microphone/t5848) が配布する DS-000479 **Rev 1.1、2026-09-15**（§1.3／§1.4／§6.3）、[Nordic nRF54L15 PS v1.0](https://files.seeedstudio.com/wiki/XIAO_nRF54L15/Getting_Start/Nordic_nRF54L15_Datasheet_v1.0.pdf) §11.9、[Zephyr I²S nrfx driver](https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/i2s/i2s_nrfx.c)。driver の実装判断はローカル NCS v3.4.1 のコードで照合した。上流 main が同じ実装の固定参照という意味ではない。

### 5.2 duty と電力の境界

`a` = I²S 実稼働割合、`p` = 32 kbps を保存する割合、`x` = 実際に送信／読出しした byte/s ÷ 4,000。追いつき同期では `x > p` もあり得る。`A` は録音中の **MCU＋clock＋DMA＋間引き＋codec の実測増分 µW**。

旧データ係数を使って計算過程を比較する場合だけ、次を仮置きする。BLE の 87 mJ/MB を rail 換算値と仮定する部分を含み、**完成品予測としては未検証**。

```text
P_rail_base[µW] = 36(1−a) + 216a + 5.2 + A a + P_extra_rail
P_cap[µW] = (P_rail_base + 348x) / η_buck
             + 39.6p + 20.4x + P_standby_cap + 37 + P_extra_cap
                          ↑ NAND 9.9／5.1 mJ/MB は cap 側のまま
```

`P_extra` に source 信号変換・逆流保護・実装 RTC・ADC・抵抗等を足す。ゲートしない NAND の待機は別項。η の実測値に Iq が含まれる場合は再加算しない。**`a=p=x` を実現できるのは、AAD で受信そのものを止め、同量を同期する運用のときだけ**。現 FW のような連続受信では `a=1`。

この未検証係数で A=65、extra=0、η=.9 を仮置きすると、`a=p=x=.4` は cap 約 0.370 mW、`a=1,p=x=.4` は約 0.534 mW。受信 duty の扱いだけで変わる。数値の差を AAD 実機の成立証拠とはしない。録音時間・保存 byte・受信 byte と rail の積算 J を同じ時間窓で測る。

### 5.3 全基板の負荷から見る稼働時間

今回の候補の使用可能量は、公称 **111.624 mWh**。容量下振れ／劣化の設計 corner `120×.8×.7=67.2 F`、OV −2 %、下降 +2 % では **54.962 mWh**。劣化率は寿命試験から置いた corner で、使用年数や温度全域の保証ではない。

```text
E[mWh] = C (Vhigh² − Vlow²) / 7.2
Pcap[mW] = Prail / 0.90 + 0.037
runtime[h] = E / Pcap
```

| 基板全体の rail 負荷 | 公称 120 F | 上記 corner 67.2 F |
|---:|---:|---:|
| 0.5 mW | 188.4 h | 92.8 h |
| 1 mW | 97.2 h | 47.9 h |
| 3 mW | 33.1 h | 16.3 h |
| 5 mW | 20.0 h | 9.8 h |
| 10 mW | 10.0 h | 4.9 h |
| 20 mW | 5.0 h | 2.5 h |

新候補の 72 h 条件は **rail ≤0.6537 mW**。旧抵抗の正確な公称／corner は 119.865／59.763 mWh、72 h の rail 上限は 0.7137 mW。0.8 mW 未満だけで 72 h 成立とはできない。

表は η=.9、固定損失 37 µW、規定の開始電圧が成立する仮定。ピーク降下・追加保護・温度・実効容量・漏れ増加で短くなる。ハッカソンでは **測った負荷と実際の開始電圧で、必要時間に余裕を持つこと**を確認する。公称表から時間を約束しない。

再現:

```sh
python3 tools/bq25505_resistors.py
python3 tools/bq25505_resistors.py --check
```

## 6. Solar と調達の調査

### 6.1 パネルは外形・電圧・実発電を同時に決める

11 月デモの第一評価品はユーザーが選んだ [uxcell φ36 mm 丸型](https://www.amazon.co.jp/dp/B0H5MJDDX8)。販売表示は 2 V／40 mA／80 mW だが、測定条件、低温 Voc・I–V・厚さ・端子は未確認。BQ25505 は boost 型なので、MPP だけでなく低 VSTOR 時の Voc と過充電停止時の挙動まで照合する。[BQ25505 §7.3.3・§8.1](https://www.ti.com/lit/ds/symlink/bq25505.pdf)。

| 候補 | 調査で分かったこと | 今回の扱い |
|---|---|---|
| 従来相当の φ36 丸型 | [PMD の販売品](https://pmdway.com/products/round-solar-panel-2v-10-pack) は φ36／2 V／35 mA の表示。メーカー仕様書は未取得 | 外観に適合する優先調達候補。ただし 80 mW 品と同一とはしない。現物 I–V／Voc 測定後に採否 |
| ANYSOLAR KXOB25-05X3F | 23×8×1.8 mm、Voc 2.07 V／Vmp 1.67 V／30.7 mW の仕様候補 | 3 個並列なら面積 23×24 mm、対角約 33.2 mm、合計 92.1 mW は STC 概算。円形 carrier 内に置く代替案。配線・間隔・遮光・実装・外観は CAD と現物で確認 |
| Panasonic AM-1801 | 屋内向け 53×25 mm。200 lx 条件で約 56.7 µW、Voc 5 V | φ40〜42 の外観と boost 入力にそのまま合わないため採らない。[メーカー仕様書](https://panasonic.net/electricworks/amorton/assets/pdf/spec_PDF/indoor/AM-1801.pdf) |
| Voltaic P121 | 52×52×2.6 mm、Voc 2.84 V、Vmp 2.43 V、0.33 W | 電源比較には使えても目標外径を超えるため本体の第一案にはしない。[メーカー仕様](https://voltaicsystems.com/P121/) |

ANYSOLAR の外形・電気値・ACTIVE 表示は [メーカーの現行 Gen3 製品表](https://anysolar.biz/products/gen3/) と、現行サイトから取得した [KXOB25-05X3F Preliminary Rev. Jun. 2026](https://anysolar.biz/KXOB25-05X3F-datasheet/) で照合した。電気値は **1 sun＝100 mW/cm²、AM1.5、25 °C の typ**。旧 PDF ホストではなく現行 URL を使う。低温 Voc・部分遮光、実装の温度と MSL3、洗浄液の film への影響は採用前に確認する。3 並列案は評価候補であり、前面の形状を変更済みとは扱わない。

**室内照明だけで収支が合うとは約束しない。** 同じ lux でも照明のスペクトル・入射角・衣服・遮光で変わる。屋外での Solar 事前充電を本番手順に含め、会場の照明で net charge を見せるにはその環境で別に確認する。

### 6.2 補充に要る時間

rail 0.5 mW のモデルでは cap 消費 0.59256 mW、3 日で 42.664 mWh。発電中も動かすので負荷を差し引く。

| 実パネル端子の平均電力 | η_h=.85 後の net charge | 3 日分の補充 |
|---:|---:|---:|
| 5 mW | 3.6574 mW | 11.7 h |
| 10 mW | 7.9074 mW | 5.4 h |
| 20 mW | 16.4074 mW | 2.6 h |

新候補の動作下限から公称満充電まで 111.624 mWh 全量を回収するなら、同条件で約 **30.5／14.1／6.8 h**。公称 80 mW を胸元平均と見なした「短時間で満充電」は採用しない。cold-start 時の仕様条件も 120 F＋完成基板の起動時間保証ではない。

### 6.3 調達の現状と優先順位

**2026-10-27 の発注計画:** 部品は手持ち・型番・到着日を確認し、確定したものから先行調達する。Premium の裸基板・ステンシルは、採用部品の footprint と電源回路のレビュー後、可能なら **10/30〜31 に発注**し、11/1 は遅くとも判断する目安とする。11/1 は日曜日で、[JLCPCB は土曜 18:00（GMT+8）以降の支払いを翌営業日に処理する](https://jlcpcb.com/jp/help/article/what-is-the-build-time)と案内している。製造時間は配送時間を含まず、仕様と送り先によって変わるため、見積時の製造・配送予定を確認する。日付は到着の保証ではない。

**2026-10-27 の NAND 選定:** [F35UQA512M-WWR の DigiKey JP 掲載](https://www.digikey.jp/en/products/detail/lexar-enterprise/F35UQA512M-WWR/26793881)は、512 Mbit、137 個、1 個 ¥382（税別）の表示。旧 8 Gbit 候補は同日の [DigiKey JP 掲載](https://www.digikey.jp/ja/products/detail/alliance-memory-inc/AS5F18G04SNDB-10LIN/28167216)で 1 個 ¥9,666（税別）なので、部品単価は約 96 % 下がる。数量・価格・送料・日本への到着日は変わるため注文画面で再確認する。購入・実装は未実施。

2026-10-23 に開いた販売ページの表示。価格は税別・送料別の少量表示。在庫の確保・日本への到着日・購入は未実施。

| 部品 | 表示の証拠 | 判断 |
|---|---|---|
| BQ25505RGRR | [DigiKey JP](https://www.digikey.jp/ja/products/detail/texas-instruments/BQ25505RGRR/4493883): 7,105 個、1 個 ¥864 | 掲載在庫から少量調達する候補。メーカー標準 20 週と掲載在庫を混同しない |
| HSL1225-3R8127-R | [DigiKey JP](https://www.digikey.jp/en/products/detail/eaton-electronics-division/HSL1225-3R8127-R/12179230): 796 個、1 個 ¥1,864 | 現物寸法・リード加工・初期電圧を早期確認 |
| CSD75208W1015 | [TI](https://www.ti.com/product/CSD75208W1015): ACTIVE | 販売店の少量在庫・到着日は別に確認。dual 2 個／台 |
| KXOB25-05X3F-TR | [DigiKey](https://www.digikey.com/en/products/detail/anysolar-ltd/KXOB25-05X3F-TR/9990481): 4,549 個、1 個 $3.23 の表示 | 外観代替案用。3 個／台なら配線・支持材も見積もる |
| W25N04LWZEIR | [DigiKey](https://www.digikey.com/en/products/detail/winbond-electronics/W25N04LWZEIR/27513142): 在庫 0、backorder 不可 | 電力計算の参照品として妥当でも、この型番の入荷待ちを日程に置かない |
| Alliance AS5F14G04SND-10LIN | [DigiKey](https://www.digikey.com/en/products/detail/alliance-memory-inc/AS5F14G04SND-10LIN/13282916): 254 個、1 個 $16.28。[Mouser](https://www.mouser.com/en/ProductDetail/Alliance-Memory/AS5F14G04SND-10LIN?qs=W%2FMpXkg%252BdQ4L7I9YqteJnw%3D%3D): 172 個、即時出荷表示、最小 1 個 | 1.8 V／on-die ECC だが **2 KB page**。4 KB 必須の製品選定条件から外れる。試作代替候補に限り、回数・電力・driver を再計算 |
| Alliance AS5F18G04SNDB-10LIN | [DigiKey](https://www.digikey.com/en/products/detail/alliance-memory-inc/AS5F18G04SNDB-10LIN/28167216): 204 個、1 個 $58.64 | **10/23 の旧候補**。短時間・低コストを優先し、11 月デモは 512 Mbit へ変更 |
| Macronix MX35UF4GE4AD-Z4I | [DigiKey](https://www.digikey.com/en/products/detail/macronix/MX35UF4GE4AD-Z4I/18110106): 0 個、backorder 不可 | on-die ECC の E4 系でも今回は在庫待ちにしない。24A 系を同等品として買わない |

Alliance の [公式 Rev 3.2、Jan 2026](https://www.alliancememory.com/wp-content/uploads/AllianceMemory_AS5F_QSPI_NAND_Flash_3V-1Gb-8Gb_1p8V-2Gb-4Gb-8Gb_Ver3.2_Jan2026-revised.pdf) で、1.7〜1.98 V、internal 8-bit ECC、4 Gbit は 2048+128 B／8 Gbit は 4096+256 B、各 64 page/block を確認した。ただし **B9h の DPD は確認できず自動 Sleep**。tWakeup は 6 ms **typ**、Sleep 移行は 4 Gbit <50 ms／8 Gbit B <20 ms の typ。Sleep の専用電流規定も未確認で、standby は 30 typ／120 max µA。固定 6 ms を最大保証値として driver に埋め込まない。power gate、起動待ち・再設定・SPI 状態を検証する。

また LGA 8×6 は端子配置が現在の WSON の側面と概ね合うが、**中央 exposed pad はない**。Standard の WSON footprint にある中央銅／paste をそのまま流用せず、Premium 側はメーカー LGA 図に合わせる。8×6 という外形だけで drop-in としない。

11 月デモは **F35UQA512M-WWR を設計採用**する。既存 Standard の U5 と同型番・WSON 8×6 で、共通部品・足跡を利用できる見込みだが、exposed pad と paste は製造前に確認する。供給は 1.7〜1.95 V、2 KiB + 64 B/page、128 KiB/block、512 block。on-die ECC は 528 B ごとに 1 bit 訂正、出荷時の有効 block は最低 502 個。[Longsys 型番表](https://www.longsys.com/products/embedded-storage/micro-storage/spi-nand-flash.html)、[Longsys DS Rev 1.1](https://cdn.icstop.com/upload/pdfs/e7/de/e7decc6bb7e9f2384da286e27083f166.pdf)。

Rev 1.1 に DPD 命令は見当たらず、指定条件での待機電流は 10 µA typ / 50 µA max。**デモ評価回路では NAND 単独の power gate を省き、1.8 V 共通 rail から測定リンク経由で常時給電する。** 既存 Standard の電源切断案は MCU が NAND の無給電入力を駆動し得るため、そのまま流用しない。常時給電による追加待機電力と電源立上り・降下時の逆給電を測定し、製品の power gate は隔離回路を含め別に設計する。2 KiB page は製品用の 4 KiB page 基準の試作例外で、32 KiB record は 16 page に収まる。初期 bad block を消去前に読み出し、ECC／P_FAIL／E_FAIL とページ順序を扱う NAND backend は別途実装する。電力は 4 KiB 基準値を流用せず測定する。

容量の机上目安は `502 × 128 KiB × 0.98 ÷ 4,000 B/s ≈ 4.48 h`（32 kbps・p = 1）。0.98 は metadata 等の仮余裕で、実際の bad block・記録形式・イベント・同期と電源を含む保証値ではない。デモは 32 kbps 固定で、満杯時は電源条件が許せば最古を破棄し欠落を通知、許せなければ保存制限に移る方針。**48 h 保存は製品目標として残し、このデモ機では要求しない。** 最初に Solar の現物・PMIC／HSL／coin holder・選定 NAND の到着見込みを確認し、採用部品の足跡と電源回路のレビュー後に基板を発注する。

## 7. FW の最小統合と見落としやすい経路

2026-10-27 に確認した分担: **ユーザーはハードウェアとペンダント側 FW（マイク・NAND・BLE・電源状態）を担当し、バックエンドはチームメンバーが担当する。** スマホ側 BLE 受信の担当は未確認。チームで担当を決め、採用 NAND、1.8 V、I²S／SPI、source／電圧測定、privacy 接続と、スマホへ渡す記録形式・同期手順を共有する。

1. **SKU ごとの board／電源入力を追加する。** 共通アプリを丸ごと fork せず、Premium overlay／config と小さな source policy を追加する。Vcap、coin、レベル処理後 source を起動時と動作中に読む。Vcap は予約 AIN1 が候補だが、追加 pin は未確定。
2. **NAND 1 品に絞って backend を接続する。** JEDEC ID、page／block、ECC、bad block、BUSY／P_FAIL／E_FAIL、power-up、timeout を実装する。内部 RRAM の動作だけでは NAND の電断安全を証明できない。
3. **許可判定を各 program／erase の直前へ置く。** record の入口だけでは不十分。boot／format／TIME event／drop event／reclaim／metadata／BBM を含める。既発行 command の完了エネルギーは §4.4 のハード条件。
4. **metadata の erase を忘れない。** 現 `ring.c` の `meta_commit` は alternate metadata block を erase し、`ring_open`／reclaim からも呼ばれる。`ring_pre_erase` は data block だけ。COIN 中の起動や更新も power gate の対象にする。
5. **録音の再開・停止の副作用まで gate する。** `main.c` は auto-record を 5 秒ごとに再試行し、`audio_stop()` は drop flush を呼ぶ。低電圧停止直後の自動再開や追加書込みを防ぐ。
6. **page pacing と watchdog を整合させる。** 現 writer は record 全体に約 7 秒の deadline。32 KB record をページごとに約 1 秒待つ案は超過し得る。ページ完了を進捗として扱い、有限の回復待ち・RAM backpressure・欠落報告を定義する。WDT の無効化で済ませない。
7. **AAD は連続録音の縦串の後に接続する。** WAKE／THSEL を Rev 1.1 と照合し、起床時の頭欠け・false trigger・実録音 duty を測る。AAD library があることを AAD 動作済みとはしない。
8. **本番 phone へ早く接続する。** foreground の手動同期でよい。切断／再接続／ACK／読戻しを第 1 週から試す。未録音期間を既存 record の sequence gap へ偽って入れず、event の意味と phone 表示を合わせる。

製品の 48 h 目標には、実 codec／phone／ASR を含む縮退側か、十分な容量を別途成立させる必要がある。**11 月の 512 Mbit デモでは 48 h を実証しない。** bitrate policy のホスト計算だけで 48 h を達成したと扱わない。[storage.md §7](../spec/storage.md)。

## 8. 外観と実物を間に合わせる順番

前面 Solar／carrier／ボタン、HSH または HSL 横置き、電池 holder、BC15M アンテナ keep-out、マイク孔、物理 privacy latch を、**回路と同じ週に実寸で配置**する。デモ候補の 22 F は最大 φ6.8 × 24 mm で、外形公差・曲げたリード・絶縁・接点・固定を含める。φ40〜42 mm／厚さ 13〜14 mm を検討目標とし、成立と最終寸法は CAD と現物で確認する。120 F の製品基準は別途保持する。[mechanical.md §3・§7.6a](../spec/mechanical.md)。

Solar を曲げるボタンにしない。rigid carrier と hard stop で荷重を受け、動く接続は spring contact を評価する。初回の短時間試験に線材を使う場合も本番の固定・疲労・接触を別に確認する。無条件の防水・汗耐久を主張しない。

privacy latch は現 Standard の solder bridge のまま実演機能とは呼べない。MCU が何をしてもマイク電源と clock／THSEL の回り込みを遮断できる、操作可能な物理部品と外観表示を具体化する。赤／黒の終端フラグも「録音を保証する表示」として使わない。

| 期間（作業目標） | 現物／証拠で閉じること | 並行して進めるもの |
|---|---|---|
| 10/27〜11/1 | Solar・HSL・PMIC・NAND の手持ち／到着見込み、電源回路、source／ADC pin、実寸配置。逆流・信号電圧・OV／UV・mux をレビューして PCB 製造へ | phone 同期、NAND backend、筐体 CAD。基板の製造・配送日数を確認し、必要なら前倒す |
| 11/1〜4 | テーマ・ルールを確認し、11/4 までに応募。実部品・基板の到着状況を確認する | 既存コード／事前製作ハードの扱い、提出期限・形式、Google 技術の使用条件を正式案内で確認。応募方法と会場を決める |
| 11/5〜8 | 組立と実機統合を進め、Solar→HSL→録音→NAND→phone の成立を目指す | 切替／NAND 過渡、T5848、AAD、筐体内 BLE、privacy、審査用の体験 |
| 11/9〜10 | 公式サイトの開発・提出期間内に成果物をまとめて提出する目標 | 本番電源・実 phone で読戻しと動作を記録。提出時刻・内容は正式案内を優先 |
| 11/11〜13 | 11/14 の本番筐体・本番電源でリハーサル | 暗所→coin→Solar 回復、phone 切断、実積算エネルギー。提出後に実演内容を変更できる範囲は大会ルールで確認 |
| 11/14・11/27 | Regional Day。Top 10 に選出された場合は 11/27 の Demo Day | 実際に確認できた性能と提出物を説明する |

これは製造・到着の保証ではない。11 月上旬に実物の縦串が成立しなければ、同じ Solar＋蓄電／coin 条件のまま、提出可能な実証範囲を見直す。USB で動かして合格へ置き換えない。応募多数の場合は事前提出物により予選ピッチの選考がある。審査は Google 技術の活用、完成度、社会課題への解決力、ピッチの 4 観点である（[大会公式サイト](https://df-hack26.gdgs.jp/)）。

出場は 2〜4 人のチーム。個人応募の場合は運営がチームを編成し、Regional Day はオンライン会場となる。チーム応募時の会場は応募内容と正式案内を確認する。

既存の lifeOS コードや開発期間前の試作ハードの持込み可否、外部サービスの利用条件、提出後の変更範囲は公式サイトで確認できていない。11/1 のルール説明などで確認し、既存部分と大会期間中に作った部分を区別できるようにしておく。録音を Google 技術で処理する案を採る場合は、本人が選択した音声・データだけを対象とし、実演前に利用者の同意と送信先を確認する。

## 9. 試作の合格条件と、その後の製品条件

| 判定 | ハッカソン試作で必要な証拠 | 製品へ進む際の追加証拠 |
|---|---|---|
| Solar が機能する | 実パネルで Vcap／ΔE が増加し、本体を動かしながら回復する条件を記録 | 胸元・季節・遮光を含む日々の収支 |
| 二段電源 | 模擬源で保護確認後、実 coin で CAP→COIN→CAP を実証。rail／reset／保存結果を記録 | 低温・消耗 coin・全挿入順・長期漏れ |
| 動作時間 | 開始電圧・平均／ピーク・終了条件を記録し、本番必要時間へ余裕を持つ | CR2032 を使わない無日照 72 h と worst condition |
| 保存・同期 | 実 NAND・本番 phone で音声読戻し。省電力で止めた区間も表示 | 48 h オフライン、満杯・BBM・縮退 codec、全電断タイミング |
| 外観・操作 | 実寸筐体、前面 Solar／button、privacy、アンテナ・音孔の成立 | 汗・防水・落下・ボタンサイクル・認証 |

今回の調査では、Premium の自立動作を保証する電力測定、PCB 設計、実端末・実 NAND 試験、購入は行っていない。文書・再計算・実装上の不足と進める順番を確定した。
