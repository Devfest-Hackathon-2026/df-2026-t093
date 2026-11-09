# Premium デモ: 秋葉原優先の部品調達

2026-10-27 調査。**秋月・千石など秋葉原の実店舗で小売りされる部品を優先**し、現物をすぐ交換できる設計を目指す。薄い丸型 MJF、実際に発電する前面 Solar、Solar＋蓄電器と非常用一次電池、USB 給電・充電なしという条件は維持する。個別の調達候補は設計・実測による採用前であり、在庫や表示価格は変化する。**店頭表示、通販在庫、取扱い・取り寄せを分ける。**

**2026-10-28 更新:** 電池ホルダーの第一評価候補は、裏面から挿して表面ではんだ付けするスルーホールの [CH005-2032LF、108967](https://akizukidenshi.com/catalog/g/g108967/) に変更。秋月の商品ページ表示は税込 80 円、通販 574 個、秋葉原店 3 個、1F 51 番売場。店頭価格・在庫は変動する。実フットプリント、極性、保持、着脱と裏面配置は未確認で、現 PCB の J3 は仮端子のまま。以下の CH7410 は旧表面実装候補として残す。

## 秋月の店頭表示を確認できた候補

秋葉原店の数量・売場は 2026-10-27 15:00〜15:06 の商品ページ表示で、将来の購入や在庫確保を保証しない。価格は同時点の**通販税込**で、店頭価格が異なる場合がある。

| 部品 | 候補・調達表示 | 用途と確認事項 |
|---|---|---|
| 蓄電器 30 F | [Vina VEL08203R8306G、販売コード 116492](https://akizukidenshi.com/catalog/g/g116492/)、1 個 360 円。秋葉原店 5 個、1F 35 番売場 5 段目 | 店頭調達の第一評価候補。3.8 V、30 F。最大本体 φ9.0 × 21.5 mm、リード別。下記の電圧・寸法・過渡を評価 |
| 蓄電器 50 F | [Vina VEL08253R8506G、131884](https://akizukidenshi.com/catalog/g/g131884/)、1 個 440 円。秋葉原店 3 個、同売場 | 同じ最大径 φ9.0 mm、最大長 26.5 mm。容量は増えるが、リードと並置空間がより厳しい比較候補 |
| 小型比較電池 | [Panasonic CR1632、110819](https://akizukidenshi.com/catalog/g/g110819/)、1 個 140 円。秋葉原店 0 個、通販 216 個 | 秋葉原の当日店頭購入は確認できない。表示の容量 120 mAh と Panasonic の別資料 140 mAh に差があり、実際の型番・資料を確認 |
| 非常用第一候補 | [Golden Power CR2032、105694](https://akizukidenshi.com/catalog/g/g105694/)、5 個 200 円。秋葉原店に在庫表示、1F 53 番売場 | φ20 × 3.2 mm。Panasonic 品の電池仕様をこの別メーカー品に転用しない。保護と電圧降下の評価が必要 |
| 旧 CR2032 ホルダー候補 | [CH7410-2032LF、102684](https://akizukidenshi.com/catalog/g/g102684/)、1 個 70 円。秋葉原店 21 個、1F 51 番売場 | 旧表面実装案。[タカチの資料](https://www.marutsu.co.jp/contents/shop/marutsu/datasheet/takachi_CH7410.pdf) は落下・衝撃で電池が外れる可能性を指摘 |
| TPS 用 2.2 µH | [Murata DFE201210S-2R2M、114976](https://akizukidenshi.com/catalog/g/g114976/)、5 個 140 円。秋葉原店 1 パック、1F 32 番売場 3 段目、在庫限りの表示 | 2.0 × 1.2 × 1.0 mm、ΔL 定格 2.1 A、ΔT 定格 1.5 A、DCR 155 mΩ。現 EVM 候補とは別の footprint。飽和・効率・レイアウトを照合する |
| BQ 用 22 µH の試験比較 | [AL0307-220K、103964](https://akizukidenshi.com/catalog/g/g103964/)、10 本 70 円。秋葉原店 4 パック、1F 32 番売場 3 段目 | 軸リード、約 φ3 × 8 mm、DCR 1.2 Ω、ΔT 電流 285 mA。飽和電流・効率・実装を確認するまでは **BQ の既存候補の代替としない**。ベンチ比較用 |

千石の [SMTU1632](https://www.sengoku.co.jp/mod/sgk_cart/detail.php?code=EEHD-07YC) は CR1632 用、表面実装、税込 220 円、秋葉原本店 2F 取扱い・在庫拠点「秋葉原」の表示。**実店舗の個数は不明**。[メーカー図](https://www.takachi-enclosure.com/assets/attachments/images/takachi_battery_boxes_battery_holders_25-27.pdf) 上、ホルダーは約 14.5 × 27.65 × 5.4 mm。電池単体が小さくてもホルダー・着脱空間を含む配置を先に検討する。

## 蓄電器を入れ替える際の条件

[Vina 30 F の図面](https://akizukidenshi.com/goodsaffix/vel08203r8306g.pdf) は定格使用 2.5〜3.8 V、85 °C 時は 2.5〜3.5 V、DC ESR 700 mΩ、漏れ 1 µA（25 °C・72 h）と記載。0.8 ±0.1 mm リード、ピッチ 3.5 ±0.5 mm。Eaton HSH 22 F の φ6.8 × 24 mm 最大、φ0.5 ±0.05 mm リード、ピッチ 2.6 ±0.5 mm と**同一 footprint ではない**。Eaton の minimum allowed 2.2 V を Vina に適用しない。

旧仮積層（前面 1.0 mm、Solar 1.5 mm、接点 1.3 mm、PCB 0.8 mm、絶縁余裕 0.5 mm、背面 1.0 mm）なら、横置き径 9.0 mm で約 **15.1 mm**。これは裏面ホルダーの高さを含まない旧比較であり、現在案の完成厚には使えない。Eaton 22 F の約 12.9 mm より約 2.2 mm 増。前面支持・接着・公差・リード等は別途必要。外径 40〜42 mm 内へのホルダー・アンテナとの平面配置は未確認。

旧 2.6063 V 公称下降閾値は部品の overall ±2% だけで約 2.554 V まで下がり、Vina の 2.5 V 動作下限に対する静的な差は約 54 mV。デモ用回路ドラフトは 2.7852 V 公称へ変更し、この差を約 230 mV に拡大した。接点抵抗やセル ESR による瞬時降下、センス位置、温度・過渡を含む保証はない。[抵抗とエネルギーの検討](./premium-hackathon.md) と [回路評価条件](./premium-power-circuit.md) を実測で確かめる。2.7 V EDLC を 3.8 V の充電設定に直結しない。直列化には均圧・漏れ・起動・故障時の検討が必要。

## Solar・電源 IC・実装部品の残課題

- **Solar:** φ40〜42 mm の前面に収まる商品で、秋葉原店の現物在庫を確認できたものは見つかっていない。[秋月 LR0GC02](https://akizukidenshi.com/catalog/g/g116017/) は 67.5 × 41 × 0.8 mm、表示 Voc 5.7 V、通販 200 円、秋葉原店 22 枚の表示（2026-10-27 15:03）。前面に入らず、BQ25505 の入力絶対最大 5.5 V を超えるため、そのまま接続しない。[TI datasheet](https://www.ti.com/lit/ds/symlink/bq25505.pdf)。
- **小型 Solar の旧比較品:** [ANYSOLAR KXOB121K04TF](https://anysolar.biz/KXOB121K04TF-datasheet/) は 23 × 25 × 1.2 ±0.3 mm。マルツの[同型番掲載](https://www.marutsu.co.jp/pc/i/40766849/)は DigiKey 連携品で、秋葉原店で当日手に入ることは未確認。11 月デモの第一評価品は下表の uxcell 丸型。大型品の切断・配線変更は前提にしない。
- **電源 IC:** BQ25505、TPS62840、LM66100、CSD75208 について、同型番を秋葉原店で当日購入できる証拠は今回確認できなかった。通販での先行確保も含めて比較する。「取り扱いが見つからない」と「販売されていない」を区別する。
- **分圧抵抗:** デモ用回路ドラフトは 0.1% で 6.34 MΩ × 2、6.49 MΩ、4.87 MΩ、0.787 MΩ。各値の秋葉原店在庫は未確認。一般的な E24・5% に置換する場合、OV／OK の全公差と電流を再計算し、実閾値を検証する。
- **マイコン:** [XIAO nRF54L15 Sense](https://akizukidenshi.com/catalog/g/g131382/) は秋月秋葉原店 6 個、1F 3 番売場、通販税込 2,980 円の表示だが、既存方針どおり開発用。BC15M 製品基板へのそのままの代替ではなく、完成品に USB 給電・充電を持ち込まない。MIC、NAND と他の部品もそれぞれ型番・店舗在庫・footprint を確認する。

秋月の[店舗案内](https://akizukidenshi.com/catalog/pages/akiba.aspx)では秋葉原店は通常 11:00〜18:00、祝日を含む。商品ページの「店舗在庫情報」で出発前に確認する。[店舗受取サービス](https://akizukidenshi.com/catalog/pages/store_receive.aspx)は通販在庫を店へ移す仕組みで、当日・翌日受取を保証しない。注文・取り置きは行っていない。

## 実行用の評価調達メモ（2026-10-27 16 時台 JST）

下記数量は **1 台分の回路実験と予備**の目安。この調査では購入・予約をしていない。ウェブ取得で表示された数量は商品ページの表示であり、取得時刻が掲載元で保証されたリアルタイム更新時刻ではない。店頭在庫、通販在庫、到着日を混同しない。送料・納期は配送先と注文時に確認する。価格・数量は変動する。

### 評価用に先行確保できる候補

| 部品 | 評価量 | 販売先と表示 | 評価の目的・条件 |
|---|---:|---|---|
| Vina VEL08203R8306G 30 F | 2 個 | [秋月 116492](https://akizukidenshi.com/catalog/g/g116492/) 秋葉原店 5 個表示、1F 35 番 5 段。通販 360 円／個税込 | 初期実験と予備。実配線前に 2.5〜3.8 V の制約と過渡を確認 |
| Eaton HSH0622-3R8226-R 22 F | 2 個 | [DigiKey JP](https://www.digikey.jp/ja/products/detail/eaton-electronics-division/HSH0622-3R8226-R/26262349) 649 個表示、539 円／個税別 | 薄型比較用。店頭・日本到着日未確認。Vina と寸法・使用下限が異なる |
| CR2032 + holder | 電池 1 シート・holder 2 個 | [秋月 105694](https://akizukidenshi.com/catalog/g/g105694/) Golden Power 5 個 200 円税込、店頭在庫表示；[CH005-2032LF 108967](https://akizukidenshi.com/catalog/g/g108967/) については冒頭の 10/28 更新を参照 | 裏面から挿し、表面側で手はんだ付けする候補。取付・接触・逆流防止を試験。メーカー別仕様を混同しない |
| CR1632 + holder | 電池 2 個・holder 2 個 | [秋月 Panasonic CR-1632/BK 110819](https://akizukidenshi.com/catalog/g/g110819/) 秋葉原 0 個、通販 216 個、140 円／個税込；[千石 SMTU1632](https://www.sengoku.co.jp/mod/sgk_cart/detail.php?code=EEHD-07YC) 220 円税込、秋葉原拠点・本店 2F 取扱い、店頭個数不明 | 小型比較。電池の秋葉原即日調達は未確認、holder の寸法・保持を確認 |
| BQ25505RGRR | 3 個 | [DigiKey JP](https://www.digikey.jp/ja/products/detail/texas-instruments/BQ25505RGRR/4493883) 7,105 個表示、870 円／個税別 | 組立・予備。基板と抵抗設定、入力 Voc の評価が別途必要 |
| LM66100DCKR | 5 個 | [DigiKey JP](https://www.digikey.jp/ja/products/detail/texas-instruments/LM66100DCKR/10273183) 掲載あり。取得ページは 5 日前キャッシュのため現在庫は再確認 | 仮回路 2 個／台＋予備。一次電池の独立した逆流防止対策を代替しない |
| CSD75208W1015 | 5 個 | [DigiKey JP](https://www.digikey.jp/en/products/detail/texas-instruments/CSD75208W1015/4960055) 11,219 個表示 | 仮回路 2 パッケージ／台＋予備。WLP 実装・漏れ・gate 過渡を評価 |
| 保護 diode 比較用 BAT54CT | 1 パック | [秋月 115277](https://akizukidenshi.com/catalog/g/g115277/) 10 個 110 円税込、秋葉原店 1 パック表示（10/27 取得） | 低順方向電圧側の比較。共通カソード 2 素子でピンを確認。逆漏れは要評価、実セル接続用と決めない |
| 保護 diode 比較用 BAS116-7-F | 5 個 | [DigiKey JP](https://www.digikey.jp/en/products/detail/diodes-incorporated/BAS116-7-F/1099070) 9,553 個表示、カットテープ 1 個 38 円税別（10/27 取得） | 低漏れ側の比較。順方向電圧の余裕を評価、実セル接続用と決めない |
| φ36 mm Solar | 2 枚 | [uxcell Amazon B0H5MJDDX8](https://www.amazon.co.jp/dp/B0H5MJDDX8) 2 枚 657 円税込、残り 1 セット表示。2026-10-27 16 時台の購入画面は翌 10/28 配達表示、無料配送は対象商品 3,000 円以上の条件付き | ユーザー指定の評価対象。販売表示は 2 V／40 mA／0.08 W だが測定条件と厚さは不明。現物の寸法、極性、光条件別 I–V・低温 Voc・端子と封止を測り、BQ 入力条件と照合。商品詳細に `36 × 36 × 36 cm` との矛盾表記があり厚さの根拠にしない |
| F35UQA512M-WWR NAND | 3 個 | [DigiKey JP](https://www.digikey.jp/en/products/detail/lexar-enterprise/F35UQA512M-WWR/26793881) 2026-10-27 の取得表示 137 個、カットテープ 1 個 382 円税別 | Marketplace 商品で平均発送 1〜3 日、別途送料の可能性と CPT（通関・税等が配送時に発生する条件）の表示。必ず checkout 前に到着日と総額を確認。512 Mbit・1.7〜1.95 V、8-WSON。footprint・実装を確認し、3.3 V GPIO に直結しない |

### 設計・供給を確認してから確定するもの

- **TPS62840DLCR:** [DigiKey JP](https://www.digikey.jp/ja/products/detail/texas-instruments/TPS62840DLCR/10445259) の 10/27 再取得では在庫 0、11 月 24 日 526 個・12 月 3 日 2,474 個入荷予定。入荷予定は顧客到着日の保証ではない。デモ回路は下記 TPS62842DGRR へ変更して評価する。
- **NAND F35UQA512M-WWR:** 上表の DigiKey カットテープは小口表示を確認したが、在庫の確保と到着日は未確認。[型番のデータシート](https://cdn.icstop.com/upload/pdfs/e7/de/e7decc6bb7e9f2384da286e27083f166.pdf) と実 footprint を照合する。`F35S` は電源電圧が異なるため同一品扱いしない。
- **受動部品:** BQ 用 22 µH、TPS 用 2.2 µH、設定抵抗、コンデンサ、一次電池保護 diode／抵抗は、実機の定数・公差・実装・電圧特性を確定してから発注リストを閉じる。上記の秋月品は実験比較候補であり、設計適合の証明ではない。
- **順序:** 店頭に向かう直前に秋月・千石の各ページを再確認し、蓄電器・電池・holder を現物確認する。並行して通販の同型 IC と HSH の到着日を確認する。Solar を受領後は電池をつなぐ前に、パネルと制御閾値を単体で測定する。

### TPS62840 の追加探索

- **デモ用の代替評価候補 TPS62842DGRR:** [DigiKey JP](https://www.digikey.jp/ja/products/detail/texas-instruments/TPS62842DGRR/12352343) は 2026-10-27 再取得で在庫 6,177 個、カットテープ 1 個税別 386 円と表示。到着日・送料・秋葉原での当日入手は未確認。販売ページは出力最小 2.4 V と表示するが、[TI 公式データシートの製品比較と Table 1](https://www.ti.com/lit/ds/symlink/tps62840.pdf) は **VSET=GND で 1.8 V** を明記しており、差を認識した上でメーカー資料に基づき評価する。現物表示と仕様の再確認は必要。
- DGR は約 3 × 3 mm の本体、リード先端間約 4.8〜5.0 mm、最大高 1.1 mm、0.65 mm pitch、EP は GND。元の DLC 2.0 × 1.5 mm より大きく、**ピン互換ではない**。DGR では VOS=1、SW=2、MODE=3、VSET=4、EN=5、VIN=6、NC=7、GND=8、EP=9。STOP 端子はなく、旧回路では STOP=GND だった。回路図と評価 footprint を変更済み。出力の誤差・過渡、実効容量、推奨 land と実装性を確認する。
- 同型番の秋月・千石・マルツ・チップワンの小売即納商品は今回の検索では確認できなかった。非取扱いの証明ではない。[Mouser 日本 TPS62840DLCR](https://www.mouser.jp/ProductDetail/Texas-Instruments/TPS62840DLCR) は取得時 403 で、現時点の在庫・少量購入可否・到着日は確認できない。[Mouser 英国の商品検索結果](https://www.mouser.co.uk/en/ProductDetail/Texas-Instruments/TPS62840DLCR?qs=%252B6g0mu59x7L1%2Fxo1joT%252Bvg%3D%3D) に 131 個という過去のキャッシュはあるが、現時点の日本向け在庫と扱わない。
- 同じ TPS62840 の [YBGR](https://www.mouser.jp/ProductDetail/Texas-Instruments/TPS62840YBGR?qs=%252B6g0mu59x7LpKmlSGts%252BtQ%3D%3D) は検索結果に 131 個表示があるが 3 か月前の取得情報で現状不明。6 端子 DSBGA で DLC の 8 端子 VSON とは footprint もピンも異なり、基板変更・実装性の検証が必要。
- [TPS62841DLCR](https://www.digikey.jp/ja/products/detail/texas-instruments/TPS62841DLCR/10445077) は同じ DLC パッケージだが、検索キャッシュの出力範囲は 0.8〜1.55 V で必要な 1.8 V を設定できない。[TPS62849DLCR](https://www.marutsu.co.jp/pc/i/40332411/) は 3.4 V 固定。**同一 footprint や在庫だけで代替しない**。代替 IC の互換性・電源特性・実負荷評価は未検証。検索結果の過去在庫は現状と扱わない。
- 早期評価の代替として [TPS62840-1DLCEVM55](https://www.digikey.jp/ja/products/detail/texas-instruments/TPS62840-1DLCEVM55/10434525) は 10/27 取得表示で 11 個、税別 11,107 円。[TI の説明](https://www.ti.com/tool/ja-jp/TPS62840-1DLCEVM55)では 1.8 V に設定可能な独立 2 回路の評価ボード。**大きく高価な実験用**であり、そのままペンダントに収まる部品でも部品単体の確保でもない。到着日、購入量と送料は確認が必要。
