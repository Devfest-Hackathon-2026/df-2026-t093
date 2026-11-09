# chunk 形式・時刻補正・ACK・BLE 同期プロトコル

**SPEC v1。§1〜§8 が chunk（レコード）形式のバイトレベルの正本で、`fw/lib/chunk/`（純 C、ホストテスト付き）の実装と一致する。** §9〜§13 が BLE 同期プロトコル（GATT、送信側の手順、受信側の手順、試験）。実装は `fw/lib/sync/`（送信側、純 C）、`fw/src/ble.c` / `fw/src/ble.h`（GATT の接着層）、`tools/pendant_rx.py`（受信ツール）。

確度の凡例: **確定** = 決定済みで実装と一致 / **方針（数値未定）** / **仮置き（実測で決める）**。特に書かない項目は確定。

録音ログの 1 レコードの形式、時刻補正レコード、欠落区間、BLE のフレーミングと ACK を 1 つに決める。Standard と Premium で形式は分けない（[product.md](./product.md)）。要求と運用の説明は [product.md](./product.md)・[storage.md](./storage.md) にあり、バイト配置はここだけに書く。

---

## 1. 方針

- ログは **レコードの連なり**。種別は音声（audio）・時刻補正（timefix）・欠落（gap）の 3 つで、同じ容器（header + payload + trailer）に入れる。
- **連番（sequence）は全種別で 1 本**。1 から始まり、レコードごとに 1 ずつ増える。順序は連番だけで決める（[firmware.md](./firmware.md) の時刻管理）。
- 書き込み順は **header → payload → CRC → commit**。commit は最後に書く（[storage.md](./storage.md)）。commit が無い、または CRC が合わないレコードは、起動時にそこで打ち切る。
- 記録済みの時刻は書き換えない。補正は timefix レコードとして追記する（[firmware.md](./firmware.md)）。
- ビット順は全フィールド **リトルエンディアン**。構造体のキャストはせず、1 バイトずつ読み書きする。

## 2. レコードの配置

```text
offset  size  field
     0     4  magic        "LOSC"（4C 4F 53 43）
     4     1  version      1
     5     1  type         1=audio  2=timefix  3=gap  4=event
     6     1  codec        0=なし  1=IMA-ADPCM 4bit / 8 kHz / mono  2〜=予約（縮退側。audio.md §6 で割り当て）
     7     1  flags        bit0 = RTC_INVALID（rtc_ms は信用できない。0 を入れる）
     8     4  sequence     1 始まり。全種別で通し番号
    12     4  boot         起動カウンタ（電源投入・リセットごとに +1、不揮発）
    16     4  mono_ms      この起動内の単調増加カウンタ（ms）。レコード開始時点
    20     8  rtc_ms       UTC の unix ms。レコード開始時点
    28     4  payload_len  payload のバイト数
    32   len  payload
 32+len     4  crc32       header と payload の全体（先頭から 32+len バイト）
 36+len     4  commit      "CMT!"（43 4D 54 21）
```

- 合計 `32 + len + 8` バイト。ring が先頭ページ境界に置き、**末尾は 0xFF のまま** にしてよい（消去状態）。
- **CRC-32/ISO-HDLC**（zlib と同じ多項式 0xEDB88320、初期値・最終 XOR とも 0xFFFFFFFF、`"123456789"` → `0xCBF43926`）。16 エントリのテーブル式。ホストと実機の両方で zlib と一致を確認済み。
- `rtc_ms` は RTC チップ（RV-3032）の unix 秒 × 1000 + 100 分の 1 秒 × 10。ms の精度は主張しない。
- **ADPCM の predictor と step index は record ごとに 0 で初期化する**（[audio.md §5.1](./audio.md)）。chunk を途中の chunk から読んでも復号が壊れない。header に初期状態は持たせない。

### 2.1 commit と電源断

- 消去状態（0xFF）が見える場所を commit の未書き込みとして扱う。**commit の最終バイトは 0xFF にならない**（`!` = 0x21）ので、書きかけと区別できる。
- 書き込み順は header → payload → CRC → commit。ホストテストで、**バイト単位のあらゆる位置で電源が切れても、完全に書けるまでは有効と判定されない**ことを確認している。
- 1 ページを 2 回 program できないデバイス（NAND）では、CRC と commit を payload 末尾と同じ最終ページに入れて 1 回で program する。ページが途中で壊れても CRC が合わず、有効にならない。2 回 program できるデバイス（Phase 0 の内蔵 RRAM など）では、CRC のあとに commit を別 program で書く。

