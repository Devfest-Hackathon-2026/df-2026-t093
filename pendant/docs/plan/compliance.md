# 認証と販売の手続き（技適・SIG・CE・FCC・UK）と問い合わせ

この文書が持つもの: 技適の確認手順と結果、Bluetooth SIG の料金と規則、EU RED・UK・FCC の調査結果、認証戦略の順位、XIAO キットを外した理由、SIG・Seeed・試験所への問い合わせ文面と回答記録、表記・商標のチェック表。
脅威モデルと暗号化は [security.md](../spec/security.md)、TX 出力とアンテナは [rf.md](../spec/rf.md)、試作の順序は [prototype.md](./prototype.md) が持つ。

**この文書は法的な判断ではない。** web 調査（EU RED Guide、47 CFR、SIG の QPRD v5 と FAQ、各社の FCC grant、総務省の検索）で読んだ内容を整理したもので、**法的な点はすべて「要書面確認」**。SIG・Seeed・試験所からの書面の回答（§7）が出るまで、確定として扱わない。

確度の表記:

- **確認済み**: 一次資料または公的な検索で確かめた事実。
- **要書面確認**: web 調査の読みで、法的な確定ではない。SIG・試験所・Seeed・専門家の書面の回答を待つ。
- **方針**: 向きは決まっているが、数値や手続きが決まっていない。
- **概算**: 調査の目安で、見積ではない。

## §1 前提

| 項目 | 内容 | 確度 |
|---|---|---|
| 販売地域 | 日本と海外。モジュールが技適・FCC・CE などを取得済みであることが条件 | 確定 |
| 販売形態 | **SIG と試験所から回答が来るまで保留する。** その間は予約・販売・Bluetooth の名称とロゴの使用をしない（§7.5） | 確定（保留） |
| モジュール | nRF54L15 に固定。dogfooding と完成品の候補は Fanstel BC15M（決定 X3）。予備は u-blox NORA-B206（§4.4） | 確定 |
| XIAO nRF54L15 | 開発専用。キット・配布物に載せない（§6） | 確定 |
| Bluetooth の構成 | Standard と Premium で同じにする（同じコントローラ・ホスト・機能。差は電源・保存・筐体だけ）。SIG の 2 つ目の Declaration を $0 にするため | 確定 |
| TX 出力 | +4 dBm（アンテナ利得込み、EIRP）以下に固定する。SAR 試験の免除を見込む（§4.5） | 確定（免除の可否は要書面確認） |
| 知人への無償配布 | SIG への問い合わせには入れない。配布の扱い（SIG・CE・FCC 上どう見るか）は未確認 | 要書面確認 |

## §2 技適（日本、issue #14）

### §2.1 モジュールの工事設計認証番号

総務省の検索で番号が出ることを確認した（番号の照合の画面の保存は担当者。§2.3）。

| モジュール | 工事設計認証番号 | 備考 |
|---|---|---|
| Fanstel BC15M | 201-250527 | dogfooding の本筋（決定 X3）。Kiwa が 2025-07-17 に発行。付属書 3 に BC15C・BC15E・BC15M・BC15P。証明書で確認した。総務省の検索での確認は未実施（§2.3） |
| Raytac AN54LQ-15 | 201-250520 | 不採用（基板が大きい）。比較のために残す |
| u-blox NORA-B206 | 003-250204 | 予備 |
| XIAO nRF54L15（Sense を含む） | 217-252398（2025-07-24） | 開発専用。認証を受けた者は Seeed |
| Ezurio BL54L15μ | 201-250075 | |
| Fanstel BM15C/M | 201-250017 | BC15M とは別の番号。取り違えない |
| Raytac AN54LV（6.4 × 8.4 mm） | 201-250932 | |
| KAGA FEI ES4L15BA1 | **検索に出ない** | 候補から外す |
| Murata LBMA0ZZ2NR、Insight SiP ISP2454 | 番号は確認したが、この文書には記録していない | |

- 技適は、認証済みのモジュールを改造せずに使う限り、完成品で追加の試験は要らない見込み（要書面確認）。
- 「技適取得済み」と書くのは、§2.3 の確認が済んでから。認証を受けた者はモジュールの製造者で、当方ではない。

