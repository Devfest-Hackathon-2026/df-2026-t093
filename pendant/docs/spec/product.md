# 製品仕様（目的・要求・Standard / Premium・BOM・全体目標）

lifeOS Pendant の製品レベルの仕様。このファイルが持つもの: 目的と要求、Standard / Premium の分岐とその理由、共通コア、SKU 別の UX、BOM の概要、全体の目標値（電池寿命・48 h オフライン・外形）、初号機の Go/No-Go。

他のファイルが持つもの（ここでは繰り返さない）:

- 電源パス・しきい値・エネルギー収支: [power.md](./power.md)
- 赤 / 黒の終端表示: [terminal-indicator.md](./terminal-indicator.md)
- NAND・ring・バックログ連動ビットレート: [storage.md](./storage.md)
- マイク・AAD・codec・ASR: [audio.md](./audio.md)
- レコード形式・BLE 同期: [formats.md](./formats.md)
- OTA・MCUboot・時刻管理: [firmware.md](./firmware.md)
- 脅威モデル・ペアリング・鍵: [security.md](./security.md)
- アンテナ・TX 電力: [rf.md](./rf.md)、ピン割当・測定点: [board.md](./board.md)、筐体・防水・Solar ボタン: [mechanical.md](./mechanical.md)
- 試作の段階: [../plan/prototype.md](../plan/prototype.md)、技適・SIG・CE・FCC: [../plan/compliance.md](../plan/compliance.md)
- 決定の経緯: [../decisions.md](../decisions.md)

確度の表記: **確定**（動かさない）/ **方針**（向きは決まり、数値や部品が未定）/ **仮置き**（実測で決める値）。

## §1 目的と要求

### §1.1 目的（確定）

日常の音声を、長時間・低消費電力で記録し、lifeOS へ取り込むための、小型のペンダント型ロガーを自作する。リアルタイム性は要求しない。基本動作は次のとおり。

```text
録音 → ローカルの NAND へ蓄積 → 約 2 時間ごとにスマートフォンへ BLE バースト同期
```

スマートフォンが無い間は同期をスキップし、NAND への蓄積を続ける。スマホの不在は異常ではなく通常の状態として扱う。BLE はストリームではなく、データの同期路にすぎない。

### §1.2 設計上の最優先事項（確定。SKU ごとの違いは §1.3）

1. 小型
2. 低消費電力
3. 数日間の完全オフライン動作
4. 充電作業を極力不要にする
5. スマートフォンを失くしても録音を続ける
6. ストレージ・BLE・録音系の故障を、互いに波及させない

### §1.3 要求と SKU（確定）

| 要求 | Standard | Premium |
|---|---|---|
| スマホ不在でも録音を続ける | buffer の容量まで（満杯なら最古から上書き） | 48 h まで、音が出続けても保証 |
| 完全オフライン動作 | 保証しない（数時間級の buffer） | 48 h（縮退モードを使ってでも守る） |
| 充電作業 | 不要（電池交換） | 原理上不要を狙う（Solar） |
| 小型 | φ40〜42 mm、厚さ 15 mm 以内 | φ40〜42 mm、厚さ 18〜22 mm 級 |
| 低消費電力 | 約 1 年の電池寿命（目標） | rail 側の平均 0.3〜0.5 mW（目標、現行 I²S 実機は未測定） |
| 故障の独立 | 共通（§1.4） | 共通（§1.4） |

要求 3・5 は Premium が満たす。Standard は、スマホを常に携帯する前提を受け入れる代わりに、要求を意図して緩める（§2）。

### §1.4 故障の独立（確定）

各サブシステムは互いに独立させる。録音・保存・通信・発電を、別々の能力として扱う。

```text
Flash の故障        ≠ MCU の故障
BLE の故障          ≠ 録音の故障
スマホの不在        ≠ 録音の故障
Solar が無い        ≠ システムの故障（Premium）
cap の残量が低い    → CR2032 へ退避（Premium）
```

特に、**ストレージの異常を理由に、BLE や本体全体を止めない。**

### §1.5 しないこと（確定）

