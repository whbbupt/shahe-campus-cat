/* Fixed UTC+8 civil time; no host locale/timezone, Flash, SDK or OS dependency. */
#include "pet_time.h"
static bool leap(unsigned y) { return y%4==0 && (y%100!=0 || y%400==0); }
static unsigned month_days(unsigned y,unsigned m)
{
    static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return m>=1 && m<=12 ? days[m-1]+(m==2 && leap(y)) : 0;
}
bool pt_encode(const pt_date_t *d,uint32_t *utc)
{
    if(!d || !utc || d->year<2024 || d->year>2037 || d->month<1 || d->month>12 ||
       d->day<1 || d->day>month_days(d->year,d->month) || d->hour>23 || d->minute>59 || d->second>59) return false;
    uint32_t days=0;
    for(unsigned y=1970;y<d->year;y++)days+=365+leap(y);
    for(unsigned m=1;m<d->month;m++)days+=month_days(d->year,m);
    *utc=(days+d->day-1)*86400U+d->hour*3600U+d->minute*60U+d->second-28800U;
    return true;
}
bool pt_decode(uint32_t utc,pt_date_t *d)
{
    /* Bounds before addition prevent overflow and signed time_t's 2038 limit. */
    if(!d || utc<1704038400U || utc>2145887999U) return false;
    uint32_t local=utc+28800U,days=local/86400U;
    unsigned year=1970,month=1;
    while(days>=365U+leap(year))days-=365U+leap(year++);
    while(days>=month_days(year,month))days-=month_days(year,month++);
    *d=(pt_date_t){year,month,days+1,(local/3600U)%24,(local/60U)%60,local%60};
    return year>=2024 && year<=2037;
}
bool pt_parse(const uint8_t *bytes,size_t length,uint32_t *utc)
{
    if(!bytes || length!=14) return false;
    unsigned value[7]={0};
    for(unsigned i=0;i<14;i++){
        if(bytes[i]<'0' || bytes[i]>'9') return false;
        value[i/2]=value[i/2]*10+(unsigned)(bytes[i]-'0');
    }
    pt_date_t date={value[0]*100+value[1],value[2],value[3],value[4],value[5],value[6]};
    return pt_encode(&date,utc);
}
unsigned pt_minute(uint32_t utc) { return ((utc+28800U)%86400U)/60U; }
pt_band_t pt_band(uint32_t utc)
{
    unsigned m=pt_minute(utc);
    return m<360 || m>=1140 ? PT_NIGHT : m<480 ? PT_MORNING : m<1020 ? PT_DAY : PT_DUSK;
}
bool pt_class_time(uint32_t utc)
{
    unsigned m=pt_minute(utc);return (m>=480 && m<720) || (m>=840 && m<1020);
}
bool pt_meal_time(uint32_t utc)
{
    unsigned m=pt_minute(utc);return (m>=360 && m<540) || (m>=660 && m<840) || (m>=1020 && m<1200);
}
unsigned pt_bedtime(uint32_t utc)
{
    unsigned m=pt_minute(utc);return m>=1380 || m<360 ? 2 : m>=1320 ? 1 : 0;
}
bool pt_selftest(void (*report)(const char *,bool))
{
    uint32_t t=0;pt_date_t d;
    bool a=pt_parse((const uint8_t *)"20240229060000",14,&t) && pt_decode(t,&d) && d.month==2 && d.day==29;
    bool b=!pt_parse((const uint8_t *)"20260229060000",14,&t) && !pt_parse((const uint8_t *)"20380101000000",14,&t);
    bool c=pt_parse((const uint8_t *)"20261108230000",14,&t) && pt_bedtime(t)==2 && pt_band(t)==PT_NIGHT && !pt_class_time(t);
    if(report){report("calendar-leap",a);report("protocol-reject",b);report("bedtime-boundary",c);}
    return a && b && c;
}
