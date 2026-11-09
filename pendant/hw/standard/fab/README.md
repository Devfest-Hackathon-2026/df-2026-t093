# Standard 基板の発注データ（JLCPCB）

- `standard-jlcpcb-gerbers.zip`: ガーバーと穴データ（マイク T5848 の版。DRC 0 件・未配線 0 件は 2026-10-21 に確認。Gerber は 2026-10-18 版で、BT1 のジャンパー設定は銅に影響しない。回路図との不一致 0 件）（KiCad 10、Protel の拡張子、X2 なし、ソルダーマスクをシルクから除く、Excellon mm）。JLCPCB の注文画面にそのまま上げる。
- `gerbers/`: zip の中身（同じもの）。
- `bom_jlc.csv`: JLCPCB 形式の部品表（Comment / Designator / Footprint / LCSC Part #）。LCSC に無い 4 点（BC15M、NAND、U7 バッファ、マイク）は LCSC 欄が空で、他の購入先を書いた。
- `bom_purchase.csv`: 部品の購入表（2026-10-18 の価格・在庫）。DNP の部品、テストパッド、J1（ポゴパッド）、SW1（はんだジャンパー）は買う物がないので入れていない。電池 CR2477 は別に買う。

## 注文画面で選ぶもの

| 項目 | 値 |
|---|---|
| 層数 | 2 |
| 寸法 | 30 × 30 mm（Ø30 の円） |
| 板厚 | 0.6 mm（2 層 0.6 mm は有鉛 HASL 不可。無鉛 HASL か ENIG） |
| 最小ビア | 0.3mm/(0.4/0.45mm)（基板のビアは穴 0.3 / 外径 0.5 mm。2 層で追加料金なし） |
| ステンシル | 表面のみ（Top only）。裏面は BT1 のスルーホールだけ |

## 部品の送り方

- LCSC の部品は、LCSC の購入画面で JLCPCB の注文と同梱にできる（同じ通貨・同じアカウント。同梱後は分けられない）。https://www.lcsc.com/faqs/ordering-quotation/combine-lcsc-jlcpcb-orders
- BC15M、NAND（F35UQA512M-WWR）、U7（SN74AUP3G34DCUR）、マイク（T5848）は LCSC に無い（U7 は LCSC で見つからなかった）。JLCPCB の Global Sourcing は実装注文専用で、部品だけを送ってはもらえない。DigiKey JP（¥6,000 以上で送料無料）か Mouser JP から直接買う。

## 見積（5 枚分、2026-10-18、$1 = ¥158）

部品は DigiKey JP を主にし、DigiKey に在庫の無い 3 点だけ LCSC にして JLCPCB と同梱する（LCSC の 3 点はどれも 5 個から買える）。

| 注文先 | 中身 | 金額 |
|---|---|---:|
| JLCPCB | 基板 5 枚 $4.10、ステンシル $7.22、OCS Express の送料 約 $7.23（日本向け。0.6 kg 以内なら OCS NEP $4.51） | 約 $18.6（¥2,900） |
| LCSC（JLCPCB と同梱） | `order_lcsc.csv`: TPS62840、電池ホルダ、C1（0805 100 µF） | $22.39（¥3,540） |
| DigiKey JP | `order_digikey.csv`: 残り 14 種（¥6,000 以上で送料無料。税抜） | $91.51（¥14,460） |
| 合計 | | 約 ¥20,900（DigiKey の消費税込みで約 ¥22,300） |

- `order_mouser.csv` は Mouser でそろえる場合の比較（マイク以外で ¥12,300、税抜。T5848 の Mouser の価格は未確認）。Mouser には NAND（F35UQA512M-WWR）が無いので、NAND だけ DigiKey になり、DigiKey 側が ¥6,000 未満で送料 ¥2,000 がかかる。DigiKey 主の方が安い。
- DigiKey と Mouser の在庫切れの部品は、同じ仕様の在庫品に替えた（4.7 µF は Samsung CL05A475MP7NRB8、10 µF は KYOCERA AVX 04026D106MAT2A / KGM05CR50J106MH）。
- マイク T5848 は DigiKey JP に在庫あり（15,115 個、1 個 ¥471、10 個以上 ¥358.8。2026-10-18）。U7 SN74AUP3G34DCUR は DigiKey JP 3,657 個、1 個 ¥69。
- 各 CSV は DigiKey の BOM Manager、Mouser の BOM ツール、LCSC の BOM Tool にそのまま上げられる（品番と数量の列）。価格は上げたときの値が正。
