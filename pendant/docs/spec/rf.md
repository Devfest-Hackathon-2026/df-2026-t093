# RF 仕様: アンテナ・TX 電力・RF ESD・RF 試験

対象: BLE の RF 部分。TX 電力の上限、BC15M モジュールのアンテナの扱いと置き方、RF ESD、RF の評価方法、裸チップまたはオプションで使う場合のアンテナ構成（matching・selector・strap・RF テスト経路）。

規制（技適・SIG・CE・FCC・UK）と問い合わせは plan/compliance.md、ピン割当・テストポイント・配置と ESD の一般規則は [board.md](./board.md)、筐体の stack-up と防水は [mechanical.md](./mechanical.md)、BLE のプロトコルは [formats.md](./formats.md) が持つ。

確度の表記:

- **確定**: 設計の前提として動かさない。変えるときは設計変更として扱う。
- **方針**: 向きは決まっているが、数値や部品が決まっていない。
- **仮置き（実測で決める）**: 実測で置き換える前提の値。

## §1 要約

| 項目 | 内容 | 確度 |
|---|---|---|
| MCU・無線 | nRF54L15。dogfooding の本筋は自宅でリフロー実装する Fanstel BC15M モジュール（技適 201-250527、FCC X8WBC15）。Standard と Premium で Bluetooth の構成は同じ | 確定 |
| アンテナ | **BC15M モジュールが持つ PCB アンテナを使う。** 基板に matching 回路・アンテナ部品・selector・RF ESD 素子は載せない（§3） | 確定 |
| TX 電力 | **+4 dBm EIRP（アンテナ利得込み）以下に固定**（§2） | 確定（届く距離は実測） |
| strap アンテナ・アンテナ selector・RF テスト用コネクタ・RF 用 ESD 素子 | **裸の nRF54L15 を使う場合か、オプションとして評価する場合だけ**の仕様（§6）。BC15M 基板には適用しない | オプション |
| RF の評価 | 実筐体・人体装着状態で、スマホの RSSI・転送速度・パケットエラー・最低 TX 電力を測る（§5） | 方針 |
| XIAO nRF54L15 | 開発専用。基板上のアンテナの扱いは製品の仕様に含めない（[board.md §2](./board.md)） | 確定 |

## §2 TX 電力

- **+4 dBm EIRP 以下に固定する**（確定）。Standard と Premium で同じ（Bluetooth の構成を共通にして、SIG の Design を 1 つにするため。plan/compliance.md）。
- 理由: FCC の SAR 試験を免除できる見込みがある（47 CFR 1.1307(b)(3)。2.45 GHz・人体から 5 mm で約 2.7 mW = 約 +4.3 dBm）。EN 50663 も 20 mW 以下なら簡易評価で済む。ペンダントの BLE は近距離の同期が主なので、両立しやすい。法的な確定ではない。試験所への確認は plan/compliance.md（#15）。
- EIRP = 設定する出力 + アンテナ利得。BC15M の技適の証明書にあるアンテナ利得は最大 1.5 dBi なので、**設定する出力は +2.5 dBm 以下**。モジュールの最大放射は +5.64 dBm（伝導 +8 dBm のとき）。胸元での実効利得と EIRP は実機で測って確定する。
- アンテナはモジュール上端の、シールド（6.0 mm）から先の約 3.6 mm の PCB トレース。**アンテナの下は表裏とも銅なし**（footprint に keep-out がある）。Fanstel の取付の助言は §3.2。
- 届く距離は Phase 1 の実機で測る。**足りない場合に TX 電力を上げて解決することはできない**（上限が固定のため）。アンテナの向き・位置・筐体の側で解決する。
- 上限の下では、スマホとの距離に応じて TX 電力を下げる余地がある（近ければ低く、リンクが悪ければ上限まで上げる）。v1 は単純に、複数の TX 電力で測り、同期が安定する最低値を決める（方針）。

## §3 BC15M モジュールのアンテナ（現行構成）

