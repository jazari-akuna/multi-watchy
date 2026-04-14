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

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.os.Handler;
import android.os.Looper;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.TimeZone;

import nodomain.freeyourgadget.gadgetbridge.devices.watchy.WatchyConstants;
import nodomain.freeyourgadget.gadgetbridge.impl.GBDevice;
import nodomain.freeyourgadget.gadgetbridge.model.CalendarEventSpec;
import nodomain.freeyourgadget.gadgetbridge.service.btle.AbstractBTLESingleDeviceSupport;
import nodomain.freeyourgadget.gadgetbridge.service.btle.TransactionBuilder;

/**
 * DeviceSupport for the Watchy MultiTZ firmware.
 *
 * <p>The watch exposes a single GATT service with one WRITE and one STATE
 * characteristic. All traffic is in 64 B packets (see {@link WatchyProtocol}).
 * This class drives the calendar sync path: it debounces the rapid-fire
 * {@link #onAddCalendarEvent(CalendarEventSpec)} calls GB emits after a
 * calendar provider rescan, applies the "all-day demotion" rule, then encodes
 * one TIME_SYNC + N EVENT + BATCH_END burst onto the WRITE characteristic.</p>
 */
public class WatchySupport extends AbstractBTLESingleDeviceSupport {
    private static final Logger LOG = LoggerFactory.getLogger(WatchySupport.class);

    /** How long we wait for the add/delete storm to subside before flushing. */
    private static final long DEBOUNCE_MS = 500L;

    /** Preference key. Default is ON — the Watchy's only real feature is the calendar. */
    private static final String PREF_SYNC_CALENDAR = "sync_calendar";

    /** Pending events collected between debounced flushes. */
    private final List<CalendarEventSpec> pendingEvents =
            Collections.synchronizedList(new ArrayList<>());

    private final Handler flushHandler = new Handler(Looper.getMainLooper());
    private final Runnable flushTask = this::flushCalendarBatch;

    public WatchySupport() {
        super(LOG);
        addSupportedService(WatchyConstants.SERVICE_UUID);
    }

    // ---------------------------------------------------------------------
    // Init
    // ---------------------------------------------------------------------

    @Override
    protected TransactionBuilder initializeDevice(final TransactionBuilder builder) {
        builder.setDeviceState(GBDevice.State.INITIALIZING);
        final BluetoothGattCharacteristic stateChar = getCharacteristic(WatchyConstants.CHAR_STATE);
        if (stateChar != null) {
            builder.read(stateChar);
        } else {
            LOG.warn("STATE characteristic not found on service discovery; continuing anyway");
        }
        builder.setDeviceState(GBDevice.State.INITIALIZED);
        return builder;
    }

    @Override
    public boolean onCharacteristicRead(final BluetoothGatt gatt,
                                        final BluetoothGattCharacteristic characteristic,
                                        final byte[] value,
                                        final int status) {
        if (characteristic != null
                && WatchyConstants.CHAR_STATE.equals(characteristic.getUuid())) {
            parseState(value);
            return true;
        }
        return super.onCharacteristicRead(gatt, characteristic, value, status);
    }

    @Override
    public boolean onCharacteristicChanged(final BluetoothGatt gatt,
                                           final BluetoothGattCharacteristic characteristic,
                                           final byte[] value) {
        if (characteristic != null
                && WatchyConstants.CHAR_STATE.equals(characteristic.getUuid())) {
            parseState(value);
            return true;
        }
        return super.onCharacteristicChanged(gatt, characteristic, value);
    }

    /**
     * STATE payload layout (16 B):
     *   [0..7]  = int64 lastSyncUtc (LE)
     *   [8..9]  = uint16 eventCount (LE)
     *   [10]    = schemaVersion
     *   [11..15] = reserved
     *
     * Never throw — GB's Nordic layer retries reads and we don't want to
     * force-disconnect just because the watch shipped a shorter buffer.
     */
    private void parseState(final byte[] value) {
        if (value == null || value.length < 11) {
            LOG.warn("STATE read returned {} bytes, expected >= 11",
                    value == null ? 0 : value.length);
            return;
        }
        try {
            final ByteBuffer buf = ByteBuffer.wrap(value).order(ByteOrder.LITTLE_ENDIAN);
            final long lastSyncUtc = buf.getLong(0);
            final int eventCount = buf.getShort(8) & 0xFFFF;
            final int schemaVersion = buf.get(10) & 0xFF;
            LOG.info("Watchy STATE: lastSyncUtc={} eventCount={} schemaVersion={}",
                    lastSyncUtc, eventCount, schemaVersion);
        } catch (final RuntimeException e) {
            LOG.warn("Failed to parse STATE payload", e);
        }
    }

    // ---------------------------------------------------------------------
    // Calendar: debounced batch push
    // ---------------------------------------------------------------------

    @Override
    public void onAddCalendarEvent(final CalendarEventSpec calendarEventSpec) {
        if (calendarEventSpec == null) {
            return;
        }
        if (!getDevicePrefs().getBoolean(PREF_SYNC_CALENDAR, true)) {
            LOG.debug("Ignoring add calendar event {}, sync disabled", calendarEventSpec.id);
            return;
        }
        synchronized (pendingEvents) {
            // Replace any prior spec for the same id so re-emitted events update in place.
            pendingEvents.removeIf(s -> s.id == calendarEventSpec.id);
            pendingEvents.add(calendarEventSpec);
        }
        scheduleFlush();
    }

