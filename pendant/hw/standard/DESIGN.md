# Standard 基板（Fanstel BC15M）回路図 rev 0 の設計根拠

対象: `standard.kicad_sch`（`gen_schematic.py` が、接続の表 `parts.py` と配置 `layout.py` から生成。自作シンボルは `pendant.kicad_sym`）。dogfood の初版で、Standard（CR2477、1.8 V 単一 rail）の基板。仕様の正本は `docs/spec/`。この文書は「回路図のどの部品・接続を、なぜそう選んだか」を記録する。

- **状態:** rev 0（2026-10-16、モジュールを Fanstel BC15M に替えたのは 2026-10-18、決定 X3）。ERC 0 件。75 部品。**生成のたびに、KiCad が出したネットリストを `parts.py` と突き合わせ、全ネットのピンの組が一致することを確かめている**（一致しなければ失敗で止まる）。
- **描き方:** **信号はすべて配線（wire）でつなぐ。** 電源だけは電源シンボル（GND、VBAT、VBAT_SYS、1V8、NAND_1V8、MIC_1V8、RTC_VDD、VLG）。モジュールの GPIO は右側に、NAND → マイク → 電池電圧 → RTC → 終端表示の順に並べ、各ブロックを同じ順で右側に積んである（電源・SWD・UART・マーカーは左側）。モジュールのピンから出る線には、読みやすさのためにネット名のラベルも付けている。配線は 2.54 mm グリッド上の最短経路探索で自動で引き、他のネットとは直交で横切るだけ（その点で曲がらない、端点・ピンに触れない）にしている。
- **再生成:** `~/.venvs/kicad/bin/python hw/standard/gen_schematic.py`。部品の位置は `layout.py` で動かす。配線できないネットがあると `UNROUTED` と表示し、ネットリストの照合で失敗する。**eeschema で手を入れたあとに再生成すると上書きされる。** 手で編集を始めたら `parts.py` / `layout.py` は使わない（または変更を両方に戻してから再生成する）。
- **確認:** `kicad-cli sch erc --severity-all --format json -o erc.json hw/standard/standard.kicad_sch`。
- **DNP:** 部品は載せないが footprint は置くもの。RTC 一式、LM66100、220 µF、終端表示の回路一式、R16（SELECT を VDD 側にする 0 Ω）。

確度の凡例: **確定** = 仕様（docs/spec）または一次資料で決まっている。**判断** = この回路図で私が選んだ（理由を書く）。**仮** = 部品や値が未定で、置き場だけ作った。

---

## 1. ブロック図

```text
CR2477 ─R1(R_BAT)─┬─R2(0Ω) / U1 LM66100(DNP)─ VBAT_SYS ─┬─ C1 100µF（C2 220µF DNP）
                  TP1                                   ├─ U2 TPS62840 → 1V8_BUCK ─R4(R_MAIN)─ 1V8
                                                        ├─ U9 TPS22917 → 分圧 1M/1M → AIN0（電池電圧）
                                                        └─ (DNP) 終端表示: U11 監視 + U12 DRV8837 + C15
1V8 ─┬─ U3 BC15M（VDD。32 MHz / 32.768 kHz の水晶と DC/DC のインダクタは内蔵）
     ├─ U4 TPS22917 → R6(R_NAND) → NAND_1V8 → U5 SPI NAND
     ├─ U6 TPS22917 → R14(R_MIC) → SW1(privacy) → MIC_1V8 → U7 バッファ / U8 T5848
     └─ (DNP) R20(R_RTC) → U10 RV-3032
```

## 2. 部品とピン表の出典

ピン表は自作シンボルに写した。**全部品に footprint が付いている。** 標準ライブラリに無いものは `pendant.pretty` に作った（BC15M と T5848 は `gen_footprints.py` がメーカーの数字から生成、電池ホルダは手書き）。 標準ライブラリにある部品は KiCad 10.0.5 の標準シンボルを使った。

