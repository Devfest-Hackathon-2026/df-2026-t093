# ローカルストレージ仕様（NAND・ring・耐電断・ビットレート連動）

この文書が持つもの: 媒体（NAND）の選択と選定基準、1.8 V 側の電気的な構成、NAND の電力、ring（追記型リング）、NAND ドライバの要件、容量とバックログ連動ビットレート、耐電断と消去の運用。

他の文書が持つもの:

- レコードのバイト配置・BLE 同期・ACK・欠落レコード: [formats.md](./formats.md)
- 電源経路・電圧しきい値・状態機械・エネルギー収支・OTA の電力条件: [power.md](./power.md)
- codec・縮退モードの codec・ASR の評価方法: [audio.md](./audio.md)
- パーティション・ファームウェアのライブラリ構成: [firmware.md](./firmware.md)
- ピン割当・測定リンク・配置規則: [board.md](./board.md)

確度の凡例: **確定**（決定済み）/ **方針**（方向は決まり、数値は未定）/ **仮置き**（仮定。実測で決める）。数値の横に何も書かない行は、データシートの値か計算結果（前提は本文に書く）。

---

## 1. 要求と SKU 別の構成

### 1.1 役割

- 録音（PCM → ADPCM）を不揮発に保持する **rolling buffer**。約 2 時間ごとにスマホへ BLE でバースト同期し、ACK された範囲を解放する。
- ストレージに要るもの: 待機電流がほぼ 0、1 MB あたりのエネルギーが小さい、耐電断、容量は「実効ビットレート × 保持したい時間」、1.8 V 単一ドメイン。
- 要らないもの:
  - ファイルシステム・SD プロトコル（スマホとは独自の BLE プロトコルで話す）。
  - 高速（平均書込みは 4 kB/s。NAND は 99.9 % 以上アイドルなので、待機電力が支配項になりうる）。
  - 高耐久（32 kbps 常時でも 1 日 346 MB。4 Gbit で 0.67 周/日で、耐久 50k〜60k サイクルに対して桁違いに余る）。

### 1.2 SKU 別

| 項目 | Standard | Premium |
|---|---|---|
| 媒体 | 1.8 V 生 SPI NAND、512 Mbit〜1 Gbit（容量は未決、未確定 #1） | 製品は 1.8 V 生 SPI NAND、4 Gbit を基準。11 月デモ機は 512 Mbit（§2.3） |
| 保持の考え方 | スマホ必携。録音は rolling buffer。**48 h の保証はしない**。満杯になれば最古から上書きし、欠落区間を記録する | 製品目標はスマホ不在 48 h を、音が出続ける最悪ケース（p = 1）でも保証する（§7）。11 月デモ機では 48 h を要求しない |
| 書込みの電源 | CR2477 1 本。電池電圧のゲートで書込み・消去を許す（§8.3） | Solar + 120 F HSL（CAP）と CR2032。source ごとに書込み方式が違う（§8.3） |
| ドライバ・ring | 同じ NAND ドライバ（§6）と同じ ring（§5）。違いは容量と電源方針だけ | 同左 |

p は保存デューティ（1 日のうち NAND に保存する時間の割合。AAD / VAD を通過した割合）。

### 1.3 確定している前提

| 項目 | 内容 | 確度 |
|---|---|---|
| 方式 | 1.8 V 生 SPI NAND を 1 個。SD 系・NOR・PSRAM は使わない（§2.2） | 確定 |
| 電圧 | 1.8 V 単一レール。DC-DC・レベルシフタ・3.3 V レールを増やさない | 確定 |
| 構造 | ファイルシステム無し。append-only の ring。CRC + commit marker + A/B metadata | 確定 |
| ビットレート上限 | 32 kbps。16 kHz が必要になっても 2-bit ADPCM 等で 32 kbps 以内に収める | 確定 |
| 容量 | Premium 製品は 4 Gbit を基準にする（縮退側の 16 kbps が ASR で使えない場合は 8 Gbit、§7.2）。容量違いの製品 SKU は作らない。11 月デモ機は 512 Mbit（§2.3） | 製品方針は条件付き。デモ機は設計採用、未実測 |
| 容量到達時 | 通常は未同期でも最古の chunk から上書きして録音を続ける（§8.4） | 確定 |
| 時刻 | chunk の時刻は書き換えない。RTC の補正はオフセット記録として残し、順序は連番で決める（[formats.md](./formats.md)） | 確定 |

---

## 2. 媒体の選択

### 2.1 方式

**1.8 V の生 SPI NAND（SLC）を 1 個。** パッケージは WSON-8 8×6 の JEDEC SPI NAND 配列を共通の足跡にして、ベンダー違いを載せ替えられるようにする。ただしピン互換でもファームウェアは drop-in ではないので、JEDEC ID から nand_descriptor を引いて差を吸収する（§6）。（確定）

理由: 1.8 V 単一レールで済み、エネルギーが最小（約 15 mJ/MB、§4.1）で、容量は電力にほとんど効かずコストにだけ効く。

### 2.2 使わない媒体（確定）

| 媒体 | 使わない理由 |
|---|---|
| SD NAND・microSD（managed、3.3 V / 1.8 V） | 読み書きが 30〜80 mJ/MB 以上で生 NAND（15 mJ/MB）の 2〜10 倍。待機 130 µA〜数 mA。初期化が最大 1 s（ACMD41）。nRF54L15 に SD / SDIO のホスト HW は無く SPI モード固定で、初期化の ≤ 400 kHz は SPIM00 で出せない。一般の microSD は 1.8 V で起動できない |
| SPI NOR | 「消してから書く」ので、書込み（消去込み）が 133 nJ/B（64 KB block 消去、typ）〜331 nJ/B で NAND（15 nJ/B）の約 9〜22 倍。64 KB 消去は typ 200 ms（max 2 s）の長いパルスで、CR2477 の reservoir で橋渡しできない。1.8 V NOR 256 Mbit は 512 Mbit NAND より高い |
| PSRAM | 生 NAND を standby で待たせたほうが PSRAM の保持電力より低い。8 MB は 32 kbps で約 34 分しか持たない |
| MRAM / FRAM | 32 KB〜512 KB 級しか無く不可 |
| 内蔵 RRAM | 1.5 MB 全部でも約 6 分分。Phase 0 の ring 開発にだけ使う（`ring_partition` 784 KB、[firmware.md](./firmware.md)） |
| eMMC / UFS | nRF54L15 にホスト IP が無い |
| 3.3 V 生 NAND + ローカル昇圧 | 書込み + 読出しが 26 mJ/MB（+76 %）、昇圧 IC・インダクタ・レベルシフタで部品が +$2〜3。製品基板には載せない。1.8 V 品の見積が 3.3 V 品 + $5 を超える、または調達不能になった場合の保険案としてだけ残す（評価基板側で試す） |
| NAND の 2 個ミラー | 電力が約 2 倍で、同じ FW の共通要因故障は防げない。媒体故障が残存リスクの首位になったときの将来オプション |

### 2.3 候補部品