### §2.2 確認手順（担当者が行う）

総務省の技術基準適合証明等の検索ページ: <https://www.tele.soumu.go.jp/giteki/SearchServlet?pageID=js01>（トップは <https://www.tele.soumu.go.jp/j/giteki/navi/index.htm>）。**直接 URL の `pageID=jg01_01` は「システムエラー」になるので、上の `js01` から入る。**

検索フォームの項目: 技術基準適合証明番号、工事設計認証番号（新表記 / 旧表記）、技術基準適合自己確認の届出番号、氏名又は名称、型式又は名称、機関名、年月日（From / To）、特定無線設備の種別（必須）。

手順:

1. 「工事設計認証番号（新表記）」に、確認する番号（§2.1）を入れ、特定無線設備の種別を「全ての特定無線設備」にして検索する。
2. 結果の **認証番号・型式又は名称・氏名又は名称（認証を受けた者）・認証日・周波数（2402〜2480 MHz）** を、§2.1 の記載と照合する。
3. 画面を、日付と URL が分かる形で保存する。**保存先は社内の共有領域。リポジトリには置かない。**
4. 型式又は名称が、実際に使う個体（Fanstel BC15M。XIAO は nRF54L15 と Sense の両方を含むか）と一致するか確認し、個体の基板表示（型番・技適マーク）と照合する。同じメーカーの別シリーズ（BM15C/M は 201-250017）と取り違えない。201-250527 の付属書 3 には BC15C・BC15E・BC15M・BC15P があるので、個体が BC15M であることを確かめる。
5. 一致しない項目があれば、§2.1 を直す。

### §2.3 確認の記録

| 番号 | 対象 | 検索で出ること | 画面の保存日 | 照合した項目（型式・認証者・認証日・周波数） | 保存先 |
|---|---|---|---|---|---|
| 217-252398 | XIAO nRF54L15 / Sense | 確認済み（2026-10-14。Sense を含む） | | | |
| 201-250527 | Fanstel BC15M | 未確認（証明書では確認済み。検索は担当者） | | | |
| 201-250520 | Raytac AN54LQ-15 | 確認済み（2026-10-14） | | | |
| 003-250204 | u-blox NORA-B206 | 確認済み（2026-10-14） | | | |

- 画面の保存と、表の空欄の記入は担当者の作業（#14 が閉じる条件）。

## §3 Bluetooth SIG（issue #15）

### §3.1 規則（要書面確認）

| 内容 | 出典 | 確度 |
|---|---|---|
| 自社の名前で売る製品は「Product」で、自社の qualification（Declaration）が要る。**lifeOS の名前で売るなら、モジュールを別部品にしても、自社の Declaration ID が要る可能性が高い** | QPRD v5（2026-04-21）§1.1 | 要書面確認（SIG への質問 1） |
| 他社がその Product の qualification を代わりに完了することはできない。FAQ が免除するのは、自社ブランドを付けない再販業者だけ | QPRD v5 §2、SIG FAQ | 要書面確認 |
| 同じ会員が同じ Design を別の製品に使う場合、追加の Declaration 料金は $0。名前・外装・Bluetooth 以外の機能は、Design を変えずに変更できる。**Standard と Premium で Bluetooth の構成を同じにすれば、2 つ目は無料になる** | QPRD v5 §3.2.1 | 要書面確認（SIG への質問 2） |
| 認証済みモジュール（Nordic の QDID）を組み合わせて、Declaration 1 件で足りる見込み。BC15M が参照する Bluetooth の DN は Q382503 / Q370633 / Q370860 で、BC15M に当たるのがどれかは未確認 | 調査の読み | 要書面確認 |
| 予約の受付が「売る・配る」に当たるか（qualification の期限） | — | 要書面確認（SIG への質問 3） |
| Bluetooth の名称・ロゴは、SIG の回答が出るまで使わない | — | 方針 |

### §3.2 料金（2026-03-01 改定の料金表、要書面確認）

