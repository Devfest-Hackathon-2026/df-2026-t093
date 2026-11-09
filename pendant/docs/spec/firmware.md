# ファームウェア仕様: OTA・パーティション・ウォッチドッグ・時刻管理

対象: nRF54L15（Zephyr / NCS、sysbuild）。このファイルが持つもの: パーティション、MCUboot、BLE DFU、ウォッチドッグとリセット復旧、時刻管理（GRTC と外付け RTC）、`fw/lib` の対応表。

他のファイルが持つもの（ここでは繰り返さない）:
- 電圧のしきい値、電源状態機械（[power.md §4.2](./power.md)）、ブラウンアウト方針（[power.md §6](./power.md)）、OTA のエネルギー条件（[power.md §9](./power.md)）
- レコード形式（chunk、timefix、event、gap）、BLE 同期、ACK: [formats.md](./formats.md)
- ring、NAND、電源断の安全: [storage.md](./storage.md)
- ペアリング、鍵、暗号化、署名鍵の運用、APPROTECT、出荷時の書込み手順: [security.md](./security.md)
- ピン割当、テストポイント、SWD パッド: [board.md](./board.md)

確度の凡例: **確定** = 決定済み。**方針** = 向きは決まっているが数値は未定。**仮置き** = 実測で決める。

---

## §1 要約

| 項目 | 内容 | 確度 |
|---|---|---|
| ブートローダ | MCUboot（ED25519 署名、test boot + confirm） | 確定 |
| 更新経路 | 出荷前に MCUboot とアプリを書く。以降はスマホから BLE（MCUmgr / SMP）で DFU する。iOS と Android の両方で使える | 確定 |
| SWD | 開発・出荷前書込み・救済用に残す（パッドの仕様は [board.md](./board.md)） | 確定 |
| 外部 NAND | 更新経路に入れない（ステージングに使わない） | 確定 |
| ウォッチドッグ | ハードウェア WDT をハートビート監督で餌やりする | 確定 |
| リセット原因 | 起動時に読み、event レコードとして記録する | 確定 |
| 時刻 | GRTC（起動内の単調カウンタ）で数え、スマホ同期のたびに遡って時刻を付ける。外付け RTC（RV-3032-C7）は footprint だけ置く | 確定（RTC を載せるかは §5.4 の基準で決める: 仮置き） |
| OTA の許可条件 | 電池状態が NORMAL のときだけ | 確定（しきい値は [power.md §4.2](./power.md) の実測で動く） |

---

## §2 NVM・パーティション・MCUboot

### §2.1 パーティション表（nRF54L15 の RRAM 1,524 KB）

| 領域 | アドレス | サイズ | 用途 | 確度 |
|---|---|---:|---|---|
| `mcuboot` | 0x00000 | 62 KB | MCUboot（実測 42.7 KB、使用率 67 %） | 確定 |
| `image-0`（slot0） | 0x10000 | 320 KB | 実行中のアプリ（実測 177 KB、55 %） | 確定 |
| `image-1`（slot1） | 0x60000 | 320 KB | OTA の受け取り先 / 旧イメージ | 確定 |
| `storage` | 0xB0000 | 36 KB | settings、初期化で作る秘密（[security.md §6](./security.md)）。現状は未使用 | 確定 |
| `ring` | 0xB9000 | 784 KB | RRAM 版の録音 ring（Phase 0 の録音先。196 ブロック） | 確定 |

- 定義は `fw/boards/pendant_partitions.dtsi`。アプリの overlay と `fw/sysbuild/mcuboot.overlay` の両方から取り込み、同じレイアウトを共有する。
- **MCUboot の overlay には `zephyr,code-partition = &boot_partition` が必須。** XIAO のボード定義は code-partition を slot0 にしているので、引き継ぐと MCUboot がアプリのスロットの上にリンクされ、起動後にロックアップする。
- 余裕はイメージスロットに 143 KB（45 %）、MCUboot に 19 KB。アプリが 320 KB に近づいたら、ring を小さくしてスロットを広げる。
- アプリが大きくなりすぎたときの優先順位: (1) 機能削減・LTO、(2) MCUboot モードの最適化、(3) スロット構成の見直し、(4) 最後に外部 NAND のステージングを検討（v1 では採らない）。
- 理由（外部 NAND を更新経路に入れない）: NAND のドライバと不良ブロック管理を boot chain の依存に入れたくない。更新失敗時の復旧経路を単純にする。MCUboot は primary / secondary の 2 スロットで安全な test / revert を持つ。
- 製品の録音先は外付け NAND（[storage.md](./storage.md)）。NAND 版で `ring` 領域をどう使うかは末尾の未確定。