- 常時接続のリアルタイム転送。
- ファイルシステム（[storage.md §1.3](./storage.md)）。
- 赤い終端表示で「録音中」を保証すること（[terminal-indicator.md §2](./terminal-indicator.md)）。
- 前面ボタンによる FW の一時停止（録音の一時停止は privacy latch が担う。§4.3）。

## §2 Standard と Premium の分岐

**StandardとPremiumを、別の要求セットとして扱う（確定）。**

| | Standard | Premium |
|---|---|---|
| 位置づけ | smartphone-required + CR2477 + 小容量 NAND の rolling buffer | smartphone-independent + 大容量 NAND + Solar / HSL の自律電源 |
| 売りの特徴 | 充電不要で、電池交換は約 1 年に 1 回 | スマホを忘れても記録が続く。充電を意識しない |

### §2.1 分岐の理由

要求（スマホ不在でも 48 h 録音する、1.8 V 単一 rail、小型、sub-mW、Solar で充電作業を無くす）を 1 つのハードで全部満たそうとすると、4 Gbit 級の 1.8 V NAND と、Solar / BQ25505 / 120 F HSL / CR2032 の非常用電源が要る。UX は強いが、BOM・部品点数・筐体容積・電源の検証項目が増える。

通常利用でスマホを常に携帯する前提を受け入れれば、Standard に 48 h の保存は要らない。そこで、スマホをシステムの構成要素として認め、storage と power の要求を意図して緩めた別 SKU にする。Standard は「Premium から部品を外しただけ」のものではない。

Standard で削れるもの: 4 Gbit の NAND、120 F HSL、BQ25505、Solar の配線、CR2032 の非常用電池、power mux。BOM だけでなく、基板の面積・筐体・故障点・試験項目も減る。

Premium が残すもの: スマホを忘れても記録が続く、長時間のオフライン、充電を意識しない、Solar を前面の意匠とボタンに使う。

Solar は全 SKU に必須の機構ではなく、**Premium を成立させる機能・意匠・技術的特徴**として扱う。

### §2.2 差を電源・保存・筐体に限る（確定）

Bluetooth の構成は、Standard と Premium で同じにする。2 つの差は電源・保存・筐体だけにする。理由: Bluetooth SIG の Declaration は、同じ Design を別製品に使うと 2 件目の追加料金が $0 になる見込みで、認証を 1 回で済ませられる（[../plan/compliance.md](../plan/compliance.md)）。

FW は可能な限り共通にし、SKU の差は次の 3 つの backend として分離する。

```text
共通: recorder / BLE / timestamp / app protocol
   ├─ Standard: NAND ring（小容量）+ CR2477 の電源方針
   └─ Premium : NAND ring（4 Gbit）+ Solar / HSL / CR2032 の電源方針
```

NAND のドライバと ring は両 SKU で同じで、違いは容量と電源の方針だけ（[storage.md §1.2](./storage.md)）。

### §2.3 作る順番（確定）

通常の dogfooding は両 SKU を作り、知人に好きな方を選んで使ってもらう。**2026-10-23 の優先順位変更により、11 月中旬のハッカソン向け Premium は Standard 完成を待たず先行する。** 共通コアの検証と電源・機構を並行する。実発電する Solar、外観、HSL＋非常用ボタン電池を必須とし、USB 給電は設けない（[Premium 試作計画](../plan/premium-hackathon.md)）。これは 48 h／72 h の製品検証を完了したという意味ではない。

## §3 共通コア

両 SKU で共通にするもの（確定。差が出る項目は §6 の BOM 表）。