| 項目 | 金額 |
|---|---|
| Adopter（年会費なし）の Declaration | $12,000 |
| Contributing Adopter（small）の年会費 | $3,500 |
| Contributing Adopter（small）の Declaration | 各会員年度の 1 件目が $8,000、2 件目以降は $12,000 |
| 1 件だけの合計の下限 | $11,500〜$12,000 |
| 同じ会員が同じ Design を別の製品に使う場合 | 追加 $0 |
| 小規模向けの Innovation Program | 2020 年に終わったと報告されている（現行の料金表にも無い） |

- 「small」の基準（年間売上の閾値など）と、初回会員年度に $8,000 が使えるかは未確認（SIG への質問 4・5）。

## §4 地域別の調査結果

### §4.1 EU（RED、要書面確認）

| 内容 | 出典 |
|---|---|
| 説明どおりに組み立てたキットの適合責任は、キットの製造者にある | RED Guide §1.7.4.10 |
| RF モジュールそのものが無線機器とされる | RED Guide §1.7.4.11 |
| 出荷できる個体の注文が確定した時点で上市になる（予約の扱いの根拠） | RED Guide §1.3 |
| 汎用の評価キットは Annex I.4 が除外している | RED Guide |
| 無線なしの carrier だけを売る場合も、carrier は EMC 指令の対象になる | EMC Guide 2025 草案 §1.5.3 |
| RoHS、WEEE、電池規則、GPSR は残る | — |
| 完成品の道筋: Module A の自己宣言。モジュールの試験報告を流用できる。サイバーは EN 18031、RF 曝露は EN 50663 | 調査の読み |

- 完成品で試験所に確認する範囲: EN 301 489、EN 300 328（部分試験で足りるか）、EN 62368-1、EN 18031。
- **EN 18031 の制限条項:** 利用者がパスワードを設定しなくても使える設計だと、Notified Body が要る場合がある。初期パスワードが無く、BLE の LESC + OOB でペアリングする設計（[security.md §3](../spec/security.md)）がどう扱われるかは、試験所に確認する（要書面確認）。

### §4.2 UK（要書面確認）

- CE の書類が使える見込み。加えて PSTI（初期パスワードの禁止、脆弱性の窓口、サポート期間の明示）。
- UKCA の DoC の有無は Seeed への質問 3 と試験所への質問 1 で聞く。

### §4.3 米国（FCC、要書面確認）

| 内容 | 出典 |
|---|---|
| §15.3(p) はキットを「別の部品を足して完成させるもの」も含めて定義する。§15.23 の自作機器の免除はキットを除外している。**組み上がった機器には Part 15 がかかる** | 47 CFR 15.3(p)、15.23 |
| Raytac・Ezurio・Fanstel（BM15C/M）・KAGA の grant には「OEM の統合者に売り、OEM が組み込む場合に限る」という条件がある。購入者が自分で組み込む方式とぶつかる可能性がある（KDB 996369 を TCB に確認する）。BC15M の grant（X8WBC15、Single Modular、2025-07-25）の条件は未確認 | 各 grant、KDB 996369 |
| 完成品の道筋: 15B の SDoC と「Contains FCC ID」の表示。モジュール統合の確認は KDB 996369 D04 | 調査の読み |
| 認証前の予約は、「認証が取れなければ返金」と明示し、認証前は渡さない | 47 CFR 2.803(c)(2)(i) |
| 開発者限定の評価キットは、表示を付けた形でだけ許される | 47 CFR 2.803(c)(2)(iv) |
| モジュールの FCC 認証が「人体から 20 cm 以上」に限られる場合、Class II の変更が要る | 各 grant |

### §4.4 モジュールの比較（認証の面。価格・在庫は issue #14 のログ）

| | **Fanstel BC15M（選定、X3）** | Raytac AN54LQ-15 | u-blox NORA-B206 | KAGA FEI ES4L15BA1 | Ezurio BL54L15μ | Fanstel BM15C/M | XIAO nRF54L15 |
|---|---|---|---|---|---|---|---|
| 技適 | 201-250527 | 201-250520 | 003-250204 | **検索に無い** | 201-250075 | 201-250017 | 217-252398 |
| FCC | X8WBC15（Single Modular、2025-07-25。RF 曝露の報告は携帯機器として人体から 5 mm で SAR 試験の免除、2.82 ≤ 3.0）。ISED 4100A-BC15 | SH6AN54LQ（Class II の変更で身に付ける用途も可と記載） | 未確認 | 2A6NFES4L15（人体から 7 mm 以上） | SQG-BL54L15U（Class II の変更で身に付ける用途も可と記載） | X8WBM15C（**人体から 20 cm 以上**。ペンダントに不向き） | Z4T-XIAONRF54L15 |
| CE | RED の試験報告あり（EU DoC は未確認） | 記載のみ | 未確認 | 記載なし | EU DoC あり | 記載のみ | 未確認 |
| その他 | NCC、SRRC、RCM | | | | | | |
| 実装 | LGA（36 パッド） | LGA | 未確認 | LGA | LGA | LGA | キャスタレーション |

