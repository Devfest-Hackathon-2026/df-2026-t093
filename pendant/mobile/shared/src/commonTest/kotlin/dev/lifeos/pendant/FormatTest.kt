package dev.lifeos.pendant

import kotlin.test.*

class FormatTest {
    @Test fun crcCheckValue() = assertEquals(0xCBF43926L, crc32("123456789".encodeToByteArray()))

    private val good = audio(1, len = 12)

    @Test fun everySingleBitFlipIsRejected() {
        assertIs<Parsed.Ok>(parseRecord(good))
        for (bit in 0 until good.size * 8) {
            val b = good.copyOf(); b[bit / 8] = (b[bit / 8].toInt() xor (1 shl (bit % 8))).toByte()
            assertIs<Parsed.Bad>(parseRecord(b), "bit $bit")
        }
    }

    @Test fun uncommittedShortAbnormalAndVersion() {
        assertIs<Parsed.Bad>(parseRecord(encodeRecord(Header(T_AUDIO, 1, 0, 1, 1, 0, 0, 12), ByteArray(12), commit = false)))
        for (n in 0 until good.size) assertIs<Parsed.Bad>(parseRecord(good.copyOf(n)), "length $n")
        val huge = good.copyOf(); huge.put32(28, 0xFFFFFFF0L)
        assertIs<Parsed.Bad>(parseRecord(huge))
        assertNull(recordSize(huge))
        val v2 = good.copyOf(); v2[4] = 2
        assertIs<Parsed.Bad>(parseRecord(v2))
        assertNull(recordSize(v2))
    }

    private fun known(raw: ByteArray) = parse(raw).known

    @Test fun knownAndOpaque() {
        assertTrue(known(audio(1)) && known(timefix(2, 5)) && known(gap(3, 5)) && known(event(6)))
        assertFalse(known(rec(T_AUDIO, 1, ByteArray(8), flags = 2)))
        assertFalse(known(rec(T_TIMEFIX, 1, ByteArray(16), flags = 4)))
        assertFalse(known(rec(T_EVENT, 1, ByteArray(8), flags = 0x80)))
        assertFalse(known(rec(T_AUDIO, 1, ByteArray(8), codec = 2)))
        assertFalse(known(rec(T_AUDIO, 1, ByteArray(8), codec = 0)))
        assertFalse(known(timefix(1, 5, len = 15)))
        assertFalse(known(rec(T_EVENT, 1, ByteArray(9))))
        assertFalse(known(rec(5, 1, ByteArray(8), codec = 0)))
        assertFalse(known(gap(3, 5, flags = 2)))
        assertFalse(known(gap(3, 5, len = 16)))
        assertFalse(known(gap(3, 5, seq = 4)))
        assertFalse(known(gap(6, 5)))
        assertTrue(parse(gap(3, 5, flags = 2)).opaqueGap)
        assertFalse(parse(rec(5, 1, ByteArray(8), codec = 0)).opaqueGap)
        assertNull(parse(rec(5, 1, ByteArray(8), codec = 0)).decoded)
    }

    @Test fun coverage() {
        assertEquals(3L to 8L, parse(gap(3, 8)).let { it.first to it.last })
        assertEquals(7L to 7L, parse(audio(7)).let { it.first to it.last })
        assertEquals(7L to 7L, parse(rec(9, 7, ByteArray(4), codec = 0)).let { it.first to it.last })
    }
}