### §2.2 MCUboot とイメージ

- sysbuild: `fw/sysbuild.conf` に `SB_CONFIG_BOOTLOADER_MCUBOOT=y`。
- 署名: ED25519。**NCS 同梱の開発用の鍵（既定）は開発専用で、配布してはならない。** 製品の署名鍵の運用は [security.md §7](./security.md)。
- アプリのバージョンは `fw/VERSION`。起動時に `ota: running image X.Y.Z+0, confirmed N` をログする。
- ビルド: `west build -p always -b xiao_nrf54l15/nrf54l15/cpuapp -d fw/build fw`。出力は `fw/build/mcuboot/zephyr/zephyr.hex` と `fw/build/fw/zephyr/zephyr.signed.hex`。

### §2.3 出荷前書込みと更新経路

```text
開発        : SWD（pyocd）で MCUboot + 署名済みアプリを書く
出荷前      : SWD で MCUboot + 初期アプリを 1 回だけ書く（確定）
出荷後      : スマホから BLE DFU（MCUmgr / SMP）。SWD は救済用に残す
```

- 利用者が USB で書く経路は作らない。理由: nRF54L15 自身に USB が無く、XIAO の USB-C は SAMD11 の CMSIS-DAP / UART ブリッジにつながるため、ドラッグ＆ドロップ（UF2 / MSC）の書込みが成り立たない。iOS には USB の経路が無い。
- dogfooding の本筋は Fanstel BC15M の自作基板。書込みは SWD パッドから行う（1.8 V のターゲットに合わせる方法と、XIAO の SAMD11 を SWD プローブとして流用できるかは未検証: [plan/prototype.md](../plan/prototype.md)、[board.md](./board.md)）。
- 出荷前に書くものと、初回起動時の秘密の作り方は [security.md §6](./security.md)。

---

## §3 BLE DFU（MCUmgr / SMP）

```text
スマホ → BLE → MCUmgr / SMP → image-1（secondary）→ MCUboot が署名検証 → test boot → アプリが confirm
```

### §3.1 設定

- `CONFIG_MCUMGR_TRANSPORT_BT`、`MCUMGR_GRP_IMG`、`MCUMGR_GRP_OS`。
- **`MCUMGR_TRANSPORT_BT_REASSEMBLY` と `MCUMGR_GRP_OS_MCUMGR_PARAMS` を両方入れる。** 再組み立てなしでパラメータだけ入れると、クライアントが 2 KB のフレームを送り、サーバが分割された書き込みを別メッセージとして処理して、エラー応答が 2 つ返る。
- クライアント: `tools/ota.py`（bleak + smpclient。Mac では Terminal.app から）。`list` / `upload FILE` / `swap`。アップロードしたイメージを test に設定してリセットする。製品のスマホアプリは別（nRF Connect Device Manager 方式）。

### §3.2 確定（confirm）と巻き戻し

- 新しいイメージは `PENDANT_OTA_CONFIRM_S`（30 s）だけ健全に動いたあと、自分で `boot_write_img_confirmed()` を呼ぶ。
- **確定前にリセットされたら MCUboot が元のイメージへ戻す（revert）。** ウォッチドッグ（§5）が動くので、ハングする新イメージは確定されずに戻る。

### §3.3 低残量ゲート

- 電池状態が NORMAL でなければ、アップロードの開始（DFU chunk）を拒否する（`MGMT_ERR_EACCESSDENIED`）。確定。
- NORMAL の定義（Standard: 電池 2.60 V 以上、仮置き）は [power.md §4.2](./power.md)。Premium の条件と OTA のエネルギー条件は [power.md §9](./power.md)。
- Phase 0 は電池電圧を測れないので、ゲートは「常に許可（`PENDANT_TEST_OTA_DENY` で拒否）」のスタブ。製品では電源状態機械（`fw/lib/powerstate`）の出力に接続する。

### §3.4 実機で確認済み（XIAO ↔ Mac、177 KB イメージ）