- FCC の記載は要書面確認（BC15M の grant の RF 曝露の報告や、Raytac の grant の「身に付ける用途も可」が当方の使い方に足りるかは、試験所への質問 4 で聞く）。
- Ezurio は在庫が無い時期がある。Fanstel の BM15C/M は FCC の条件（人体から 20 cm 以上）でペンダントに向かない。**これは BM15C/M の話で、BC15M は別の grant（X8WBC15）で身に付ける用途（5 mm）が評価されている。** KAGA は技適が見つからない。BM15C/M・Ezurio・KAGA は候補にしない。
- Raytac AN54LQ-15 は不採用（基板が大きい）。比較のために残す。
- モジュールを選ぶ条件: nRF54L15 であること、技適・FCC・CE を取得済みであること、SIG のリスト済みの設計を流用できること。32 kHz 水晶と DC/DC インダクタがモジュールに載っているか（BC15M は両方内蔵）、寸法は [prototype.md §3.1](./prototype.md)・[mechanical.md](../spec/mechanical.md)。

### §4.5 TX 出力と SAR（要書面確認）

- **送信出力を +4 dBm 以下（アンテナ利得込み）に固定すると、FCC の SAR 試験を免除できる見込み**（47 CFR 1.1307(b)(3)。2.45 GHz・人体から 5 mm で約 2.7 mW）。
- EN 50663 も 20 mW 以下なら簡易評価で済む見込み。
- ペンダントの BLE は近距離で足りるので両立しやすい。届く距離は Phase 1 で実測して確かめる（[prototype.md §3.4](./prototype.md)）。出力の設計は [rf.md](../spec/rf.md)。

## §5 認証戦略の順位（要書面確認、すべて概算）

SIG と認証の固定費を払う前に、需要を確かめる。

| 順位 | 策 | 残る義務 | 費用の目安（概算） | UX |
|---|---|---|---|---|
| ① | **予約だけ受けて、需要を確かめてから認証する**（他の策の前に置く） | US は 47 CFR 2.803(c)(2)(i)：「認証が取れなければ返金」と明示し、認証前は渡さない。EU は、出荷できる個体の注文が確定した時点で上市になる（RED Guide §1.3）。SIG で予約が「販売開始」に当たるかは書面で確認する | 文言と記録だけ | 納期は「認証後」 |
| ② | **自社の完成品として 1 回認証し、2 つの SKU で共用する**（本命） | SIG は Nordic の QDID を組み合わせて 1 件。EU / UK は Module A の自己宣言（モジュールの試験報告を流用。EN 18031 のサイバー、EN 50663 の RF 評価。UK は PSTI）。US は 15B の SDoC と「Contains FCC ID」の表示。JP は技適モジュールを無改造で使うので追加試験なし | SIG 約 $12k、EU / UK 試験 約 €8〜20k、US 約 $2〜5k（モジュールが人体から 20 cm の条件なら Class II の変更で +$5〜15k） | 最良 |
| ③ | 無線なしのキャリアと OSS の FW を売り、XIAO は購入者が別に買う | **日本以外では効果が薄い。** US はキットを Part 15 の対象と定義している（15.3(p)。15.23 はキットを除外）。EU は、説明どおりに組んだ状態の適合を売り手が負う（§1.7.4.10）。SIG は、lifeOS の名前で売ると自社の Bluetooth 製品とみなされる恐れがある | 日本だけならほぼ 0 | 悪い（購入者が部品を買い、書込み、組み立てる） |
| ④ | 開発者向けの評価キット（B2B、US 中心） | US は 2.803(c)(2)(iv)（開発者限定、表示が要る）。EU の Annex I.4 は汎用の評価キットを除外している。SIG に免除規定は無い | 小 | 一般向けには売れない |