| 項目 | 内容 | 仕様 |
|---|---|---|
| MCU / BLE | Nordic nRF54L15。dogfooding の本筋は Fanstel BC15M モジュール（自宅リフロー、1.8 V の rail で直接動かす）。XIAO nRF54L15 は開発専用 | [board.md](./board.md)、[../plan/prototype.md](../plan/prototype.md) |
| マイク | TDK T5848（I²S 出力）を 1 個。AAD で待ち、音で起こす（運用は方針。T5838 から替えた理由は [decisions.md H7](../decisions.md)） | [audio.md](./audio.md) |
| system rail | TI TPS62840 で 1.8 V。1.9 V 化は未決 | [power.md §2](./power.md) |
| ストレージ | 1.8 V の生 SPI NAND。容量だけが違う | [storage.md](./storage.md) |
| 録音形式 | 8 kHz・mono・IMA-ADPCM 32 kbps。上限は 32 kbps。縮退側は 16 kbps 級（Premium） | [audio.md §5・§6](./audio.md) |
| BLE 同期 | 約 2 時間ごとの短い advertise とバースト転送、累積 ACK | [formats.md](./formats.md) |
| 時刻 | GRTC で数え、スマホ同期のたびに遡って時刻を付ける。外付け RTC（RV-3032-C7）は footprint だけ | [firmware.md §5](./firmware.md) |
| アンテナ | BC15M モジュールの PCB アンテナ（認証済みの RF 経路のまま使う。基板に整合・selector は載せない）。送信は +4 dBm（アンテナ利得込み）以下 | [rf.md §2・§3](./rf.md) |
| 書込み・更新 | 出荷前に MCUboot を書き、以降はスマホから BLE DFU。SWD は開発と救済用 | [firmware.md §2・§3](./firmware.md) |
| セキュリティ | LESC + OOB、機器名を広告に載せない、保存データを暗号化 | [security.md](./security.md) |
| 終端表示 | 赤 ⇄ 黒の双安定フラグ（電源が無くても保持）。検出する電池だけが違う | [terminal-indicator.md](./terminal-indicator.md) |
| privacy latch | 録音の物理的な停止（§4.3） | 下記 §4.3 |
| 筐体の思想 | 非金属・円形・前面ボタン・膜防水・ePTFE のマイク vent・汗を主な環境条件にする | [mechanical.md](./mechanical.md) |
| 測定 | 電流測定は INA228 の治具に固定。基板に rail 別の 0 Ω と test point を置く | [../test/current-measurement-fixture.md](../test/current-measurement-fixture.md)、[board.md](./board.md) |

## §4 UX

### §4.1 Standard

**スマホ必携。充電不要。電池交換は通常利用で約 1 年に 1 回（目標。保証値ではない）。**

- 電池は CR2477 を 1 個、ユーザーが背面から交換する。
- 録音は短時間だけ本体の NAND に溜め、約 2 時間ごとにスマホへ同期する。スマホが一時的に無くても buffer の容量までは保持する（512 Mbit なら 32 kbps・p = 1 で約 4.5 h、1 Gbit なら約 9.0 h。[storage.md §7.1](./storage.md)）。**48 h は保証しない。** 満杯なら最古から上書きし、欠落区間をスマホへ通知する。
- 電池が減ると、終端の前にスマホへ通知する（LOW 段）。スマホが近くに無いときは、赤 ⇄ 黒のフラグが代わりになる（[terminal-indicator.md §12・§13](./terminal-indicator.md)）。
- 電池を換えると、起動の門を越えたところで、フラグが黒から赤に戻る。

### §4.2 Premium

**スマホが無くても録音が続く。48 h 級のオフライン buffer を保証し、Solar で通常は充電作業が要らない。**