| Ref | 部品 | 資料（版） | fp | 注意 |
|---|---|---|---|---|
| U3 | Fanstel BC15M（nRF54L15、CSP47 版。PCB アンテナ、32 MHz / 32.768 kHz 水晶と DC/DC インダクタ内蔵） | Fanstel データシート Ver 1.05（2026-09、ピン表 p.11、図面 p.7、取付 p.12）、Eagle ライブラリ BC15M-V3-LIB.lbr、Nordic nRF54L15 PS v1.0 表 81（CSP47） | `pendant:Fanstel_BC15M`（Eagle の数字から生成。アンテナ部の keep-out 付き） | 36 パッドの LGA（6 × 6、ピッチ 0.9 mm、Ø0.4）。外周 16 個は端まで出るが、B1〜E5 の 20 個はモジュールの下。GND は E1・F1 の 2 つだけ。6.0 × 9.6 × 1.9 mm |
| U2 | TI TPS62840DLCR | SLVSEC6D（2020-03） | `Package_SON:Texas_VSON-HR-8_1.5x2mm_P0.5mm` | DLC は VSON-HR（SON-8）。ピッチ 0.5 mm |
| U4、U6、U9 | TI TPS22917DBVR | SLVSDW8B（2021-12） | SOT-23-6 | アクティブ High。**TPS22917L はアクティブ Low なので誤発注しない** |
| U5 | Lexar F35UQA512M-WWR（代替: MX35UF1GE4AD-Z4I6、GD5F1GQ5REYIGR。ピン同一） | Lexar Rev 1.1（2022-02）、Macronix REV 1.7（2026-02）、GD Rev 1.4（2021-12） | WSON-8 8×6 | EP の接続先はどのデータシートにも無い（§3.4） |
| U8 | TDK MMICT5848-00-012（T5848、I²S、AAD 付き） | DS-000479 | `pendant:TDK_T5848_LGA-8_3.5x2.65mm`（Fig. 3 / 33 から生成。音孔 φ0.6 mm） | **パッドとピンの対応は図からの読み取り**。TDK の STEP か実物で確認する。底面ポート。レジスタは T5838 と同じ（decisions.md H7） |
| U7 | TI SN74AUP3G34DCUR | TI データシート（ti.com/lit/ds/symlink/sn74aup3g34.pdf） | `Package_SO:VSSOP-8_2.3x2mm_P0.5mm`（DCU） | 3 回路。Ioff 0.6 µA max |
| U10 | Micro Crystal RV-3032-C7 | データシート v1.3（2022-02） | `Package_SON:MicroCrystal_C7_SON-8_1.5x3.2mm_P0.9mm`（DNP） | Application Manual は未読 |
| U12 | TI DRV8837DSGR | 標準シンボル DRV8837C（同ピン） | 標準 | DNP |
| U11 | 監視 IC（アクティブ High のプッシュプル出力。仮に TI TPS3840PH） | SNVSB04（DBV のピン: 1 RESET、2 VDD、3 GND、4 MR、5 CT） | SOT-23-5 | DNP。MR と CT は開放（MR は内部プルアップ、CT 開放で最短の遅延）。閾値の品番は未定。§3.8 |
| U1 | TI LM66100DCK | 標準シンボル | SC-70-6 | DNP |
| L1 | Murata DFE201612E-2R2M=P2（2.2 µH） | TI の標準部品表 | `Inductor_SMD:L_Murata_DFE201610P` | 同じ 2016 の DFE201610P の land を流用。DFE201612E の推奨 land と実物で確認する |

## 3. ブロックごとの根拠

### 3.1 電池・逆流防止・reservoir

- **R1（R_BAT、0 Ω）**: 電池電流の測定リンク（board.md §5、確定）。INA228 の治具をここに入れる。
- **R2（0 Ω）と U1 LM66100（DNP）を並列**: 逆流防止は footprint だけ（確定、決定 D6）。BC15M の基板には USB が無く、充電経路の逆流は無いので、初版は 0 Ω（判断）。LM66100 は CE を GND（常時 ON）、ST は未接続。
- **C1 100 µF 0805（実装）+ C2 220 µF 1206（DNP）**: reservoir は 220 µF 級が第一候補（決定 B7）。ただし 220 µF 1206（GRM31CR60J227ME11L）は DigiKey 在庫 0（2027-02 入荷予定）。漏れの小さい MLCC を優先する規則（board.md §6.3）に沿って、初版は入手できる 100 µF を載せ、220 µF の footprint を並べた（判断）。DC バイアスで実効容量は下がるので、実測で決める（power.md §4.4）。
- **BT1 + ホルダ MYOUNG MY-2477-01**（LCSC C2979173、決定 H4）: CR2477 用のスルーホールのクリップ。外形 26.8 × 22.6 mm で、電池（Ø24.5）からはみ出すのはピンの所だけ（パッドの外端の間 28.5 mm）。＋はクリップの 2 本のピン（間隔 25.5 mm、穴 Ø1.85）、−は電池の中心の下の Ø8 mm のパッド（同じ系列の MY-2477-01-J の図。ENIG にする）。高さ 8.5 mm、1.4 g。0.33 ドル（5 個以上、2,500 個で 0.18 ドル）、LCSC 在庫 825（2026-10-17）。データシート: https://www.lcsc.com/datasheet/C2979173.pdf
  - **SMT 品を使わない理由**: CR2477 の SMT ホルダ（MPD BRX1-2477-SM 1.07 ドル、MYOUNG MY-2477-02、Renata SMTU2477N）はタブが両横に張り出して幅 37 mm。Ø31.9（AirTag 互換、mechanical.md §7.4a）に入らない。
  - **外形と厚さ**: 幅 28.5 mm は Ø31.9 の内径（約 29.9 mm）にぎりぎり入る。厚さは電池 7.7 + ばね 0.3 = 8.0 mm の所が 8.5 mm になり、+0.5 mm（§7.4a の想定 12.55 → 約 13.05 mm）。
  - **スルーホールの注意**: ピンが反対面（モジュール側）に 3.3 mm 出るので、はんだ付け後に切る。リフローの後に手はんだ。

