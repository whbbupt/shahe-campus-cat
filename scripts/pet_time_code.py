"""Print the 14-byte UTC+8 phone time payload and its LightBlue hex encoding."""
from datetime import datetime, timezone, timedelta
import argparse
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--at', help='Test-only Beijing civil time: YYYYMMDDhhmmss; defaults to current time')
a=p.parse_args()
try:
    stamp=a.at or datetime.now(timezone(timedelta(hours=8))).strftime('%Y%m%d%H%M%S')
    if len(stamp)!=14 or not stamp.isascii() or not stamp.isdecimal():raise ValueError('14 ASCII digits required')
    date=datetime.strptime(stamp,'%Y%m%d%H%M%S')
    if not 2024<=date.year<=2037:raise ValueError('RTC supported years: 2024-2037')
except ValueError as error:
    p.error(str(error))
print('Beijing UTC+8:',date.isoformat(sep=' '))
print('ASCII:',stamp)
print('LightBlue HEX:',stamp.encode('ascii').hex().upper())
print('Write to 0004 with response, then Read; success is result=ok, not state=pending.')