**11 月のハッカソン用デモ機は例外:** 短時間の実演を目的に 512 Mbit の F35UQA512M-WWR を設計採用する。48 h 保存はデモ機で保証しない。上記は製品目標として残す（[試作計画](../plan/premium-hackathon.md#63-調達の現状と優先順位)）。

- 前面の Solar パネルが、そのまま押せるボタンになる（[mechanical.md](./mechanical.md)）。
- 通常は Solar と 120 F の cap だけで動く。日照が足りず cap が減ると、非常用の CR2032 に自動で退避し、cap が回復すると戻る。
- 48 h の保証は、バックログに連動したビットレート（32 → 16 kbps）で守る（[storage.md §7.3](./storage.md)）。
- 「永久に動く」とは保証しない。成立は、胸元での Solar の実発電量と実消費電力で検証する。

### §4.3 両 SKU に共通する UX

- **初期設定**: 出荷前に MCUboot とアプリを書いた状態で渡す。LESC + OOB でペアリングし、初期設定が終わるまでマイクを動かさない（ペアリング前の録音は保存しない）。手順は [security.md §6](./security.md)。
- **終端表示**: 通常は赤が見える。電池が終端に達する直前に黒へ反転し、全電源を失っても黒のまま。赤は「電源系が動作可能」の意味で、「録音中」ではない。
- **前面ボタン**: 前面を大きなボタンにする。押下は event レコードに記録するだけ（録音の一時停止には使わない。[audio.md §3.5](./audio.md)）。
- **privacy latch（確定: 採る。機構は未決）**: スイッチが PRIVATE のとき、**FW が何をしても**マイクが音を拾えない。条件: スイッチの状態を GPIO で読まない。マイクの VDD を切るだけでは足りない（T5838 のデジタル入力は VDD + 0.3 V まで。T5848 も同じ扱いにする。VDD = 0 V で SCK を駆動し続けると絶対最大定格を超える）。SCK / WS / THSEL を駆動する側の電源も latch の先から取る。FW への影響: latch を戻すとマイクは初期状態（AAD 無効）に戻るので、FW は定期的にマイクを起こして AAD を設定し直す。切っていた間の欠落は欠落レコードで残す。機構（スライドか磁気式）は、封止の穴を作らない案を筐体と一緒に決める。
- **周囲の人への表示**: 筐体・箱・説明書に、常時録音であることを書く（[security.md §9](./security.md)）。

## §5 全体の目標

### §5.1 一覧

| 目標 | 値 | 確度 |
|---|---|---|
| 同期周期 | 約 2 時間 | 確定 |
| Standard の電池寿命 | 約 1 年（交換まで） | 方針。目標であり保証値ではない。成り立つ条件は §5.2 と §8 |
| Standard の buffer | 512 Mbit で約 4.5 h、1 Gbit で約 9.0 h（32 kbps・p = 1）。容量は未決 | 方針 |
| Premium の 48 h オフライン | スマホ不在 48 h を、p = 1 でも保持する | 確定（4 Gbit は、縮退側の 16 kbps が ASR で使えることが条件） |
| Premium の平均電力（rail 側） | 最終目標 0.3〜0.5 mW。Phase 1 以降のスクリーニングは 0.8 mW 未満（72 h の保証とは別） | 方針 |
| Premium の無日照運転 | **CR2032 を使わず 72 h**（容量下振れ・劣化・温度・しきい値誤差・実測の効率と負荷を含めて） | 確定（Go/No-Go。数値は仮置き） |
| Premium の Solar 収支 | 屋外で、panel 端子の実効平均 5〜20 mW でも成立する。消費分の回収は別に確認する | 仮置き（胸元の実発電は未測定） |
| 録音のビットレート上限 | 32 kbps | 確定 |
| BLE の送信電力 | +4 dBm（アンテナ利得込み）以下 | 確定 |
| 外径 | φ40〜42 mm | 方針 |
| 厚さ | Standard 15 mm 以内（12 mm 側は届かない）、Premium 18〜22 mm 級 | 方針（§5.3） |
| 質量 | Premium は約 15〜22 g | 方針 |
| 終端表示 | 両 SKU にある | 確定 |

### §5.2 電力と電池寿命の目標

- Premium の式・無日照運転・Solar 収支は [power.md §7](./power.md) が持つ。rail 側 0.8 mW だけで 72 h を保証しない。今回のしきい値候補の設計 corner は、効率 .9・固定 37 µW の仮定で 0.8 mW なら約 59.4 h、0.5 mW なら約 92.8 h。温度・実効容量・追加部品・ピークを含む実機の最悪保証ではない。
- 旧モデルでは BLE 同期が大きな項だが、87 mJ/MB は別条件の代理値で、現行 I²S 全体の負荷は未測定。受信 duty／保存 duty／同期量を分け、保存量を減らすだけで clock・mic の電力が下がるとは扱わない。
- Standard の電池寿命の第一近似は、32 kbps で p の上限が 0.36〜0.38（[power.md §7.6](./power.md)）。約 1 年を満たす条件（ビットレート、p）は、NAND の待機を入れて出し直す（#17）。

### §5.3 外形と厚さ

- 外径は Standard・Premium とも φ40〜42 mm 級。円形ペンダントで、前面が押せるボタンになる。
- Standard は **15 mm 以内**（方針）。BC15M の基板を電池の脇に置く並置の想定は 14.5 mm で、余裕は 0.5 mm。電池の上に重ねると想定 15.2〜15.3 mm で超える。12 mm 側は、全層が最小でも 12.4 mm で届かない（[mechanical.md §7.4・§7.5](./mechanical.md)）。ボタンは前面に残す（厚さより操作性を取る。確定）。XIAO を載せる積層（実測 3.33 mm）は想定 16.8 mm で、開発用の参考値にとどまる。
- Premium は 18〜22 mm 級（想定 19.6 mm）。Solar パネル（φ36 × 約 2〜3 mm）、120 F HSL（φ12.5 × 25 mm、横置きで厚さ 12.5 mm）、CR2032（φ20 × 3.2 mm）、PCB を収める。最終の厚さは 3D 配置で決める（[mechanical.md §7.6](./mechanical.md)）。
- AirTag 互換の外形は要件ではなく、採るかは未決（E1。中央 12 mm 級にも入らない。[mechanical.md §8](./mechanical.md)、#24）。

## §6 BOM の概要

主な部品。型番が「候補」のものは、評価で決める。価格と在庫は変動が大きいので書かない。

### §6.1 共通

| 部品 | 型番・仕様 | 確度 |
|---|---|---|
| MCU モジュール | Fanstel BC15M（nRF54L15、CSP47 版、6.0 × 9.6 × 1.9 mm、LGA 36 パッド、技適 201-250527、FCC X8WBC15、VDD 1.8〜3.6 V（表紙は 1.7 V）。32 MHz と 32.768 kHz の水晶、DC/DC のインダクタはモジュールに内蔵）。予備は u-blox NORA-B206（初版では使わない）。https://www.nordicsemi.com/Products/nRF54L15 、https://fanstel.squarespace.com/s/BC15M-Product-Specifications-675g.pdf | 確定（VDD の下限は試作で確認） |
| マイク | TDK T5848（MMICT5848-00-012、I²S、3.5 × 2.65 × 0.98 mm、8 pad）× 1。出典 TDK DS-000479 Rev 1.0（[audio.md §2](./audio.md)） | 確定 |
| 降圧 | TI TPS62840（1.8 V、静止電流 60 nA typ）。https://www.ti.com/product/TPS62840 | 確定 |
| NAND | 1.8 V 生 SPI NAND × 1（WSON-8 8×6 の共通の足跡）。製品は Standard 512 Mbit〜1 Gbit、Premium 4 Gbit 基準。11 月の Premium デモ機は F35UQA512M-WWR、512 Mbit。[storage.md §2.3](./storage.md) | 製品は方針。デモは設計採用、未実測 |
| RTC | Micro Crystal RV-3032-C7 の footprint。載せるかは実測で決める | 確定（載せるかは仮置き） |
| アンテナ | モジュール内蔵の PCB アンテナ。strap・selector・RF test の経路は、裸チップまたは比較用に使う場合だけ（[rf.md §6](./rf.md)） | 確定 |
| load switch | TI TPS22917DBVR（Standard の現回路と共通。DPD 未確認／無しの品は power gate を検証） | 方針 |
| 終端表示 | 双安定の機械式 / 磁気式フラグ、電圧 supervisor（TPS3839 級）、last-gasp コンデンサ | 方針 |
| 電池電圧の測定 | スイッチ付き分圧器 + SAADC（Standard は電池、Premium は cap） | 方針 |
| 書込み・測定 | SWD のパッド、rail 別の 0 Ω リンク、test point。INA228 の治具は基板の外 | 確定 |
| privacy latch | スイッチ（機構未決）+ マイクの電源とデジタル信号を切る回路 | 方針 |
| 筐体 | 非金属（PC+ABS / PC / ASA 系）の円形ケース、膜の前面ボタン、ePTFE のマイク vent、背面の gasket | 方針 |

### §6.2 Standard だけ

| 部品 | 型番・仕様 | 確度 |
|---|---|---|
| 電池 | CR2477 × 1（Jauch CR2477J 1000 mAh、Renata CR2477N 950 mAh）、ユーザー交換式のホルダ | 確定（品番は仮） |
| reservoir | 220 µF 級を第一候補、footprint は 100〜470 µF | 方針 |
| NAND | 512 Mbit〜1 Gbit | 未決 |
| 終端表示の検出 | CR2477 用の supervisor と last-gasp 回路（Premium の値は使えない） | 方針 |
| 逆流防止 | footprint のみ（Schottky / LM66100 / 0 Ω）。載せるかは運用次第 | 方針 |

### §6.3 Premium だけ

| 部品 | 型番・仕様 | 確度 |
|---|---|---|
| Solar | 前面で実発電する panel。従来の φ36 mm / 2 V / 80 mW 級は目安で、メーカー型番・Voc・納期は未固定。[調達候補](../plan/premium-hackathon.md#6-solar-と調達の調査) | 機能は必須、型番・実発電は未確定 |
| harvester | TI BQ25505（MPPT + 充電 + 一次電池用 mux の gate driver）＋TPS62840。今回の試作は BQ25570 へ変更しない | 方針（[比較根拠](../plan/premium-hackathon.md#3-電源構成と-pmic-の判断)） |
| 主蓄電 | Eaton HSL1225-3R8127-R（120 F、最大定格 3.8 V、最低定格 2.5 V、最低許容 2.2 V、φ12.5 × 25 mm 公称、約 5.4 g）。外形公差・リードを別に見る | 確定 |
| 非常用電池 | Panasonic CR2032（225 mAh）。https://energy.panasonic.com/jp/business/products/lithium/coin-cr-standard/models/CR2032 | 確定 |
| power mux | common-source back-to-back PMOS **2 ペア＝4 素子／dual package 2 個**。secondary は VSTOR から。CSD75208W1015 ×2 を第一評価候補にする | 接続方針、品番・実装・一次電池の逆流保護は要検証 |
| OV の抵抗 | ROV1 = 6.34 MΩ、ROV2 = 6.49 MΩ（0.1 %、BQ25505 を採った場合。OV 3.673 V） | 確定 |
| NAND | 製品は 4 Gbit 基準。11 月デモ機は 512 Mbit F35UQA512M-WWR | 製品方針／デモ設計採用、実装・実測は未実施 |
| Solar ボタンの給電 | 金メッキの spring contact × 2 | 方針 |
| CR2032 運転中の bulk | 位置・容量は未確定 | 未決 |

電源の詳細は [power.md §3](./power.md)。

## §7 初号機の Go/No-Go

初号機の最低条件（11 項目。対象 SKU つき）は [../plan/prototype.md §9](../plan/prototype.md) が持つ。製品としての要点は次のとおり。

- Premium の電力は、**CR2032 を使わず無日照 72 h** を守れることで判定する。屋外での消費分の回収は別に確認する（§5.1、[power.md §7.4・§7.5](./power.md)）。
- Standard は、NAND を含む電力モデルで、約 1 年の条件（ビットレート、p）が成り立つこと（§8）。
- 両 SKU で、ASR が成立すること、胸元で BLE 同期が安定すること、終端で黒に反転したフラグが電源 0 V でも保持されること、OTA の中断・電源断・SWD 救済で brick もログの喪失も起きないこと。

## §8 Standard の電池寿命と CR2477 の容量 3.0 Wh の仮定

Standard の電池寿命の見通し「約 1 年」は、CR2477 を **3 V × 1 Ah ≒ 3.0 Wh** と置いた単純計算（旧 doc 22 の見積り。p ≈ 0.4 で約 1 年）から出た（確度: 仮置き）。`tools/standard_power.py` は、この 3.0 Wh を上限寄りの比較列として持つ。

| 容量の置き方 | エネルギー | 根拠 |
|---|---:|---|
| 3.0 Wh（上の単純計算） | 3.0 Wh | 公称 3 V × 1 Ah。上限寄り |
| 1000 mAh、2.0 V 終端 | 2.9 Wh | Jauch CR2477J の放電曲線から読んだ平均 2.9 V（粗い値）。[power.md §7.6](./power.md) |
| 950 mAh、2.0 V 終端 | 2.76 Wh | Renata CR2477N |
| 950 mAh、2.40 V 終端 | 約 2.6 Wh | 2.40 V の段までしか使えない場合は容量の 95 %（仮置き） |

`tools/standard_power.py --nor 15`（NAND 15 nJ/B、BLE 87 mJ/MB、32 kbps）の出力。電池側の平均電力は、p = 0.25 / 0.4 / 0.6 / 1.0 で 230 / 339 / 484 / 774 µW。

| p | 3.0 Wh | 2.9 Wh（1000 mAh） | 2.76 Wh（950 mAh） |
|---:|---:|---:|---:|
| 0.25 | 536 日 | 518 日 | 493 日 |
| 0.40 | 365 日 | 353 日 | 336 日 |
| 0.60 | 257 日 | 248 日 | 236 日 |
| 1.00 | 161 日 | 155 日 | 148 日 |

- 365 日を満たす p の上限は、3.0 Wh で 0.40、実際の容量では 0.36〜0.38。**3.0 Wh は、約 1 年が見える上限であり、保証の根拠にしない。**
- 上の表は NAND の待機（DPD の有無と電流）を含まない。DPD の無い品で 10〜20 µA なら、電池側で +18〜36 µW になり、p の上限は下がる。
- 約 1 年の目標は、NAND 前提で成り立つ条件（ビットレート、p）を出し直して確定する（[power.md §7.6](./power.md)、#17）。16 kbps に落とすと p の上限は上がる（再計算は #17）。
- 実際の寿命は、保存デューティ p、BLE の転送量、温度、パルス負荷、セルの実容量で決まる。

## §9 提供形態

- **販売形態は保留（確定）。** SIG と試験所から回答が来るまで決めない。XIAO を同梱するキットは採らない（開発専用）。はんだ付けが要らない組立形態（K1）は、販売形態が決まるまで扱わない。経緯と調査は [../plan/compliance.md](../plan/compliance.md)。
- 販売地域は日本と海外。使うモジュールは、技適・FCC・CE などを取得済みであること（確定）。
- 筐体・箱・説明書に常時録音であることを書く。販売表示と規約の法的確認は、日本と海外で専門家に取る（[security.md §9](./security.md)）。
- 知人への無償配布（dogfooding）は、SIG への問い合わせには含めない。

## 未確定

| 項目 | 状態 |
|---|---|
| 販売形態（認証済みのモジュールの扱い、SIG の Declaration、CE の製造者責任、FCC の条件）。SIG・試験所への書面確認 | #15、[../plan/compliance.md](../plan/compliance.md) |
| Standard の NAND 容量（512 Mbit か 1 Gbit） | 新規 issue 候補: NAND 型番の選定（Standard 512 Mbit〜1 Gbit / Premium 4 Gbit） |
| Standard の電池寿命（約 1 年）を NAND 前提で出し直す。16 kbps での p の上限 | #17、#27 |
| Premium の電源 IC と電源回路 | 今回は BQ25505＋TPS62840。しきい値・逆流保護・source 信号変換は [Premium 試作計画](../plan/premium-hackathon.md) の評価候補 |
| AirTag 互換の可否（E1）。Standard の Z の実測（並置の想定 14.5 mm、余裕 0.5 mm） | #24、[mechanical.md §7.5・§8](./mechanical.md) |
| privacy latch の機構（スライドか磁気式）と、封止との両立 | 新規 issue 候補: privacy latch の機構（[mechanical.md](./mechanical.md)。配線部品は [board.md §6.1](./board.md)） |
| 要求する時刻精度（スマホに同期しない最長の区間で、何秒まで許すか） | 新規 issue 候補: 時刻精度の要求値（[firmware.md §5.4](./firmware.md) の RTC 判断に使う） |
| Standard が縮退モード（バックログ連動ビットレート）を使うか | 新規 issue 候補: Standard のビットレート連動の適用 |
| ASR が 8 kHz ADPCM と縮退側 16 kbps で成立するか（成立しなければ Premium は 8 Gbit） | #19 |
| 実生活での保存デューティ p | #20 |
| 既製ビーコンケースの流用の可否と商標 | #16 |
| 常時録音の表示・規約の法的確認 | 新規 issue 候補: 常時録音の表示・規約の法的確認（#16 の販売表現と併せる） |
