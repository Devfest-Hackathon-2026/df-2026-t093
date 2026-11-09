# mobile/ — LifeOS Pendant の Android / iOS アプリ（開発 FW 用）

開発 FW（`fw/src/ble.c` + `fw/lib/sync`、`docs/spec/formats.md` §6・§9〜§11）と同期するアプリです。KMP の共有 core（`shared/`）と、OS ごとのネイティブ UI（`androidApp/` Compose、`iosApp/` SwiftUI）でできています。Refs #32。

```
bash mobile/verify.sh        # 全部。最後に PASS / FAIL / SKIPPED の表
MOBILE_REQUIRE_IOS=1 bash mobile/verify.sh   # iOS を飛ばしたら失敗にする
```

`verify.sh` は shared のテスト → C interop → `:androidApp:assembleDebug` → （macOS + Xcode + iOS Simulator runtime があるとき）`:shared:iosSimulatorArm64Test` と `xcodebuild` による iosApp の build、の順に走り、最初の失敗で非 0 で終わります。iOS を SKIPPED にするのは、macOS でない・xcodebuild が無い・Simulator runtime が無い、のときだけです（PASS とは表示しません）。`JAVA_HOME`（既定は環境変数、無ければ Homebrew の openjdk@17、@21）と `ANDROID_HOME` / `ANDROID_SDK_ROOT`（既定 `/opt/homebrew/share/android-commandlinetools`）は `env.sh` が **そのプロセスの中でだけ** export します。`~/.gradle/gradle.properties`、シェルの rc、git の global 設定には触れません。compileSdk / targetSdk は 36、minSdk は 31 です。

## pendant-api（server/）への接続

URL とトークンは Git に入れない。未設定ならアプリは固定データ（ハリボテ）と台本で動く。

- Android: `pendant/mobile/local.properties` に `pendant.api.base=https://…` と `pendant.api.token=…`
- iOS: `iosApp/Sources/Secrets.plist`（`ApiBase` / `ApiToken` の 2 キー、文字列）

Gemini に送るかどうかは、設定タブの「クラウドに送信する」（`PATCH /settings`）でサーバーが判定する。

## 構成と責任の境界

| shared（commonMain） | platform |
|---|---|
| CRC-32、レコードの parse・検証、形式の判定（known / opaque）、timefix・gap・event の decode（`Chunk.kt`） | BLE: Android は `BluetoothGatt`（`AndroidBleLink`）、iOS は CoreBluetooth（`CoreBluetoothLink.swift`） |
| 受信手順 `SyncSession`（再組み立て、reorder buffer、累積 ACK、RESEND は 1 回、終端マーカー後 3 s × 6 回、20 s 無通信で打ち切り） | 権限（Android の実行時権限、iOS の usage description）、ライフサイクル（前面 / 背面） |
| 耐久ログ `RecordLog` とメモリ上のメタデータ表、機器ごとの保存 `DeviceStore`、UTC の推定 | 保存先のルートディレクトリ、backup からの除外、ファイル保護 |
| `FakePendant`（`fw/lib/sync` と ring の Kotlin 移植、損失・重複・破損・並べ替え・切断・上書きを注入）、`Demo`、`Controller`（`StateFlow` と Swift 向け `subscribe`） | UI（Compose / SwiftUI）、設定の保存（SharedPreferences / UserDefaults） |
| `Fs`（`expect class PlatformFs`: Android は `RandomAccessFile` + `FileChannel.force`、iOS は posix + `F_FULLFSYNC`、失敗したら fsync） | |

`Link` は callback 型のインターフェースで、Swift から coroutine のブリッジ無しで実装できます。`Controller` もスレッド前提は「呼ぶのも callback も main スレッド」だけです。

## 耐久性の設計と不変条件