進め方（方針）:

1. SIG と試験所の回答（§7）を受ける。**予約の開始は、SIG への質問 3（予約が売る・配るに当たるか）と試験所への回答が出たあと。**
2. 回答を踏まえて ①（予約）を始め、需要を確かめる。
3. 需要が確かめられたら ②（完成品を 1 回認証し、Standard と Premium で共用）に進む。Bluetooth の構成が同じなので、SIG の 2 つ目は $0 の見込み（§3.1）。
4. ③ は、SIG が「無線を含まないキットの売り手や OSS の FW の公開者は qualification の対象外」と書面で回答した場合にだけ候補に戻る（§6）。④ は一般向けの販売には使わない。

## §6 XIAO キットを外した理由

XIAO nRF54L15 は開発専用にする。キット・配布物には載せない（確定）。

- **決め手は書込みの経路。** nRF54L15 自体に USB が無く、XIAO の USB-C は SAMD11（CMSIS-DAP と UART のブリッジ）につながっている。UF2 のように「USB ドライブにコピーするだけ」で書ける形を作れない。iOS には USB で書く経路が無い。利用者の更新は、出荷前に MCUboot を書き、BLE で DFU する形にする（[firmware.md §2.3](../spec/firmware.md)）。それは当方が書いて出荷することになり、「未書込みの別部品」という前提と両立しない。
- **規制の狙いが日本以外で成り立たない可能性が高い。** XIAO を別部品にすれば SIG の Declaration と認証の固定費を払わずに済む、という狙いだった。SIG（lifeOS の名前で売れば自社の Bluetooth 製品）、EU（キットの適合責任は製造者、RED Guide §1.7.4.10）、FCC（キットは Part 15 の対象、15.3(p)・15.23）のいずれでも、日本以外では効果が薄い（要書面確認）。
- 狙いが外れると、XIAO を使う不利だけが残る: 3.3 V 系で 1.8 V の周辺にレベル変換が要る、厚さ（Standard の想定 16.8 mm、全層を最小にしても 14.9 mm で、AirTag 互換が成り立たない）、側面 14 接点を余りなしで使い電池電圧測定・privacy latch を足せない、電池は 5 V ピンから入れるので USB と同時につなげない手順を購入者に守らせる。
- **日本だけで売る場合は規制の面でいちばん軽い**（技適 217-252398 は確認済み）。次のどれかが成り立てば XIAO キットを選び直せる。どれも §7 の回答が前提（要書面確認）。
  1. SIG が書面で、無線を含まないキットの売り手と OSS の FW の公開者は qualification の対象外と回答する（SIG への質問 1）。
  2. まず日本だけで、メーカー層向けに少数を売る。海外は完成品の認証（§5 ②）を取ってから。
  3. SIG と CE の費用を払う前提で、XIAO を「はんだ付けの要らない完成品の部品」として使う。XIAO の FCC 認証の種類と、身に付ける用途の条件の確認が先（Seeed への質問 1）。

## §7 問い合わせ

下書きだけで、**何も送っていない。** 送付は担当者が行う。法的な判断は含まない。販売形態は回答が来るまで保留（§1）なので、**2 つの構成を並べて聞く。** 先に予約を受けて需要を確かめる案（§5 ①）も一緒に聞く。

### §7.1 聞く構成

```text
構成 A: 完成品（§5 ②、本命）
  lifeOS Pendant（Standard / Premium）
   └ 当方が認証済みのモジュール（Fanstel BC15M または u-blox NORA-B206）を実装した PCB
      ・マイク / NAND / 電源、ケース、電池
   FW: 当方が開発し、出荷前に MCUboot を書く。以降はスマホから BLE で DFU
   Bluetooth の構成は Standard / Premium で同じ。送信出力は +4 dBm（EIRP）以下に固定

構成 B: 無線なしのキット（§5 ③）
  当方が売るもの: carrier PCB（マイク / NAND / 電源、無線なし）、ケース、電池
  購入者が別に買うもの: Seeed Studio XIAO nRF54L15（未改造。Seeed などの販売店から）
  FW: 当方がオープンソースで公開し、購入者が書く
  BLE を動かすのは XIAO 上の nRF54L15（Nordic のスタック）。carrier は無線に関与しない
```