### 3.2 1.8 V 降圧（TPS62840）

- **VSET → GND で 1.8 V**、**MODE → GND**（Power-Save、IQ 60 nA）、**STOP → GND**（High にすると VIN の静止電流が typ 70 µA になる）。いずれも TI のデータシートの表 1・表 7.5 による。
- **EN は R3（0 Ω）で VIN へ**（判断）。常時 ON。後で起動の門（2.70 V 仮置き、power.md §4.2）を監視 IC で作るときに、R3 を外して EN を監視 IC につなげるようにした。
- **入力 4.7 µF、出力 10 µF、L1 2.2 µH**: TI の標準部品表。出力の GRM155R60G106ME44D は DigiKey で取扱い終了なので、YAGEO CC0402MRX5R4BB106 を値に書いた（DigiKey が代替として表示。定格は要確認）。
- **R4（R_MAIN、0 Ω）**: 1.8 V rail の電流測定リンク（確定）。TP3（降圧の出力側）と TP4（rail 側）で両側を当てられる。
- **入手性**: TPS62840DLCR は DigiKey JP 在庫 0（2026-11-24 入荷予定）、LCSC に在庫あり（hw/standard/fab/README.md、C2071859）。

### 3.3 モジュール（BC15M）

- **外付けの水晶・インダクタは無い**: 32 MHz と 32.768 kHz（20 ppm）の水晶、DC/DC のインダクタ、VDD のコンデンサがモジュールに入っている（Fanstel、確定）。DECD・DCC・XL1・XL2 はパッドに出ていない（P1.00 / P1.01 は内蔵の水晶が使う）。
- **VDD に 4.7 µF（C5）+ 100 nF（C6）**: モジュールに VDD のコンデンサは入っているが、1.8 V rail の受けとして残した（判断。要らなければ DNP にする）。
- **VDD の下限**: データシートの表紙は 1.7〜3.6 V、ピン表は 1.8〜3.6 V で食い違う。rail は 1.8 V −1.5 %（1.773 V）まで下がる。中のチップは 1.7 V から動くので、試作で 1.75 V 付近まで下げて動作を確かめる（決定 X3）。
- **FW の設定**（Fanstel のデータシートと注意書き）: 内蔵の負荷容量を HFXO 17 pF、LFXO 7 pF に設定する（`&hfxo` / `&lfxo` の `load-capacitors = "internal"`）。32 MHz 水晶の起動待ちを `mpsl_clock_hfclk_latency_set(1400)`（新しい NCS では `CONFIG_MPSL_HFCLK_LATENCY`）に戻す。戻さないと接続が切れることがある。DC/DC は FW で有効にする。
- **RESET**: チップ内でプルアップ。J1（SWD のポゴパッド）に出す。

### 3.4 NAND