- BC15M の PCB アンテナを使う。モジュールは技適（201-250527）・FCC（X8WBC15）を取得済みで、**RF 経路（matching とアンテナ）は認証された状態のまま使う**。基板側で変えるのは、keep-out、GND、周囲の部品配置だけ。
- モジュールの RF 経路に外部アンテナ・strap・RF スイッチを足す構成は、認証の条件が変わる可能性がある。足す場合は §6 と plan/compliance.md を先に確認する（要確認）。
- アンテナ keep-out の寸法と基板端への置き方は Fanstel のデータシートと footprint（§3.2）に従う。RF 端子がパッドに出ているかは未確認。下の規則はデータシートより優先しない。

### §3.1 人体の影響

2.4 GHz では人体は大きな誘電体で損失体。胸元に置くと、共振がずれ、整合が悪化し、放射効率が下がり、人体側へ電力が吸収される。人が近づくと数 dB 級の利得低下と detuning が起こりうる（Silicon Labs などのウェアラブルアンテナ資料）。

- ベンチで 50 Ω に合っていても足りない。**最終の筐体、Solar、120 F の cap、strap、人体装着の状態で評価する**。
- モジュールのアンテナは調整できないので、対策は置き方で行う（§3.2）。BC15M 基板の RSSI は、評価ボード EV-BC15M（回路図と Gerber が公開されている）と胸元で比べ、基板の置き方の影響を切り分ける（§5）。

### §3.2 置き方（方針）

```text
        strap
         │
   ┌─────┴─────┐
   │ アンテナ   │ ← 基板の最上端（人体から最も遠い側）
   │ keep-out  │
   │           │
   │   基板    │
   │ cap / 電池 │
   └───────────┘
       人体側
```

- アンテナは **基板の最上端、人体から最も遠い側**に置く。
- 電池（CR2477 / CR2032）、120 F の cap、Solar の金属、磁気フラグのコイル、ストラップの金属部品を近づけない。
- Fanstel の取付の助言（確定。BC15M のデータシート）: シールドの上端を、基板の GND の端に揃える。次善は基板の角で、アンテナの端から GND まで 5 mm 以上あけること。基板の中央には置かない。アンテナの下に銅を置かない。金属は 30 mm 離すと最も遠くまで届く。
- モジュールのデータシートの keep-out と GND の指定を守る。keep-out は機構の CAD にも反映し、cap・電池・Solar 配線が近い状態で干渉を確認する（stack-up は [mechanical.md](./mechanical.md)）。部品単体の外形だけで収まったと判定しない。
- 金属の背面カバーを使う場合は RF への影響がさらに大きい。アンテナ側は樹脂の領域を確保する。
- 実効性能は §5 の測定で確かめる。

## §4 RF ESD（モジュール構成）

- PCB アンテナは筐体の内部にあり、利用者が RF の導体に触れない。この構成では strap の feed より ESD のリスクは小さい（確定）。**RF 経路に ESD 素子は置かない。**
- 外周の金属や service 用の接点が RF の近くにある場合は、試験で確認する。
- ESD の侵入経路（マイク開口、Solar 接点、pogo、筐体の隙間）と帰路の規則は [board.md §7](./board.md)。
- **ESD 試験のあとに、RSSI・転送速度・実効距離が試験前と変わっていないことを確認する**（RF 経路の損傷の検出）。ESD 試験のレベルと方法は、試験の前に定義する（[board.md §7.4](./board.md)）。

## §5 RF の評価（方針）

最大距離ではなく、**最低の TX 電力で、同期間隔ぶんのバックログを安定して転送できるか**を見る（同期の仕組みは [formats.md](./formats.md)）。

| 条件 | 内容 |
|---|---|
| 装着状態 | 自由空間 / 胸元に装着 / シャツの上 / ジャケットの下 |
| 向き | 前向き / 後ろ向き / 横向き |
| 濡れ | 汗・濡れた状態（[mechanical.md](./mechanical.md)） |