### 2.2 decode の結果

| 結果 | 条件 | ring の動作 |
|---|---|---|
| OK | magic・version・長さ・CRC・commit がすべて正しい | 取り込む |
| EMPTY | 先頭 4 バイトが 0xFF | ログの終端 |
| INCOMPLETE | header は正しく、commit が未書き込み | 捨てる（直前の未 commit chunk） |
| BAD | magic・version・長さ・CRC・commit のどれかが不正 | 捨てる |

起動時は、先頭（tail）から順に decode して、最初に OK でなかった場所をログの末尾にする。

## 3. 音声レコード（type 1）

- payload は ADPCM のバイト列。1 バイトに 2 サンプルを入れ、**先のサンプルが下位ニブル**（`ima_adpcm.h`）。
- 8 kHz・4 bit なので 4000 B/s = 32 kbps。`payload_len` のサンプル数は `2 × payload_len`（奇数サンプルなら最後のニブルは無視してよいが、v1 は偶数サンプル数だけ使う）。
- **chunk 長（時間）は形式に固定しない。** `payload_len` で自由に決められる。方針は次のとおり。
  - 基準は **約 8 秒**（32000 B。header と trailer の割合は 0.125 %）。
  - デバイスのページに合わせるなら、**レコード全体がページの整数倍になる長さ**にする（4 KiB ページ × 8 = 32768 B なら payload は 32728 B = 8.18 秒）。端数のパディングを減らせる。ring の容量計算（[storage.md](./storage.md)）はこの長さを前提にする。
  - RAM に溜めるバッファは payload 1 個ぶん（32 KB は XIAO の RAM 256 KB の約 13 %）。
- 縮退側の codec（16 kbps 級）は `codec` の値を変えるだけで同じ容器に入る。値は [audio.md §6](./audio.md) の判断後に割り当てる。

## 4. 時刻補正レコード（type 2）

スマホ同期で RTC を補正したとき、または RTC が無効だったときに追記する（[firmware.md](./firmware.md)）。

```text
header: type=2, codec=0, sequence/boot/mono_ms は通常どおり
        rtc_ms = rtc_after_ms, flags は補正時点の RTC_INVALID
payload (16 B):  rtc_before_ms (8)   補正前の RTC の値
                 rtc_after_ms  (8)   補正後の値（phone の UTC）
```

- スマホ側は、隣り合う timefix レコードの間の `rtc_ms` を直線補間して UTC に直す（±2.5 ppm なら補正量は 1 日あたり最大約 0.22 s）。
- RTC が時刻を失った区間は `flags.RTC_INVALID` を立て `rtc_ms = 0` にする。その区間の時刻は、連番と `boot`・`mono_ms` から次の timefix で推定する。

## 4.1 イベントレコード（type 4）

押したボタンや、AAD の起床・睡眠、privacy の検出、リセットの要因の時刻を残す（[audio.md §4.6](./audio.md)）。

```text
header: type=4, codec=0
payload (8 B):  kind (1)   1=button  2=AAD wake  3=AAD sleep  4=reset  5=privacy ON  6=audio block drop (arg = 100 ms blocks)
                arg  (1)   kind=4: リセット要因（1=POR 2=PIN 3=WATCHDOG 4=BROWNOUT 5=SOFT 6=OTHER）、kind=6: 100 ms block 数（1..255、256 以上は複数 event）、それ以外 0
                pad  (2)   0
                mono_ms (4)  この起動内の単調カウンタ。出来事の時刻
```

ASR の前処理で、ボタンを押したクリックの区間を雑音として扱える（[audio.md §3.5](./audio.md)）。連番を消費する通常のレコード。

## 5. 欠落レコード（type 3）

ring が満杯で、**未 ACK の古いレコードを上書きして捨てた範囲**を表す。

```text
header: type=3, codec=0, sequence = first
payload (12 B):  first (4)   捨てた最初の sequence
                 last  (4)   捨てた最後の sequence（first 以上）
                 reason (1)  1=overwritten
                 pad (3)     0
```

