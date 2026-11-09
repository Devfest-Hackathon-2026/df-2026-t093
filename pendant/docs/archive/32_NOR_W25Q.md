# 32. SPI NOR（W25Q..JW）— ID判別・DPD・書込みエネルギー

Status: **DRAFT（2026-10-14、issue #8）。** 実装は `fw/lib/nor/`（純C、コマンドレベルのモデルでホスト試験）。実機（Phase 1基板）の項目は §6。

> **2026-10-14: Standard の NOR は不採用（[22 §3.3](./22_製品バリエーション_Standard_Premium.md)）。** 価格（1.8 V NOR 256 Mbit ¥1,178〜1,787 に対し、1.8 V NAND 512 Mbit ¥380、1 Gbit ¥1,799）でも、書込みエネルギー（§3、NAND の約 9〜22 倍）でも NAND に負ける。§3 の「要判断」は (c)「Standard でも NAND を使う」で解決した（障害だった NAND の価格と在庫は [20 §3.8](./20_ストレージ方式_設計方針からの再分解.md) で解消）。本書と `fw/lib/nor/` は記録として残す（ID 判別・DPD の手順は NAND driver の参考になる）。

## 1. 結論

- JEDEC IDから容量を決め、未知のIDは起動時にエラーにするドライバと、**コマンド単位のNORモデル**に対する試験を作った。128 / 256 / 512 Mbit の3種で、ringの範囲がチップ全体に一致する。
- **重要な発見: NORの書込みエネルギーはNAND想定の約9倍で、Standardの電池寿命（22 §3.2「約1年」）が成り立つ条件が狭くなる**（§3）。消去を4 KBセクタから **64 KBブロック（D8h）に変えると、消去のエネルギーは1/4** になるので、ring は64 KBブロックで消去する。それでも p=0.4 で約235日、約1年は p ≲ 0.25 まで。
- 256 Mbit は 3 バイトアドレスで届かない（16 MiB まで）ので、**4バイト命令（13h / 12h / 21h / DCh）を使う**。モードビットは使わない（リセットをまたいで状態が残らない）。

## 2. データシートの値（W25Q256JW_DTR Rev K、2021-03-17）

出典: https://www.winbond.com/resource-files/W25Q256JW_DTR%20RevK%2003172021.pdf（§8.1、§9.2、§9.4、§9.6）。2017年版の SPI 版（Rev B）は同じ型番のIDを `6019h` と記載し、Rev K は `8019h`。**ID は両方を受け付ける**（`60 19` と `80 19`）。

| 項目 | 値 |
|---|---|
| JEDEC ID（9Fh） | `EF 80 19`（Rev K）、`EF 60 19`（Rev B）|
| VCC | 1.7〜1.95 V |
| 待機電流 ICC1 | typ 10 µA、max 35 µA |
| **Power-down 電流 ICC2** | **typ 1 µA、max 15 µA** |
| tDP（/CS high → power-down）| max 3 µs |
| **tRES1（ABh → 次の命令）** | **max 30 µs** |
| SPI クロック | Read 03h/13h: 50 MHz、その他: 104 MHz |
| Page Program | tPP typ 0.8 ms、max 5 ms |
| 4 KB Sector Erase | tSE typ 50 ms、max 400 ms |
| 64 KB Block Erase | tBE2 typ 200 ms、max 2,000 ms |
| Chip Erase | tCE typ 90 s、max 400 s |
| 電流: Read 50 MHz / Program / Erase（ICC3 / ICC5 / ICC6）| typ 6 mA / 12 mA / 12 mA（max 8 / 20 / 20 mA）|

- 命令: Release Power-down / Power-down = ABh / B9h、Read 03h / 13h（4バイト）、Page Program 02h / 12h、Sector Erase 20h / 21h、64 KB Block Erase D8h / DCh、Read SFDP 5Ah。
- **128 Mbit / 512 Mbit のIDは慣習から決めたもの**（最後のバイトが log2(bytes): 0x18 / 0x20、種別 0x60 / 0x80）で、**データシートで確認していない**。確認するまで、これらの型番はコード上の「想定」。

## 3. 書込みエネルギー（データシートの typ / max から計算）

`E = I × V × t`（V = 1.8 V、電池側の変換効率は含めない）。

| 動作 | typ | max |
|---|---:|---:|
| Page Program（256 B）| 17.3 µJ（67.5 nJ/B）| 180 µJ（703 nJ/B）|
| 4 KB Sector Erase | 1.08 mJ（**264 nJ/B**）| 14.4 mJ（3,516 nJ/B）|
| 64 KB Block Erase | 4.32 mJ（**65.9 nJ/B**）| 72 mJ（1,099 nJ/B）|
| **書込み合計 / byte（消去 + program）、4 KB セクタ** | **331 nJ/B** | 4,219 nJ/B |
| **書込み合計 / byte、64 KB ブロック** | **133 nJ/B** | 1,802 nJ/B |

- NAND の 15 nJ/B（20 §4、program + read）と比べて、**64 KB ブロックでも約9倍、4 KB セクタでは約22倍**。NOR は「消してから書く」ので、書く量に比例した消去が必ず付く。
- 32 kbps（4,000 B/s）を連続で書く（p=1）と、64 KB ブロックで **約0.53 mW**、4 KB セクタなら 約1.32 mW。

### Standard の電池寿命への影響（22 §3.2 の表の差し替え、typ 値）