- **U4 TPS22917 で NAND の rail を切る（必須）**: 候補 3 品のデータシートに **DPD（Deep Power-down）の命令が無い**（待機 Lexar 10 µA typ / 50 µA max、GD 50 µA max、Macronix 110 µA max）。storage.md §2.4 の基準 3（DPD あり）を満たさないので、ゲートが必須になる（power.md §5.1 の「DPD の無い品はゲート必須」）。
- **TPS22916 ではなく TPS22917**: TPS22916 は 0.78 mm 角の WCSP（バンプピッチ 0.4 mm）で、自宅リフローには向かない。TPS22917 は SOT-23-6 で、IQ 0.5 µA、OFF 10 nA、RON 120 mΩ（1.8 V）と数値が同じ（判断）。
- **QOD → VOUT**（NAND の rail を OFF で確実に放電する。power.md §5.1）、**CT は開放**（最速。突入は NAND の 4.7 µF の充電で決まる）。
- **R5 1 MΩ プルダウン（NAND_EN）**: ON ピンは浮かせてはいけない（TI）。P2 の出力が System OFF で保持されるかが Nordic の資料から読めないので、既定を OFF にする（判断）。
- **R6（R_NAND、0 Ω）**: NAND の電流測定リンク（確定）。
- **WP# / HOLD# / CS# を 100 kΩ で NAND_1V8 にプルアップ**: x1 SPI では QE = 0 なので WP# と HOLD# は入力として生きており、HOLD# が Low だと動作が止まる（3 社のデータシート）。どれも浮かせない。CS# は電源投入時に VCC に追従させる（Macronix §13-1）。プルアップの先を切り替え後の rail にしたので、NAND を切ったときにプルアップから電流は流れない（判断）。
- **R10〜R12（0 Ω）**: SCK / MOSI / MISO の直列 footprint。反射が出たら 22〜47 Ω に替える（storage.md §3）。
- **EP（ピン 9）は未接続**: 3 社とも GND か NC かを書いていない。パッドだけ置いて、配線しない（判断。ベンダーに確認するまで）。
- **GPIO の回り込み**: NAND を切る前に SPI のピンを Hi-Z にする手順は FW 側（power.md §5.2）。

### 3.5 マイク・privacy latch・バッファ

- **U6 TPS22917 でマイクの rail（MIC_EN）**: NAND とは別の gate（power.md §5.1、確定）。QOD は開放。R13 で既定 OFF（初期化が終わるまで録音しない。決定 F8）。
- **R14（R_MIC、0 Ω）**: マイクの電流測定リンク（確定）。
- **SW1（privacy latch）**: 機構が未決なので、**初版はブリッジ済みのソルダージャンパの footprint**（判断）。切れば latch を開いた状態を再現でき、board.md §6.1 の「部分給電」の試験ができる。部品が決まったら、スライドスイッチやリードスイッチの footprint に替える。
- **U7 SN74AUP3G34（SCK・WS・THSEL）**: board.md §6.1 の要求「latch を切ったら、FW が何をしてもマイクは音を拾えない」の実装。**電源を latch の先（MIC_1V8）から取る**ので、latch を切るとバッファも落ち、CLK / THSEL を駆動するものが無くなる。Ioff（0.6 µA max）により、nRF 側から 1.8 V が来ても回り込まない。直列抵抗だけでは T5848 の絶対最大定格（VDD + 0.3 V）を超えるので採らない（board.md §6.1）。
- **SD と WAKE はバッファを通さない**: マイク → nRF の向きで、マイクが落ちていれば駆動しない。**SD には R26（100 kΩ）のプルダウン**（T5848 のデータシートの指定。I²S の空きスロットで SD はハイインピーダンスになる）。latch を切っても SD は 0 に落ちる。
- **LR（MIC_SEL）**: R15（0 Ω）で GND を実装、R16（0 Ω、DNP）で VDD に変えられる（判断）。FW の I²S の左右チャネルの設定と合わせる。
- **C11（バッファ）、C12（T5848、X7R）の 100 nF**: T5848 は 0.1 µF X7R を VDD の直近に（TDK）。
- **レベルの余裕**: マイクの VOH と nRF の VIH 0.7 × VDD の余裕は小さい。実機で SD / WAKE の波形を見る。
- **クロック（I2S20）**: SWIDTH 24・RATIO 48X で MCK ≈ 768 kHz → SCK 768 kHz、WS 16 kHz（LP モード）。AAD に入れるときは 50〜200 kHz のクロックを 2 ms 以上出してから止める（MCK ≈ 192 kHz）。AAD 中はクロック停止で 20 µA。

### 3.6 電池電圧の測定・ボタン

- **U9 TPS22917 + 1 MΩ / 1 MΩ + 10 nF → AIN0（P1.04）**: モジュール基板では TPS62840 が VDD を 1.8 V に保つので、SAADC の VDD では電池電圧にならない（決定 B4 の訂正、power.md §4.6）。TPS22917 の ON は VIH 1.0 V なので、**1.8 V の GPIO でそのまま、3.3 V までの電池側を切れる**（P ch + N ch の組より部品が少ない。判断）。
- 分圧 1/2: 電池 3.3 V で 1.65 V（VDD 1.8 V の範囲内）。テブナン抵抗 500 kΩ は SAADC の TACQ を長くすれば 800 kΩ まで許される（Nordic §8.18.8、表 57）。1 回の測定で 1.65 µA。常時分圧にしない理由は board.md §6.2。
- R17 で既定 OFF。校正は実基板で 2 点以上（power.md §4.6）。
- **SW2 前面ボタン（P1.07）**: P1 は起床できる（Nordic §8.8.3）。nRF の内蔵プルアップを使う。B3U-1000P は仮で、**高さ 1.8 mm 以下（モジュールより低い）を選ぶ**（mechanical.md §7.4a）。