- **durableNext**: fsync を終えたログが連続して覆っている次の seq（空のログは 1）。**ACK と SYNC(have) に送ってよい値は、接続中の機器のログの `durableNext − 1` 以下だけ**。ペンダントはこの値までのデータを捨ててよいからです。
- **受理の規則**（`chunk_rx_feed` と同じ。D = 保存直前の durableNext）: `last < D` は DUP（保存しない）、`first > D` は AHEAD（RAM の reorder buffer に置き、未 ACK のまま）、それ以外は ACCEPT（生のバイトをそのまま追記 → fsync → `durableNext = last + 1`）。
- **coverage**: そのレコードが覆う seq の範囲。known の gap は `first..last`、それ以外は `header.seq` の 1 件。表の行には、新しく覆った部分（gap は `max(first, D)..last`）を入れます。
- **ログの順**: 受理して frontier を進めた順。`header.seq` は単調とは限りません（例 1..5 のあとに gap(3..8)）。
- **open の順序**（手順 1〜3 は書き込まない）: ① 先頭から CRC つきで decode し、最初に decode できなかった位置を P とする → ② P の後ろに CRC と commit の正しいレコードがあれば途中の破損 → ③ 0..P に受理の規則を当てはめる（DUP / AHEAD / opaque の gap があれば不整合）→ ④ どちらも無く P < ファイル長のときだけ、P に truncate して fsync。⑤ 整合したログは（truncate の有無にかかわらず、空でなければ）records.log から root の親（Application Support / filesDir）までを、ACK/SYNC の前に fsync してから durableNext を使う（filesDir / Application Support より上の dataDir / Library は OS 側の責任とみなす、というのが既知の上限）。読めることは durable であることを意味しない（直前の fsync 失敗で page cache にだけ残った record を、電源断なしの再 open で SYNC に使わないため）。fsync に失敗したら『保存エラー』。
- 手動の「今すぐ同期」と機器の scan は、前面にいて BLE が許可されている（または demo）ときだけ動く（Android の権限拒否中は Controller が無視し、AndroidBleLink も権限なしでは BLE を呼ばない）。
- **『ログ不整合』**: ②または③に当たったとき。torn tail を含めてファイルを 1 バイトも変えず、その機器の同期（SYNC・ACK・TIME）を始めません。fsync 済みのデータを truncate で消さないためです。自動修復はしません（後続）。
- **保存失敗**: append か fsync が例外を出したら、その `RecordLog` は使えなくなり、session はすぐ止まり、以後 ACK も SYNC も送りません。失敗の時点の D（fsync 済みの frontier）より先は何も ACK していないので、ペンダントの ring には seq ≥ D が残ります。次の session は開き直し、再走査で決まった durableNext から再開します。画面は『保存エラー』で、利用者の操作を待ちます。
- **メタデータ表をメモリ上に置き、索引ファイルを持たない理由**: 真実を機器ごとのログ 1 本にして、索引とログの食い違いという故障を作らないためです。表は open の検証走査のついでに作り（audio の payload は持たない）、以後は fsync 後に行を足します。一覧と UTC の推定は表だけから作り、ファイルを読み直しません。ponytail: メモリが問題になったら（1 日で数十万行）、その時に考える。open がログ全体を読む点も同じ上限です。
- Python の受信ツール（`tools/pendant_rx.py`）の「fsync なし、最後の ACK のあとに state を保存」は模倣していません。

## 機器ごとの保存

```
<root>/registration                 いま登録している 1 台の識別子と名前
<root>/devices/<key>/records.log    その機器から受理したレコード
<root>/devices/<key>/device         識別子、名前、これまでに見た boot の最大値
```
`<key>` は OS 識別子（Android は `BluetoothDevice.address`、iOS は `CBPeripheral.identifier` の UUID 文字列）の UTF-8 を小文字 16 進にしたものです。`<root>` は実機用と demo 用で別です。`registration` と `device` は一時ファイル → fsync → rename → ディレクトリ fsync で置き換えます。作ったディレクトリ（`<root>`、`devices/`、`devices/<key>`）は、それぞれの親を fsync してから使います（`mkdirsDurable`）。さらに `register` と、ログを open／最初に追記した時は `syncUp` で records.log から root の親（Application Support / filesDir）までを fsync するので、アプリが前もって作った root のエントリも、最初の ACK/SYNC の前に durable になります。filesDir / Application Support より上（dataDir / Library）は OS 側の責任とみなします。接続のたびに、接続した機器の識別子が registration と一致することを確かめ、その機器のディレクトリだけから durableNext と boot を決めます（違えば SYNC・ACK・TIME を送らずに切断）。

- **登録を忘れる**: `registration` を消し、メモリ上の `RecordLog` と画面状態を捨てます。`devices/<key>` は消しません。
- **別の識別子を登録**: 空のログ（durableNext = 1）から始まり、最初の SYNC は have = 0（FW が捨ててよい範囲は無い）。
- **同じ識別子を再登録**: 残っているログと boot の最大値をそのまま使い、そのログの durableNext − 1 で SYNC します。
- **制約**: 開発 FW の INFO にはデバイス ID が無いので、OS 識別子が変わると（iOS の Bluetooth 設定のリセットなど）同じペンダントでも別の機器として空のログから取り直します。重複して保存されることはありますが、未保存のデータを捨てさせることはありません。

## 形式の判定と opaque

CRC と commit が正しいレコードは、type ごとの decode より先に判定します。known は「flags の bit0 以外が 0、type が 1〜4、codec が audio=1・他=0、payload_len が timefix=16・gap=12・event=8、gap は first == header.seq かつ first ≤ last」。known でないものは opaque として **生のまま保存し、header.seq の 1 件分だけ ACK** します。payload は decode せず、一覧には『未対応の形式（暗号化の可能性）』と出し、UTC の推定では自分の時刻にも他のレコードの timefix の参照先にも使いません。

