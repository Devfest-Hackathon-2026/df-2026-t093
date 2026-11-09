# Premium 評価用裸基板の発注データ

2026-11-06 に KiCad 10.0.5 から出力した **評価用試作基板**。実セルの接続、24 時間動作、筐体への収納、RF および実装の成立を保証するものではない。[PCB の検査と残課題](../PCB_DRAFT.md) と [回路の電源保護条件](../README.md) を併読する。基板または設計ルールを編集した場合、この ZIP を再出力する。

- [premium.zip](premium.zip)：JLCPCB の裸基板用。表裏の銅・レジスト・シルク、外形、めっき穴 PTH と非めっき穴 NPTH の計 9 ファイル。JLCONE が ZIP 名の `eval` を禁止するため、外側の ZIP 名を変更した。中身は変更していない。
- [gerbers/](gerbers/)：ZIP と同じ個別ファイル。Gerber X2・Protel 拡張子、Excellon は mm・絶対原点・小数形式。シルクはソルダーマスク開口から除外している。
- 基板 SHA-256：`c6f1bb39f40da328f4f63908ae103066d50be8a8f14ce433b6230a26ebf60ee0`。ZIP SHA-256：`0fe08be29dabede2051cd4705830f490507f4f9bc84ce35c5c00e0c6717dfd3f`。
- ステンシル用の Paste 層、JLC 実装用 BOM・座標は ZIP に含めていない。**Q1/Q2 の F.Paste は未定義**で、現状の設計から作るステンシルにはこの 2 部品の開口がない。0.5 mm ピッチの DSBGA を自宅で接合する方法と検査方法を、ステンシル注文・組立前に決める。

## JLCPCB で指定する条件

| 項目 | この設計の条件 |
|---|---|
| 基材・層数 | FR-4、2 層、外形 φ39 mm |
| 板厚 | 0.6 mm。公表される 1.0 mm 未満の厚さ公差は ±0.1 mm |
| 銅厚 | **1 oz**。銅間隔に 0.10 mm を含み、2 oz の公表最小間隔 0.16 mm は満たさない |
| 表面処理 | **ENIG を選択**。Q1/Q2 は 0.25 mm 角の小さな DSBGA パッド。[JLCPCB は直径 0.20〜0.25 mm の BGA パッドに ENIG を要求](https://jlcpcb.com/jp/capabilities/pcb-capabilities)しており、角丸四角の本パッドの扱いと価格・納期は見積画面で確認 |
| レジスト／ビア | 設計上のビアは両面テントを基本とする。パッド内ビアは開口と重なる箇所があり、樹脂埋めではない。アップロード後のプレビューでパッド・穴の処理を確認 |
| 組立 | 自宅で SMD を F 面に実装。J2 の Vina と J3 のホルダーは B 面に配置し、各スルーホール足を F 面からはんだ付けする |

板厚、銅厚および表面処理は [JLCPCB の製造能力](https://jlcpcb.com/capabilities/Capabilities?type=1)、[銅厚と最小線幅・間隔](https://jlcpcb.com/help/article/jlcpcb-copper-weight)、[表面処理](https://jlcpcb.com/help/article/jlcpcb-surface-finish) の公表値に基づく。最終的な加工可否と納期は注文画面で確認する。[KiCad の Gerber・ドリル出力案内](https://jlcpcb.com/help/article/how-to-generate-gerber-and-drill-files-in-kicad-9) は PTH と NPTH の分割を許容している。

## 検査と未確定事項

- 保存基板を再充填して KiCad DRC：**未接続 0、エラー 0、回路図との不一致 0**。警告 200 件の大半はシルクの重なり・銅上・外周・文字高で、行き止まりのビアと短い線も各 1 件残る。シルクは重なりや欠けがあり、部品識別には KiCad の配置を参照する。
- 回路図 ERC：LM66100 の未使用 ST を GND につなぐ箇所に既知の 2 エラー。[TI の指示](https://www.ti.com/lit/ds/symlink/lm66100.pdf)に従った接続と KiCad のピン型の食い違いであり、ERC を無視して安全性を保証したものではない。重要ネット 79 件の別チェックと抵抗値計算は通過した。
- Gerber の 7 層は別のパーサで読めた。ドリルを基板と照合し、PTH **213 穴**、NPTH **1 穴**を確認。NPTH はマイクの φ0.6 mm 音孔。表裏の銅・シルクと穴の重ね表示も確認したが、JLCPCB 側の DFM・見積もり・プレビューは未実施。
- ユーザーは CH005 と Vina の現物について、極性・穴位置・リードの曲げを確認済みと回答。個別の測定値は未記録のため、基板の穴・極性が確認結果と一致する前提で使う。ケース内干渉と電池ホルダーの絶縁は未検証。
- C8 は **1210 の仮パッドで容量未選定**。U2 の外周ランドは TI 例より広く、Q1/Q2 の実装と NAND の露出パッドも未評価。電池側の逆充電・過渡・起動・切替・24 時間は未検証で、**実セルや Vina を接続する前に模擬源で検証する**。

## 再出力

リポジトリルートから、対象基板が確定していることを確認して実行する。ZIP は `gerbers/` 内の 9 ファイルだけを固め、ステンシルやドリルマップを混ぜない。

```sh
kicad-cli pcb export gerbers \
  --layers F.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,Edge.Cuts \
  --subtract-soldermask --precision 6 --check-zones \
  --output hw/premium/fab/gerbers/ hw/premium/premium_power.kicad_pcb
kicad-cli pcb export drill --format excellon --excellon-separate-th \
  --excellon-units mm --drill-origin absolute --excellon-zeros-format decimal \
  --excellon-oval-format alternate --output hw/premium/fab/gerbers/ \
  hw/premium/premium_power.kicad_pcb
```

KiCad が作る `*-job.gbrjob` はこの ZIP に含めていない。ZIP 生成前に出力先の古いファイルを除き、DRC と Gerber・穴位置を再検査する。