価格・在庫は変動が大きいので、この文書には書かない（未確定 #1）。表の「未確認」は、データシートで確認していないという意味。

**Standard（512 Mbit〜1 Gbit。容量は未決）**

| 容量 | 型番 | パッケージ | 備考 |
|---|---|---|---|
| 512 Mbit | Lexar F35UQA512M-WWR | WSON-8 8×6 | Rev 1.1: 2 KiB + 64 B/page、128 KiB/block、on-die 1-bit ECC / 528 B。DPD 命令は同版に見当たらず power gate を使う。11 月の Premium デモにも設計採用 |
| 1 Gbit | GigaDevice GD5F1GQ5REYIGR | WSON-8 8×6 | データシート未確認 |
| 1 Gbit | Macronix MX35UF1GE4AD-Z4I6 | WSON-8 8×6 | on-die ECC（E4 系）。DPD は同系 MX35UFxG24AD で 1 typ / 15 max µA（付録 §2.5） |
| 1 Gbit | ISSI IS37SMW01G8B-JJLI | WSON-8 8×6 | 8 bit ECC（品名から）。DPD 未確認 |
| 1 Gbit | Micron MT29F1G01ABBFDWB-IT:F | UPDFN-8 8×6 | 新規設計非推奨なので、選定基準（§2.4）を満たさない |

**Premium（4 Gbit。複数ソースで評価して決める）**

| 型番 | 量産状況 | page | ECC | DPD | 待機 |
|---|---|---|---|---|---|
| GigaDevice GD5F4GM7RE（4 KB page 品。2 KB page の GD5F4GM8RE は新規非推奨で選ばない） | MP | 4 KB | on-die（製品表で ECC-free） | あり（電流未確認、DS-01327 未取得） | 未確認 |
| Winbond W25N04LW | Active | 4 KB＋256 B（Rev A 2025-02-12 参照） | on-die 8 bit | 参照版で 1 typ / 15 max µA | 参照版で 10 typ / 50 max µA（≤85 °C）。発注 suffix の最新版を再確認 |
| Micron MT29F4G01ABBFDWB-IT:F | Active | 4 KB（Linux spinand の定義） | on-die 8 bit（Linux ドライバ。DS はログイン必須で未確認） | 未確認 | 未確認 |
| Macronix MX35UF4GE4AD（on-die ECC 版） | DS 未取得（24A 系は Rev 1.4、2024-03） | 4 KB（4096 + 256 B） | on-die | 24A 系で 1 typ / 15 max µA、復帰 tRDP ≥ 35 µs | 24A 系で 10 / 50 µA |
| Alliance AS5F14G04SND-10LIN（LGA-8 8×6） | 公式 DS Rev 3.2（Jan 2026）、小口掲載在庫あり | **2 KB＋128 B**。4 KB 必須条件から外れるため試作代替候補 | on-die 8 bit | 明示的 DPD は未確認、自動 Sleep。tWakeup 6 ms typ は最大保証ではない | standby 30 typ / 120 max µA、Sleep 専用値は未確認 |
| XTX XT26Q14D / XT26Q04D | XT26Q14D は CS（試作段階）。XT26Q04D は状況未確認 | 未確認 | on-die 8 bit / 528 B | XT26Q14D は未確認 | XT26Q04D は 10 µA（販売ページの表記） |

- Macronix の MX35UF4G24AD（24A 系）は **host で 8 bit / 544 B の ECC が必須** なので使わない（nRF54L15 に BCH の HW は無い。1 page あたり数十〜数百 kcycle、数〜十数 µJ と見込む。仮置き）。同じ型番の on-die ECC 版（E4 系）を使う。
- 評価の優先は、技術要件（DPD・on-die ECC・4 KB page・量産品）の満足度で決める。

**8 Gbit（縮退側 16 kbps が ASR で使えなかった場合の戻り先、§7.3）**: Winbond W25N08LW（WSON-8 8×6。標準リード 36 週・最小 126〜480 個）、XTX XT26Q18D（DPD なし）、Alliance AS5F18G04SNDB-10LIN（LGA-8 8×6）。