**opaque な gap（type = 3 で known でないもの）だけは止まります。** gap の first..last は「どこまで失われたか」の宣言なので、解釈できないまま 1 件分だけ進めると、実際には先まで失われているのに ACK 済みの範囲に穴ができる、または逆に未取得の範囲を ACK してペンダントに捨てさせるおそれがあるからです。位置（DUP / AHEAD / frontier）にかかわらず、保存も ACK も SYNC もせずに session を止め『未対応の形式』にします。それ以前に送った ACK は有効です。

## 検証の行列

「Kotlin fake」= shared のテスト（`FakePendant`、メモリ上のファイル層。ホスト JVM と iOS Simulator の両方で走る）。「C interop」= 実物の `fw/lib/{sync,ring,chunk}` を RAM ring でつないだテスト（`interop/harness.c`、ホスト JVM のみ、`cc` が要る）。「実機」はすべて **未検証**。

| 項目 | Kotlin fake | C interop | Android build | iOS simulator build | 実機 |
|---|---|---|---|---|---|
| レコードの形式、CRC、fixtures（`sample.rec`、`backwards.rec`）、known / opaque | ○ | | | | 未検証 |
| 受信手順（clean / lossy / lossy+disconnects / corrupt-heavy / overwrite × 20 シード） | ○ | ○（5 シード × 4 シナリオ） | | | 未検証 |
| ACK・SYNC ≤ durableNext − 1、保存失敗 5 ケース、クラッシュ（全バイト位置） | ○ | ○（reclaim された seq は fsync 済み） | | | 未検証 |
| 実ファイルの `RecordLog`（追記・開き直し・任意長で切り詰め） | ○（ホスト JVM の実ファイル） | | | | **電源断の物理試験は未検証** |
| 機器の分離、食い違いの検出、ログ不整合 8 種 | ○ | | | | 未検証 |
| demo の模擬ペンダントは起動・demo の切替のたびに作り直すが、保存済みの demo ログの続き（seq は継続、boot は +1）から始める（さもないと next_seq が小さく『不一致』になる） | ○ | | | | |
| demo transport（登録 → 同期 → 一覧 → 切断と再接続 → gap） | ○ | | ○ | ○（simulator で demo 起動を確認。フローは shared のテストが simulator で走る） | 未検証 |
| Android の BLE（`AndroidBleLink`、権限） | | | ○（build のみ） | | 未検証 |
| iOS の BLE（`CoreBluetoothLink`） | | | | ○（build のみ。simulator に BLE は無い） | 未検証 |
| allowBackup=false、backup 除外（INTERNET は pendant-api 接続のため 2026-11-09 に追加） | | | ○ | ○（backup 除外・ファイル保護は実装のみ） | 未検証 |
| CI（`.github/workflows/mobile.yml`） | | | | | 実行結果を確認するまで未検証。required gate ではない |

## 照合した FW

`fw/lib/chunk/chunk.{c,h}`、`fw/lib/sync/sync.{c,h}`、`fw/lib/ring/ring.{c,h}`、`fw/src/ble.c`（INFO、sync スレッド）、`tools/pendant_rx.py`、`tools/test_rx.py`、`tools/fixtures/*.rec`。commit `48f9ea86`。仕様書とコードが食い違えばコードに従いました。

## 開発 FW と製品のセキュリティ構想の違い

開発 FW の GATT は無認証（`CONFIG_PENDANT_BLE_SECURE=n`）で、bond を要求せず、privacy も無く（identity address）、INFO にデバイス ID も FW 版数もありません。そのためアプリの「ペアリング」は **登録**（service UUID で scan、選んだ機器の OS 識別子と名前を保存）です。OS の bond（`createBond` など）は呼びません。`security.md` §3 は Just Works を禁止しており、製品は LESC + OOB、RPA / IRK、AES-128-GCM、Keystore / Keychain を使う構想ですが、**#26 が決まるまで実装せず、プレースホルダも置いていません**。

**食い違い**: `docs/spec/security.md` は AAD を「レコードの header（40 B）」とし、実測も header 40 B で書かれていますが、現在の形式（`chunk.h`、`formats.md` §2）の header は 32 B です。この PR では直しません（`docs/spec/` は対象外）。ENCRYPTED フラグの解釈、鍵の受け渡し、OOB の方法は #26 で未確定です。

## 後続

background での同期（Android の foreground service connectedDevice、iOS の bluetooth-central と state restoration）／OTA（MCUmgr / SMP、低残量の拒否と巻き戻しの表示）／製品のペアリングと暗号化（#26）／フラグメント単位の再送（formats.md §11、#6）／ログ不整合の修復（truncate や隔離）／忘れた機器のログの削除と書き出し。