### 3.7 RTC（footprint だけ、DNP）

- 決定 D5（footprint のみ）。載せる場合の接続は firmware.md §5.3: system rail だけから給電（R20 = R_RTC）、VBACKUP は 10 kΩ で GND、INT は未接続、I²C は 1.8 V で 100 kHz 以下。
- **プルアップ（R22・R23）は RTC_VDD 側に置いた**（判断）。RTC を載せない間は R20 も DNP なので、I²C のピンにプルアップの電流が流れない。
- EVI と CLKOUT は未接続にした。**EVI を浮かせてよいかは Application Manual で確認していない**（載せるときに確認する）。

### 3.8 終端表示（仮・すべて DNP）

アクチュエータ（双安定の赤 / 黒の機構）が未選定で、遷移エネルギーもパルス幅も決まっていない（terminal-indicator.md の未確定、#12）。**回路の置き場と、MCU のピン（P2.09 = IND_RUN）を確保するための仮の回路**で、初版には載せない。

- **last-gasp の電源 VLG**: VBAT_SYS → R24（10 kΩ、電流制限）→ D1 → C15。CR2477 は内部抵抗が高いので、充電は電流制限付き（terminal-indicator.md §13）。C15 の容量は未定。
- **TERMINAL（黒）**: 監視 IC U11（**アクティブ High のプッシュプル出力**。仮に TPS3840PH。ピンは SOT-23-5 の配置に合わせた。閾値の品番は未定）が、電池が閾値を下回ると TERM_TRIG を High にし、DRV8837 の IN2 を駆動する。**MCU を介さない**（terminal-indicator.md §7）。
- **RUN（赤）**: MCU の IND_RUN（P2.09）が IN1 を駆動する。
- **nSLEEP**: TERM_TRIG と IND_RUN のダイオード OR（D2・D3）と 1 MΩ のプルダウン。どちらの指令も無いときは DRV8837 が sleep（静止電流が小さい）。
- 決めること: アクチュエータ、監視 IC の品番と閾値（CR2477 の放電曲線の実測後、#12）、C15 の容量、DRV8837 の VCC の下限（1.8 V）と VLG の関係、ダイオードの降下を含めた nSLEEP の VIH。

### 3.9 デバッグ・テストポイント

- **J1 SWD のポゴパッド（TC2030 の配置で穴なし）**（決定 H6）: `pendant:SWD_Pogo_2x03_P1.27mm_NoHoles`。Tag-Connect TC2030 と同じ 2 × 3、ピッチ 1.27 mm、Ø0.79 mm のパッドで、Ø0.99 mm の位置決め穴 3 つを除いた。裏面は電池ホルダと −パッドが占め、位置決めピンを裏へ通せないため。ポゴピンをこの配置で束ねた治具を、基板の外形で位置を決めて当てる（TC2030 のケーブルも手で押し当てれば使える）。ピン: 1 = VTREF（1V8）、2 = SWDIO、3 = nRESET、4 = SWCLK、5 = GND、6 = 未接続。SWO は未接続（P2.07 はマーカーに使う）。SWD の単独のテストパッド（旧 TP9〜TP11）は J1 と重複するので削除した。
- 電源の TP: VBAT、VBAT_SYS、1V8_BUCK、1V8、NAND_1V8、MIC_1V8、MIC_1V8_PRE（latch の前）、GND × 2。
- UART TX / RX（P0.00 / P0.01、UARTE30）、マーカー 3 本（P2.06〜P2.08）、VBAT_ADC、終端表示の出力（IND_A / IND_B。アクチュエータの線もここに付ける）。

## 4. ピン割当

規則（Nordic PS v1.0）: P0 = 低電力ドメイン、P1 = 周辺ドメイン（どちらも起床可、GPIOTE あり）、P2 = MCU ドメイン（起床不可、GPIOTE なし）。周辺は自分のドメインのピンだけ。SPIM00 は P2 に専用ピン。クロック信号はクロックピン。**BC15M の中のチップは CSP47 版**（ピン表の玉番号が board.md §2.1 の表 81 と一致）なので、XIAO と同じ規則がそのまま当てはまる。AIN は P1.04〜P1.07、P1.11〜P1.14。**SPIM00 と UARTE00 は同じ instance**（同時に使えない）。