- 捨てた sequence はログから消えるので、**gap レコードは永続化せず、同期の開始時にリング側が合成して送る**。リングのメタデータが持つ `acked`（受信側が確認済み）と `tail`（残っている最古の sequence）から、`tail > acked + 1` なら `first = acked + 1`、`last = tail - 1` の gap を作る。
- CR2032 時に電力のために録音しなかった区間（Premium）は sequence を消費しないので、別の表現が要る。**v1 では未定義**（Premium の CR2032 運用を決める [power.md](./power.md)・[storage.md](./storage.md) の後で追加する）。

## 6. BLE のフレーミングと ACK

ここでは形式を決める。同期の手順は §9〜§11。

- **送信側（ペンダント → スマホ）**: レコード全体（header から commit まで）を、GATT notification に断片で載せる。1 断片は `seq (4) + offset (4) + データ`。MTU 247 なら 1 断片のデータは 236 B。スマホは `offset` が連続していることを確認して組み立て、**CRC と commit が合った後でだけ** そのレコードを受理する。
- **欠落通知**: 同期の開始時に、必要なら合成した gap レコード（§5）を先に送る。
- **ACK（スマホ → ペンダント）**: `0x01 + upto_seq (4)` の 5 B。**累積 ACK** で、「sequence が `upto_seq` 以下のレコードはすべて、順序どおりに、CRC と commit の検証を終えて受け取った」という意味。
  - 順序どおりに受け取れたレコードまでしか進めない。先の sequence が先に届いても ACK は進めない（`CHUNK_RX_AHEAD`）。
  - 重複して届いたレコードは受理しないが、ACK は現在値を返す（`CHUNK_RX_DUP`）。
  - gap レコードを受け取ると、`last + 1` まで進む。すでに受け取った範囲と重なる gap も、終端まで進めるだけ。
  - ペンダント側は ACK を **単調増加（最大値）** として扱う。`upto_seq` が自分が書いた最新の sequence を超えていたら無視する。ACK が古くても重複していても害はない。
- **回収**: ペンダントは **ACK を受けた範囲だけ** を回収する。新しいメタデータを commit してから消去する（[storage.md](./storage.md)）。ACK を返す前に回収してはならない。

### 6.1 制御（`fw/src/ble.h`）

GATT service は DATA（notify、ペンダント → スマホ）、CTRL（write、スマホ → ペンダント）、INFO（read、32 B: version、録音中、boot、tail、next、acked、uptime、RTC）の 3 つ。

| CTRL op | 内容 |
|---|---|
| `0x01 ACK upto(4)` | 累積 ACK（§6）。何も解放せず、切断時または ring が満杯のとき解放 |
| `0x02 SYNC have(4)` | スマホが `have` まで持っている。`have` 以降を送る。ring が `have+1` より先から始まっていれば、gap（`have+1 .. tail-1`）を先に送る |
| `0x03 TIME unix_ms(8)` | 時計を合わせ、timefix レコード（§4）を追記 |
| `0x04 RESEND seq(4) off(4)` | レコード `seq` を `off` から送り直す。複数を同時に受けられる（8 件）。**ring から消えていたら、そのレコードの代わりに gap を返す**（待ち続けないため） |
| `0x05 STOP` | 中止 |

- DATA は断片 `seq(4) + off(4) + データ`。**`seq = 0` は終端マーカー**で、`off` = この同期で送り終えた次の連番（同期中に追記されたレコードも含めて送る）。
- スマホは、断片が途切れたら（`off` が連続しない）`RESEND(seq, 手元の長さ)` を **1 回だけ** 出し、そのレコードの後続の断片は再送が来るまで捨てる。完成したが連番が先のレコードは、順番が来るまで溜める（reorder buffer）。終端マーカーのあとも足りなければ、3 s ごとに最大 6 回、足りない最小の連番を要求し直す。
- 実機でこの経路を確かめた（§7.1）。

## 7. 実装とテスト

`fw/lib/chunk/`（`cc -std=c99 -Wall -Wextra -Werror -o /tmp/t test_chunk.c chunk.c && /tmp/t`）。

| テスト | 内容 |
|---|---|
| CRC | zlib と同じ `0xCBF43926`、継ぎ足し計算 |
| 往復 | 全フィールド、バイト配置、末尾の 0xFF パディング |
| 破損 | header・payload・CRC の全ビット反転（1 ビットずつ）が OK にならない。commit の 1 ビット反転は BAD |
| 電源断 | レコードの全バイト位置で打ち切り（残りは 0xFF）。完全に書けるまで OK にならない |
| commit 未書き込み | INCOMPLETE。全部 0xFF は EMPTY。短い読み出しと異常な長さは BAD |
| timefix / gap | encode と decode が一致 |
| 受信側 | 順序入れ替え、重複、gap による飛び越し、重なる gap |