| 項目 | 結果 |
|---|---|
| アップロード速度 | 約 16〜20 kB/s（再組み立てあり）、約 27 s |
| 通信の途中切断と再開 | 切断した位置（60,659 B）から再開して完了 |
| swap | test で起動 → 30 s 後に自己確定 |
| 確定前のリセット | revert で元のイメージが起動（6 回すべて） |
| 低残量の拒否 | `upload refused: supply not good enough` をログし、クライアントはエラーを受ける |
| リセットの連発 | テスト起動を保留にして 10〜160 ms 間隔で 60 回リセット（5 回繰り返し）。ブリックせず、有効なイメージで起動 |

詳細なログは issue #11。

### §3.5 試験項目（OTA の失敗モデル）

| 項目 | 状態 |
|---|---|
| upload 途中の BLE 切断 | 確認済み |
| upload 途中のシステムリセット | 確認済み（上のリセット連発） |
| confirm 前のリセット | 確認済み |
| 低残量の拒否 | 確認済み（スタブ） |
| **swap 途中の電源断（USB を手で抜く）** | **未実施。今の XIAO で行う（確定）** |
| 不正な署名のイメージ | 未確認 |
| バージョンの巻き戻し（ダウングレード） | 未確認 |
| test イメージ起動後の watchdog リセット | 未確認（設計上は revert される） |

---

## §4 ウォッチドッグとリセット復旧

### §4.1 ウォッチドッグ

- ハードウェア WDT（Devicetree の `wdt31`）を有効にする。タイムアウトは `PENDANT_WDT_TIMEOUT_MS` = 8 s。
- 単一の main loop から定期的に餌をやるだけでは足りない。**各タスクが期限内にハートビートを返したときだけ、supervisor が餌をやる**（`fw/lib/health`、純 C、ホスト試験あり。FW 側は `fw/src/supervisor.c`）。CPU が動いていても NAND ドライバや BLE の状態機械がデッドロックした状態を検出できる。

| タスク | ハートビートの期限 | 監督する条件 |
|---|---:|---|
| audio | 2 s | 録音中のみ |
| writer | 10 s | ring への書き込み中のみ |
| BLE | 40 s | 常に（ATT の通知は死んだリンクを 30 s 待つことがあるため） |
| main | 20 s | 常に |

- 動いていないはずのタスクは武装解除して待たない（`health_arm` / `health_disarm`）。32 ビットのミリ秒カウンタの折り返し（49 日）でも誤報しない。

### §4.2 リセット原因の記録

- 起動時に RESETREAS（`hwinfo`）を読んで消し、**event レコード（type 4、kind = reset、arg = POR / PIN / WATCHDOG / BROWNOUT / SOFT）として ring に追記する**。レコードの形式は [formats.md](./formats.md)。受信ツールが `reset (WATCHDOG) at boot 73` のように表示する。
- 実機で確認済み: `PENDANT_TEST_HANG_S=15` で録音の 15 秒後に audio スレッドを止めると、supervisor が餌をやらなくなり、約 8 秒後にリセット。次の起動で WATCHDOG がログされ、ring に `reset (WATCHDOG)` が残る。

### §4.3 起動時の復旧手順

```text
reset
 ↓ 1. リセット原因を読む（§4.2）
 ↓ 2. 時刻の状態を確認（§5.2。RTC を載せた場合は PORF / VLF も）
 ↓ 3. 電源状態を確認（電池電圧・キャパ電圧。[power.md](./power.md)）
 ↓ 4. ストレージの復旧（commit の無い・CRC の合わないレコードで打ち切る。[storage.md](./storage.md)）
 ↓ 5. 周辺の自己診断
 ↓ 6. 録音を再開
```

- 初期化が済んでいない個体（鍵なし、ペアリング前）は 6 で録音を再開しない（[security.md §6](./security.md)）。

### §4.4 BOR と UV 縮退の順序

原則: **通常の UV しきい値 > BOR のしきい値**。BOR は最後の安全網で、通常のシャットダウン手段には使わない。BOR に入る前に電源管理側が低電圧を検出し、(1) 新規 NAND の erase / program を禁止、(2) metadata の commit を終える、(3) 録音を停止または縮退、(4) system rail を安全に落とす。しきい値の数値と縮退の対応は [power.md §6](./power.md)。

nRF54L15 のリセット回路の値（Product Specification の power-fail 表。原本の節は未確認）:

| 項目 | 値 |
|---|---|
| BOR（VDD） | System ON 1.57〜1.64 V、System OFF 1.56〜1.64 V |
| POF（power-fail 警告、設定可能） | 1.7〜3.2 V を 100 mV 刻み、ヒステリシス 40〜55 mV、±2 % |
| POR（VDD,POR） | 1.75 V（PS v1.0 §11.14.1。VDD の動作範囲は 1.7〜3.6 V） |
| nRF の動作下限 | 1.7 V（通常）、POR 時 1.75 V |

- 電池ベースの UV 段階（[power.md §4.2](./power.md)）は 1.8 V の rail が崩れる前に効くので、電池が 2.25 V のとき rail がまだ 1.8 V であれば「UV が BOR より先」が成り立つ（仮置き。TPS62840 の dropout とパルス時の電圧降下の実測が要る）。
- POF を 1.7 V に設定すると、rail が BOR に落ちる前に警告を受けられる（負荷パルスで rail が落ち込む場合の最後の保険）。使うかは実測のあとで決める（方針、未実装）。

### §4.5 再起動のチャタリング対策

低電圧からの再起動はヒステリシスを持たせる。シャットダウンは低いしきい値、再起動はそれより十分高いしきい値まで充電してから。boot 電流で電圧が再び落ちて reset loop になるのを防ぐ。しきい値は [power.md §6](./power.md)。

### §4.6 試験項目

| 項目 | 状態 |
|---|---|
| わざとハングさせる（audio） | 確認済み（§4.2） |
| NAND のタイムアウト注入 | 未実施（NAND 基板が前提） |
| BLE タスクのハング | 未実施 |
| キャパ電圧をゆっくり下げるブラウンアウト試験 | 未実施 |
| 再起動しきい値付近のチャタリング試験 | 未実施 |
| リセット原因が正しくログされるか（POR / PIN / SOFT / BROWNOUT） | WATCHDOG のみ確認済み |

---

## §5 時刻管理（GRTC と外付け RTC）

### §5.1 方針（確定）

- 時刻は GRTC（起動内の単調カウンタ）で数える。外付け RTC（RV-3032-C7）は **footprint だけ置き**、載せるかは実測で決める（§5.4）。
- スマホと同期するたびに時計を合わせ、**それ以前の区間に遡って時刻を付ける**（back-dating）。
- **記録済みのレコードの時刻は書き換えない。** 補正は timefix レコード（オフセット）として追記し、UTC への変換はスマホ側で行う。append-only のログを書き換えないので NAND の in-place 更新が要らず、電源断の方針（[storage.md](./storage.md)）と矛盾しない。補正は元に戻せる。
- **レコードの順序は時刻ではなく、連番（sequence）と boot カウンタで決める。** 時計が後ろへ戻っても順序は壊れない。
- 理由: Standard はスマホを常に持つので、遡って付ける方式が成り立ちやすい。RTC が守れるのは MCU だけがリセットされたとき（rail が生きている間）であり、rail が消えれば RTC も時刻を失う（§5.3）。

### §5.2 動作

レコードの項目（`boot`、`mono_ms`、`rtc_ms`、`flags.RTC_INVALID`）と timefix の形式は [formats.md](./formats.md)。`rtc_ms` は「時計の値」で、RTC を載せない構成では GRTC とスマホ同期から作る。

| 状況 | 動作 |
|---|---|
| 起動直後（この起動で時計が未設定） | レコードは `RTC_INVALID`、`rtc_ms = 0`。`boot` と `mono_ms` は常に入れる |
| BLE の TIME 操作 | 時計を phone UTC に合わせ、timefix レコード（`rtc_before` = 補正前の値、無効なら 0、`rtc_after` = phone UTC）を追記。以降は GRTC から時刻を数える |
| 同一起動内の遡り | スマホが、timefix の `mono_ms` と phone UTC の対応から、同じ `boot` のレコードの `mono_ms` を UTC に直す |
| 起動をまたぐ区間 | 時刻は失われている。スマホが連番と `boot`・`mono_ms` から区間を推定する（失った絶対時刻を復元できたとはみなさない） |
| 電源投入直後のペアリング前 | 録音しない（[security.md §6](./security.md)）。最初の TIME が入ってから録音を始める |

- Phase 0 の実装は `fw/src/clock.c`（uptime ベース。TIME で合わせるまでは `RTC_INVALID`）。
- 時刻誤差の感覚（LFXO は温度補償なしで ±20 ppm を仮置き。BC15M に内蔵の 32.768 kHz 水晶の仕様が 20 ppm）:

```text
 1 ppm   ≈ 0.0864 s/日 ≈  2.6 s/30 日
 2.5 ppm ≈ 0.216  s/日 ≈  6.5 s/30 日   （RV-3032-C7 の -40〜85 ℃ 規格）
20 ppm   ≈ 1.73   s/日 ≈ 51.8 s/30 日   （GRTC + LFXO の仮置き）
```

BLE 接続のたびに phone UTC で再同期するので、同期間隔が短いほど誤差は小さい。

- GRTC に使う 32.768 kHz の水晶（20 ppm）は BC15M に内蔵で、外付けは無い（XL1 / XL2 はパッドに出ない。P1.00 / P1.01 は内蔵の LFXO が使う）。[board.md](./board.md)。
- BC15M の clock と電源の設定（Fanstel のデータシートと注意書き。確定）:
  - 内蔵の負荷容量を、HFXO 17 pF、LFXO 7 pF にする（devicetree の `&hfxo` / `&lfxo` で `load-capacitors = "internal"`）。
  - MPSL の HFCLK の latency を 1400 にする（`mpsl_clock_hfclk_latency_set(1400)`。新しい NCS では `CONFIG_MPSL_HFCLK_LATENCY`）。戻さないと接続が切れることがある。
  - DC/DC は FW で有効にする（インダクタは内蔵）。

### §5.3 外付け RTC（RV-3032-C7、footprint のみ）

載せる場合の仕様。

| 項目 | 値 |
|---|---|
| 寸法 | 3.2 × 1.5 × 0.8 mm、I²C、内蔵 32.768 kHz 水晶 |
| 消費 | 160 nA @ 3 V、25 ℃（約 0.48 µW。1.8 V では約 0.29 µW） |
| 精度 | ±1.5 ppm（0〜50 ℃）、±2.5 ppm（-40〜85 ℃）、温度補償済み |
| timekeeping 電圧 | 1.3〜5.5 V |
| その他 | 温度センサ内蔵、16 B RAM / 32 B EEPROM、event timestamp、自動 backup 切替 |
| 公式 | https://www.microcrystal.com/en/products/real-time-clock-rtc-modules/rv-3032-c7 、Application Manual Rev. 1.3: https://www.microcrystal.com/fileadmin/Media/Products/RTC/App.Manual/RV-3032-C7_App-Manual.pdf |

載せる場合の接続と設定（確定。footprint を置くときにこの接続にする）:
- VDD は常時給電の system rail だけから供給する。マイクや NAND のゲートと一緒に切らない。
- **内蔵の backup switchover は使わない。** `BSM = 00`（EEPROM C0h、出荷時の既定）、`TCM = 00`（トリクル充電オフ）。未使用の VBACKUP は浮かせず、**10 kΩ を介して VSS に接続**する。電池への RTC 専用配線は設けない。
- 理由: LSM の切替しきい値は typ. 2.0 V（1.8〜2.2 V）で、1.8 V の VDD と約 3 V の VBACKUP の組み合わせでは backup 側へ移る。**VBACKUP Power state では I²C が無効**になる。
- SDA / SCL のプルアップは同じ system rail に接続する。**1.8 V では I²C を 100 kHz 以下**にする（100 kHz は VDD ≥ 1.4 V、400 kHz は VDD ≥ 2.0 V）。RTC の近くにデカップリングを置く。
- INT は未接続。alarm / タイマ / イベント入力は使わない。
- 配置: BC15M 基板は 1.8 V の rail で直接動かすので、レベル変換なしで 1.8 V 側に置く。XIAO 開発基板では 3V3 側に置いてよい（[board.md](./board.md)）。
- 副次利用: 内蔵の温度センサで筐体内温度のログや RTC 補正の診断ができる。体温計としては使わない。

**rail 消失時は時刻保持を保証しない**（電池が残っていても、regulator 停止・rail 消失では失う）。電源復帰時に PORF / VLF を確認し、時刻が信頼できなければ `RTC_INVALID` で記録し、次の BLE 同期で復旧する。

### §5.4 RTC を載せるかの判断基準（仮置き、実測で決める）

GRTC で足りるなら載せない。次のいずれかが成り立つなら載せる。