**2026-10-27 のデモ設計:** ユーザーの短時間・低コスト優先により、デモ機の NAND は **F35UQA512M-WWR、512 Mbit（64 MiB）**へ変更する。1.7〜1.95 V、2 KiB + 64 B/page、64 page/block、512 block。内部 ECC は 512 B + 16 B ごとに 1 bit 訂正（初期状態で有効）、出荷時の有効 block は 502〜512 個。DPD 命令は参照 Rev 1.1 に見当たらず、待機は 10 µA typ / 50 µA max なので TPS22917 による power gate を使う。[Longsys の型番表](https://www.longsys.com/products/embedded-storage/micro-storage/spi-nand-flash.html)、[メーカー DS Rev 1.1](https://cdn.icstop.com/upload/pdfs/e7/de/e7decc6bb7e9f2384da286e27083f166.pdf)。

これは**デモの設計採用**で、未購入・未実装。2 KiB page と DPD 命令のない参照仕様は製品選定基準（§2.4）の例外として扱い、製品の 4 Gbit 基準・48 h 目標は変更しない。内蔵 ECC は旧 8 Gbit 候補の 8 bit 訂正より弱い。JEDEC ID、初期不良 block の読み出し、ECC status、program / erase status、gate 復帰、実効容量・消費と電源切替を実機で確認する。Standard の同型番 footprint は流用候補だが、exposed pad の扱いと solder paste を製造前に確認する。

### 2.4 製品向け選定基準（確定）

11 月デモ機の F35UQA512M-WWR は、短時間デモを優先して項目 3 の DPD と項目 4 の 4 KiB page を満たさない。power gate と 2 KiB page 対応を検証する試作例外とする。

1. 1.8 V（VCC 1.7〜1.95 V）。
2. **on-die ECC**。host ECC が必須の品は避け、on-die 版があればそちらを使う。
3. **DPD 命令あり**。DPD が無い品は待機にロードスイッチが必須になる（[power.md §5.1](./power.md)）。
4. page 4 KB（2 KB page 品は program 回数が倍になり、§4.1 の mJ/MB が変わるので再計算が要る）。
5. 量産品（MP / Active）。新規設計非推奨・生産中止は選ばない。
6. パッケージは自宅リフローで載るもの（WSON-8 8×6 を共通の足跡とし、LGA-8 6×8 / 8×6 が載るかは寸法図で確認）。BGA・パラレル NAND・SD NAND・モジュールは選ばない（BGA は 2 層基板の配線と検査が難しく、SD NAND・モジュールは ECC・bad block を中に隠すので生 NAND の FW を試せない）。
7. 複数ベンダーを同一の足跡で載せ替えられる。
8. 量産見積で揃える項目: 1k / 10k / 100k の単価、MOQ、リードタイム、PCN 方針とライフサイクル、セカンドソース、ロットのトレーサビリティ、データシートのリビジョン。

### 2.5 データシート値（設計計算の基準）

W25N08LW と XT26Q18D は電力計算（§4）の基準品、Macronix は DPD の確認できた 4 Gbit 品。

| 項目 | Winbond W25N08LW（Rev A、2026-02-05） | XTX XT26Q18D（Rev 1.0、2024-03-19） | Macronix MX35UFxG24AD（Rev 1.4、2024-03-20） |
|---|---|---|---|
| VCC | 1.7〜1.95 V | 1.7〜1.95 V（abs max 2.25 V） | 1.7〜1.95 V（abs max 2.4 V） |
| page / block | 4096 + 256 B、64 page/block | 同左 | 4 Gbit: 4096 + 256 B / 256K + 16K B。1 / 2 Gbit は 2048 + 128 B |
| ECC | 8 bit / 544 B（on-die） | 8 bit / 528 B（on-die） | **host で 8 bit / 544 B が必須**（E4 系は on-die） |
| 待機 | 20 typ / 100 max µA（≤ 85 ℃） | 15 typ / 120 max µA | 10 typ / 50 max µA |
| DPD | B9h / ABh、2 typ / 30 max µA、tDP ≤ 10 µs、tRES1 ≤ 1.6 ms | **なし** | B9h で入り CS# トグルで復帰、1 typ / 15 max µA、tRDP ≥ 35 µs |
| 読出し電流 | 15 typ / 25 max mA（104 MHz、単線 buffer read） | 20 typ / 40 max mA（108 MHz） | 23 typ / 30 max mA |
| program / erase 電流 | 20 / 25 mA、20 / 25 mA | 18 / 40 mA、18 / 40 mA | 23 / 30 mA、15 / 30 mA |
| tPROG | 440 typ（ECC 有効）/ 800 max µs | 400 typ / 750 max µs | 320 typ / 700 max µs（randomizer 無効） |
| tBERS | 3 / 10 ms | 3.5 / 10 ms | 4 / 6 ms |
| tRD | 100 µs max（ECC 有効） | 210 / 270 µs | ≤ 25 µs |
| 起動 | tVSL ≥ 1 ms、tPUW 3.3 ms | tVSL ≥ 3 ms | tVSL 2 ms |
| 耐久 | 60k | 50k（ECC あり） | 60k typ（8 bit ECC 付き）/ 保持 10 年 |
| 不良 block | BBM 命令 + LUT（host が登録する支援機能） | 有効 block ≥ 4016 / 4096 | — |
| spare（OOB）の ECC 保護 | 16 B ごとに先頭 4 B（BB マーカー + User Data II）は**非保護**、残り 12 B は保護（Fig 5-1） | User meta data 0〜7（各 16 B）は保護（Table 12）。1000H は不良 block マーク予約 | — |
| 型番の違い | サフィックス（G / T / E / U / R）で既定の read モードが違う | — | `-Z4I`: column address に plane-select ビットが要る（2 / 4 Gbit）。`-Z4I8`: 不要 |
| パッケージ | WSON-8 8×6 / TFBGA24 | WSON-8 8×6 | WSON-8 8×6 |

出典:

- W25N08LW Rev A: <https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/8921/256_W25N08LWxxxx.pdf>
- XT26Q18D Rev 1.0: <https://file.x-semi.com/xtx_file/2024/03/425607733795401728/XTX_SPI%20Nand_1.8V_8G_XT26Q18D_Spec_v1.0.pdf>
- MX35UF4G24AD Rev 1.4: <https://www.macronix.com/Lists/Datasheet/Attachments/9033/MX35UF4G24AD%2C%201.8V%2C%204Gb%2C%20v1.4.pdf>
- GigaDevice の SPI NAND 製品表: <https://www.gigadevice.com/product/flash/spi-nand-flash>

---

## 3. 電気的な構成（1.8 V 側）

```text
1.8 V MAIN ─ 電流測定用 0 Ω ─┬─ [TPS22916 ロードスイッチ: 実装パッドのみ、DNP 可] ─┬─ NAND VCC
                             │                                                    └─ 4.7 µF + 0.1 µF
                             └─ nRF54L15 SPIM00（32 MHz・1 bit）
                                  ├─ CS   ───────────── NAND /CS（スイッチ後の VCC へプルアップ）
                                  ├─ SCK  ─ 0/22/33 Ω ─ NAND CLK
                                  ├─ MOSI ─ 0/22/33 Ω ─ NAND DI
                                  └─ MISO ─ 0/22/33 Ω ─ NAND DO

NAND: 1.8 V の生 SPI NAND（WSON-8、JEDEC 配列）
PSRAM なし / SD ソケットなし / レベルシフタなし / 3.3 V レールなし
```

（配線は方針。値は実測で決める。ピン割当・測定リンクの置き方は [board.md](./board.md)。）

### 3.1 SPI

| 項目 | 内容 |
|---|---|
| インスタンス | SPIM00（32 MHz、1 bit）。最小周波数は約 1.016 MHz（128 MHz ÷ 126）。NAND 専用にできるかは未決（未確定 #11） |
| 転送時間 | 4 KB の転送 = (4096 + 4) × 8 ÷ 32 MHz = 1.03 ms |
| Quad / Dual | 使わない。nRF54L15 に QSPI の HW は無く、FLPR によるソフト実装（最大 64 MHz、工数大）しか無い |
| 直列抵抗 | SCK / MOSI / MISO に 0〜47 Ω の実装パッド。32 MHz の波形を見て値を決める（/CS には入れない） |
| /CS | **スイッチ後の NAND VCC へプルアップ**する。W25N08LW は電源の立上げ・立下げ中に /CS が VCC に追従することを要求する。MCU がリセット中でピンが浮いても選択状態にならない |
| 遮断前 | ゲートを切る前に MCU 側の SPI ピンを Hi-Z にする（back-power の防止） |

### 3.2 電圧

| 項目 | 内容 | 確度 |
|---|---|---|
| NAND の VCC | 1.7〜1.95 V（候補品共通）。1.7 V へは下げない | 確定 |
| NAND 端子の下限 | **負荷時 1.75 V 以上**。BLE TX と NAND 書込みが重なる ≈ 28 mA の状態で確認する。これとは別に nRF54L15 端子の POR 時 1.75 V 以上も確認する | 仮置き（28 mA は設計仮説） |
| NAND 端子の上限 | 起動時・負荷解除時の最高電圧が、採用品の上限（W25N04LW は 1.95 V）に対して余裕を持つこと | 仮置き（実測） |
| バイパス | NAND の program 20 mA・440 µs のパルスに備え、NAND 近傍に 4.7 µF 程度 | 方針 |

system rail を 1.8 V にするか 1.9 V にするかは [power.md §2.2](./power.md) が持つ。NAND 側の条件は上の 2 行で、最終 NAND と全負荷での実測で確認する（未確定 #12）。

---

## 4. 電力と待機

### 4.1 1 page あたりのエネルギー

W25N08LW の typ 値で計算する。SPI は SPIM00 の 32 MHz・1 bit、降圧効率 0.9。個別の行は降圧前、「書込 + 消去」「読出」「合計」は降圧後（キャパ側）の値。

| 項目 | 内容 | 値 | 確度 |
|---|---|---|---|
| 4 KB 転送 | (4096 + 4) × 8 ÷ 32 MHz | 1.03 ms | |
| 書込み load | 1.8 V × 8 mA × 1.03 ms | 14.8 µJ | 仮置き（8 mA） |
| 書込み program | 1.8 V × 20 mA × 440 µs | 15.8 µJ | |
| 消去（64 page 按分） | 1.8 V × 20 mA × 3 ms ÷ 64 | 1.7 µJ | |
| MCU + SPIM（CPU は寝る） | 1.8 V × 1.5 mA × 1.5 ms | 4.1 µJ | 仮置き |
| **書込み + 消去（降圧込み）** | | **40.4 µJ/page = 9.9 mJ/MB** | |
| 読出し | tRD 60 µs × 10 mA + 転送 1.03 ms × 8 mA + MCU | 20.9 µJ/page = 5.1 mJ/MB | 仮置き |
| **合計** | | **≈ 15 mJ/MB**（振れ幅 10.7〜19.1） | 仮置き |

- 32 kbps 常時（p = 1、346 MB/日）の NAND は ≈ 60 µW（書込み ≈ 40 + 読出し ≈ 20）。**容量（1 Gbit か 8 Gbit か）はエネルギー/MB にほとんど効かない。**
- XT26Q18D（18 mA、400 µs、3.5 ms、tRD 210 µs）でもほぼ同値。
- 式:

```text
page            = 4096 B            PAGES_PER_MB = 1e6 / 4096 = 244.1
t_xfer          = (4096 + 4) * 8 / 32e6                       = 1.03 ms
E_write_page    = [V*I_io*t_xfer + V*I_prog*t_prog] / eta + V*I_mcu*(t_xfer + t_prog + 50us) / eta
E_erase_page    = V*I_erase*t_erase / 64 / eta
E_read_page     = [V*I_rd*t_rd + V*I_io*t_xfer] / eta + V*I_mcu*(t_xfer + t_rd) / eta
mJ/MB           = E_page[uJ] * PAGES_PER_MB / 1000
P_avg[uW]       = (MB/day) * (mJ/MB) * 1000 / 86400
仮置き: I_io = 8 mA、I_mcu = 1.5 mA、t_rd = 60 µs、eta = 0.9、I_rd = 10 mA
```

- 2 KB page の品は program 回数が倍になるので、上の値を再計算する（未確定 #7）。
- NAND を含むシステム全体のエネルギー収支（BLE の mJ/MB、保存デューティ p ごとの平均電力）は [power.md §7](./power.md) が持つ。この文書は NAND の値だけを持つ。

### 4.2 待機（1.8 V）

| 状態 | 電流 | 電力 |
|---|---|---|
| アイドル（CS = H）W25N08LW | 20 typ / 100 max µA | 36 / 180 µW |
| アイドル XT26Q18D | 15 typ / 120 max µA | 27 / 216 µW |
| **DPD**（W25N08LW、B9h / ABh） | **2 typ / 30 max µA** | **3.6 / 54 µW** |
| ロードスイッチ OFF（TPS22916） | 0.01 µA typ | ≈ 0.02 µW |

16 s のバースト間隔 1 回あたりの待機 + 復帰エネルギー:

| 方式 | typ（µJ） | max（µJ） |
|---|---:|---:|
| アイドルで待つ（W25N08LW / XT26Q18D） | 580 / 430 | 2,900 / 3,500 |
| DPD（W25N08LW、復帰 8 µJ を含む） | 66 | 870 |
| ハードゲート（再投入 50 µJ を含む） | 50 | 50 |

- バースト間を standby で待つと DPD / ゲートの 6〜12 倍。typ だけで「standby で十分」とは言えない（XT26Q18D の max は 120 µA = 216 µW）。
- DPD の復帰は tRES1 ≤ 1.6 ms（W25N08LW）で、電源 ON（tVSL 1 ms + tPUW 3.3 ms + reset）より速い。
- 64〜128 KB のバッチ書込みなら、DPD 復帰・電源再投入のセッションオーバーヘッドは ≤ 6 µW。

### 4.3 待機の運用

待機の方針（DPD 優先・ハードゲートは補助・DPD の無い品はゲートが必須）、ロードスイッチ、OFF / ON の順序は [power.md §5.1](./power.md)・[power.md §5.2](./power.md) が持つ。この文書は NAND 側の事実だけを持つ。

- DPD の有無と電流は部品ごとに違う（§2.3、§2.5）。選定基準は §2.4。
- DPD の実測電流が個体差で max（W25N08LW は 30 µA = 54 µW）に近い場合に備え、ロードスイッチの実装パッドを残す。
- 遮断前に commit → BUSY 解除 → /CS = H → SPI ピン Hi-Z の順で止める（§3.1）。
- 実機の 24 h 平均 µW と起動成功率で、DPD・常時 standby・周期ゲート・発話セッション単位のゲートを比べて決める（未確定 #4）。

---

## 5. ring（追記型リング）

実装は `fw/lib/ring/`（純 C）。デバイスはコールバックで渡すので、同じコードが Zephyr の `flash_area`（Phase 0 の内蔵 RRAM）、SPI NAND、ホストのフラッシュシミュレータで動く。レコードのバイト配置は [formats.md](./formats.md)。

### 5.1 デバイスの構成（確定）

```text
[ meta A: 1 block ][ meta B: 1 block ][ data: N blocks ......................... ]
```

| 項目 | 意味 | RRAM（Phase 0） | NAND（4 KB page 品） |
|---|---|---:|---:|
| `block` | 消去単位 | 4 KB（論理） | 256 KB |
| `page` | program 単位。レコードはこの整数倍に切り上げて置く。4 の倍数で、`wunit` の倍数で、32 以上 | 256 B | 4 KB |
| `wunit` | 最小の program 単位 | 16 B | = page |
| `single_program` | 1 page を 1 回しか program できない | 0 | 1 |

- `single_program = 0` のデバイスは commit を別の program で最後に書く。`1` のデバイス（NAND）は CRC と commit を最終 page の 1 回の program に含める（[formats.md](./formats.md) §2.1）。
- 2 KB page の NAND は page / block を nand_descriptor（§6）から決める。
- data 領域の位置は**絶対バイト位置（u64、巻き戻らない）**で持ち、物理アドレスは `位置 mod 領域サイズ`。周回しても大小比較がそのまま使える。
- Phase 0 の RRAM は `ring_partition`（784 KB = 196 block。うち 2 block が metadata）。

### 5.2 レコードの置き方

- レコードは **page の境界から**置き、末尾は page の整数倍にパディング（0xFF のまま）する。
- **1 つのレコードは領域の終端をまたがない。** 残りに入らなければ次の周回の先頭（0）から始め、残りは空ける（hole）。
- 書込み位置 `wp` が新しい block に入るとき、その block を**消去してから** program する（erase-on-enter）。`ring_pre_erase()` で先に消しておけば、追記の経路から消去を外せる（CR2032 時などの電力平準化、§8.3）。
- 占有範囲は `[tp, wp)`（`tp` = 最古の保持レコード）。block を新しく使うには `k - tp/block < N` が必要で、満たさなければ満たすまで最古のレコードを捨てる。
- 連番は ring が振る（`ring_append` が `seq` と `boot` を埋める）。起動時に末尾から再計算するので、書きかけで捨てたレコードの番号は次のレコードが使う。
- payload 長は 4 の倍数。1 レコードは `maxrec` 以下で、`maxrec × 2 ≤ data 領域`。
- **chunk の長さは 8〜16 s（32〜64 KB）** で、現在の実装は約 8 s（payload 32,728 B = 8.18 s。flash 上で 32 KiB ちょうど）。音声は 1 page ≈ 1 s で、ヘッダ込みだと「1 page = 1 chunk」は窮屈なため。page の整数倍に揃えないとパディング（約 2.4 %）で容量が減る（§7.4）。
- **header / CRC / commit は main 領域（ECC 保護の 4096 B）に置く。** spare（OOB）へ重要 metadata を置くのは、そのリビジョンの ECC 保護範囲を確認してから（§2.5）。

### 5.3 書込み順（耐電断の根拠、確定）

| 操作 | 順序 | 途中で切れると |
|---|---|---|
| レコードの追記 | header → payload → CRC → commit（最後） | commit が無い、または CRC が合わない。起動時に捨てる |
| 古いレコードを捨てる（満杯・ACK 回収） | **新しい metadata を commit → その後で消去 → program** | metadata が先なので、消えたレコードを参照しない。切れても古い metadata が残る |
| metadata | 使っていない側の block を消去 → 書く。世代番号が大きい有効な方を採用 | 書きかけの側は無効。もう一方が有効 |
| 起動 | 回復後に起動カウンタを +1 して metadata を commit | 同上 |

metadata は `gen / tp / tail_seq / acked / boot / version / maxrec` の 36 B + CRC + commit（v2）。**レコードの追記ごとには書かない**（消去を伴うため）。書くのは tail が動いたとき（満杯での破棄、ACK 回収）と起動時だけ。

### 5.4 起動時の回復（`ring_open`）

1. metadata の 2 つの block を読み、有効で世代が最大のものを採る。
2. `tp` / `tail_seq` から **ヘッダと commit word だけ**を読んで進む（payload は読まない）。次のレコードは、直前のレコードの終端にあるか、**その先の block 境界のどれかにある**（書込みが block 境界へ飛んだ後）。見つからなければ末尾。前の周回の古いレコードは連番が小さいので一致しない。連番が期待する値ちょうどのものだけを採る。
3. 最後に見つけたレコードだけ、CRC を payload 全体で検証する。壊れていたら捨てる（その連番は次のレコードが再利用する）。
4. 書込み位置は最後のレコードの終端。その block の残りが空白でなければ（書きかけの残骸がある）、次の block 境界へ飛ぶ。block は丸ごとしか消せないため。
5. 起動カウンタを +1 して metadata を commit。

NAND 全体を毎回 scan しない。metadata から tail 近傍だけを scan する。

### 5.5 満杯・ACK・欠落区間

- **満杯**: 最古のレコードから、ACK 済みかどうかを問わず捨てて追記を続ける。捨てた範囲は metadata に残る（`acked` と `tail_seq` の差）。
- **ACK**: `ring_ack(upto)`。後ろへ戻る、または最新より先の ACK は無視する。ACK 自体では何も解放しない。
- **回収**: `ring_reclaim()` が `tail` を ACK の直後へ進めて metadata を commit する。
- **欠落区間**: `ring_gap()` が `first = acked + 1`、`last = tail_seq - 1` を返す。同期の開始時に gap レコード（[formats.md](./formats.md) §5）として送る。スマホが ACK して gap を取り込むと消える。再起動しても残る。

### 5.6 空き容量の 2 つの見方

- `ring_free()` は空き block 数。消去前でも、追記時の erase-on-enter が書込みより先に消すので使える。
- `ring_erased_ahead()` は、既に消去済みの先の領域（RAM の知識で、起動すると忘れる）。**消去を書込みより先に済ませられない運用（Premium の CR2032 時、Standard のゲートが閉じている間）では、こちらを空きとして数える。**
- 空き領域が減る要因は、tail より前の死んだバイト、領域終端の hole、最後に捨てたレコードの端数、切れたため書けなかったレコード、クラッシュ後に飛ばした block の残り。合計で **レコード 3 個と block 2 個以内**（テストで検証済み）。

### 5.7 制約と RAM

- **起動時の走査はレコード数に比例**する（1 件につき小さい読出し 2 回）。516 MB を満杯まで溜めた状態は約 16k 件で、1 回の読出しが 100〜200 µs 級なら数秒（未実測）。チェックポイントか二分探索は、起動時間が問題になってから入れる。
- **bad block は ring が扱わない。** ring は全 block が使える前提で、不良 block の読み飛ばし・置換は NAND ドライバの仕事（§6）。
- **追記中に次の chunk を溜める RAM が要る。** 音声は 4 kB/s で来続けるので、書込み中も音声の取り込みが続く。現在の `ring_append` は payload 全体を渡す形なので、32 KB の面が 2 つ（64 KB、RAM 256 KB の 25 %）になる。ストリーミング追記（page 単位で渡す）にすれば 1 面で済む（仮置き。#4 で決める）。
- 電流とエネルギーは NAND の実機で測る（未確定 #3、#4）。

---

## 6. NAND ドライバの要件

方針。未知の部品の上では動かさないことを基本にする。

### 6.1 識別と抽象化

- JEDEC ID（9Fh）から `nand_descriptor` を引く。ベンダーごとに違う項目を descriptor に持たせる:
  - page size（2 KB / 4 KB）、block 構成、plane 数と plane-select ビット
  - ECC の方式（on-die / host）とステータスのビット配置
  - feature register のアドレス
  - 不良 block マーカーの位置
  - read モード（buffer / continuous）
  - DPD の有無・コマンド・復帰時間
  - tPROG / tBERS / tRD の最大値
  - 最大 SPI クロック
- chunk の page 数は page size から決める（32 KB = 4 KB × 8 page、または 2 KB × 16 page）。
- **未知の ID（他社、3 V 品、未知の容量、`00 00 00`、`FF FF FF`＝バスが浮いている）はすべてエラー**にする。ring を理解できない部品の上で起動しない。
- 起動時の最初に、DPD に残っているチップを起こしてから ID を読む。最初の転送が失敗しても止まらない。
- 忙しいチップに命令を出さない（直前に idle を確認する）。BUSY が解除されない場合はタイムアウトのエラーにする。

### 6.2 読み書きの規則

- **Buffer Read（BUF = 1）を使う。** W25N08LW の Continuous Read / Sequential Read は読出し電流が 45〜55 mA typ（単線）で、Buffer Read の 15 mA typ の約 3 倍。§4.1 は Buffer Read 前提。
- **BUF と ECC-E は起動時に明示設定する**（既定値は型番サフィックスで異なる）。
- ECC は ON。読出しのたびに ECC ステータスを確認する。
- page は block 内で順番に program する。ランダムな page への program は禁止。1 page を 2 回 program しない（NAND）。
- フル page の program を優先し、各 program / erase の後にステータス（P_FAIL / E_FAIL）を確認する。

### 6.3 bad block 管理（BBM、必須）

- bad block の管理は **host（FW）の仕事**。W25N08LW の BBM 命令と LUT は、host が検出した不良 block を置換登録するための支援機能で、SSD のようにチップが自動で管理するわけではない。
- 起動時に工場出荷時の不良マークを読む。運用中も、program / erase の失敗と ECC 異常を検出して block を retire し、spare pool へ置換する。bad block table 自体も冗長化する。
- ring から見える block は全部使えるものにする（§5.7）。有効容量と空きは、不良 block を除いた実数で数える（§7.3）。

### 6.4 電源投入・遮断

- 起動: tVSL（W25N08LW 1 ms、XT26Q18D 3 ms、Macronix 2 ms）と tPUW（W25N08LW 3.3 ms）を守り、reset と status 確認、feature の設定（§6.2）のあとで append を再開する。/CS は VCC に追従させる（§3.1）。
- 遮断の順序: [power.md §5.2](./power.md)。

---

## 7. 容量とバックログ連動ビットレート

### 7.1 利用可能容量

```text
usable_bytes = Gbit * 2^30 / 8 * (4016 / 4096) * 0.98
hours        = usable_bytes / (kbps * 1000 / 8 * p) / 3600
```

`4016 / 4096` は有効 block 率（XT26Q18D の保証値。他の品は確認する）、`0.98` は metadata と ring の余裕。b_eff = kbps × p。

| 容量 | 利用可能 | b_eff の上限（48 h 保証） |
|---|---|---|
| 512 Mbit | 64.5 MB | 3.0 kbps |
| 1 Gbit | 129 MB | 5.97 kbps |
| 2 Gbit | 258 MB | 11.9 kbps |
| 4 Gbit | 516 MB | 23.9 kbps |
| 8 Gbit | 1,032 MB | 47.8 kbps |

壁時計での保持時間（h。固定レート、満杯まで）:

| 容量 | 32 kbps、p = 1.0 | 32 kbps、p = 0.6 | 32 kbps、p = 0.4 | 32 kbps、p = 0.25 | 16 kbps、p = 1.0 | 16 kbps、p = 0.6 | 16 kbps、p = 0.4 | 16 kbps、p = 0.25 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 512 Mbit | 4.5 | 7.5 | 11.2 | 17.9 | 9.0 | 14.9 | 22.4 | 35.8 |
| 1 Gbit | 9.0 | 14.9 | 22.4 | 35.8 | 17.9 | 29.9 | 44.8 | 71.6 |
| 2 Gbit | 17.9 | 29.9 | 44.8 | 71.6 | 35.8 | 59.7 | 89.6 | 143 |
| 4 Gbit | 35.8 | 59.7 | 89.6 | 143 | 71.6 | 119 | 179 | 287 |
| 8 Gbit | 71.6 | 119 | 179 | 287 | 143 | 239 | 358 | 573 |

- 512 Mbit・1 Gbit の行は Standard の rolling buffer の目安（§1.2。Standard は 48 h を保証しない）。
- 11 月の Premium デモ機 F35UQA512M も上の目安に近い。出荷時の最小有効 502 block × 128 KiB × 0.98 を 4,000 B/s で割ると約 **4.48 h**（32 kbps、p = 1）。0.98 の管理領域等は設計仮定で、イベント・padding・実際の bad block と同期を含む実測の保証ではない。デモの 48 h 保存は対象外。
- AAD は「音がある」で動くので、p は音声だけの割合ではない。街中なら 0.9 以上になりうる。**p は実データで測って決める**（未確定 #6）。

### 7.2 縮退側の最低レートと容量の階層（Premium、方針）

p = 1 で 48 h を保証できる最低レートが、NAND の階層を決める。

| 縮退側の最低レート | 満杯まで（p = 1） | 必要な容量 |
|---|---|---|
| 32 kbps（縮退なし） | 4 Gbit: 35.8 h / 8 Gbit: 71.6 h | 8 Gbit |
| 24 kbps | 4 Gbit: 47.8 h（0.2 h 不足） | 8 Gbit。または p < 1 を前提にする |
| **16 kbps** | 4 Gbit: 71.6 h | **4 Gbit** |
| 12 kbps 級 | 2 Gbit: 47.8 h（境界） | 2 Gbit。ただし ASR で現実的でない見込み |

- **4 Gbit 基準は「縮退側の 16 kbps が ASR で使える」ことが前提。** 使えなければ最低レートを上げ、容量は 8 Gbit へ戻る。
- 縮退側のレートは 16 kbps（2,000 B/s）を**仮置き**している。縮退側の codec（2-bit / 3-bit ADPCM 級。G.726 は演算量で不利なので使わない）と下限レートは ASR の評価で決める（[audio.md §6](./audio.md)、未確定 #5）。

### 7.3 バックログ連動ビットレート（Premium 製品、確定）

**11 月デモ機には、この 48 h 用の空き容量ルールを適用しない。** 512 Mbit では開始時点で 48 h 分の `need` を満たせない。デモは既存 IMA-ADPCM の 32 kbps 固定とし、満杯時は §8.4 の最古破棄・欠落通知を行う方針。いずれも NAND backend を含め未実装・未実測。

固定 32 kbps だと 4 Gbit は p = 1 で 35.8 h しか持たない。**空き容量に応じて録音ビットレートを落とし、p の仮定なしで 48 h を保証する。**

- 通常は 32 kbps。同期できない時間が延びて満杯が近づいたら縮退モード（16 kbps）へ移る。スマホの同期（ACK）で空きが戻れば 32 kbps へ復帰する。
- 音質を落とすのは「長時間スマホ不在 + 音が出続ける」場合だけ。

**空き容量ルール**（chunk ごとに判定する）:

```text
age    = now − 最古の未同期 chunk の時刻
T_left = max(0, 48 h − age)
need   = 1.1 × R_floor × T_left          # R_floor = 縮退側のバイト/秒（16 kbps = 2,000 B/s）
free   = リングの空き（消去済みのブロックだけ数える。予備ブロックは除く）
32 kbps で録る条件 : free − R_32 × T_chunk ≥ need
復帰（縮退 → 32 kbps）: 同期で free が増え、free ≥ need + 5 % × 利用可能容量
```

- 4 Gbit・p = 1 では、32 kbps で約 20.9 h 録ってから 16 kbps へ移る。p ≲ 0.55 では縮退が起きず、p = 0.75 でも縮退は 47 h 付近まで始まらない。
- 式: `slack = U − 1.1 × R16 × 48 h = 135.7 MB（4 Gbit）`、`t(32 kbps) = slack / (R32 × p − 1.1 × R16)`（U = 利用可能バイト）。
- 固定のしきい値で組むなら、48 h × 1.1 = 52.8 h を満たす必要がある（2 段なら 52 % 以下、3 段なら 35 % / 65 % など）。「50 % 超で 24 kbps、75 % 超で 16 kbps」は p = 1 で 47.8 h しか持たず 48 h に届かないので使わない。2 段（32 / 16 kbps）で保証には足りる。
- 利用可能容量と free は、不良 block を除いた**実数**で数える（bad block が増えても保証が崩れない）。
- ACK 済みで消去待ちの block を free に数える場合は、その消去を書込みより先に実行する。低エネルギーで消去を延期している間は数えない（§8.2）。その運用では `ring_erased_ahead()`（§5.6）を使う。
- 48 h を超えてスマホ不在が続くと、ring は最古の chunk を上書きしながら、直近 48 h を保持するようにレートを自動調整する見込み（要シミュレーション、未確定 #5）。

実装要件:

- chunk header の codec フィールドで、chunk ごとにモードを切り替える（[formats.md](./formats.md) §2）。
- **chunk 先頭で ADPCM の predictor と step index を再初期化**する（IMA-ADPCM のブロック形式）。モード切替や欠落があっても、その chunk だけで復号できる。
- 空き容量が境界付近でモードが行き来しないよう、ヒステリシスを持たせる（復帰は 5 % の余裕）。
- 縮退中は BLE と NAND のエネルギーも半減する。
- 実装は `fw/lib/bitrate/`（`struct br_cfg`: `hi_Bps` 4000、`lo_Bps` 2000、`chunk_s`、`horizon_s` 48 h、`margin_pct` 10、`resume_pct` 5）。

### 7.4 シミュレーションで確かめた結果（実装は `fw/lib/bitrate/`）

実際の ring を RAM 上のフラッシュ（256 KB block、4 KB page、1,968 data block = 515.9 MB）に載せ、8 s ごとのレコードで、スマホ不在のまま流した。

- p = 1 の最初の縮退（LO）は **20.9 h** で、§7.3 の計算と一致した。p = 1・0.75・0.5 のいずれも **48 h 以内に上書きは起きない**。p = 0.5 は縮退に入るのが 71.5 h。
- 切替は各条件で 1 回で、しきい値付近で往復しない（ヒステリシス 5 %）。
- **レコードが page の整数倍にならないと、パディングで p = 1 の最初の縮退が 19.9 h に縮む。** 容量計算の前提として、レコード全体を page の整数倍にする（[formats.md](./formats.md) §3）。
- 詳細な表は issue のログに残す（#5）。

---

## 8. 耐電断と消去の運用

録音中・metadata 更新中・erase / program 中に電源が落ちても、commit 済みの既存ログを壊さない。filesystem より単純な append-only ring を使う（§5）。

### 8.1 原則（確定）

- **commit marker は最後に書く。** 起動時に commit marker と CRC が揃わないレコードは未完了として捨てる。CRC は header と payload の全体にかける。
- **metadata は single copy にしない。** A/B 二重化 + 世代番号。新しい copy を完成させてから古い copy を無効扱いにする（§5.3）。
- bad block は工場出荷時だけでなく運用中にも発生すると考える（§6.3）。
- やってはいけないこと:
  - 有効な block をその場で書き換える
  - metadata を in-place で更新する
  - ACK 済み範囲を消去する前に、新しい metadata を commit しない（順序は逆: metadata が先）
  - erase と重要な metadata の更新を同時に行う
- chunk の中の program 順序を守り、header・commit・metadata を一度に連打しない。最後の CRC / commit が完了するまで、そのレコードは valid にしない。

### 8.2 消去（GC）の運用（方針）

- NAND の消去はまとまったエネルギーを使うので、低残量時は延期できる設計にする。
- BLE 同期の後、ACK 済み block を即座に消さず **消去候補の queue に入れ**、電力に余裕のあるときにまとめて処理する。
- 余裕のあるうちに消去済み block を確保しておく（`ring_pre_erase()`）。

```text
低電力 / 電圧ゲート不通過 : erase を延期し、消去済み block へ書く
余裕あり                  : まとめて erase
```

### 8.3 電源条件別の書込み方式

電源 source・電圧しきい値・reservoir は [power.md](./power.md)（Premium の CR2032 運転は §3.6、Standard の書込み・消去の電圧ゲートは §4.5、reservoir は §4.4）。ここでは書込み側の方針を持つ。

| 条件 | 書込み | 確度 |
|---|---|---|
| Premium の CAP 運転 | RAM に 64〜128 KB 溜め、フル page を順次 program（連続バッチ）。≈ 24 ms は通常バッチの概算で、実機の最悪時間は別途確認する | 方針 |
| Premium の CR2032 運転 | 連続バッチ program は禁止。フル page を 1 枚ずつ program し、BUSY 解除・P_FAIL の確認と電源回復待ちを挟む。間隔は、最終 page size・ヘッダ・metadata・復帰処理を含めて決める（4 KB / 32 kbps の payload だけで約 1.024 秒/page） | 仮置き |
| Standard（CR2477） | 電池電圧のゲートを通ったときだけ program・erase を行う。ゲートが閉じている間は erase を延期し、消去済み block へ書く。消去済み block が尽きたら、下の CR2032 運転の規則（保存制限・欠落記録）と同じ扱いにする。バッチ長・page 分散は未決（未確定 #8） | 方針（数値未定） |

CR2032 運転の追加規則（Premium）:

現アプリは内蔵 RRAM backend で、以下は外付け NAND へ接続するときの要件。`ring_pre_erase` は data block 用であり、metadata commit／起動更新／reclaim の erase を除去しない。**新規 program／erase の許可は backend の各 command 直前で判定**し、発行済み command は最悪時間の電源余裕で完了させる。未検証の COIN 書込みは閉じ、有限の保存範囲と欠落報告を持つ。

- CAP 運転中に消去済み block を確保し、CR2032 運転はそこへフル page 単位で書いて電源回復を待つ。chunk の header / CRC / commit と A/B metadata も同じ電力予算・回復待ちに含める。
- page を分散しても erase パルスは小さくならない。消去済み容量と保存レートから CR2032 中の継続可能時間を計算し、予備 block・ヘッダ・運用中の bad block も差し引く。
- 消去済み block が尽きたら、電池の劣化・低温を含めて検証済みの電源条件でだけ erase する。ストレージ満杯を理由に無条件で erase しない。
- 検証済みの条件を満たせないときは、新しい音声の保存を制限する。RAM に滞留させる量は有限とし、保存できなかった区間は、電源・空き容量が回復した後に欠落として記録する。保存の異常を他の機能へ波及させず、電源復帰後に再開してスマホへ通知する。
- この制限は電源不足時の例外で、無期限に消去を延期しつつ保存を続けられるとはしない。
- 回復待ちの間も音声の取り込みが続く。RAM 容量は、遅い保存速度・mode 切替時のバックログ・最大回復待ち時間を含めて決める（CAP 時の「バッチ + 数十 ms 分」を CR2032 時へ流用しない）。保存速度が生成速度を下回るときは、検証済みの低レートへ縮退するか、保存制限・欠落記録へ移る。
- source の切替は program の途中でも起こりうる。現在の page を安全に完了できる電源余裕を測り、それ以降を分散する。

### 8.4 容量到達時（確定）

通常は **未同期でも最古の chunk から上書きして録音を続ける。** ただし、erase の電力を確保できない場合は §8.3 の保存制限・欠落記録を優先し、満杯だけを理由に電源条件を迂回しない。

```text
空き block が無い
 ↓
最古の未同期 block を「破棄範囲」として metadata に記録（連番の範囲。`acked` と `tail_seq` の差、§5.5）
 ↓
新 metadata を commit
 ↓
電源条件を確認し、その block を erase（満たせなければ保存制限・回復待ち）
 ↓
append を続ける
```

- 破棄範囲は metadata とスマホ同期で「欠落区間」として通知する（§5.5）。
- Premium 製品目標では、バックログ連動ビットレート（§7.3）でスマホ不在 48 h までは上書きが起きない設計とする。Standard と **512 Mbit の 11 月デモ機**は 48 h を保証せず、数時間で上書きが始まりうる（§7.1）。

### 8.5 電源断試験（確定）

自動の電源断試験を必須にする。注入点:

- chunk の header 書込み中 / payload 書込み中 / CRC・commit 書込み中
- metadata の更新中（A/B の切替中）
- block の erase 中
- bad block の置換中
- BLE の ACK 直後
- CAP → CR2032 の切替直前（Premium）

合格条件: **失ってよいのは直前の未 commit の chunk だけで、それ以前の commit 済みログは 100 % 残る。**

| 段階 | 回数 | 現状 |
|---|---|---|
| ホストのシミュレータ（`fw/lib/ring/test_ring.c`） | ランダムに 1,000 回以上 | 実施済み（ランダム 3,412 回 + 1 操作の全点注入 118,968 点、損失 0、規則違反 0）。詳細は issue のログ（#3） |
| 実機 | ランダム 10,000 回 | 未実施（NAND の実機が要る、未確定 #4） |
| フォールト注入（実機） | bad block の増加、ECC 訂正ビット数の増加、SPI 通信エラー、BUSY 固着 | 未実施 |
| brownout 前の shutdown | commit が成功する率 | 未実施 |

---

## 9. 検証の仕組み

### 9.1 ホスト試験（`fw/lib/ring/test_ring.c`）

フラッシュシミュレータは、program を AND のみ、消去を block 単位とし、NAND では page の 2 回 program を禁止する。電源断は **program の任意のバイトの途中**（一部のビットだけ書けた状態）と、**消去の任意の page の途中**（半分消えた状態）に注入する。

- 試験の形状: NOR 形状（1 KB / 64 B。製品では使わないが、ring の汎用性の確認として残す）、NAND 2 KB / 256 B（単一 program）、RRAM 4 KB / 256 B / 16 B（Phase 0 の ring）、NAND 256 KB / 4 KB（4 Gbit の形）。
- 確認すること: 回復後に、保持しているレコードの連番が連続で、全 payload が CRC まで一致し、commit 済みが欠けず、tail が戻らない。
- 変異試験: 次の 6 つを入れると、いずれもテストが落ちる。metadata の更新を消去の後に回す / クラッシュ後に block の残りを再利用する / 探索の深さを 1 にする / 領域終端の hole を作らない / 空の ring で `tp` を更新しない / 最新より先の ACK を受理する。
- 実行: `make -C fw/lib ring`（約 2.5 分）、`make -C fw/lib ring-quick`（NOR 形状だけ、約 10 秒）。

### 9.2 実機（Phase 0 の RRAM）で確認済みのこと

満杯での上書き、再起動後の復元、ACK 回収が RRAM 上で動く。書込み中は CPU が寝る（RRAM コントローラ待ち）ので、DWT のサイクルカウンタは止まる。時間は wall clock（`k_cycle_get_32`）で測る。測定値は issue のログ（#3）。

---

## 未確定

| # | 項目 | 状況 | 紐づけ |
|---|---|---|---|
| 1 | **NAND の型番と容量**: Standard の 512 Mbit か 1 Gbit（価格差と DPD で決める）、Premium の 4 Gbit のベンダー（複数ソースで評価）。候補品のデータシート確認（ECC・page・DPD・待機、Alliance の元ダイと PCN）と、LGA-8 6×8 / 8×6 が WSON-8 8×6 の足跡に載るかの確認。3.3 V 生 NAND の保険案（§2.2）を持つかどうか | 調査中 | 新規 issue 候補: NAND 型番の選定（Standard 512 Mbit〜1 Gbit / Premium 4 Gbit） |
| 2 | NAND ドライバの実装（nand_descriptor、ECC ステータス、BBM、spare block の数、read モード設定） | 未着手 | 新規 issue 候補: NAND ドライバ（ID 判別・ECC・BBM） |
| 3 | NAND の実機項目: ID 読出し、DPD 電流、program / erase の電流と時間、Zephyr の SPI 接続、負荷時の NAND 端子の最低電圧 | Phase 1 基板の到着後 | #27 |
| 4 | 実機の NAND でのエネルギー（§4.1 の mJ/MB）と待機運用の比較（DPD / ゲート）、実機の電源断 10,000 回、フォールト注入、brownout 前 shutdown の成功率、起動時走査の時間 | 実測待ち | #27 |
| 5 | 縮退側の最低レートと codec、それに伴う容量階層（4 Gbit か 8 Gbit か）。48 h 超の挙動のシミュレーション | ASR 評価待ち | #19 |
| 6 | 保存デューティ p の実測（AAD / VAD 通過割合） | 実測待ち | #20 |
| 7 | Standard の電池寿命（約 1 年）の再計算（NAND の書込みエネルギー、DPD の有無、page size、p、ビットレート） | 計算待ち | #17 |
| 8 | Standard の書込み方式（バッチ長・page 分散）。電圧ゲートのしきい値と reservoir の容量は [power.md §4.4・§4.5](./power.md) | Phase 1 の実測待ち | #18、#12 |
| 9 | Standard に縮退モード（バックログ連動ビットレート）を使うか。使う場合の `horizon_s`（48 h を保証しないので、何時間分を基準にするか） | 未検討 | 新規 issue 候補: Standard のビットレート連動の適用 |
| 10 | Premium の CR2032 運転の保存方式: page 分散の間隔、継続可能時間、消去を許す電源条件、RAM 滞留量、bulk の容量 | 実測待ち | 新規 issue 候補: Premium の CR2032 運転中の bulk と電池電流（保存方式を含む） |
| 11 | SPIM00 を NAND 専用にできるか。I²S・BLE・OTA・codec・ログを引いた RAM マップで、64〜128 KB のバッチを確保できるか。ストリーミング追記の採否 | 未決 | #4 |
| 12 | system rail を 1.8 V にするか 1.9 V にするか（NAND の上限と MCU の POR 下限の両側で確認。決定は [power.md §2.2](./power.md)） | 実測待ち | 新規 issue 候補: system rail 電圧（1.8 V / 1.9 V）の判断 |
| 13 | 起動時の走査（レコード数に比例）の最適化: Premium で満杯のとき数秒かかる見込み | 起動時間が問題になってから | 新規 issue 候補: ring の起動時間（チェックポイント / 二分探索） |