    @Override
    public void onDeleteCalendarEvent(final byte type, final long id) {
        if (!getDevicePrefs().getBoolean(PREF_SYNC_CALENDAR, true)) {
            LOG.debug("Ignoring delete calendar event {}, sync disabled", id);
            return;
        }
        synchronized (pendingEvents) {
            pendingEvents.removeIf(s -> s.id == id);
        }
        scheduleFlush();
    }

    private void scheduleFlush() {
        flushHandler.removeCallbacks(flushTask);
        flushHandler.postDelayed(flushTask, DEBOUNCE_MS);
    }

    private void flushCalendarBatch() {
        final List<CalendarEventSpec> snapshot;
        synchronized (pendingEvents) {
            snapshot = new ArrayList<>(pendingEvents);
            // We intentionally keep pendingEvents as-is: it represents the
            // current known set so a subsequent delete can prune, and we
            // always re-push the full set on flush.
        }
        pushBatch(snapshot);
    }

    // ---------------------------------------------------------------------
    // All-day demotion rule
    // ---------------------------------------------------------------------

    /**
     * If any timed (non-all-day) event starts later today, suppress all-day
     * entries whose day == today. Keeps the watch face focused on the timed
     * event rather than "Holiday" banners when there's a real appointment
     * coming up.
     */
    List<CalendarEventSpec> applyAllDayRule(final List<CalendarEventSpec> in) {
        final long now = System.currentTimeMillis() / 1000L;
        final Calendar cal = Calendar.getInstance(TimeZone.getDefault());
        cal.set(Calendar.HOUR_OF_DAY, 0);
        cal.set(Calendar.MINUTE, 0);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 0);
        final long todayStart = cal.getTimeInMillis() / 1000L;
        final long todayEnd = todayStart + 86400L;

        boolean hasTimedToday = false;
        for (final CalendarEventSpec s : in) {
            if (s == null) {
                continue;
            }
            final long begin = s.timestamp;
            if (!s.allDay && begin > now && begin < todayEnd) {
                hasTimedToday = true;
                break;
            }
        }
        if (!hasTimedToday) {
            return in;
        }
        final List<CalendarEventSpec> out = new ArrayList<>(in.size());
        for (final CalendarEventSpec s : in) {
            if (s == null) {
                continue;
            }
            if (s.allDay && s.timestamp >= todayStart && s.timestamp < todayEnd) {
                continue;
            }
            out.add(s);
        }
        return out;
    }

    // ---------------------------------------------------------------------
    // Wire: TIME_SYNC + EVENTs + BATCH_END
    // ---------------------------------------------------------------------

    private void pushBatch(final List<CalendarEventSpec> rawEvents) {
        final List<CalendarEventSpec> demoted = applyAllDayRule(rawEvents);

        final long nowS = System.currentTimeMillis() / 1000L;

        // Filter to events that haven't already ended, sort ascending by start, cap.
        final List<CalendarEventSpec> filtered = new ArrayList<>(demoted.size());
        for (final CalendarEventSpec s : demoted) {
            if (s == null) {
                continue;
            }
            final long start = s.timestamp;
            final long end = start + Math.max(0, s.durationInSeconds);
            if (end <= nowS) {
                continue;
            }
            filtered.add(s);
        }
        filtered.sort(Comparator.comparingInt(s -> s.timestamp));
        final List<CalendarEventSpec> capped = filtered.size() > WatchyConstants.MAX_EVENTS_PER_BATCH
                ? filtered.subList(0, WatchyConstants.MAX_EVENTS_PER_BATCH)
                : filtered;

        try {
            final TransactionBuilder b = performInitialized("watchy_calendar_push");
            final BluetoothGattCharacteristic wr = getCharacteristic(WatchyConstants.CHAR_WRITE);
            if (wr == null) {
                LOG.warn("WRITE characteristic not available; dropping calendar batch");
                return;
            }
            final int gmtOffS = TimeZone.getDefault().getOffset(System.currentTimeMillis()) / 1000;
            b.write(wr, WatchyProtocol.makeTimeSync(nowS, gmtOffS));
            for (final CalendarEventSpec s : capped) {
                final long start = s.timestamp;
                final long end = start + Math.max(0, s.durationInSeconds);
                final String title = s.title != null ? s.title : "";
                b.write(wr, WatchyProtocol.makeEvent(start, end, title, s.allDay));
            }
            b.write(wr, WatchyProtocol.makeBatchEnd());
            b.queue();
            LOG.info("Watchy calendar push queued: {} event(s) (of {} pending)",
                    capped.size(), rawEvents.size());
        } catch (final IOException e) {
            LOG.error("Watchy calendar push failed", e);
        }
    }

    // ---------------------------------------------------------------------
    // Misc overrides
    // ---------------------------------------------------------------------

    @Override
    public boolean useAutoConnect() {
        return true;
    }

    @Override
    public void onSendConfiguration(final String config) {
        // Nothing watch-side is configurable over BLE at the moment. Just log
        // and let the super handle intent-bridge plumbing.
        LOG.debug("onSendConfiguration({}) — no-op", config);
        super.onSendConfiguration(config);
    }

    @Override
    public void dispose() {
        flushHandler.removeCallbacks(flushTask);
        super.dispose();
    }
}