実機（XIAO nRF54L15、128 MHz、-Os）:

- CRC-32: **17.0 cycle/byte**。8 秒 chunk（32000 B）で 544k cycle（約 4.3 ms、約 33 µJ）。書き込み時の計算と読み出し・検証時の計算で 2 回かかる前提でも平均で約 8 µW で、500 µW 級の予算に対して無視できる。
- `chunk_decode`（CRC を含む）: 544,871 cycle。結果は OK で、CRC はホストの zlib と一致。

### 7.1 実機の BLE 同期（XIAO ↔ Mac、bleak。仮置き: Mac 相手の値で、実スマホでは違う可能性がある）

| 項目 | 結果 |
|---|---|
| 通信 | 2M PHY、データ長 251、MTU 247、接続間隔 15 ms（接続の数秒後に Mac が適用） |
| **スループット** | **約 53〜56 kB/s**（payload のみ。接続直後の 30 ms 間隔の間は約 20 kB/s）。ペンダント側の送信バッファは詰まっていない（再試行 0）ので、律速はリンク（Mac 側）。想定（約 175 kB/s）には届かない。エネルギー見積りへの影響は [power.md](./power.md) |
| 同期 | 受信側が連番・CRC・commit を確かめて ACK し、2 回目の同期は `have` から新しい分だけ。重複なし |
| 欠落 | ring が満杯のとき、捨てた範囲が gap として報告され、WAV に残る |
| 断片の損失 1 %（受信側で捨てて試験） | 全レコードを回復。同期の最中に追記された分も含めて完走 |
| 音 | 音を鳴らして同期すると、その位置の rms が周囲の約 7 倍。PDM → ADPCM → ring → BLE → WAV の全経路が通る |

## 8. 未確定（chunk 形式）

- 縮退側 codec の値と、それが 2〜3 bit で chunk 内のサンプル数をどう表すか（[audio.md §6](./audio.md)）。
- CR2032 時の「録音しなかった区間」の表現（Premium）。

---

## 9. GATT service

Phase 0 の実装（`fw/src/ble.c`）の値。UUID は 128 bit で、下位 12 バイトが共通（`…-2b6b-4d0c-9f6e-6c6966654f53`）、先頭 4 バイトが `a3c1f000 + n`。

| n | UUID | 特性 | 方向・属性 |
|---|---|---|---|
| 0 | `a3c1f000-2b6b-4d0c-9f6e-6c6966654f53` | service | |
| 1 | `a3c1f001-…` | DATA | ペンダント → スマホ。notify（CCC を有効にしないと送らない） |
| 2 | `a3c1f002-…` | CTRL | スマホ → ペンダント。write / write without response |
| 3 | `a3c1f003-…` | INFO | read |

- CTRL は §6.1 の op。長さの規定は ACK・SYNC が 5 B、TIME・RESEND が 9 B、STOP が 1 B。長さが違うと ATT の Invalid Attribute Length、未知の op は Value Not Allowed を返す。コマンド待ち行列（16 件）が満杯のときは Insufficient Resources を返す（write without response ではスマホに見えない）。
- INFO（32 B、リトルエンディアン）:

| offset | size | 内容 |
|---:|---:|---|
| 0 | 1 | INFO の version（1） |
| 1 | 1 | 録音中なら 1 |
| 2 | 2 | 0 |
| 4 | 4 | boot（起動カウンタ） |
| 8 | 4 | tail_seq（リングに残っている最古の sequence） |
| 12 | 4 | next_seq（次に書く sequence） |
| 16 | 4 | acked（ペンダントが記録している ACK 済みの最大 sequence） |
| 20 | 4 | uptime（秒） |
| 24 | 8 | rtc_ms（時計が未設定の間は 0） |

- 暗号化（LESC + OOB、保存データの暗号化）、広告に機器名を載せないこと（bonded のスマホが IRK で見つける）は [security.md](./security.md) が持つ。Phase 0 の開発ビルドは広告に service UUID、scan response に機器名（`LifeOS-Pendant`）を載せ、属性は `CONFIG_PENDANT_BLE_SECURE` で暗号化に切り替える（仮置き）。

