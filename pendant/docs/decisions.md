# 決定ログ

lifeOS Pendant の設計判断を、古い順に 1 行ずつ並べる。**今の仕様は docs/spec/ にある。** ここは「いつ・何を・なぜ決めたか」と、置き換えられた判断の行き先を残す。

- **ID**: 旧ノート 48 の ID（A2、B1、X1、P1 …）はそのまま使う。旧ノート 48 より前の判断は `L01` から、旧ノート 48 の §0 にある 3 つの前提変更は `Δ1`〜`Δ3` とする。旧ノート 49（docs/archive/） が「48 R2」と呼ぶ決定は F1 と同じ内容。48 に行が無い ID（F7、R2、R3）は欠番。
- **日付**: 決めた日（YYYY-MM-DD）。ノートに日付が無いものは、その内容が最初に入った git の commit の日付。
- **保持先**: その判断を今持っている仕様ファイル。撤回された判断は、置き換えた判断の保持先を指す。
- **撤回/置換**: 置き換えられた判断だけに、後継の ID を書く。「保留」は決めずに止めている状態。
- 「未決」の行は、決める項目の ID を残すための行。

## 決定の一覧

| ID | 日付 | 決定 | 理由（1 行） | 保持先 | 撤回/置換 |
|---|---|---|---|---|---|
| L01 | 2026-10-10 | 主蓄電を Eaton HSL1225-3R8127-R（120 F）、発電を φ36 mm / 2 V / 80 mW 級の既製 Solar パネルにする（Premium） | 数日の無日照運転と、充電作業の削減を両立する | [power.md §3.1](./spec/power.md)、[§7.5](./spec/power.md) | — |
| L02 | 2026-10-10 | PMIC は BQ25570 級（内蔵 buck） | 当初の暫定候補 | [power.md §3.1](./spec/power.md) | 置換: L04 |
| L03 | 2026-10-10 | CR2032 は low-voltage backup と cap 復旧に使い、BLE・NAND の瞬間負荷は 120 F から取る | 当初の役割分担案 | [power.md §3.2](./spec/power.md) | 置換: L05、L37 |
| L04 | 2026-10-10 | 電源パスは BQ25505 + 外付け PMOS 2 個 + TPS62840。TPS2116 など別の mux IC は使わない（L02 の後継） | 当初は素子数と package 数を区別していなかった。BQ25505 は buck を持たない | [power.md §3.1](./spec/power.md) | 訂正: C4（4 素子／dual 2 個、VSTOR 接続） |
| L05 | 2026-10-10 | CR2032 は非常用だけ。cap が 2.5 V 未満で CR2032 へ、2.7 V まで回復で cap へ戻る（ヒステリシス）。CR2032 から cap は充電せず、cap から CR2032 へ逆流させない（L03 の後継） | 切替点が 1 つだと flapping で brownout を招く。2.5 V は HSL の下限 2.2 V への余裕 | [power.md §3.2](./spec/power.md)、[§3.5](./spec/power.md) | — |
| L06 | 2026-10-10 | 通常運転を 3.8 → 2.5 V（約 136.5 mWh）で使う | 当初の充電上限の置き方 | [power.md §3.3](./spec/power.md) | 置換: L30 |
| L07 | 2026-10-10 | ストレージは Winbond W25N08LW（8 Gbit）、目標は最低 1 GB | 当初の暫定候補 | [storage.md §1](./spec/storage.md) | 置換: L09 |
| L08 | 2026-10-10 | 1.8 V の生 SPI NAND を 1 個。SD / SD NAND / PSRAM は使わない。ファイルシステム無しの append-only ring（A/B metadata + CRC + commit marker + runtime の bad block 置換） | 1.8 V 単一 rail で、エネルギーが最小。耐電断を ring で作る | [storage.md §2](./spec/storage.md)、[§5](./spec/storage.md) | — |
| L09 | 2026-10-10 | 容量は 4 Gbit 基準。バックログ連動ビットレート（32 → 16 kbps）で、スマホ不在 48 h を p = 1 でも守る。上限は 32 kbps。満杯なら未同期でも最古から上書きし、欠落区間を通知する | 固定 32 kbps だと 4 Gbit は p = 1 で 35.8 h しか持たない | [storage.md §7.3](./spec/storage.md)、[§8.4](./spec/storage.md) | — |
| L10 | 2026-10-10 | ASR の精度が足りなければ 16 kHz / 64 kbps へ上げる | 当初の保険案 | [audio.md §5](./spec/audio.md) | 置換: L09（上限 32 kbps） |
| L11 | 2026-10-10 | 録音は 8 kHz・mono・IMA-ADPCM 32 kbps。LC3 / Opus は v0.1 では見送る | 目的は会話ログと ASR。codec 演算の電力が BLE + NAND の節約を上回る | [audio.md §5](./spec/audio.md) | — |
| L12 | 2026-10-10 | NAND の待機は DPD を優先し、ハードゲート（TPS22916）は補助。マイクは AAD のため通常 ON。ADC の分圧器は測定時だけ ON | 待機電流が µA 級のシステムで支配項になる | [power.md §5](./spec/power.md) | — |
| L13 | 2026-10-10 | マイクは T5838 を 1 個。AAD を本命の運用にする。上側面〜斜め上のポート、短い空洞、ePTFE vent | 無音区間の電力を 20 µA に落とす。短い音響経路で損失を減らす | [audio.md §1](./spec/audio.md)、[§3](./spec/audio.md) | 一部置換: H7（マイクを T5848 に。AAD・ポート・vent の方針はそのまま） |
| L14 | 2026-10-10 | アンテナはストラップ内蔵（strap antenna） | 当初の構想 | [rf.md §3](./spec/rf.md) | 置換: L15、X2 |
| L15 | 2026-10-10 | v1 は PCB 上端の chip antenna を標準にし、strap は比較用の option。chip / strap / RF test は 0 Ω で選ぶ。active RF switch は使わない（L14 の後継） | strap は機械品質と RF 品質の管理が増える | [rf.md §3](./spec/rf.md)、[§6](./spec/rf.md) | 一部置換: X2（Raytac ではモジュール内蔵のアンテナを使う。selector と strap は裸チップ・比較用だけ） |
| L16 | 2026-10-10 | 汗を主な環境条件にする（acoustic vent、膜、gasket、conformal coating、金接点、人工汗の加速試験） | 常時装着で、防滴より汗の腐食が支配的 | [mechanical.md](./spec/mechanical.md) | — |
| L17 | 2026-10-10 | Solar ボタンは剛体パネルごと平行移動させ、給電は金メッキの spring contact 2 本（Premium） | パネルを繰り返し曲げない。接点を封止した空洞に置く | [mechanical.md](./spec/mechanical.md) | — |
| L18 | 2026-10-10 | SWD の pogo パッドは確定仕様で、v1 では削らない。通常の FW 更新は BLE OTA、SWD は初回書込み・開発・救済 | brick の救済経路を残す | [firmware.md §1](./spec/firmware.md)、[board.md](./spec/board.md) | — |
| L19 | 2026-10-10 | OTA は MCUboot + MCUmgr / SMP（BLE）。外部 NAND を boot chain に入れず、内蔵 RRAM の dual-slot で成立させる | boot chain に外部部品を入れない | [firmware.md §2](./spec/firmware.md)、[§3](./spec/firmware.md) | — |
| L20 | 2026-10-10 | v1 は APPROTECT / SECUREAPPROTECT / ERASEPROTECT をすべて OFF にし、OTA と復旧が成熟してから段階導入する | 試作中の救済と開発を止めない | [security.md §8](./spec/security.md) | — |
| L21 | 2026-10-10 | hardware watchdog を使い、BOR は最後の安全網にする。通常は BOR より上の電圧で graceful shutdown し、再起動にヒステリシスを持たせる | restart chatter の防止 | [firmware.md §4](./spec/firmware.md)、[power.md §6](./spec/power.md) | — |
| L22 | 2026-10-10 | cap の残量は電圧の線形 % ではなく、V² に基づく energy で出す | 蓄えるエネルギーは電圧の 2 乗に比例する | [power.md §8.1](./spec/power.md) | — |
| L23 | 2026-10-10 | CR2032 のみの運転と cap 低残量では OTA を禁止する。閾値は固定電圧ではなく、実測の OTA energy に margin を掛ける | OTA 中の電力切れで brick しない | [power.md §9](./spec/power.md) | — |
| L24 | 2026-10-10 | RTC は RV-3032-C7 を搭載する（YES） | 160 nA で、0.5 mW 目標の約 0.1 %。温度補償 ±2.5 ppm の価値が大きい | [firmware.md §5](./spec/firmware.md) | 置換: D5（footprint のみ） |
| L25 | 2026-10-10 | RTC の VBACKUP を CR2032 系から給電し、RTC 内蔵の backup switchover を使う | 当初の推奨 | [firmware.md §5.3](./spec/firmware.md) | 置換: L32 |
| L26 | 2026-10-10 | 終端電源断の表示は、通常の残量表示と分離した双安定のフラグ（保持電力ゼロ）にする | 全電源を失っても状態が残る | [terminal-indicator.md §1](./spec/terminal-indicator.md)、[§3](./spec/terminal-indicator.md) | — |
| L27 | 2026-10-10 | フラグは通常 BLACK（隠れ）、終端で RED（dead） | 当初の極性 | [terminal-indicator.md §2](./spec/terminal-indicator.md) | 置換: L45 |
| L28 | 2026-10-10 | 試作の Phase 1 は nRF54L15 DK + T5838 + NAND | 当初の試作順序 | [prototype.md](./plan/prototype.md) | 置換: L38 |
| L29 | 2026-10-10 | 最重要の Go/No-Go は「平均 0.8 mW 未満」 | 当初の判定基準 | [prototype.md §9](./plan/prototype.md) | 置換: L36 |
| L30 | 2026-10-11 | VBAT_OV は 3.70 V 公称を設計基準にする（通常運転 3.70 → 2.5 V、124.0 mWh）（L06 の後継） | ±2 % の誤差込みで HSL の最大定格 3.8 V 以下にする | [power.md §3.2](./spec/power.md)、[§3.3](./spec/power.md) | 置換: C3（3.673 V） |
| L31 | 2026-10-11 | system rail は 1.8 V を評価基準にし、1.9 V も条件付きで比較する | MCU の POR 下限 1.75 V と NAND の上限の両側で決める | [power.md §2.2](./spec/power.md) | — |
| L32 | 2026-10-11 | RTC は常時給電の system rail だけから給電する。内蔵 switchover と trickle charger は無効、VBACKUP は 10 kΩ で GND（L25 の後継） | VBACKUP Power state では I²C が無効になる。1.8 V + CR2032 の組合せで切替条件が成立する | [firmware.md §5.3](./spec/firmware.md)、[power.md §1.3](./spec/power.md) | — |
| L33 | 2026-10-11 | 製品を Standard / Premium の 2 系統に分ける | 1 つのハードで全要求を満たすと BOM・部品点数・容積・検証が増える | [product.md §2](./spec/product.md) | — |
| L34 | 2026-10-11 | Standard は CR2477 × 1 + reservoir コンデンサ + TPS62840。Solar・BQ25505・120 F・CR2032・power mux は持たない。電池交換は約 1 年を目標 | スマホ必携を受け入れて、BOM と故障点を減らす | [power.md §1.1](./spec/power.md)、[§4](./spec/power.md) | — |
| L35 | 2026-10-11 | Standard の保存は 256 Mbit 級の 1.8 V SPI NOR | 当時は NAND が高く在庫が薄いと見たため | [storage.md §2.2](./spec/storage.md) | 置換: Δ2 |
| L36 | 2026-10-11 | 最終の電源 Go/No-Go は、CR2032 を使わず無日照 72 h を守れること（容量下振れ・劣化・温度・誤差込み）と、屋外での回収収支の別確認。平均 0.8 mW 未満はスクリーニング（L29 の後継） | 0.8 mW 未満だけでは 72 h を保証しない（当時の corner 約 65 h） | [power.md §7.4](./spec/power.md)、[prototype.md §9](./plan/prototype.md) | 数値・証拠の位置づけを C5・C6 で更新 |
| L37 | 2026-10-11 | CR2032 運転中は 120 F を負荷のピーク補助に使わず、通常の BLE 同期を止め、NAND はページ単位で電源回復を待って書く | CR2032 運転では 120 F の経路が切り離される | [power.md §3.6](./spec/power.md)、[storage.md §8.3](./spec/storage.md) | — |
| L38 | 2026-10-12 | Phase 0 は XIAO nRF54L15 Sense（改造なし）で FW の縦串と差分電流。DK・PPK2・T5838 評価ボードは買わず、シャント + オシロで測る（L28 の後継） | 同じ SoC の FW が XIAO で書ける。最終電力は自作基板で測る | [prototype.md](./plan/prototype.md) | — |
| L39 | 2026-10-12 | Phase 1 は XIAO を差し替え式に挿す Standard の評価キャリア（XIAO の L1 を外して 1.8 V 化） | 電源・NAND・マイク・RTC の検証はモジュールに依存しない | [prototype.md](./plan/prototype.md) | 置換: P1 |
| L40 | 2026-10-12 | Phase 1 基板の実装は JLCPCB の PCBA | ホットエアーが無く、T5838（LGA）などは手はんだできない | [prototype.md](./plan/prototype.md) | 置換: Δ1 |
| L41 | 2026-10-12 | Phase 4 用の SWD プローブは Pico（debugprobe）+ pyOCD + 基板上の SWD レベル変換 IC | 1.8 V のターゲットに 3.3 V のプローブを直結できない | [prototype.md](./plan/prototype.md) | 置換: P4 |
| L42 | 2026-10-13 | 録音ログのレコード形式 v1（magic "LOSC"、CRC-32、commit "CMT!"、連番は全種別で 1 本、時刻補正は timefix の追記） | 記録済みの時刻を書き換えず、耐電断を保つ | [formats.md §2](./spec/formats.md) | — |
| L43 | 2026-10-13 | XIAO を無改造・別部品で同梱するキット（低背の着脱キャリア）。USB-C はケース外へ出さない | 需要を確かめる前に SIG と認証の固定費を避ける狙い | [firmware.md §2.3](./spec/firmware.md) | 置換: X1 |
| L44 | 2026-10-13 | 筐体は非金属の円形、前面を全面ボタンにする、膜防水、ePTFE のマイク vent。Standard は φ40〜42 × 12〜15 mm 級、Premium は φ40〜42 × 18〜22 mm 級 | RF を阻害せず、日常装着で防滴を成立させる | [mechanical.md §1・§2](./spec/mechanical.md)、[product.md §5.3](./spec/product.md) | 一部置換: L51（Standard の厚さ） |
| L45 | 2026-10-13 | 終端フラグの極性を反転する。通常は常時可視の RED（電源系が動作可能）、終端で BLACK（L27 の後継） | RED は REC の慣習に沿う。通常時が見えない設計では周囲に何も伝わらない | [terminal-indicator.md §1](./spec/terminal-indicator.md)、[§2](./spec/terminal-indicator.md) | — |
| L46 | 2026-10-13 | AirTag 互換の外形は要件にしない（調査ログ） | 内部実装が物理的に収まるか未検討 | [product.md §5.3](./spec/product.md) | — |
| L47 | 2026-10-14 | Standard の電圧段階は NORMAL / LOW / CRITICAL / TERMINAL の 4 段。TERMINAL は再起動までラッチ。起動の門を置く（段階の形は方針、数値は仮置き） | 平らな電池を入れた直後の起動 → TERMINAL のループを防ぐ | [power.md §4.2](./spec/power.md) | — |
| L48 | 2026-10-14 | BLE 同期はフラグメント送信 + 累積 ACK + 再送（純 C の送信側 `fw/lib/sync`） | 接続中のロスを再送で吸収し、ACK までを解放しない | [formats.md §6](./spec/formats.md)、[§10](./spec/formats.md) | — |
| L49 | 2026-10-14 | （B4 の原案）電池電圧はキット版では SAADC の VDD で読み、足りなければ分圧器を D3 とジャンパで共有する | XIAO は VDD が電池に追従する | [power.md §4.6](./spec/power.md) | 置換: B4（訂正） |
| L50 | 2026-10-14 | （45 の草案）購入者の PC の書込みツールが鍵を作って SWD で書き、QR でスマホへ渡す | 当時の前提は XIAO のキット | [firmware.md §2.3](./spec/firmware.md)、[security.md §6](./spec/security.md) | 置換: F1 |
| Δ1 | 2026-10-14 | リフロー炉を用意したので、実装は自宅で行う。JLCPCB の PCBA は使わない（裸基板とステンシルだけ外注）（L40 の後継） | 部品の選定が JLCPCB の在庫に縛られなくなる | [prototype.md](./plan/prototype.md) | — |
| Δ2 | 2026-10-14 | Standard も NOR をやめ、1.8 V の生 SPI NAND（512 Mbit〜1 Gbit）にする（L35 の後継） | NOR は価格でも書込みエネルギー（約 9〜22 倍）でも NAND に利点が無い | [storage.md §2.2](./spec/storage.md) | — |
| Δ3 | 2026-10-14 | 電流の測定は INA228 の治具に固定する | 測定の再現性 | [current-measurement-fixture.md](./test/current-measurement-fixture.md) | — |
| A2 | 2026-10-14 | 製品の系統は Standard / Premium の 2 系統を維持する。Standard を先に作る | Premium の部品は残し、Standard で検証を前倒しする | [product.md §2](./spec/product.md) | — |
| A3 | 2026-10-14 | 認証済みのモジュールを別部品として同梱し、購入者が組み立てる（販売形態の原案） | SIG・CE の固定費を避ける狙い | [product.md §9](./spec/product.md)、[compliance.md](./plan/compliance.md) | 保留: R1 |
| K1 | 2026-10-14 | 組立方法は、はんだ付けが要らない形だけにする（差す、またはバネ接点）。素のモジュールは試作用 | 購入者の組立を単純にする | [product.md §9](./spec/product.md) | 保留: X2（販売形態が決まるまで扱わない） |
| A4 | 2026-10-14 | 販売地域は日本と海外。モジュールは技適・FCC・CE などを取得済みであること | 海外でも売る | [product.md §9](./spec/product.md) | — |
| A5 | 2026-10-14 | Phase 4 は認証済みの素のモジュール 2 つ（Raytac AN54LQ-15、u-blox NORA-B206）を自宅でリフロー実装して試す | 技適が確認でき、在庫があるのはこの 2 つ | [prototype.md](./plan/prototype.md) | 一部置換: X3（Raytac を Fanstel BC15M に） |
| B1 | 2026-10-14 | Standard の電池寿命は約 1 年を目標にする。NAND 前提で電力モデルを計算し直し、成り立つ条件（ビットレート、p）を出す | 充電不要が Standard の売り | [power.md §7.6](./spec/power.md)、[product.md §8](./spec/product.md) | — |
| B2 | 2026-10-14 | Standard の NAND 容量（512 Mbit か 1 Gbit） | 価格差を見て決める | [storage.md](./spec/storage.md) | 未決 |
| B3 | 2026-10-14 | 終端表示（赤 ⇄ 黒）を Standard にも Premium にも入れる。Standard（CR2477）用の last-gasp 回路を別に設計する | 電池が切れたことを、スマホが無くても伝える | [terminal-indicator.md §13](./spec/terminal-indicator.md) | — |
| B5 | 2026-10-14 | privacy latch を採る。機構（スライドか磁気式）は筐体と一緒に後で決める | FW が何をしても録音できない物理的な停止を持つ | [product.md §4.3](./spec/product.md)、[security.md §9](./spec/security.md) | — |
| B6 | 2026-10-14 | NAND の書込み・消去に電圧ゲートを掛ける。閾値は Phase 1 の実測のあとで決める | 長いパルスは reservoir で橋渡しできない | [power.md §4.5](./spec/power.md) | — |
| B7 | 2026-10-14 | reservoir は 220 µF 級を第一候補にし、footprint は 100〜470 µF | BLE と program の短いパルスを救う | [power.md §4.4](./spec/power.md) | — |
| B8 | 2026-10-14 | 低温だけの別の制限は設けない（電圧ゲート B6 に任せる） | 低温は内部抵抗が増え、同じゲートが先に働く | [power.md §4.5](./spec/power.md) | — |
| C1 | 2026-10-14 | Premium の 48 h オフラインは維持する。録音の解像度を下げてでも守る（縮退モード。codec と下限レートは ASR の評価で決める） | 48 h が Premium の価値 | [storage.md §7](./spec/storage.md)、[audio.md §6](./spec/audio.md) | — |
| C2 | 2026-10-14 | Premium の電源 IC（BQ25505 か BQ25570） | 比べてから freeze する | [power.md §1.2](./spec/power.md) | 今回の試作は C4 |
| C3 | 2026-10-14 | OV は 3.673 V（ROV1 = 6.34 MΩ、ROV2 = 6.49 MΩ、0.1 % 抵抗）。BQ25505 を採った場合（L30 の後継） | ±2 % の上側 3.746 V で、HSL の最大定格 3.8 V に 54 mV の余裕 | [power.md §3.3](./spec/power.md) | — |
| D1 | 2026-10-14 | #23 は JLCPCB PCBA の見積が不要なので閉じ、「部品の調達と、裸基板・ステンシルの見積」として作り直す | Δ1 | [prototype.md](./plan/prototype.md) | — |
| D2 | 2026-10-14 | Phase 1 は XIAO 差し替え式のキャリアで回路図に着手する | 検証はモジュールに依存しない | [prototype.md](./plan/prototype.md) | 置換: P1 |
| D3 | 2026-10-14 | 基板の発注先は JLCPCB（裸基板とステンシル） | Δ1 | [prototype.md](./plan/prototype.md) | — |
| D4 | 2026-10-14 | レベル変換は TXU0204（マイク）と TXU0304（NAND）を第一候補にする。SPI は 10 MHz で足りる | XIAO は 3.3 V 系で 1.8 V の周辺と合わない | [board.md](./spec/board.md) | 一部置換: X2（XIAO の開発基板だけの話になる） |
| D5 | 2026-10-14 | 外付け RTC は footprint だけ置く（L24 の後継）。要るかは実測で決める。GRTC で数えて次の同期で遡って付ける案もある | Standard はスマホを常に持つので成り立ちやすい | [firmware.md §5.3](./spec/firmware.md)、[§5.4](./spec/firmware.md) | — |
| D6 | 2026-10-14 | 逆流防止（Schottky / LM66100 / 0 Ω）は footprint だけ置く。載せるかは運用次第 | 保守作業での事故に対する保険（必須ではない） | [product.md §6.2](./spec/product.md)、[board.md](./spec/board.md) | — |
| E1 | 2026-10-14 | AirTag 互換の外形 | モジュールの寸法が分かってから決める | [product.md §5.3](./spec/product.md) | 置換: E1b |
| E1b | 2026-10-15 | AirTag 互換（Ø31.9、中央 12 mm）は CR2477 のまま詰めて狙う。詰めた積層は想定 12.45 mm、最良 11.2 mm。実部品で入らなければ、互換をやめるか CR2450 にするかを決め直す | 両面実装や基板の分割は Ø31.9 では効かず、厚さは電池で決まる。電池寿命（約 1 年）を優先する | [mechanical.md §7.4a](./spec/mechanical.md) | — |
| E2 | 2026-10-14 | ボタンは前面のまま（厚さより操作性を取る） | 前面全面ボタンが操作の基本 | [mechanical.md](./spec/mechanical.md)、[product.md §5.3](./spec/product.md) | — |
| E3 | 2026-10-14 | XIAO の USB-C 部を電池の外周より外へ出す | USB-C 部（4.44 mm）が電池と重ならず、全厚が増えない | [mechanical.md §6.1](./spec/mechanical.md) | 適用範囲を縮小: X1、X2（XIAO を入れる筐体だけ。Raytac 基板は USB を前提にしない） |
| F1 | 2026-10-14 | 出荷前に MCUboot を書き、以降はスマホから BLE で DFU する（iOS と Android の両方で使える）。スマホから USB で書く案は採らない。鍵は初回起動時に本体が作る方向（L50 の後継。旧ノート 49（docs/archive/） では「48 R2」） | nRF54L15 に USB が無く、iOS には USB の経路が無い | [firmware.md §2.3](./spec/firmware.md)、[security.md §6](./spec/security.md) | — |
| F2 | 2026-10-14 | ペアリングは LESC + OOB | Just Works は中間者攻撃に弱い | [security.md §3](./spec/security.md) | — |
| F3 | 2026-10-14 | 保存データを暗号化する（AES-128-GCM） | 紛失・盗難で外付け NAND を読まれても守る | [security.md §5](./spec/security.md) | — |
| F4 | 2026-10-14 | BLE の広告に機器名を載せない。ボンド済みのスマホが IRK で見つける | 通りすがりの追跡を避ける | [security.md §4](./spec/security.md) | — |
| F5 | 2026-10-14 | 署名鍵はメーカーの鍵 1 本で全員のイメージに署名する。鍵はオフラインで保管する | 利用者が鍵を管理しない | [security.md §7](./spec/security.md) | — |
| F6 | 2026-10-14 | APPROTECT はキット版では OFF のまま。保存データは F3 の暗号化で守る | 試作中の救済を止めない | [security.md §8](./spec/security.md) | — |
| F8 | 2026-10-14 | ペアリング前の録音は保存しない（初期化が終わるまでマイクを動かさない） | 暗号鍵が無い間の録音を残さない | [security.md §6.2](./spec/security.md)、[audio.md §1](./spec/audio.md) | — |
| F9 | 2026-10-14 | 鍵の置き場所は `storage` パーティション（KMU は後で調べる） | 現状は未使用の 36 KB がある | [security.md §6](./spec/security.md)、[firmware.md §2.1](./spec/firmware.md) | — |
| F10 | 2026-10-14 | ring を `ring_partition`（784 KB）へ移すことを了承する（slot は 320 KB × 2） | MCUboot の dual-slot を成立させる | [firmware.md §2.1](./spec/firmware.md) | — |
| F11 | 2026-10-14 | OTA は電池電圧が NORMAL（2.60 V 以上）のときだけ許す（閾値は実測で動く） | OTA 中の電圧降下で brick しない | [firmware.md §3.3](./spec/firmware.md)、[power.md §9.1](./spec/power.md) | — |
| F12 | 2026-10-14 | MCUboot の電源断の試験は、今の XIAO で USB を手で抜いて行う | 実機で swap の中断を確かめる | [firmware.md §3.5](./spec/firmware.md) | — |
| F13 | 2026-10-14 | 誤用・誤認対策を 3 つ入れる。筐体・箱・説明書に常時録音を書く、privacy latch を「録音の一時停止」として見せる、販売表示と規約の法的確認（専門家）。前面ボタンによる FW の一時停止は入れない | 盗聴器と隣り合わせの機能なので、周囲の人への表示を持つ | [security.md §9](./spec/security.md) | — |
| R1 | 2026-10-14 | 販売形態は、SIG と試験所から回答が来るまで保留する。その間は XIAO で Phase 1 を進める（A3 の後継） | 同梱にしても SIG と CE が要る可能性が高い | [compliance.md](./plan/compliance.md)、[product.md §9](./spec/product.md) | — |
| R4 | 2026-10-14 | Bluetooth の構成は Standard と Premium で同じにする。2 つの差は電源・保存・筐体だけ | SIG の 2 件目を $0 にし、認証を 1 回で済ませる | [product.md §2.2](./spec/product.md)、[compliance.md](./plan/compliance.md) | — |
| R5 | 2026-10-14 | 送信出力を +4 dBm（アンテナ利得込み）以下に固定する | FCC の SAR 試験を免除できる見込み | [rf.md](./spec/rf.md) | — |
| X1 | 2026-10-15 | XIAO nRF54L15 は開発専用にする。キットに同梱する案はやめる（L43 の後継） | nRF54L15 に USB が無く、XIAO の USB-C は SAMD11 のブリッジにつながる。UF2 のように書けない | [firmware.md §2.3](./spec/firmware.md)、[prototype.md](./plan/prototype.md) | — |
| X2 | 2026-10-15 | dogfooding の本筋は、Raytac AN54LQ-15 を自宅でリフロー実装した基板。1.8 V の rail で直接動かす（レベル変換は不要）。32.768 kHz の水晶は外付け | 技適 201-250520、FCC SH6AN54LQ（C2PC で身に付ける用途も可）。1.7〜3.6 V で動く | [prototype.md](./plan/prototype.md)、[board.md](./spec/board.md)、[product.md §6.1](./spec/product.md) | 置換: X3 |
| P1 | 2026-10-15 | Phase 1 の XIAO キャリアは作らない。Raytac の基板へ進み、電流測定用の 0 Ω と TP をそこに載せる（L39・D2 の後継） | X2 で前提が変わった | [prototype.md](./plan/prototype.md) | — |
| P2 | 2026-10-15 | dogfooding は Standard と Premium の両方を作り、知人に好きな方を選んで使ってもらう。順番は Standard → Premium | 両 SKU の使われ方を比べる | [product.md §2.3](./spec/product.md)、[prototype.md](./plan/prototype.md) | 今回の試作順は P6 |
| P3 | 2026-10-15 | 予備のモジュール u-blox NORA-B206 は後で試す。初版は Raytac だけ | 初版の手間を減らす | [prototype.md](./plan/prototype.md) | 一部置換: X3 |
| P4 | 2026-10-15 | SWD プローブは XIAO（SAMD11 の CMSIS-DAP）を流用する。外部のターゲットに使えるかは要検証（L41 の後継） | 新しいプローブを買わずに済む | [prototype.md](./plan/prototype.md) | — |
| P5 | 2026-10-15 | 知人への無償配布は、SIG への問い合わせには足さない | 問い合わせの論点を増やさない | [compliance.md](./plan/compliance.md) | — |
| B4（訂正） | 2026-10-15 | 電池電圧は基板に入れる。TPS62840 が VDD を 1.8 V に保つので、スイッチ付きの分圧器 + SAADC で測る。SAADC の VDD を読む案は、VDD が電池に追従する XIAO の場合だけ使える（L49 の訂正） | Raytac 基板では VDD を読んでも電池電圧が分からない | [power.md §4.6](./spec/power.md) | — |
| L51 | 2026-10-15 | Standard の厚さの目標を 15 mm 以内にする。12 mm 側は届かない。Raytac 基板を電池の脇に置く並置の想定が 14.5 mm（L44 の厚さの修正） | 全層が最小でも 12.4 mm。重ねる配置は想定 15.1〜15.35 mm で超える | [mechanical.md §2](./spec/mechanical.md)、[§7.4・§7.5](./spec/mechanical.md) | — |
| H1 | 2026-10-16 | Standard 基板 rev 0 の回路図を生成した（`hw/standard/`）。ロードスイッチは TPS22917（SOT-23-6）を 3 個（NAND・Mic・電池の分圧器）。DC/DC インダクタと 32.768 kHz 水晶は載せる。privacy latch は初版ではブリッジ済みのソルダージャンパ。reservoir は 100 µF を実装し 220 µF は DNP。終端表示と RTC は DNP | TPS22916 は WCSP で自宅リフローに向かない。部品が未定の所は置き場だけ作る | [hw/standard/DESIGN.md](../hw/standard/DESIGN.md) | — |
| H2 | 2026-10-16 | Raytac 基板のピン割当を確定（board.md §3） | Nordic のポートの規則と、XIAO で動作確認済みの組を使う | [board.md §3](./spec/board.md)、[DESIGN.md §4](../hw/standard/DESIGN.md) | 一部置換: X3（GPIO は同じ、パッド番号を BC15M に） |
| H3 | 2026-10-17 | Standard 基板に CR2477 のホルダ MPD BRX1-2477-SM（SMT リテーナ、−は基板のパッド）を付ける | 電池の交換と試作を楽にする（ユーザー指示）。37.0 mm 幅なので Ø31.9 には入らず、ホルダ付きの基板は Ø40〜42 が前提。Ø31.9 の版は背面カバーのばね（E1b）のまま | [DESIGN.md §3.1](../hw/standard/DESIGN.md) | 置換: H4 |
| H4 | 2026-10-17 | 電池ホルダを MYOUNG MY-2477-01（LCSC C2979173、スルーホール、26.8 × 22.6 mm、0.33 ドル）に替える | BRX1-2477-SM は幅 37 mm で大きく高い（1.07 ドル）。CR2477 の SMT ホルダはどれも 37 mm 級。MY-2477-01 は電池からピンの所しかはみ出さず、Ø31.9 にも入る | [DESIGN.md §3.1](../hw/standard/DESIGN.md) | — |
| H5 | 2026-10-18 | Standard 基板の NAND は WSON-8 8×6 のまま（F35UQA512M-WWR）。6×5（F35UQA512M-VWR、GD5F1GM7REWIGR、W25N512GWPIT）に小さくする案は取らない | 1.8 V の 6×5 品は 2026-10-18 時点で DigiKey・Mouser・LCSC のどれも在庫 0。Ø31.9 に入らない問題は配置か外形で解く | [storage.md §2](./spec/storage.md) | — |
| X3 | 2026-10-18 | dogfooding の本筋のモジュールを Fanstel BC15M（nRF54L15、6.0 × 9.6 × 1.9 mm、PCB アンテナ、水晶と DC/DC インダクタ内蔵）にする。GPIO の割当は H2 のまま。VDD の下限（表紙 1.7 V、ピン表 1.8 V）は試作で確かめる | AN54LQ-15 は大きく、AN54LV-15 は入手経路が細い。BC15M は技適 201-250527（証明書の付属書 3 に BC15M）、FCC X8WBC15 は身に付ける用途で評価済み、DigiKey JP に在庫があり、外付け部品が減る | [hw/standard/DESIGN.md](../hw/standard/DESIGN.md)、[compliance.md](./plan/compliance.md) | — |
| H6 | 2026-10-18 | SWD は Tag-Connect TC2030 と同じ配置（2 × 3、1.27 mm）の穴なしのポゴパッド（J1）にし、ポゴピンを束ねた治具を当てる。TC2030 の位置決め穴 3 つは付けない | 裏面は電池ホルダと −パッドが占め、位置決めの穴もピンも通せない | [hw/standard/DESIGN.md §3.9](../hw/standard/DESIGN.md)、[board.md](./spec/board.md) | — |
| H7 | 2026-10-18 | マイクを TDK T5848（MMICT5848-00-012、I²S 出力、AAD あり）にする。nRF54L15 の I2S20 で受ける（SCK P1.12、WS P1.03、SD P1.13。WAKE・THSEL は今のまま） | T5838 は DigiKey・Mouser とも在庫 0（12 月入荷予定）。ゼロベースで探し直しても、AAD 20 µA・低電力録音 120 µA・価格・在庫の全部で T5848 を上回るものは無かった。AAD の方式とレジスタは T5838 と同じで、電池寿命も同じ（p = 0.4 で約 355 日） | [audio.md §2](./spec/audio.md)、[board.md §3](./spec/board.md) | 電力・Rev 1.1 の評価を C6 で更新 |
| P6 | 2026-10-23 | 11 月中旬のハッカソン向け Premium は Standard 完成を待たず、共通コアと並行して先行する。実 Solar・外観・HSL／非常用ボタン電池を必須にし、USB 給電は設けない | ユーザーの期限・実演条件。通常の製品評価と実演の証拠を分ける | [premium-hackathon.md](./plan/premium-hackathon.md)、[product.md §2.3](./spec/product.md) | — |
| C4 | 2026-10-23 | 今回は BQ25505＋TPS62840。secondary は VSTOR、各枝に common-source back-to-back PMOS 2 素子（計 4 素子／dual 2 個）。source 出力の 1.8 V レベル処理と一次電池の逆流保護を別に評価する | BQ25570 は同じ primary gate driver を持たない。TI の回路で電流経路と信号電圧を再確認 | [power.md §3.1〜§3.5](./spec/power.md)、[試作計画 §3・§4](./plan/premium-hackathon.md) | — |
| C5 | 2026-10-23 | OV 6.34／6.49 MΩ は維持。OK 4.87／5.62／0.787 MΩ、公称 2.606／2.802 V を推奨評価候補とする。実採用は回路・波形確認後 | HSL の最低定格 2.5 V を公差上も上回る。従来の 2.510／2.701 V と最悪 energy を区別 | [power.md §3.3・§7.4](./spec/power.md) | 評価候補、回路変更未実施 |
| C6 | 2026-10-23 | 電力モデルの受信 duty／保存 duty／同期量を分離。65 µW は未検証、42 µW は cycle 実測からの代理換算、NAND 15 mJ/MB は cap 側 | HFXO／I²S／FIR と損失の二重計上を見直し、基板全体の実測で判断する | [power.md §7](./spec/power.md)、[audio.md §5.1](./spec/audio.md) | — |
| C7 | 2026-10-23 | ハッカソンの NAND は小口在庫のある Alliance 8 Gbit B を先行評価。既存 32 kbps を維持し、Sleep／power gate／LGA を確認後に 1 品固定する | 4 Gbit Winbond の入荷待ちと新しい縮退 codec の同時実装を避ける。48 h 保証・採用の決定は別 | [storage.md §2.3](./spec/storage.md)、[試作計画 §6.3](./plan/premium-hackathon.md) | 試作の選定は C8 に置換。調達・採用未実施 |
| C8 | 2026-10-27 | 11 月の Premium デモ機には 512 Mbit の F35UQA512M-WWR を設計採用する。2 KiB page と power gate を使用し、製品の 48 h 要求とは分ける | ユーザーはデモで長時間保存を求めず、大幅な低価格化と既存 Standard の部品・足跡の再利用を優先 | [storage.md §2.3](./spec/storage.md)、[試作計画 §6.3](./plan/premium-hackathon.md) | 取得・実装・電力／保存時間の検証は未実施 |
| C9 | 2026-10-27 | 11 月デモの電源回路ドラフトは ROK1／2／3 を 4.87／6.34／0.787 MΩ、下降／復帰約 2.785／2.981 V に変更。前面 Solar は uxcell φ36 mm を第一評価品とする | Vina 30 F の 2.5 V 下限に対する静的余裕と丸形・短納期を優先。旧案より切替までの理想エネルギーは約 14.4% 減 | [デモ電源](./plan/premium-power-circuit.md) | 評価ドラフト。過渡・逆流・蓄電器の放置・Solar の実測は未検証。製品向け C5 は維持 |