### §7.2 Bluetooth SIG への質問（英語、送付は担当者）

送付先は SIG の qualification サポート（<https://support.bluetooth.com/>）。

> We are a small company developing a wearable BLE voice logger ("lifeOS Pendant") based on Nordic nRF54L15. We would like to confirm our qualification obligations before we start selling. We are considering two configurations:
>
> (A) A finished product under our brand, built on a pre-qualified, regulatory-certified module (e.g. Fanstel BC15M or u-blox NORA-B206), in two variants (Standard and Premium) that use an identical Bluetooth design (same controller and host, same feature set) and differ only in power source, storage and enclosure.
>
> (B) A radio-less kit: we sell only a carrier PCB (microphone, flash, power circuitry, no radio), an enclosure and a battery. The buyer purchases an unmodified Seeed Studio XIAO nRF54L15 separately from a third-party distributor and flashes open-source firmware that we publish.
>
> Questions:
> 1. In configuration (B), are we, as the seller of a radio-less kit and the publisher of open-source firmware, required to become a member and complete qualification? Does the answer change if we market the result as "lifeOS Pendant", or if we mention Bluetooth in our product description?
> 2. In configuration (A), if both variants contain the same Design, can the second Product be listed under the same Design at no additional Declaration fee (QPRD v5 §3.2.1 / Option 1)? Which differences (power source, storage, enclosure, product name) keep the Design unchanged?
> 3. Does taking pre-orders (no units delivered, full refund if we do not ship) count as "selling or distributing" for the purpose of the qualification deadline?
> 4. How is "small company" defined for the Contributing Adopter small-company fee (e.g. annual revenue threshold), and is the first-Declaration price of US$8,000 in the current fee schedule applicable to a company in its first membership year?
> 5. Is there any current fee reduction for start-ups or for low-volume products?
>
> Thank you. A written answer we can keep on file would be very helpful.

### §7.3 Seeed への質問（英語、送付は担当者）

> We are evaluating the XIAO nRF54L15 (and the Sense version) for a product. Could you tell us:
> 1. The FCC ID and the type of grant. Is it a single modular approval for Z4T-XIAONRF54L15, and does Z4T-NRF54L15 also apply? Which one covers current production units, and are portable (body-worn) use conditions covered?
> 2. The Bluetooth SIG Declaration ID / QDID for the XIAO nRF54L15, and whether it is listed as an End Product or a Component.
> 3. The EU Declaration of Conformity (RED) and the UKCA DoC, and whether the board is placed on the market as finished radio equipment or as a development/evaluation kit.
> 4. Whether the MIC certification 217-252398 covers both XIAO nRF54L15 and XIAO nRF54L15 Sense, and which marking appears on the board.

### §7.4 試験所（日本の EMC / 無線の試験所）への質問（日本語、送付は担当者）

1. 構成 A で EU / UK / US に出す場合、必要な試験と書類の範囲を知りたい。対象は EN 301 489、EN 300 328（部分試験で足りるか）、EN 62368-1、EN 18031、FCC 15B の SDoC、KDB 996369 D04 の統合確認。モジュールの試験報告を流用する前提での費用と期間も知りたい。
2. 送信出力を +4 dBm（EIRP）以下に固定し、身体に付ける機器（人体から 5 mm 程度）として使う。この条件で、FCC の SAR 免除（47 CFR 1.1307(b)(3)）と EN 50663 の簡易評価で足りるか。
3. EN 18031 の制限条項について知りたい。初期パスワードが無く、BLE の LESC + OOB（スマホアプリが鍵を受け取る）でペアリングする設計が、Notified Body が必要な場合に当たるか。
4. モジュールの FCC 認証が「人体から 20 cm 以上」に限られる場合、Class II の変更にかかる費用と期間。
5. 構成 B（無線なしの carrier を売り、購入者が XIAO を足す）で、当方に残る EU（EMC 指令・RED §1.7.4.10）と US（Part 15 のキット）の義務。

### §7.5 回答の記録