### 9.1 接続パラメータ

- 接続したらペンダントから 2M PHY、最大データ長（251 B）、接続間隔 7.5〜15 ms（latency 0、supervision timeout 4 s）を要求する。MTU は 247（L2CAP TX MTU）。
- 1 断片のデータ長 = `min(MTU − 3 − 8, 244)`。MTU 247 で 236 B。
- スマホ側が間隔を変えるのは OS 任せで、Mac は接続の数秒後に 15 ms を適用した（§7.1）。iOS / Android の制約は未確認（§未確定）。

## 10. 送信側の手順（`fw/lib/sync/`）

リンク・コマンド待ち行列・ring のロックはコールバック（`tx` / `poll` / `other` / `lock` / `unlock` / `mono_ms` / `rtc_ms`）で、同じコードが Zephyr とホスト試験の両方で動く。`fw/src/ble.c` は GATT・接続・待ち行列・コールバックだけを持つ。

1. **SYNC(have)** を受けたら `sync_run(have)` を始める。`have` は ACK と同じに扱い、ring の `acked` を `have` まで進める。
2. `tail > have + 1` なら、gap（`first = have + 1`、`last = tail − 1`。record の `sequence` = `first`）を先に送る。
3. `seq = max(tail, have + 1)` から `next_seq − 1` まで、1 レコードずつ送る（§6 の断片）。送っている間に追記されたレコードも、この同期に含める。
4. レコードを 1 つ送るごとに、コマンド待ち行列を処理する。
   - **ACK**: ring の `acked` を進める（§6）。
   - **TIME**: 送信側は処理せず、`other` コールバック（時計の更新と timefix の追記）に渡す。
   - **RESEND(seq, off)**: 未処理の再送要求として最大 8 件まで覚える。同じ `seq` の要求は小さい方の `off` にまとめる。8 件を超える要求は捨てる（スマホが再度要求する）。要求は次のレコードの前に、覚えた順に処理する。
   - **SYNC(have)**: 進行中の送信を捨てて、新しい `have` で 1 から（手順 1）やり直す。
   - **STOP**: 送信をやめる。終端マーカーは送らない。
5. 送ろうとしたレコードが、送信中に上書きされて ring から消えていたら、`gap(seq, tail − 1)` を送って先へ進む。RESEND の対象が ring から消えていた場合も、`tail > seq` なら `gap(seq, tail − 1)` を返す（スマホが待ち続けないため）。
6. 全部送り終えたら、**終端マーカー**（`seq = 0`、`off = その時点の next_seq`）を送る。
7. 切断すると STOP と「回収」が待ち行列に入る。回収は ACK 済みの範囲の消去（[storage.md](./storage.md)）で、ACK を返す前には行わない。ring が満杯のときも回収する。

送信の失敗（リンクが無い、notify の送信バッファが詰まって再試行の上限に達した）は、その同期を終了する。BLE のタスクは watchdog の heartbeat に入る（[firmware.md](./firmware.md)）。

## 11. 受信側（スマホ）の手順

`tools/pendant_rx.py` が参照実装。

1. 接続して INFO を読み、DATA の notify を有効にする。必要なら TIME を送る。
2. `SYNC(手元で受理済みの最大 sequence)` を送る。
3. 断片を `seq` ごとに組み立てる。`offset` が手元の長さに一致しなければ穴（または重複）。穴なら `RESEND(seq, 手元の長さ)` を **1 回だけ** 出し、そのレコードの後続の断片は、再送の断片（`off` が要求した値）か、新しい開始（`off = 0`。gap で置き換えられた場合）が来るまで捨てる。
4. レコードが完成したら、magic・version・長さ・CRC・commit を検証する（`chunk_decode` 相当）。通らなければ `RESEND(seq, 0)`。通ったレコードだけを受理する。
5. 受理したレコードを連番順に取り込む。先の sequence が先に完成したら、順番が来るまで溜める（reorder buffer）。gap レコードは、取り込むと `last + 1` まで飛ぶ。
6. 取り込みが進むたびに、累積 ACK（`0x01 + upto_seq`）を送る。重複は受理せず、ACK は現在値のままにする。
7. 終端マーカー（`seq = 0`、`off = next`）を受けたら、`next` 未満の連番が全部取り込めたかを見る。足りなければ、3 s ごとに最大 6 回、足りない最小の連番を `RESEND` で要求し直す。それでも足りなければ打ち切る（受信ツールは、データが 20 s 来ない場合も打ち切る）。
8. 全部取り込めたら、最後の ACK を送って切断する。受理済みの最大 sequence は、次の同期の `have` として手元に保存する。

