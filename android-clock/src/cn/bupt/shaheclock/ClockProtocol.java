package cn.bupt.shaheclock;

import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.time.format.ResolverStyle;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class ClockProtocol {
    static final ZoneOffset BEIJING = ZoneOffset.ofHours(8);
    private static final DateTimeFormatter WIRE =
            DateTimeFormatter.ofPattern("uuuuMMddHHmmss", Locale.ROOT);
    private static final DateTimeFormatter REPLY_TIME =
            DateTimeFormatter.ofPattern("uuuu-MM-dd HH:mm:ss", Locale.ROOT)
                    .withResolverStyle(ResolverStyle.STRICT);
    private static final Pattern CONFIRMED =
            Pattern.compile("time=([0-9 :-]{19});state=synced;result=ok;source=manual");

    static byte[] encode(Instant instant) {
        LocalDateTime local = LocalDateTime.ofInstant(instant, BEIJING);
        if (local.getYear() < 2024 || local.getYear() > 2037) {
            throw new IllegalArgumentException("手表支持 2024–2037 年，请先检查手机日期");
        }
        return local.format(WIRE).getBytes(StandardCharsets.US_ASCII);
    }

    static LocalDateTime verifiedTime(String reply, Instant sent, long elapsedMs) {
        Matcher match = CONFIRMED.matcher(reply);
        if (!match.matches()) {
            throw new IllegalArgumentException("手表未确认本次校时");
        }
        LocalDateTime local;
        try {
            local = LocalDateTime.parse(match.group(1), REPLY_TIME);
        } catch (java.time.DateTimeException ex) {
            throw new IllegalArgumentException("手表返回的日期无效", ex);
        }
        Instant actual = local.toInstant(BEIJING);
        long drift = Duration.between(sent.plusMillis(elapsedMs), actual).toMillis();
        // Seconds precision plus BLE transport and RTC scheduling.
        if (actual.isBefore(sent.minusSeconds(1)) || Math.abs(drift) > 3000) {
            throw new IllegalArgumentException("手表读回时间与手机不一致");
        }
        return local;
    }

    private ClockProtocol() {}
}