| # | 基準 | 測るもの |
|---|---|---|
| 1 | **ドリフト。** 内蔵水晶（20 ppm）の GRTC が、最長の無同期区間（Standard はスマホを常に持つので短い。Premium は 48 h のオフライン）で、要求する時刻精度を超えてずれる | 30 日の同期ログから GRTC のドリフト（ppm）を測り、RTC の ±2.5 ppm と比べる。要求精度は [product.md](./product.md) の UX から決める（未定） |
| 2 | **起動の頻度。** 同期の間に MCU のリセット（WDT・ソフトリセット・BOR で rail は生きている）が頻繁に起き、起動をまたぐ区間の時刻の推定が許容を超える | dogfood の event レコード（§4.2）で、同期間隔あたりのリセット回数を数える |
| 3 | **rail 消失。** RTC を載せても rail 消失では時刻を失う（§5.3）。電池の交換や深い放電が多いなら RTC の効果は小さい | dogfood で rail 消失の回数を数える |
| 4 | **コスト。** 消費は約 0.3 µW で無視できる（0.5 mW の平均電力を想定した試算で約 0.1 %）。判断は面積、BOM、I²C の 2 ピン、XIAO 開発基板での 3V3 側の配置のみ | — |

- 基準 1 と 2 が主で、3 は RTC を載せる側の効果を打ち消す。4 は判断を変えない。
- 判断の時期: Phase 1 の実測のあと。載せる場合のドライバは `fw/lib/rtc`（§5.5）が済んでいる。

### §5.5 RV-3032 ドライバ（`fw/lib/rtc`）

実装済み・ホスト試験済み（`make -C fw/lib rtc`）。製品のビルドに入れるのは RTC を載せる場合のみ。

- **自前の最小ドライバ**で、Zephyr の `rtc_rv3032` は使わない。理由: Zephyr の mfd 初期化は PORF / VLF を読んで直ちに 0 を書いてクリアする。この 2 つが「時刻を失った」という唯一の証拠なので、スマホが時刻を合わせるまで消してはいけない。
- RV-3032 に UNIX 時間のレジスタは無い。時刻は 00h〜07h の BCD カレンダー（100 分の 1 秒、秒、分、時、曜日、日、月、年。2000〜2099 年）。UNIX ms への変換が要る。
- レジスタ: I²C アドレス 7 ビット 0x51。Status（0Dh）bit1 **PORF**（電源投入でセット。ユーザが 0 を書くまで残る）、bit0 **VLF**（内部電圧が VLOW（typ. 1.2 V）を下回った。同様に残る）。EEPROM PMU（C0h）bit5:4 **BSM**、bit1:0 **TCM**。
- 時刻は **1 回のバースト（00h〜07h）で読む**。100 分の 1 秒が 00 か 99 のときは、2 回続けて同じ値になるまで読み直す（Application Manual 4.5）。書き込みも Seconds〜Year を 1 回で行う（分けるとカレンダーが壊れる）。Seconds に書くと 100 分の 1 秒が 00 に戻り、アクセス時間が 950 ms 未満のあいだ時刻カウンタは止まる。
- API（`rv3032.h`）: `rv_config_ok()` は C0h を読み、BSM = 00 または 11、TCM = 00 でなければ `-EPERM`（EEPROM の書き換えは実装しない。起動時に 1 回）。`rv_status()` / `rv_read_ms()` / `rv_set_ms()`（時刻を書いた**あとで** PORF と VLF をクリアする。2000 年より前・2100 年以降は `-ERANGE`）。`rv_civil_to_unix()` / `rv_unix_to_civil()`（UTC、Hinnant のアルゴリズム）。
- `tk_observe()`: RTC が期待どおり進んだかを判定する。期待 = 前回の RTC + 単調カウンタの経過。許容 = 250 ms + 経過の 200 ppm。結果は `TK_OK` / `TK_JUMP_FWD` / `TK_JUMP_BACK` / `TK_INVALID`（PORF / VLF）。`TK_INVALID` のあとの最初の読み出しは基準が無いので `TK_OK`。`TK_JUMP_*` では timefix レコードを追記する。
- 起動時のフロー: `rv_config_ok` → `rv_status`（PORF / VLF を記録してからクリアしない）→ `rv_read_ms` → `tk_observe`。
- Zephyr の I²C への接続（`rv_bus` を `i2c_burst_read` / `write` につなぎ、`i2c-bitrate` を 100000 に）は未実装。

---

## §6 FW libraries map

`fw/lib/*` の各ディレクトリ（純 C、ホスト試験あり。`make -C fw/lib test`）と、それを定義する仕様。コードのコメントはここの仕様ファイルと節を指す。