| ネット | 向き | ピン | パッド（BC15M） | 理由 |
|---|---|---|---:|---|
| I2S_SCK_MCU（→ U7 → MIC_SCK） | 出力 | P1.12 | D1 | クロックピン、I2S20 は P1 |
| I2S_WS_MCU（→ U7 → MIC_WS） | 出力 | P1.03 | F4 | クロックピン（P1 のクロックピンで空いていたのはここだけ）。**NFC のピンなので FW で GPIO として使う設定にする** |
| MIC_SD | 入力 | P1.13 | E2 | I2S20 の SDIN |
| MIC_WAKE | 入力 | P1.14 | D4 | 起床が要るので P1 |
| THSEL_MCU（→ U7 → MIC_THSEL） | 出力 | P1.09 | C3 | µs 級のパルス列を timer + GPIOTE + DPPI で出せるよう P1（P2 には GPIOTE が無い） |
| I2C_SDA / I2C_SCL（RTC 用、DNP） | 双方向 / 出力 | P1.10 / P1.11 | D2 / D5 | TWIM は P1、SCL はクロックピン。XIAO の D4 / D5 と同じ |
| VBAT_ADC | アナログ | P1.04（AIN0） | C5 | AIN |
| BATSENSE_EN | 出力 | P1.08 | B1 | 任意（EXTREF を使わない限り干渉しない） |
| BTN | 入力 | P1.07 | B4 | 起床が要るので P1 |
| NAND_SCK / MOSI / MISO / CS | — | P2.01 / P2.02 / P2.04 / P2.05 | C6 / D6 / F5 / A6 | SPIM00 の専用ピンの組（00/20）。32 MHz では E0/E1 |
| NAND_EN | 出力 | P2.03 | E4 | 任意。R5 で既定 OFF |
| MIC_EN | 出力 | P2.00 | C4 | 任意。R13 で既定 OFF |
| IND_RUN | 出力 | P2.09 | A5 | 終端表示の RUN（仮）。P2.10 は予備で未接続 |
| MK_REC / MK_STORE / MK_BLE | 出力 | P2.06 / P2.07 / P2.08 | F2 / B3 / D3 | 測定用のマーカー（prototype.md §2.4）。P2.07 には SWO もある |
| UART_TX / UART_RX | — | P0.00 / P0.01 | A2 / A1 | UARTE30（P0）。UARTE00 は NAND の SPIM00 と同じ instance、UARTE20 を P1 に置くと AIN を 1 本使う |
| SWDIO / SWCLK / nRESET | — | 専用 | A3 / A4 / F3 | |
| （内蔵の水晶） | — | P1.00 / P1.01 | — | パッドに出ていない |
| 未接続 | — | P1.02（NFC）、P1.05（Premium の Vcap 用に予約、AIN1）、P1.06、P2.10、P0.02、P0.03、P0.04 | | P1.15 も未接続 |

**仕様に沿って割り当てなかったもの:** privacy latch の状態の読み取り（board.md §6.1・product.md §4.3「GPIO で読まない」）、RTC の INT（firmware.md §5.3「未接続」）。方針が変わったら P0.03 / P1.06 が空いている。

## 5. レイアウトへの申し送り

1. **アンテナ**: モジュールの上側（パッド 1 行目の側）の、シールドから先の約 6.0 × 3.6 mm が PCB アンテナ。footprint に表裏とも銅を置かない keep-out を入れた。**Fanstel の推奨はシールドの上端を基板の GND の端に揃える置き方**（次善は基板の角で、アンテナの端から 5 mm 以上 GND を離す。中央は非推奨）。金属は 30 mm 離すのが理想で、CR2477 の缶との距離が RF の最大のリスク（mechanical.md §7.4a）。
2. **隠れパッド**: B1〜E5 の 20 個はモジュールの下。使う信号のうち 13 本（P1.08、P2.07、P1.07、P1.09、P2.00、P1.04、P1.12、P1.10、P2.08、P1.14、P1.11、P1.13、P2.03）と GND（E1）がここにある。パッドの間は 0.5 mm なので 0.1 mm の配線を 1 本通すか、斜めの隙間に小径ビアを置いて逃がす。はんだは目視できないので、組立後に全信号を動かして確かめる。
3. **送信出力**: 技適のアンテナ利得は最大 1.5 dBi なので、+4 dBm EIRP の上限（rf.md）に対して**設定は +2.5 dBm 以下**。
4. **モジュールの実装**: LGA、MSL 3。リフローは Fanstel のプロファイル（データシート p.23）、2 回まで。高さ 1.9 mm（図面。Web の表記は 2.0 mm）。