再送の粒度はレコード（約 8 KB）単位で、約 11 % の断片を落とす条件では完走しない（再送した断片がまた損なわれる確率が高い）。フラグメント単位で再送を要求する設計にすると、極端な損失でも完走しやすくなる（スマホアプリの設計で決める。§未確定）。

## 12. 試験

純 C の送信側と、ring と、電話側のモデル（`tools/pendant_rx.py` の受信側の移植）を、欠陥のあるリンクにつないで試験する（`fw/lib/sync/test_sync.c`。`make -C fw/lib sync`、300 シードで約 8 秒、`quick` は 20 シード）。

電話側のモデル: フラグメントの再組み立て、`chunk_decode` による CRC 検証、順序どおりの累積 ACK、穴で RESEND、待っている再送の扱い、終端マーカーの後の再要求（最大 20 回）、先に届いた記録の保持。

| シナリオ | リンクの欠陥（フラグメントあたり） | その他 |
|---|---|---|
| clean | なし | 記録しながら同期 |
| lossy | 損失 3 %、重複 2 %、破損 2 %、並べ替え 2 % | 古い RESEND の再送 2 % |
| lossy+disconnects | 同上 + STOP 0.2 % | 接続が途中で切れる（300 パケット以内）、再接続 |
| corrupt-heavy | 破損 15 % | 切断あり |
| overwrite | 損失 2 % ほか + STOP | **ring が小さく、記録が電話を追い越す**（上書き） |

確認する不変条件:

1. **全シーケンス番号が、記録または gap のどちらか 1 回だけで受信側に反映される**（重複して反映されない。上書きなしなら gap は出ない）。
2. **受信側が受理した記録は CRC が通り、中身が記録した値と一致する**（破損した記録を ACK しない）。ACK の値は常に受理済みの範囲内。
3. 7 接続ごとにペンダントを再起動（`ring_open`）しても、`next_seq` と `tail_seq` が変わらない。終了後、ring を開き直して整合する。
4. すべてのシナリオで終了する（デッドロックしない）。

実機（XIAO ↔ Mac）では、損失なしと 50 フラグメントに 1 つ落とす条件で完走することを確認している（結果の数値は GitHub issue #7）。

## 13. 実装との対応

| 場所 | 内容 | 節 |
|---|---|---|
| `fw/lib/chunk/` | レコードの encode / decode、CRC-32、timefix・event・gap のペイロード、受信側の累積 ACK（`chunk_rx`） | §1〜§6 |
| `fw/lib/sync/` | 送信側の状態機械（`sync_run`、`sync_drain`、`sync_resend`）と、ロス注入の試験 | §6.1, §10, §12 |
| `fw/src/ble.h`, `fw/src/ble.c` | GATT service、CTRL の解釈、INFO、接続パラメータ、接着層 | §6.1, §9 |
| `tools/pendant_rx.py` | 受信ツール（開発用、スマホアプリではない）。再組み立て・検証・ACK・WAV 出力 | §11 |
| `tools/ring_dump.py` | ring のダンプ。レイアウトは [storage.md](./storage.md) | §2 |

## 未確定

| 項目 | issue |
|---|---|
| 縮退側 codec の `codec` 値と、2〜3 bit での payload のサンプル数の表し方（§8） | #19 |
| CR2032 時に電力のために録音しなかった区間の表現（§5、§8。Premium） | 新規 issue 候補: CR2032 時の録音しなかった区間の表現（chunk v2） |
| 実スマホ（iOS / Android）の接続パラメータ・MTU の制約。§7.1 の転送速度は Mac の値 | #7 |
| 転送エネルギー（mJ/MB）の実測（オシロ + シャント）。実測の転送速度は 53〜56 kB/s で、100 kB/s を前提にした見積りより低い | #7 |
| 2 時間分（最大 28.8 MB）の 1 回同期（55 kB/s で約 8.7 分）。ring 容量内で完了することの実機確認 | #7 |
| 再送の粒度（フラグメント単位の要求）。スマホアプリの設計で決める | #6 |
| advertise の時間と周期、CR2032 時の同期の停止・再開の条件 | #7 |
