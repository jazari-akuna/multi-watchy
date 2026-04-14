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
package nodomain.freeyourgadget.gadgetbridge.devices.watchy;

import java.util.UUID;

/**
 * Constants shared between the Watchy coordinator, support and protocol
 * classes. Values here MUST match the firmware header at
 * sketches/WatchyMultiTZ/src/platform/watchy/BleEventProvider.h — any
 * divergence will break the phone <-> watch handshake.
 */
public final class WatchyConstants {
    private WatchyConstants() {}

    /** BLE advertised name. Scan filter uses SERVICE_UUID instead, but this
     *  is handy for human-readable discovery logging. */
    public static final String DEVICE_NAME = "Watchy-WMT";

    public static final UUID SERVICE_UUID =
            UUID.fromString("4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b");
    /** WRITE characteristic — 64 B packets, phone -> watch. */
    public static final UUID CHAR_WRITE =
            UUID.fromString("4e2d0002-6f00-4d1a-9c7b-8f7c2e0a1d3b");
    /** STATE characteristic — READ + NOTIFY, 16 B, watch -> phone. */
    public static final UUID CHAR_STATE =
            UUID.fromString("4e2d0003-6f00-4d1a-9c7b-8f7c2e0a1d3b");

    // Packet tags (byte 0 of every WRITE packet).
    public static final byte TAG_TIME_SYNC = 0x01;
    public static final byte TAG_EVENT     = 0x02;
    public static final byte TAG_BATCH_END = 0x03;
    public static final byte TAG_CLEAR     = 0x04;

    /** Fixed WRITE packet length in bytes. */
    public static final int PACKET_LEN = 64;
    /** Max events per batch — must match RING_CAPACITY on the watch. */
    public static final int MAX_EVENTS_PER_BATCH = 10;
    /** Title payload length (UTF-8, null-padded) inside an EVENT packet. */
    public static final int TITLE_BYTES_MAX = 46;

    /** Event.flags bit 0 — all-day event. */
    public static final byte EVENT_FLAG_ALL_DAY = 0x01;
}
