package cn.bupt.shaheclock;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.time.Instant;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Locale;

public final class MainActivity extends Activity implements BleClock.Listener {
    private static final int PAPER = 0xfff4e4c7;
    private static final int LIGHT_PAPER = 0xfffff8e8;
    private static final int INK = 0xff573f30;
    private static final int MOSS = 0xff475d3e;
    private static final int EDGE = 0xffc6a784;
    private static final int PERMISSION_REQUEST = 1;
    private static final int ENABLE_REQUEST = 2;
    private static final DateTimeFormatter DISPLAY =
            DateTimeFormatter.ofPattern("HH:mm:ss", Locale.ROOT);
    private final Handler main = new Handler(Looper.getMainLooper());
    private BleClock bluetooth;
    private LinearLayout devices;
    private TextView time, title, detail;
    private Button action;
    private boolean busy;

    private final Runnable ticker = new Runnable() {
        @Override public void run() {
            time.setText(DISPLAY.format(LocalDateTime.ofInstant(Instant.now(), ClockProtocol.BEIJING)));
            main.postDelayed(this, 1000);
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setStatusBarColor(PAPER);
        getWindow().setNavigationBarColor(PAPER);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        bluetooth = new BleClock(this, this);
        makePage();
        status("准备校时", "打开手表，扫描并点选你的小猫。", false);
    }

    @Override protected void onStart() {
        super.onStart();
        ticker.run();
    }

    @Override protected void onStop() {
        main.removeCallbacks(ticker);
        bluetooth.stop();
        if (busy) {
            devices.removeAllViews();
            status("准备校时", "回到这里后，可以重新扫描手表。", false);
        }
        super.onStop();
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private GradientDrawable panel(int color, int radius, boolean outline) {
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(color);
        bg.setCornerRadius(dp(radius));
        if (outline) bg.setStroke(dp(1), EDGE);
        return bg;
    }

    private TextView text(String value, int sp, int color) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(sp);
        view.setTextColor(color);
        return view;
    }

    private LinearLayout column() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        return layout;
    }

    private void add(LinearLayout parent, View child, int marginTop) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.topMargin = dp(marginTop);
        parent.addView(child, params);
    }

    private Button button(String label, boolean primary) {
        Button b = new Button(this);
        b.setAllCaps(false);
        b.setText(label);
        b.setTextSize(16);
        b.setTextColor(primary ? LIGHT_PAPER : INK);
        b.setGravity(primary ? Gravity.CENTER : Gravity.CENTER_VERTICAL);
        b.setMinHeight(dp(52));
        b.setPadding(dp(16), dp(10), dp(16), dp(10));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33573f30),
                panel(primary ? MOSS : LIGHT_PAPER, 8, !primary), null));
        return b;
    }

    private void makePage() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(PAPER);
        LinearLayout page = column();
        page.setPadding(dp(24), dp(28), dp(24), dp(28));
        scroll.addView(page);
        setContentView(scroll);

        TextView campus = text("北邮 · 沙河校园", 14, INK);
        add(page, campus, 0);
        TextView headline = text("小猫校时", 34, INK);
        headline.setTypeface(Typeface.create("serif", Typeface.BOLD));
        add(page, headline, 4);
        add(page, text("把手机的时间同步给手表。", 16, INK), 4);

        LinearLayout card = new LinearLayout(this);
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(dp(16), dp(14), dp(16), dp(14));
        card.setBackground(panel(LIGHT_PAPER, 12, true));
        LinearLayout clockText = column();
        card.addView(clockText, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1));
        add(clockText, text("手机时间 · 北京", 14, MOSS), 0);
        time = text("", 27, INK);
        time.setTypeface(Typeface.MONOSPACE);
        add(clockText, time, 2);
        ImageView icon = new ImageView(this);
        icon.setImageResource(R.drawable.clock_cat);
        icon.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        card.addView(icon, new LinearLayout.LayoutParams(dp(54), dp(54)));
        add(page, card, 24);

        title = text("", 20, INK);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        add(page, title, 24);
        detail = text("", 15, INK);
        add(page, detail, 5);
        devices = column();
        add(page, devices, 0);
        action = button("寻找小猫", true);
        action.setOnClickListener(v -> {
            devices.removeAllViews();
            if (busy) {
                bluetooth.stop();
                status("已取消", "可以重新寻找手表。", false);
            } else {
                scan();
            }
        });
        add(page, action, 18);
    }

    private void scan() {
        String[] permissions = Build.VERSION.SDK_INT >= 31
                ? new String[] {Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT}
                : new String[] {Manifest.permission.ACCESS_FINE_LOCATION};
        for (String permission : permissions) {
            if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(permissions, PERMISSION_REQUEST);
                return;
            }
        }
        BluetoothManager manager = getSystemService(BluetoothManager.class);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        if (adapter == null) {
            status("蓝牙不可用", "这台手机没有可用的蓝牙适配器。", false);
            return;
        }
        try {
            if (!adapter.isEnabled()) {
                startActivityForResult(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE), ENABLE_REQUEST);
                return;
            }
            bluetooth.startScan(adapter);
        } catch (SecurityException ex) {
            status("需要蓝牙权限", "请在手机设置中允许访问附近设备。", false);
        }
    }

    @Override public void onRequestPermissionsResult(int request, String[] permissions, int[] grants) {
        super.onRequestPermissionsResult(request, permissions, grants);
        if (request != PERMISSION_REQUEST) return;
        if (grants.length != permissions.length || grants.length == 0) {
            status("权限未开启", "允许扫描所需权限后，再寻找小猫。", false);
            return;
        }
        for (int grant : grants) {
            if (grant != PackageManager.PERMISSION_GRANTED) {
                status("权限未开启", Build.VERSION.SDK_INT >= 31
                        ? "请在手机设置中允许访问附近设备。"
                        : "请允许位置权限，以便这个安卓版本扫描蓝牙设备。", false);
                return;
            }
        }
        scan();
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != ENABLE_REQUEST) return;
        if (result == RESULT_OK) scan();
        else status("蓝牙未开启", "打开手机蓝牙后，再寻找小猫。", false);
    }

    @Override public void status(String heading, String description, boolean working) {
        busy = working;
        title.setText(heading);
        detail.setText(description);
        action.setText(working ? "取消" : "寻找小猫");
    }

    @Override public void device(BluetoothDevice device, String name) {
        String address = device.getAddress();
        Button row = button(name + "   ·   " + address.substring(address.length() - 5), false);
        row.setOnClickListener(v -> {
            devices.removeAllViews();
            bluetooth.connect(device);
        });
        add(devices, row, 10);
    }

    @Override public void synced(LocalDateTime value) {
        status("校时成功", "手表已确认北京时间 " + DISPLAY.format(value) + "。", false);
    }
}
