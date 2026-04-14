/*  Copyright (C) 2026 Watchy contributors

    This file is part of Gadgetbridge.

    Gadgetbridge is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Gadgetbridge is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */
package nodomain.freeyourgadget.gadgetbridge.service.devices.watchy;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

import nodomain.freeyourgadget.gadgetbridge.devices.watchy.WatchyConstants;

/**
 * Pure static encoder for the 64 B Watchy BLE wire format.
 *
 * All packets are fixed {@link WatchyConstants#PACKET_LEN} bytes, little-endian.
 * See {@code sketches/WatchyMultiTZ/src/platform/watchy/BleEventProvider.h} on
 * the firmware side — any divergence here will break the handshake.
 */
public final class WatchyProtocol {
    private WatchyProtocol() {}

    /**
     * TIME_SYNC (tag=0x01):
     *   [0]      = tag
     *   [1..7]   = 0 (pad)
     *   [8..15]  = int64 currentUtcSeconds (LE)
     *   [16..19] = int32 gmtOffsetSeconds  (LE)
     *   [20..63] = 0
     */
    public static byte[] makeTimeSync(final long utcSeconds, final int gmtOffsetSeconds) {
        final ByteBuffer buf = ByteBuffer.allocate(WatchyConstants.PACKET_LEN)
                .order(ByteOrder.LITTLE_ENDIAN);
        buf.put(0, WatchyConstants.TAG_TIME_SYNC);
        buf.position(8);
        buf.putLong(utcSeconds);
        buf.putInt(gmtOffsetSeconds);
        return buf.array();
    }

    /**
     * EVENT (tag=0x02):
     *   [0]      = tag
     *   [1]      = flags (bit0 = all-day)
     *   [2..9]   = int64 startUtcSeconds (LE)
     *   [10..17] = int64 endUtcSeconds   (LE)
     *   [18..63] = UTF-8 title, 46 B, null-padded & truncated
     */
    public static byte[] makeEvent(final long startUtc,
                                   final long endUtc,
                                   final String title,
                                   final boolean allDay) {
        final ByteBuffer buf = ByteBuffer.allocate(WatchyConstants.PACKET_LEN)
                .order(ByteOrder.LITTLE_ENDIAN);
        buf.put(0, WatchyConstants.TAG_EVENT);
        buf.put(1, (byte) (allDay ? WatchyConstants.EVENT_FLAG_ALL_DAY : 0));
        buf.position(2);
        buf.putLong(startUtc);
        buf.putLong(endUtc);

        // Title: copy up to TITLE_BYTES_MAX bytes, truncating mid-character-safely
        // is not required by the firmware (it just null-terminates), but we
        // avoid emitting a dangling partial UTF-8 sequence.
        final String safeTitle = title != null ? title : "";
        final byte[] encoded = safeTitle.getBytes(StandardCharsets.UTF_8);
        int copyLen = Math.min(encoded.length, WatchyConstants.TITLE_BYTES_MAX);
        copyLen = trimUtf8(encoded, copyLen);
        if (copyLen > 0) {
            System.arraycopy(encoded, 0, buf.array(), 18, copyLen);
        }
        // Remaining bytes in the title region are already zero from allocate().
        return buf.array();
    }

    /** BATCH_END (tag=0x03): [0]=tag, rest zeros. */
    public static byte[] makeBatchEnd() {
        final byte[] out = new byte[WatchyConstants.PACKET_LEN];
        out[0] = WatchyConstants.TAG_BATCH_END;
        return out;
    }

    /** CLEAR (tag=0x04): [0]=tag, rest zeros. */
    public static byte[] makeClear() {
        final byte[] out = new byte[WatchyConstants.PACKET_LEN];
        out[0] = WatchyConstants.TAG_CLEAR;
        return out;
    }

    /**
     * If {@code len} falls in the middle of a UTF-8 continuation sequence,
     * walk back to the last complete code point boundary so we never emit a
     * truncated codepoint (the firmware just null-terminates and would print
     * a mojibake glyph).
     */
    private static int trimUtf8(final byte[] bytes, int len) {
        if (len <= 0 || len >= bytes.length) {
            return len;
        }
        // If the byte at [len] is a continuation byte (10xxxxxx), walk back
        // until we find a start byte, then check whether dropping to there
        // keeps us whole.
        int i = len;
        while (i > 0 && (bytes[i] & 0xC0) == 0x80) {
            i--;
        }
        if (i == 0) {
            return 0;
        }
        final int lead = bytes[i] & 0xFF;
        final int expected;
        if ((lead & 0x80) == 0) {
            expected = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            expected = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            expected = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            expected = 4;
        } else {
            // malformed — drop the byte entirely
            return i;
        }
        return (i + expected <= len) ? len : i;
    }
}