| ディレクトリ | 役割 | 定義する仕様 |
|---|---|---|
| `fw/lib/aad` | T5848 の AAD（音で起こす）制御 | [audio.md](./audio.md) |
| `fw/lib/t5838` | THSEL の 1 線設定と CLK モード表（T5848 も同じレジスタ・手順。名前は T5838 のまま） | [audio.md](./audio.md) |
| `fw/lib/ima_adpcm` | IMA-ADPCM エンコーダ / デコーダ（4 bit、8 kHz） | [audio.md](./audio.md) |
| `fw/lib/chunk` | レコード形式（header / payload / CRC / commit）、timefix、gap、event | [formats.md](./formats.md) |
| `fw/lib/sync` | ペンダント側の同期送信（フラグメント、累積 ACK、再送、欠落通知） | [formats.md](./formats.md) |
| `fw/lib/ring` | 耐電断の ring（ストレージ backend 抽象化） | [storage.md](./storage.md) |
| `fw/lib/bitrate` | バックログ連動のビットレート制御（32 → 16 kbps） | [storage.md](./storage.md) |
| `fw/lib/powerstate` | Standard の電池電圧の状態機械（NORMAL / LOW / CRITICAL / TERMINAL） | [power.md §4.2](./power.md) |
| `fw/lib/health` | ハートビート監督（WDT の餌やり） | この文書 §4 |
| `fw/lib/rtc` | RV-3032-C7 ドライバと時刻の飛び検出 | この文書 §5 |

`fw/src` と周辺:

| パス | 定義する仕様 |
|---|---|
| `fw/src/ota.c`、`fw/sysbuild/`、`fw/boards/pendant_partitions.dtsi`、`tools/ota.py` | この文書 §2・§3 |
| `fw/src/supervisor.c`、`fw/boards/*.overlay` の `wdt31` | この文書 §4 |
| `fw/src/clock.c` | この文書 §5 |
| `fw/src/ble.c`、`fw/src/ble.h`、`tools/pendant_rx.py` | [formats.md](./formats.md)（BLE の暗号化要求とプライバシーは [security.md §3・§4](./security.md)） |
| `fw/src/crypto_bench.c` | [security.md §5](./security.md) |
| `fw/src/store.c`、`tools/ring_dump.py` | [storage.md](./storage.md) |
| `fw/src/audio.c` | [audio.md](./audio.md) |
| `fw/src/marker.c`、`fw/src/scenario.c`、`fw/measure.conf` | [plan/prototype.md](../plan/prototype.md)（測定用ビルド）、ピンは [board.md](./board.md) |

---

## 未確定

| 項目 | 状態 |
|---|---|
| RTC を載せるか（§5.4 の基準 1〜3 の実測。GRTC のドリフト、同期間隔あたりのリセット回数、rail 消失の頻度） | 新規 issue 候補: 時刻保持を GRTC で足りるか実測する（RV-3032 を載せるかの判断） |
| 要求する時刻精度（無同期の最長区間で何秒まで許すか） | 新規 issue 候補: 時刻精度の要求値を [product.md](./product.md) に決める |
| 実機の電源断での swap 試験（USB を手で抜く。今の XIAO で行う） | #11 |
| OTA 低残量ゲートの本実装（`fw/lib/powerstate` の出力に接続）。Standard の電圧しきい値は実測待ち | #12 |
| 不正署名、バージョンの巻き戻し、test イメージ + watchdog の試験 | #11 |
| MCUboot の復旧モード（起動時にボタンを押したまま、または serial recovery）を入れるか。未検証 | 新規 issue 候補: MCUboot の復旧モード |
| POF（1.7 V）で最後の警告を受けるか | #11（実測のあと） |
| NAND 版での `ring` 領域（784 KB）の扱い。NAND が録音先になるため、領域の再配分を検討する | 新規 issue 候補: NAND 版のパーティション再配分 |
| アプリが 320 KB のスロットに収まり続けるか（現在 177 KB、55 %） | #11 |
| 実機での WDT・リセット原因の試験のうち未実施の項目（§4.6） | #11 |
| RTC を載せる場合の Zephyr I²C 接続と 1.8 V / 100 kHz での動作確認、30 日ドリフトの実測、rail 消失後の挙動 | 新規 issue 候補: 時刻保持を GRTC で足りるか実測する（上と同じ） |