5. **T5848**: 底面ポート。基板に φ0.5〜1.0 mm の音孔。**ペーストを音孔に入れない**。MSL 3。I²S の SCK（P1.12）は SCL（P1.11）と並走させない。
6. **TPS62840**: SW のループを小さく（入力 C3・L1・出力 C4 を最短）。VSET の GND への寄生容量は 100 pF 以下。VOS は出力コンデンサの直近から。
7. **電流測定リンク**: R1・R4・R6・R14・R20 は 0402 の 0 Ω。治具（docs/test/current-measurement-fixture.md）を当てるので、両側に TP を置くか、パッドを触れる位置に置く。
8. **自作の footprint**（`pendant.pretty`）: BC15M（Fanstel の Eagle ライブラリから）と T5848 は `gen_footprints.py` で生成（数字の出典はスクリプトの先頭）。CR2477 のホルダは手書き（`pendant.pretty/BatteryHolder_MYOUNG_MY-2477-01`、データシートの PCB Layout から。−パッドの位置と大きさは MY-2477-01-J の図を借りたので、実物で確認する。−パッドにはペーストを載せない）。

## 6. 未決・要確認（回路図で決めきれないもの）

| 項目 | 次の手 |
|---|---|
| TPS62840DLCR の入手（DigiKey JP 在庫 0）。VSON-HR ピッチ 0.5 mm が手作業で不安なら TPS62842DGR（HVSSOP-8） | LCSC から買う（hw/standard/fab/README.md、C2071859） |
| NAND の EP を GND に落としてよいか | ベンダーに確認 |
| T5848 のパッドとピンの対応 | TDK の STEP / 実物 |
| 終端表示のアクチュエータ、監視 IC、last-gasp 容量 | #12 |
| privacy latch の部品（スライドか磁気式） | 機構と一緒に（mechanical.md） |
| 220 µF の reservoir の入手と DC バイアス | 実測 |
| BC15M の VDD の下限（1.7 V か 1.8 V か） | 試作で rail を下げて確かめる |
| BC15M のアンテナと電池の缶 | 胸元の RSSI を EV-BC15M と比べる（rf.md） |
| EVI（RV-3032）の未使用時の扱い | RTC を載せるとき |
| XIAO を SWD プローブにできるか | 実機（prototype.md §3.5） |

## 7. 基板のルール（DRC）

KiCad の既定値（クリアランス 0.2 mm、配線幅 0.2 mm、ビア 0.6 / 0.3 mm、基板端 0.5 mm）は高密度の Ø31.9 基板には広すぎるので、**発注先の JLCPCB（2 層、銅 1 oz、板厚 0.6 mm。prototype.md）の製造能力**まで下げた。値は `standard.kicad_pro`（基板全体の最小値と Default ネットクラス）と `standard.kicad_dru`（全体の最小値で書けない JLCPCB の条件）に入れた。出典: https://jlcpcb.com/capabilities/pcb-capabilities （2026-10-18 に読んだ値）。

| 項目 | KiCad 既定 | 新しい値 | JLCPCB の値（2 層、1 oz） | 置き場所 |
|---|---:|---:|---|---|
| 配線の最小幅 | 0.2 | **0.1** | 最小 0.10 mm（4 mil） | 基板の最小値 |
| 異なるネットのクリアランス | 0.2 | **0.1** | 最小 0.10 mm | 基板の最小値、Default |
| 配線の既定幅（引くときの幅） | 0.2 | 0.15 | — | Default（判断。最小に余裕を持たせる） |
| ビア（外径 / 穴） | 0.6 / 0.3 | **0.5 / 0.3** | 2 層は穴 0.3 mm 未満が追加料金（見積画面の注記）。追加料金なしは穴 ≥ 0.3 かつ外径 ≥ 0.4。外径 0.5 なら穴の縁から配線までリング 0.1 + クリアランス 0.1 = 0.2 mm で Via hole to Track 0.2 も満たす | 基板の最小値（穴 0.3、外径 0.5）、Default |
| 穴の縁から銅（ビアの穴と配線） | 0.25 | **0.2** | Via hole to Track 0.2 mm | 基板の最小値 |
| スルーホール部品の穴から配線 | 0.25 | 0.28 | PTH to Track 最小 0.28 mm（推奨 0.35） | `.kicad_dru` |
| 穴と穴（ビア） | 0.25 | **0.2** | Via hole-to-hole 0.2 mm | 基板の最小値 |
| 穴と穴（スルーホールのパッド） | 0.25 | 0.45 | Pad hole-to-hole 0.45 mm | `.kicad_dru` |
| 異なるネットの SMD パッド同士 | 0.2 | 0.15 | SMD pad-to-pad 0.15 mm | `.kicad_dru`（配線のクリアランス 0.1 より厳しい） |
| 銅から基板端（ルーター加工） | 0.5 | **0.2** | 0.2 mm | 基板の最小値 |

