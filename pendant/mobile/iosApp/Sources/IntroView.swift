import SwiftUI

// 起動アニメーション: 金具とリング → A が下から伸びて頂点で出会う → 表示灯が灯る → 文字。
// 座標は pendant/brand/anam-mark.svg と同じ 100×120（Android の Intro.kt と同じ動き）。

private let gold = Color(red: 0.784, green: 0.690, blue: 0.541)
private let face = Color(red: 0.090, green: 0.094, blue: 0.106)
private let ivory = Color(red: 0.929, green: 0.902, blue: 0.847)
let anamBg = Color("LaunchBackground")

private struct Seg: Shape {
    let pts: [CGPoint]
    func path(in r: CGRect) -> Path {
        let s = r.width / 100
        return Path { p in p.addLines(pts.map { CGPoint(x: $0.x * s, y: $0.y * s) }) }
    }
}

private struct Ring: Shape {
    func path(in r: CGRect) -> Path {
        let s = r.width / 100
        return Path { $0.addArc(center: CGPoint(x: 50 * s, y: 68 * s), radius: 40 * s, startAngle: .degrees(-90), endAngle: .degrees(270), clockwise: false) }
    }
}

private struct Bail: Shape {
    func path(in r: CGRect) -> Path {
        let s = r.width / 100
        return Path(roundedRect: CGRect(x: 44 * s, y: 6 * s, width: 12 * s, height: 24 * s), cornerRadius: 4 * s)
    }
}

struct IntroView: View {
    let done: () -> Void
    @State private var ring: CGFloat = 0
    @State private var a: CGFloat = 0
    @State private var dot = 0.0
    @State private var word = 0.0
    private let w: CGFloat = 110

    var body: some View {
        ZStack {
            anamBg.ignoresSafeArea()
            VStack(spacing: 0) {
                ZStack {
                    Bail().stroke(gold, lineWidth: 1.6 * w / 100).opacity(ring)
                    Ring().fill(face).opacity(ring)
                    Seg(pts: [CGPoint(x: 25, y: 97), CGPoint(x: 50, y: 32)]).trim(from: 0, to: a).stroke(gold, lineWidth: 1.8 * w / 100)
                    Seg(pts: [CGPoint(x: 75, y: 97), CGPoint(x: 50, y: 32)]).trim(from: 0, to: a).stroke(gold, lineWidth: 1.8 * w / 100)
                    Ring().trim(from: 0, to: ring).stroke(gold, lineWidth: 2 * w / 100)
                    Circle().fill(.white).frame(width: 7.2 * w / 100, height: 7.2 * w / 100).position(x: 50 * w / 100, y: 86 * w / 100).opacity(dot)
                }
                .frame(width: w, height: w * 1.2)
                // ponytail: 文字は端末の serif。Cormorant Garamond を同梱するなら Info.plist の UIAppFonts に足して差し替える
                Text("anam").font(.system(size: 44, weight: .light, design: .serif)).tracking(4).foregroundColor(ivory).padding(.top, 22).opacity(word)
                Text("THE LIVING ANAMNESIS").font(.system(size: 11, design: .serif)).tracking(5).foregroundColor(gold).padding(.top, 10)
                    .opacity(word)
            }
        }
        .task {
            withAnimation(.easeInOut(duration: 0.45)) { ring = 1 }
            try? await Task.sleep(for: .milliseconds(450))
            withAnimation(.easeOut(duration: 0.45)) { a = 1 }
            try? await Task.sleep(for: .milliseconds(450))
            withAnimation(.easeIn(duration: 0.25)) { dot = 1 }
            try? await Task.sleep(for: .milliseconds(250))
            withAnimation(.easeIn(duration: 0.4)) { word = 1 }
            try? await Task.sleep(for: .milliseconds(900))
            done()
        }
    }
}