22 §3.2 の表は 19 / 20 の電力モデル（NAND の 15 nJ/B、約 60 µW × p）から電源の固定損失を引いたもので、NOR の書込みエネルギーを含まない。ストレージ分を NOR（64 KB ブロック）に置き換えると:

| p | 22 の表（µW）| NOR に置換（µW）| CR2477（3 Wh）の目安 |
|---:|---:|---:|---:|
| 1.00 | 773 | **約 1,247** | 約 100 日 |
| 0.60 | 483 | **約 767** | 約 163 日 |
| 0.40 | 343 | **約 532** | **約 235 日** |
| 0.25 | 233 | **約 351** | 約 356 日 |

- 4 KB セクタ消去のままなら、p=0.4 で約 849 µW（約 147 日）。**ringは64 KBブロックで消去する**ことで約 1.6 倍良くなる。
- **約 1 年を保つ条件は p ≲ 0.25**（AAD / VAD で保存する割合を下げる）。または縮退側の 16 kbps を主にする（書込み量が半分になる）。
- max 値（tSE 400 ms、tPP 5 ms、20 mA）では約 10 倍悪くなる。実際の値は Phase 1 で測る（§6）。
- **要判断**: Standard に NOR を使い続けるか。選択肢: (a) p を下げる運用（AAD）、(b) 16 kbps を標準にする、(c) Standard でも NAND を使う（コストと在庫が問題、20）、(d) 電池寿命の目標を約 8 か月に下げる。決定は 22 の Standard 要求の見直しと一体。

## 4. 実装（`fw/lib/nor/`）

- **`nor_identify(id)`**: 製造者 `EF`、種別 `60` / `80`、容量コード `18 / 19 / 20` を表で引く。**他社、3 V 品（`40`）、未知の容量、`00 00 00`、`FF FF FF`（バスが浮いている）はすべて `-ENODEV`**。黙って動かさない。
- **`nor_probe()`**: 最初に ABh（Release Power-down）と tRES1 の待ちを入れる。リセットの後に NOR が power-down に残っていても IDが読める。次に 9Fh。最初の転送が失敗しても止まらない。
- **書込み**: 256 B のページ境界で分割（ページをまたぐ program はチップ内で巻き戻る。モデルが巻き戻りを再現して検出）。WREN → program → WIP をポーリング（最大 10 ms）。**忙しいチップに命令を出さない**（直前に idle を確認）。WIP が消えなければ `-ETIMEDOUT`。
- **消去**: 4 KB（20h / 21h）と 64 KB（D8h / DCh）。アライメントが違えば `-EINVAL`。
- **DPD**: B9h + tDP 3 µs、ABh + tRES1 30 µs。
- **`nor_ring_dev()`**: ring（29）の `ring_dev`。block = 64 KB、page = 256 B、wunit = 1、2回 program 可。`format` はセクタ単位なら 8,190 回の消去（約7分、typ）なので、**工場・開発では Chip Erase（C7h、typ 90 s）を使う**（ドライバへの追加は未実装）。

### 試験（`make -C fw/lib nor`）

コマンドレベルのモデル（WREN / WIP の規則、ページ内の巻き戻り、AND だけの program、3 / 4 バイトアドレス、deep power-down とその待ち時間）に対して:
1. 3種類のIDと、256 Mbit の 2 つの種別バイトで容量・消去単位・アドレス長が一致
2. 他社・3 V 品・未知容量・浮いたバス・バス故障を拒否
3. power-down に残ったチップを probe が起こし、tRES1 を守る
4. 16 MiB を超える領域で4バイトアドレスが効く（24 ビットに別名が出ない）
5. 300 B を 200 から書くと 2 回の program に分かれ、巻き戻らない
6. program は 1→0 だけ、消去で戻る、範囲外と未アライメントは拒否
7. DPD: B9h のあと ABh 以外は聞こえず、exit が tRES1 を待つ
8. WIP が消えない場合は `-ETIMEDOUT`
9. ring が各容量のチップ全体（128 / 256 / 512 Mbit = 254 / 510 / 1,022 ブロック + meta 2）を使い、再起動後も復元できる。消去は64 KBのみ

## 5. 1.8 V と SPI

- VCC は 1.7〜1.95 V。TPS62840 の ±1.5 %（19 §5）と、電池低下時の下限は別に確認する（30 §4 #3）。
- Read は 50 MHz まで。キットのレベル変換（25）を挟むと、変換 IC の遅延で上限が下がる。**変換 IC の数値は未確認**（#22 の調査）。
- ring の運用で読むのは同期のときだけで、Read 50 MHz の電流は 6 mA（約 1.7 nJ/B）。無視できる。

## 6. 未了・Phase 1（別 issue 候補）

- Zephyr の SPI への接続（`nor_bus` の実装）と devicetree。Phase 1 基板が要る
- 実機: ID 読出し、DPD 電流（待機 10 µA → 1 µA の確認）、**program / erase の電流と時間（§3 の typ との差）**
- Chip Erase（format 用）
- 128 / 512 Mbit のデータシートで ID を確認
- 電源断中の NOR（消去・program の途中）を含む耐電断試験は ring 側（29、NOR 型の形状で 3,412 回など）で済み。NOR コマンドレベルのモデルでの電断は未実施

## 7. 出典

- Winbond W25Q256JW_DTR Rev K（2021-03-17）: https://www.winbond.com/resource-files/W25Q256JW_DTR%20RevK%2003172021.pdf
- 同 Rev B（2017-12-08、ID `6019h`）: https://www.winbond.com/resource-files/w25q256jw%20spi%20revb%2012082017.pdf
