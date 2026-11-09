# lifeOS Pendant

日常の音声を、長時間・低消費電力で記録し、lifeOS へ取り込むための小型ペンダント型ロガー（自作）。

```text
録音 → 本体の NAND に蓄積 → 約 2 時間ごとにスマートフォンへ BLE でバースト同期
```

リアルタイム性は要求しない。スマホが無い間は同期をスキップし、蓄積を続ける。スマホの不在は異常ではなく、通常の状態として扱う。録音・保存・通信・発電は互いに独立させ、1 つの故障を他へ波及させない。

状態: 試作の段階。仕様は **確定 / 方針 / 仮置き** で確度を書き分けている（各ファイルの冒頭に凡例）。電力・音声・RF の成立は未検証で、実測で決める項目は各仕様の「未確定」に issue 番号つきで並べている。

## 2 つの製品（SKU）

Bluetooth の構成は同じ。差は電源・保存・筐体だけ。

| | Standard | Premium |
|---|---|---|
| スマホ | 必携（不在中は buffer の容量まで） | 不在でも 48 h まで録音を保証 |
| 電源 | CR2477 × 1（ユーザー交換、約 1 年が目標） | Solar + 120 F HSL + 非常用 CR2032 |
| 保存 | 1.8 V 生 SPI NAND 512 Mbit〜1 Gbit | 1.8 V 生 SPI NAND 4 Gbit 基準 |
| 外形の目標 | φ40〜42 mm、厚さ 15 mm 以内 | φ40〜42 mm、厚さ 18〜22 mm 級 |

どちらも、前面が押せるボタン、電源が尽きると黒に反転する赤 / 黒の終端表示フラグ、物理の privacy latch を持つ。**2026 年 11 月中旬のハッカソンに向け、Premium の試作を共通コアと並行して先行する。Solar・外観・非常用ボタン電池を必須とし、USB 給電は設けない。** [調査と実行計画](docs/plan/premium-hackathon.md)。

上の表は製品目標。**11 月のデモ機は短時間の録音を優先し、512 Mbit NAND を設計採用する。48 h のオフライン保存はデモ機の対象外**とする。実機の保存・電源時間は未検証。

## 構成（概要）

```text
Standard  CR2477 ─ reservoir ───────────────────────────┐
Premium   Solar → BQ25505 ↔ 120 F HSL → VSTOR ─ mux ───┤→ TPS62840 → 1.8 V rail
                  CR2032 ─ 一次電池逆流保護 ──── mux ───┘
                                              │
              ┌───────────────────────────────┼──────────────┐
        nRF54L15（Fanstel BC15M）       T5848 I²S マイク    1.8 V SPI NAND
        MCU + BLE + 内蔵アンテナ        + AAD               append-only ring
```

- 録音: I²S（16 kHz）→ 8 kHz mono → IMA-ADPCM 32 kbps（AAD で音があるときだけ）。Premium 製品は空きが減ると 16 kbps 級へ縮退する方針。11 月デモは縮退なしの 32 kbps を使う。
- 保存: ファイルシステム無し。耐電断の append-only ring。通常はスマホが ACK した範囲を解放し、満杯時は最古を破棄して欠落を通知する方針。
- 同期: 短い advertise の後に、累積 ACK 付きでバースト転送。送信は +4 dBm 以下。
- 更新: 出荷前に MCUboot を書き、以降はスマホから BLE DFU。SWD は開発と救済用。
- 安全: LESC + OOB、広告に機器名を載せない、保存データを暗号化する。
- 開発用の MCU は XIAO nRF54L15。製品の基板は Fanstel BC15M を自宅でリフロー実装する。

## ドキュメント

今の仕様は `docs/spec/`。決定の経緯は `docs/decisions.md`。

| パス | 内容 |
|---|---|
| [docs/spec/product.md](docs/spec/product.md) | 目的・要求、Standard / Premium の分岐、UX、BOM、全体の目標 |
| [docs/spec/power.md](docs/spec/power.md) | 電源パス、しきい値と状態機械、brownout、エネルギー収支、OTA の電力条件 |
| [docs/spec/terminal-indicator.md](docs/spec/terminal-indicator.md) | 赤 / 黒の終端表示（電源ゼロで保持） |
| [docs/spec/storage.md](docs/spec/storage.md) | NAND の選定、ring、耐電断、バックログ連動ビットレート |
| [docs/spec/audio.md](docs/spec/audio.md) | マイク、音響、AAD、codec、縮退モード、ASR の評価方法 |
| [docs/spec/formats.md](docs/spec/formats.md) | レコード形式、時刻補正、BLE 同期、ACK |
| [docs/spec/firmware.md](docs/spec/firmware.md) | OTA・MCUboot・パーティション、watchdog、時刻管理、`fw/lib` の対応表 |
| [docs/spec/security.md](docs/spec/security.md) | 脅威モデル、ペアリング、鍵、暗号化、デバッグ保護 |
| [docs/spec/rf.md](docs/spec/rf.md) | アンテナ、整合、RF の ESD、送信電力、RF 試験 |
| [docs/spec/board.md](docs/spec/board.md) | ピン割当、test point、測定リンク、配置・ESD・EMI の規則 |
| [docs/spec/mechanical.md](docs/spec/mechanical.md) | 筐体、汗・防水、Solar ボタン、積層 |
| [docs/plan/prototype.md](docs/plan/prototype.md) | 試作の段階、基板、調達、道具 |
| [docs/plan/premium-hackathon.md](docs/plan/premium-hackathon.md) | 11 月中旬の Premium 試作: 電源回路の修正、一次資料調査、エネルギー再計算、調達と実装順 |
| [docs/plan/compliance.md](docs/plan/compliance.md) | 技適、SIG、CE、FCC、UK、問い合わせの下書き |
| [docs/decisions.md](docs/decisions.md) | 決定ログ（日付つき、撤回された判断も残す） |
| [docs/test/](docs/test/) | 試験・測定の固定仕様（INA228 の電流測定治具） |
| [docs/archive/](docs/archive/) | やめた案と、置き換えられた古いノート（今の仕様ではない） |

調査ログ・価格の調査・測定結果・未決の問いは、GitHub の issue に置く。仕様の「未確定」の表が、該当する issue を指す。

## リポジトリの構成

```text
fw/      Zephyr / NCS のファームウェア（nRF54L15、sysbuild + MCUboot）
  lib/     純 C のライブラリ（ホストで試験できる）。aad、bitrate、chunk、ima_adpcm、
           powerstate、ring、rtc、sync、t5838 など。`make -C fw/lib test`
  src/     アプリ（録音、BLE、OTA、時刻、supervisor）
  boards/  パーティションと board overlay
hw/      基板（KiCad）。Standard 基板は hw/standard/
tools/   PC 側のツール（Python / C）: 受信 `pendant_rx.py`、OTA `ota.py`、
           電力と reservoir の計算、ASR 評価、BQ25505 の抵抗計算
docs/    仕様・計画・決定ログ・試験・archive
```

コードのコメントは `docs/spec/<ファイル> §n` の形で、根拠の節を指す。仕様を変えるときは、その節と `docs/decisions.md` を一緒に直す。