- **訂正（2026-10-18）: ビアは 0.5 / 0.3。** 最初は 0.45 / 0.2 を「追加料金なし」としたが、それは多層基板だけの条件で、2 層では 5 枚で基板 $4.10 → $53.90（Plugged Via と 4-Wire Kelvin Test が自動で付く）。配線済みの基板のビア 168 個を 0.5 / 0.3 に替え、違反の出たビアは配線の端ごと少しずらして直した（143 件 → 21 件）。残りは U3 / U5 のまわりの混んだ所のビア約 11 個を手で直し、最終 DRC は 0 違反・0 未配線になった。外径 0.45 のまま穴だけ 0.3 にする案は、穴の縁から配線 0.2 mm を満たさない所が 112 件出たので取らない。板厚 0.6 mm なので穴のアスペクト比は 2。BC15M の隠れパッドの斜めの隙間（0.87 mm）には 0.5 mm のビア + クリアランス 0.1 mm × 2 = 0.7 mm が入る。
- **さらに下げる余地**: 穴 0.15 / 外径 0.25 mm のビアや、4 層（配線 0.09 mm）は JLCPCB で作れるが追加料金。2 層で収まらないときに使う（判断）。
- **マイクの音孔は Ø0.6 mm**。GND リングの内径 Ø1.025 mm までが 0.21 mm になり、穴の縁から銅 0.2 mm を満たす。TDK の推奨は φ0.5〜1.0 mm、JLCPCB の NPTH の最小は 0.5 mm。
- 文字（シルク）の規則は変えていない（JLCPCB の文字高の最小は 1.0 mm、線幅 0.15 mm）。

**GND のベタ**（2026-10-18）: 表（F.Cu）と裏（B.Cu）の両面に GND のゾーンを置いた。設定は、クリアランス 0.2 mm（配線の最小 0.1 mm より広くして、ベタと信号の間の短絡と容量を減らす。判断）、最小幅 0.2 mm、パッドはサーマル接続（隙間 0.25 mm、スポーク 0.3 mm。手はんだと手直しのため。判断）、孤立した島は消す。BC15M のアンテナ部はモジュールの footprint の keep-out で銅が入らない。配線で分断された島はビアと短い配線でつないだ（KiCad Routing Tools の repair_planes。そのあと JLCPCB のルールで DRC を通した）。

**BT1 の ＋ピン 2 本**: ホルダの金具でつながっているので、footprint に「同じ番号のパッドはジャンパー」（`duplicate_pad_numbers_are_jumpers`）を付けた。基板上で 2 本を配線でつなぐ必要はない。

**配線幅とビアの選択肢**（Board Setup の「定義済みサイズ」。配線中に切り替えて使う）。電流の目安は IPC-2221 の外層の式（銅 35 µm、温度上昇 10 ℃）で出した概算。

| 配線幅（mm） | 電流の目安 | 用途（判断） |
|---:|---:|---|
| 0.1 | 0.45 A | JLCPCB の最小。BC15M の隠れパッドの間など、どうしても通らない所だけ |
| 0.127（5 mil） | 0.54 A | 細ピッチの部品からの引き出し |
| 0.15 | 0.60 A | 信号の既定（Default ネットクラス） |
| 0.2 | 0.74 A | 長めの信号、1V8 の枝 |
| 0.25 / 0.3 | 0.88 / 1.0 A | 電源（VBAT、VBAT_SYS、1V8、NAND_1V8、MIC_1V8） |
| 0.4 / 0.5 | 1.2 / 1.5 A | 降圧の SW・VIN のループ（TPS62840 の電流制限に余裕を持たせる）、電池の経路 |

| ビア（外径 / 穴、mm） | 用途（判断） |
|---|---|
| 0.5 / 0.3 | 既定。2 層で追加料金なしの最小。BC15M の隠れパッドの逃がし |
| 0.6 / 0.3 | GND の縫い、電源 |
| 0.8 / 0.4 | 電池・降圧の電流経路、熱の逃がし |

- どれも JLCPCB の「外径は穴 + 0.1 mm 以上（0.15 mm 推奨）」と、2 層で追加料金のかからない条件（穴 ≥ 0.3、外径 ≥ 0.4）を満たす。
- 電流はどこも小さい（CR2477 の電池側は数十 mA、BLE の送信は 10 mA 級）ので、幅は電流より電圧降下とインダクタンス（降圧のループ）で決める。
