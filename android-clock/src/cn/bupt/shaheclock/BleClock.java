package cn.bupt.shaheclock;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.os.SystemClock;

import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.time.LocalDateTime;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;

final class BleClock {
    interface Listener {
        void status(String title, String detail, boolean busy);
        void device(BluetoothDevice device, String name);
        void synced(LocalDateTime time);
    }

    private enum Stage { IDLE, CONNECTING, DISCOVERING, MTU, WRITING, VERIFY_WAIT, READING }
    private static final UUID SERVICE = UUID.fromString("8a7c0000-2f73-4f25-9a4d-534841484543");
    private static final UUID TIME = UUID.fromString("8a7c0004-2f73-4f25-9a4d-534841484543");
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Context context;
    private final Listener listener;
    private final Set<String> seen = new HashSet<>();
    private BluetoothLeScanner scanner;
    private ScanCallback scan;
    private BluetoothGatt link;
    private BluetoothGattCharacteristic clock;
    private Stage stage = Stage.IDLE;
    private Instant sent;
    private long sentAt;
    private boolean retriedBusy;

    BleClock(Context context, Listener listener) {
        this.context = context;
        this.listener = listener;
    }

    void startScan(BluetoothAdapter adapter) {
        stop();
        seen.clear();
        try {
            scanner = adapter.getBluetoothLeScanner();
            if (scanner == null) {
                fail("蓝牙扫描未就绪，请打开蓝牙后重试");
                return;
            }
            scan = new ScanCallback() {
                @Override public void onScanResult(int type, ScanResult result) {
                    main.post(() -> {
                        if (scan != this) return;
                        BluetoothDevice device = result.getDevice();
                        String name = result.getScanRecord() == null
                                ? null : result.getScanRecord().getDeviceName();
                        if (seen.add(device.getAddress())) {
                            listener.device(device, name == null ? "ShaheCat" : name);
                            listener.status("发现小猫", "点选手表，连接后自动校时。", true);
                        }
                    });
                }

                @Override public void onScanFailed(int code) {
                    main.post(() -> {
                        if (scan == this) fail("蓝牙扫描失败（" + code + "），请重新扫描");
                    });
                }
            };
            ScanFilter filter = new ScanFilter.Builder()
                    .setServiceUuid(new ParcelUuid(SERVICE)).build();
            scanner.startScan(Collections.singletonList(filter),
                    new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),
                    scan);
            listener.status("正在寻找小猫", "让手表保持开机，并放在手机附近。", true);
            ScanCallback started = scan;
            main.postDelayed(() -> {
                if (scan != started) return;
                stopScan();
                if (seen.isEmpty()) {
                    fail(Build.VERSION.SDK_INT <= 30
                            ? "没找到手表。请开启手机定位，并确认手表已开机、未被其他手机占用"
                            : "没找到手表。请确认手表已开机、未被其他手机占用");
                } else {
                    listener.status("选择手表", "点选下方设备，连接后自动校时。", false);
                }
            }, 10000);
        } catch (SecurityException | IllegalStateException ex) {
            fail("无法扫描，请检查蓝牙和附近设备权限");
        }
    }

    void connect(BluetoothDevice device) {
        stop();
        stage = Stage.CONNECTING;
        retriedBusy = false;
        listener.status("正在连接手表", "连接成功后会发送手机的北京时间。", true);
        try {
            link = device.connectGatt(context, false, callbacks, BluetoothDevice.TRANSPORT_LE);
            if (link == null) {
                fail("无法建立蓝牙连接");
                return;
            }
            BluetoothGatt attempt = link;
            main.postDelayed(() -> {
                if (link == attempt) fail("等待手表响应超时，请重新扫描后再试");
            }, 22000);
        } catch (SecurityException | IllegalStateException ex) {
            fail("无法连接，请检查蓝牙权限和手表状态");
        }
    }

    void stop() {
        main.removeCallbacksAndMessages(null);
        stopScan();
        stage = Stage.IDLE;
        clock = null;
        BluetoothGatt old = link;
        link = null;
        if (old != null) {
            try {
                old.disconnect();
            } catch (SecurityException ignored) {
                // Permission may have changed while the activity was open.
            } finally {
                try {
                    old.close();
                } catch (SecurityException ignored) {
                    // The local handle has already been invalidated.
                }
            }
        }
    }

    private void stopScan() {
        ScanCallback old = scan;
        scan = null;
        if (scanner != null && old != null) {
            try {
                scanner.stopScan(old);
            } catch (SecurityException | IllegalStateException ignored) {
                // Bluetooth or permissions may already be unavailable.
            }
        }
        scanner = null;
    }

    private void fail(String reason) {
        stop();
        listener.status("校时未完成", reason, false);
    }

    private final BluetoothGattCallback callbacks = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt gatt, int status, int state) {
            main.post(() -> {
                if (gatt != link) return;
                if (status != BluetoothGatt.GATT_SUCCESS || state == BluetoothProfile.STATE_DISCONNECTED) {
                    fail("蓝牙连接已断开（" + status + "），请让手表靠近手机后重试");
                } else if (state == BluetoothProfile.STATE_CONNECTED && stage == Stage.CONNECTING) {
                    stage = Stage.DISCOVERING;
                    try {
                        if (!gatt.discoverServices()) fail("无法读取手表的蓝牙服务");
                    } catch (SecurityException ex) {
                        fail("蓝牙连接权限已关闭");
                    }
                }
            });
        }

        @Override public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            main.post(() -> {
                if (gatt != link || stage != Stage.DISCOVERING) return;
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    fail("读取手表服务失败（" + status + "）");
                    return;
                }
                BluetoothGattService service = gatt.getService(SERVICE);
                clock = service == null ? null : service.getCharacteristic(TIME);
                int required = BluetoothGattCharacteristic.PROPERTY_WRITE
                        | BluetoothGattCharacteristic.PROPERTY_READ;
                if (clock == null || (clock.getProperties() & required) != required) {
                    fail("这台手表没有可读写的校时接口，请检查固件版本");
                    return;
                }
                stage = Stage.MTU;
                try {
                    // The response can exceed the default 22-byte ATT read payload.
                    if (!gatt.requestMtu(128)) writeClock(gatt);
                } catch (SecurityException ex) {
                    fail("蓝牙连接权限已关闭");
                }
            });
        }

        @Override public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
            main.post(() -> {
                if (gatt == link && stage == Stage.MTU) writeClock(gatt);
            });
        }

        @Override public void onCharacteristicWrite(BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic, int status) {
            main.post(() -> {
                if (gatt != link || stage != Stage.WRITING || !TIME.equals(characteristic.getUuid())) return;
                if (status == 0x11 && !retriedBusy) {
                    // The firmware may still be handling its first CTS request.
                    retriedBusy = true;
                    main.postDelayed(() -> {
                        if (link == gatt && stage == Stage.WRITING) writeClock(gatt);
                    }, 500);
                } else if (status != BluetoothGatt.GATT_SUCCESS) {
                    fail("手表未接收时间（" + status + "），请重新连接");
                } else {
                    listener.status("正在核对时间", "等手表读回已设置的时间。", true);
                    scheduleRead(gatt);
                }
            });
        }

        @Override public void onCharacteristicRead(BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic, byte[] value, int status) {
            received(gatt, characteristic, value, status);
        }

        @Override @SuppressWarnings("deprecation")
        public void onCharacteristicRead(BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic, int status) {
            // Android 13+ delivers the immutable value through the other overload.
            if (Build.VERSION.SDK_INT < 33) received(gatt, characteristic, characteristic.getValue(), status);
        }
    };

    @SuppressWarnings("deprecation")
    private void writeClock(BluetoothGatt gatt) {
        stage = Stage.WRITING;
        listener.status("正在同步时间", "按北京时间发送手机当前时刻。", true);
        try {
            sent = Instant.now();
            byte[] payload = ClockProtocol.encode(sent);
            sentAt = SystemClock.elapsedRealtime();
            boolean accepted;
            if (Build.VERSION.SDK_INT >= 33) {
                accepted = gatt.writeCharacteristic(clock, payload,
                        BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS;
            } else {
                clock.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                accepted = clock.setValue(payload) && gatt.writeCharacteristic(clock);
            }
            if (!accepted) fail("无法发送时间，请重新连接手表");
        } catch (IllegalArgumentException ex) {
            fail(ex.getMessage());
        } catch (SecurityException ex) {
            fail("蓝牙连接权限已关闭");
        }
    }

    private void scheduleRead(BluetoothGatt gatt) {
        stage = Stage.VERIFY_WAIT;
        main.postDelayed(() -> {
            if (link != gatt || stage != Stage.VERIFY_WAIT) return;
            if (SystemClock.elapsedRealtime() - sentAt > 6000) {
                fail("手表未及时确认时间，请重新连接");
                return;
            }
            stage = Stage.READING;
            try {
                if (!gatt.readCharacteristic(clock)) fail("无法读回手表时间");
            } catch (SecurityException ex) {
                fail("蓝牙连接权限已关闭");
            }
        }, 250);
    }

    private void received(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic,
            byte[] value, int status) {
        String reply = value == null ? "" : new String(value, StandardCharsets.US_ASCII);
        main.post(() -> {
            if (gatt != link || stage != Stage.READING || !TIME.equals(characteristic.getUuid())) return;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                fail("读回时间失败（" + status + "）");
            } else if (reply.equals("state=pending")) {
                scheduleRead(gatt);
            } else if (reply.contains(";result=io-error") || reply.contains(";result=verify-error")) {
                fail("手表无法设置或核对时钟，请检查手表后再试");
            } else {
                try {
                    LocalDateTime time = ClockProtocol.verifiedTime(reply, sent,
                            SystemClock.elapsedRealtime() - sentAt);
                    stop();
                    listener.synced(time);
                } catch (IllegalArgumentException ex) {
                    fail(ex.getMessage());
                }
            }
        });
    }
}