## 置き換えた判断の一覧

| 撤回された判断 | → 後継 |
|---|---|
| Standard の保存は NOR（L35） | NAND（Δ2） |
| XIAO を同梱するキット（L43、A3） | 開発専用（X1）。販売形態は保留（R1） |
| JLCPCB の PCBA（L40） | 自宅リフロー（Δ1） |
| RTC を搭載する（L24） | footprint だけ（D5） |
| PC の書込みツール + QR（L50） | MCUboot を出荷前に書き、BLE DFU（F1） |
| 電池電圧を SAADC の VDD で読む（L49） | スイッチ付きの分圧器 + SAADC（B4（訂正）） |
| Phase 1 は XIAO 差し替えキャリア（L39、D2） | 作らず Raytac 基板へ（P1） |
| Pico + 基板上のレベル変換の SWD プローブ（L41） | XIAO の SAMD11 を流用（P4） |
| 充電上限 3.8 V（L06）、3.70 V（L30） | 3.673 V（C3） |
| BQ25570 級の PMIC（L02） | BQ25505 + PMOS + TPS62840（L04）。ただし IC は BQ25570 と比べるまで freeze しない（C2） |
| strap antenna（L14） | chip antenna を標準（L15）。Raytac ではモジュール内蔵のアンテナ（X2） |
| 8 Gbit / 1 GB（L07）、ASR 不足なら 16 kHz / 64 kbps（L10） | 4 Gbit + バックログ連動ビットレート、上限 32 kbps（L09） |
| RTC の VBACKUP を CR2032 から給電（L25） | system rail だけから給電（L32） |
| 終端フラグの極性 BLACK = 通常（L27） | RED = 通常（L45） |
| 平均 0.8 mW 未満が最重要の Go/No-Go（L29） | CR2032 なしで無日照 72 h（L36） |
| Phase 1 は DK + PPK2（L28） | XIAO で Phase 0（L38） |
| XIAO の USB-C の配置（E3）、レベル変換（D4） | XIAO を使う開発基板・筐体だけの話（X1、X2） |
| Standard の厚さ 12〜15 mm（L44） | 15 mm 以内（L51） |
