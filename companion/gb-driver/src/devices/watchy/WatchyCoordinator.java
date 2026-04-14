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

import android.bluetooth.le.ScanFilter;
import android.os.ParcelUuid;

import androidx.annotation.NonNull;

import java.util.Collection;
import java.util.Collections;
import java.util.regex.Pattern;

import nodomain.freeyourgadget.gadgetbridge.R;
import nodomain.freeyourgadget.gadgetbridge.devices.AbstractBLEDeviceCoordinator;
import nodomain.freeyourgadget.gadgetbridge.impl.GBDevice;
import nodomain.freeyourgadget.gadgetbridge.service.DeviceSupport;
import nodomain.freeyourgadget.gadgetbridge.service.devices.watchy.WatchySupport;

/**
 * Coordinator for the SQFMI Watchy running the WatchyMultiTZ firmware.
 *
 * Scope for MVP: one-way calendar push from the phone to the watch, via the
 * custom 4e2d0001 GATT service defined in
 * sketches/WatchyMultiTZ/src/platform/watchy/BleEventProvider.h. No activity
 * tracking, no notifications — the watch is deep-sleeping between draws and
 * only wakes when the user explicitly requests a sync.
 */
public class WatchyCoordinator extends AbstractBLEDeviceCoordinator {

    @Override
    protected Pattern getSupportedDeviceName() {
        // Firmware advertises as "Watchy-WMT"; keep the prefix match loose so
        // user-renamed devices still show up.
        return Pattern.compile("Watchy.*");
    }

    @Override
    public String getManufacturer() {
        return "SQFMI";
    }

    @NonNull
    @Override
    public Class<? extends DeviceSupport> getDeviceSupportClass(final GBDevice device) {
        return WatchySupport.class;
    }

    @Override
    public int getDeviceNameResource() {
        return R.string.devicetype_watchy;
    }

    @Override
    public int getDefaultIconResource() {
        // TODO: replace with a Watchy-specific icon (ic_device_watchy.xml).
        return R.drawable.ic_device_pinetime;
    }

    @Override
    public boolean suggestUnbindBeforePair() {
        return false;
    }

    @Override
    public DeviceKind getDeviceKind(@NonNull GBDevice device) {
        return DeviceKind.WATCH;
    }

    @NonNull
    @Override
    public Collection<? extends ScanFilter> createBLEScanFilters() {
        // Filter by service UUID so we only match watches running our GATT
        // profile, not arbitrary devices that happen to share the name.
        return Collections.singletonList(
                new ScanFilter.Builder()
                        .setServiceUuid(new ParcelUuid(WatchyConstants.SERVICE_UUID))
                        .build()
        );
    }
}
