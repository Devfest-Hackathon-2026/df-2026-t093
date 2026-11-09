package dev.lifeos.pendant.android

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.geometry.lerp
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

private val Bg = Color(0xFF0E0F11)
private val Gold = Color(0xFFC8B08A)
private val Face = Color(0xFF17181B)
private val Ivory = Color(0xFFEDE6D8)

/**
 * 起動アニメーション: 金具とリング → A が下から伸びて頂点で出会う → 表示灯が灯る → 文字。
 * 座標は pendant/brand/anam-mark.svg と同じ 100×120。
 */
@Composable
fun Intro(onDone: () -> Unit) {
    val draw = remember { Animatable(0f) }
    val dot = remember { Animatable(0f) }
    val word = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        draw.animateTo(1f, tween(900, easing = FastOutSlowInEasing))
        dot.animateTo(1f, tween(250))
        word.animateTo(1f, tween(400))
        delay(500)
        onDone()
    }
    Column(Modifier.fillMaxSize().background(Bg), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
        Canvas(Modifier.size(110.dp, 132.dp)) {
            val s = size.width / 100f
            fun p(x: Float, y: Float) = Offset(x * s, y * s)
            val ring = (draw.value * 2).coerceAtMost(1f)
            val a = (draw.value * 2 - 1).coerceIn(0f, 1f)
            val l = p(25f, 97f); val apex = p(50f, 32f); val r = p(75f, 97f)
            drawRoundRect(Gold, p(44f, 6f), Size(12 * s, 24 * s), CornerRadius(4 * s), alpha = ring, style = Stroke(1.6f * s))
            drawCircle(Face, 40 * s, p(50f, 68f), alpha = ring)
            if (a < 1f) {
                drawLine(Gold, l, lerp(l, apex, a), 1.8f * s)
                drawLine(Gold, r, lerp(r, apex, a), 1.8f * s)
            } else {
                drawPath(Path().apply { moveTo(l.x, l.y); lineTo(apex.x, apex.y); lineTo(r.x, r.y) }, Gold, style = Stroke(1.8f * s, join = StrokeJoin.Miter))
            }
            drawArc(Gold, -90f, 360f * ring, false, p(10f, 28f), Size(80 * s, 80 * s), style = Stroke(2 * s))
            drawCircle(Color.White, 3.6f * s, p(50f, 86f), alpha = dot.value)
        }
        Spacer(Modifier.height(22.dp))
        // ponytail: 文字は端末の serif。Cormorant Garamond を同梱するなら res/font に入れて FontFamily を差し替える
        Text("anam", Modifier.alpha(word.value), color = Ivory, fontSize = 44.sp, fontFamily = FontFamily.Serif, fontWeight = FontWeight.Light, letterSpacing = 4.sp)
        Text("THE LIVING ANAMNESIS", Modifier.alpha(word.value).padding(top = 10.dp), color = Gold, fontSize = 11.sp, fontFamily = FontFamily.Serif, letterSpacing = 5.sp)
    }
}
