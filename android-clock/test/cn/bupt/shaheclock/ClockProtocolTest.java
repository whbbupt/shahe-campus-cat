package cn.bupt.shaheclock;

import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.time.LocalDateTime;

public final class ClockProtocolTest {
    public static void main(String[] args) {
        Instant sent = Instant.parse("2026-11-09T16:03:04.500Z");
        String payload = new String(ClockProtocol.encode(sent), StandardCharsets.US_ASCII);
        check(payload.equals("20261110000304"), "Beijing conversion across midnight");
        check(new String(ClockProtocol.encode(Instant.parse("2023-12-31T16:00:00Z")),
                StandardCharsets.US_ASCII).equals("20240101000000"), "lower boundary");
        reject(() -> ClockProtocol.encode(Instant.parse("2023-12-31T15:59:59Z")));
        reject(() -> ClockProtocol.encode(Instant.parse("2037-12-31T16:00:00Z")));

        String good = "time=2026-11-10 00:03:05;state=synced;result=ok;source=manual";
        check(ClockProtocol.verifiedTime(good, sent, 700)
                .equals(LocalDateTime.of(2026, 11, 10, 0, 3, 5)), "successful device readback");
        reject(() -> ClockProtocol.verifiedTime("state=pending", sent, 700));
        reject(() -> ClockProtocol.verifiedTime(good.replace("manual", "cts"), sent, 700));
        reject(() -> ClockProtocol.verifiedTime(good.replace("result=ok", "result=verify-error"), sent, 700));
        reject(() -> ClockProtocol.verifiedTime(good.replace("11-10", "11-31"), sent, 700));
        reject(() -> ClockProtocol.verifiedTime(good.replace("00:03:05", "00:03:00"), sent, 700));
        reject(() -> ClockProtocol.verifiedTime(good.replace("00:03:05", "00:03:15"), sent, 700));
        reject(() -> ClockProtocol.verifiedTime(good.substring(0, 22), sent, 700));
        System.out.println("PASS: UTC+8, year boundaries, confirmed readback, stale/truncated/error replies");
    }

    private static void check(boolean value, String reason) {
        if (!value) throw new AssertionError(reason);
    }

    private static void reject(Runnable action) {
        try {
            action.run();
        } catch (IllegalArgumentException expected) {
            return;
        }
        throw new AssertionError("Expected the clock protocol to reject the input");
    }
}