測定項目:

- スマホの RSSI（0.5 / 1 / 2 / 5 m）
- BLE 2M PHY の転送速度
- パケットエラー率・リトライ
- **安定して使える最低の TX 電力**（TX 電力は +4 dBm EIRP 以下の範囲で複数点を振る。選べる段階はデータシートで確認する）
- VNA の S11（RF 端子にアクセスできる構成のときだけ。BC15M で RF 端子が出ていなければ測らない）

評価の時期: PCB 発注前は keep-out と置き方のレビュー、最初の基板は上の実測（[board.md §7.4](./board.md)）。同じ FW・同じ TX 電力で比較する。

## §6 裸チップまたはオプションの構成（BC15M 基板には適用しない）

この節は、**nRF54L15 の裸のチップ（QFN48 / CSP47）を基板に載せる場合**と、**アンテナ構成の違いをオプションとして比べる場合**だけの仕様。BC15M モジュールを使う限り、基板にこれらの部品は載らない。

### §6.1 トポロジ（方針）

```text
nRF54L15 RF
    │
 π matching（footprint を必ず残す）
    │
 selector（0 Ω の実装選択）
    ├── 0 Ω ──── チップアンテナ（既定で実装）
    ├── DNP ──── strap feed
    └── RF テスト用のコネクタ / パッド
```

- chip を既定で実装し、strap feed は DNP。RF テストは試作だけ。**同じ PCB で chip / strap / conducted RF を比べる。**
- selector は **能動の RF スイッチを使わず、受動の 0 Ω の実装選択**にする（確定。Iq が要らず、挿入損失が増えず、BOM が安く、筐体を開ければ実装を変えられる）。
- selector の分岐点と DNP の枝は極短にして、分岐の stub を作らない。
- 裸チップの場合は、採用するパッケージと電源構成に対応する Nordic の reference layout と matching を基準にし、RF 経路・GND・via・デカップリングを独自に省略しない。基板の stack-up の違いは確認する。
- 評価用の RF テストコネクタ: Murata MM8030-2610（スイッチ付き、2 × 2 × 0.9 mm、DC〜11 GHz、6 GHz 以下で低挿入損失、プローブを挿したときだけ外部測定器へ切り替わる）。U.FL は大きいので使わない。**量産相当では面積と費用のため削除する**。

### §6.2 チップアンテナの候補（部品は未選定）

| 部品 | 仕様 |
|---|---|
| Antenova Weii SRCW004 | 2.4〜2.5 GHz、約 1.0 × 0.5 × 0.5 mm、ウェアラブル向け |
| Antenova Abdita SRCW090 | 同クラスの超小型セラミックアンテナ |
| Johanson 2450AT18A0100001E | 2400〜2500 MHz、50 Ω、1206。データシートの性能はメーカー評価基板の値 |

- 部品の寸法が 1 mm 級でも、**必要なのはアンテナ周囲の keep-out の面積**。部品の寸法だけで選ばない。
- データシートの性能は、小型の円形 PCB、人体近接、Solar・cap・コイン電池の周辺では必ず再調整が必要。
- 最終の部品のランド・keep-out・matching の資料は、部品の確定時に追加する。
- 出典: https://www.antenova.com/weii 、https://www.johansontechnology.com/products/antennas/rf-antennas/2450at18a0100001e/ 、https://www.murata.com/ja-jp/products/connector/switchconnector/overview/lineup/swg

### §6.3 strap アンテナ（オプション。既定にしない）

strap の中にアンテナを入れる案。pendant 本体の電池・cap・Solar・人体から離せる利点があるが、strap 専用の構造、RF 接点、汗対策、引張りの strain relief、製造ばらつき、装着状態での調整、strap 交換時の RF 保証が RF 部品の仕事に加わる。BOM の単価だけでなく、機構の組立と品質管理のコストが増える。**strap を設計上の必須条件から外す**（確定）。

作る場合の形（方針）:

- strap 全体をアンテナにしない。pendant の直上の片側の strap の内部に、**約 25〜35 mm の定義された radiator**（細線または FPC）を置く。
- strap の強度部材とは分離し、アンテナの導体に機械的な荷重を持たせない。
- 本体との接続は spring contact。

```text
基板の RF パッド → spring contact → strap anchor → 約 30 mm の radiator（strap 内）
```

strap 全体を導体にしない理由: 長さが利用者で変わる、首への接触状態が変わる、clasp の金属で detune する、曲げ形状でインピーダンスが変わる、アクセサリの交換で RF 特性が変わる。

**採用の基準（方針）:** chip antenna で同期が十分に安定するなら strap にはしない。次のどれかが明確に改善する場合だけ採る。
- TX 電力を数 dB 以上下げられる
- 同期の時間が明確に短くなる
- 体による遮蔽での切断が大幅に減る

評価は §5 と同じ項目で、chip と strap を同じ FW・同じ TX 電力で比べる。

### §6.4 RF 用の ESD 素子（裸チップ・strap のとき）

- **strap の RF 経路: ESD 保護は必須**（確定）。RF の feed の直近に超低容量の素子を置く。strain relief を付け、導体を露出させず、汗の経路から RF 接点を隔離する。
- chip antenna の経路: footprint を用意し、実装の要否を評価する。
- 素子は 2.4 GHz で 0.2 pF 級の寄生容量でも無視できないので、**目標は 0.2 pF 以下**（配置を含めて VNA で確認）。

| 部品 | 容量 | 備考 |
|---|---|---|
| onsemi ESD8472 | 0.2 pF | RF / アンテナ ESD 用、0201 級 |
| Infineon ESD178-B1-W0201 | 0.21 pF | 0201、低挿入損失 |
| Littelfuse SESD0201 系 | 0.1〜0.2 pF 級 | |

- 配置: 保護対象の IC の近くではなく、**ESD が入ってくる物理的なインタフェース側**にできるだけ近く置く。GND の帰路を極短にする（長いと、ESD パルス時のインダクタンスで clamp の性能が落ちる）。

```text
strap / 接点 → ESD 素子（極短の GND 帰路）→ matching / selector → nRF54L15 RF
```

- 素子の有無で S11・RSSI・転送速度・実効距離を比べる。素子を入れて RF が大きく悪化する場合は、素子の変更か matching の再調整を行う。**素子を入れたあとの matching は必ず実測する。**
- 出典: https://www.onsemi.jp/download/data-sheet/pdf/esd8472-d.pdf 、https://www.infineon.com/ja/part/ESD178-B1-W0201 、https://www.infineon.com/assets/row/public/documents/24/42/infineon-an178-rf-antenna-protection1-applicationnotes-en.pdf 、Nordic nRF54L15 reference layout: https://www.nordicsemi.com/Products/nRF54L15/Reference-layout

## 未確定

| 項目 | 状態 |
|---|---|
| +4 dBm EIRP 以下での RF 評価と、SAR 免除（FCC 1.1307(b)(3)）・EN 50663 の簡易評価が成り立つかの試験所への確認 | #15、plan/compliance.md |
| BC15M のアンテナの置き方（Fanstel の取付の助言どおりにできるか）、RF 端子の有無（データシートで確認） | 新規 issue 候補: BC15M 基板の RF 評価（keep-out・EIRP 設定・胸元での RSSI と最低 TX 電力） |
| 胸元に装着した状態での RSSI・転送速度・最低 TX 電力・届く距離（Phase 1 の実機。EV-BC15M との RSSI の比較を含む） | 新規 issue 候補: BC15M 基板の RF 評価（同上） |
| モジュールの RF 経路に外部アンテナを足す構成が、モジュールの認証（技適・FCC）の条件に収まるか | plan/compliance.md。strap を採る場合だけ |
| 裸チップ構成の採否（現在の計画には無い） | 採る場合に新規 issue |