| 宛先 | 送付日 | 回答日 | 回答者 | 要旨 | 書面の保存先 |
|---|---|---|---|---|---|
| Bluetooth SIG | | | | | |
| Seeed | | | | | |
| 試験所 | | | | | |

**回答が揃うまで、販売・予約・Bluetooth の名称やロゴの使用はしない。** 回答を受けて販売形態（§1）を決める。

### §7.6 回答で決まること

| 質問 | 決まること |
|---|---|
| SIG 1 | 構成 B（無線なしのキット）と XIAO キットが選べるか（§6） |
| SIG 2 | Standard と Premium で 2 つ目の Declaration が $0 になる条件（Design を変えない差の範囲） |
| SIG 3、試験所 1・5 | 予約を始めてよい時期と文言（§5 ①） |
| SIG 4・5 | SIG の費用の見積（§3.2） |
| 試験所 1 | EU / UK / US の試験の範囲・費用・期間（§5 ②） |
| 試験所 2 | +4 dBm での SAR・RF 評価の簡略化（§4.5） |
| 試験所 3 | EN 18031 で Notified Body が要るか（§4.1） |
| 試験所 4、Seeed 1 | FCC の Class II の費用（§4.3）、XIAO の FCC の種類 |
| Seeed 2〜4 | XIAO の SIG 登録・EU / UK の位置づけ・技適の対象範囲 |

## §8 表記と商標のチェック

販売品名・広告文・販売資料の表現のチェック表。**確認は担当者。**

| 語・項目 | 扱い |
|---|---|
| AirTag / Apple / Find My | 「対応」「互換」「Apple 純正」と読める表現を使わない。外形の比較に使う場合は「AirTag と同程度のサイズ」までで、商標表記を添える |
| Minew / MOKO（既製ケースのメーカー名） | 販売資料に載せる場合の扱いは、メーカーへの問い合わせ時に確認する（[prototype.md §6.4](./prototype.md)） |
| 「技適取得済み」 | §2.3 の確認（#14）が閉じるまで書かない。認証者はモジュールの製造者で、当方ではない |
| Bluetooth（ロゴ・名称） | SIG の回答（#15）が出るまで、ロゴを使わない |
| 防水（IP 等級） | 既製ケースの等級は**ケース単体**の値。当方の PCB・ボタン・音孔を足した製品の等級ではない |
| 常時録音であることの表示 | 筐体・箱・説明書に書く。privacy latch は「録音の一時停止」として見せる（前面ボタンによる FW の一時停止は入れない） |
| 販売表示と規約 | 日本と海外の法的確認を専門家から取る（要書面確認） |

商標の確認（特許庁の J-PlatPat など）: 「lifeOS」「Pendant」の称呼で、類似商標が無いかを担当者が検索する。**この文書では検索していない。**

## 未確定

| 項目 | 状態 | issue |
|---|---|---|
| 技適の原本確認（画面の保存と照合の記録、§2.3。BC15M の 201-250527 の総務省の検索を含む） | 担当者の作業 | #14 |
| BC15M の EU DoC（CE）、Bluetooth の DN（Q382503 / Q370633 / Q370860 のどれが BC15M か）、FCC grant の OEM 条件 | 未確認 | #15 |
| Bluetooth SIG への質問の送付と回答（§7.2） | 未送付 | #15 |
| Seeed への質問の送付と回答（§7.3） | 未送付 | #15 |
| 試験所への質問の送付と回答（§7.4） | 未送付 | #15（試験所分は 新規 issue 候補: 試験所への問い合わせ） |
| Bluetooth 向けの構成図の作成（構成 A と構成 B を図にする） | 未着手 | #15 |
| 既製ケースの問い合わせ（[prototype.md §6.4](./prototype.md)） | 未送付 | #16 |
| 商標（「lifeOS」「Pendant」）の検索 | 未実施 | #16 |
| 販売形態（構成 A / B、予約の開始時期） | 回答待ち | #15 |
| 完成品の技適表示（モジュールの技適マークを製品の外面に出す要否）。資料に記載がないので、質問に足すか担当者が決める | 要書面確認 | 新規 issue 候補: 完成品の技適表示の確認 |
| 知人への無償配布の扱い | 問い合わせに入れていない | #15 |
