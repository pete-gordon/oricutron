#include <time.h>
#include <stdlib.h>

#include "ds1501.h"

struct tm *ds1501_get_system_time(unsigned char delta_seconds)
{
    time_t now;
    struct tm *now_tm;

    now = time(NULL);
    now_tm = localtime(&now);
    return now_tm;
}

unsigned char ds1501_to_bcd(unsigned char binaryInput)
{
   unsigned char bcdResult = 0;
   unsigned char digit = 0;
   unsigned char i=1;

   while (binaryInput > 0) {
 
      digit = binaryInput %10;          //pick digit
      bcdResult = bcdResult+digit*i;
      i=16*i;
      binaryInput = binaryInput/ 10;
   }
   return bcdResult;
}


struct ds1501 *ds1501_create(unsigned char battery_state)
{
    int i;
    struct ds1501 *ds1501 = malloc(sizeof(struct ds1501));
    ds1501->seconds_delta_from_system=0;
    ds1501->minutes_delta_from_system=0;
    ds1501->hours_delta_from_system=0;
    ds1501->date_delta_from_system=0;
    ds1501->day_delta_from_system=0;
    ds1501->month_delta_from_system=0;
    ds1501->year_delta_from_system=0;
    ds1501->century_delta_from_system=0;
    ds1501->ctrlA_register=0;
    ds1501->ctrlB_register=0;
    ds1501_set_battery_state(ds1501,battery_state);
    // initialize RAM
    for (i=0;i<256;i++)
      ds1501->internal_ram[i]=0;
    return ds1501;
}

void ds1501_set_internal_ram_adress_register(struct ds1501 *ds1501,unsigned char address)
{
    ds1501->internal_ram_adress_register=address;
}

unsigned char ds1501_get_internal_ram(struct ds1501 *ds1501)
{
    return ds1501->internal_ram[ds1501->internal_ram_adress_register];
}

void ds1501_set_internal_ram(struct ds1501 *ds1501,unsigned char value)
{
    ds1501->internal_ram[ds1501->internal_ram_adress_register]=value;
}


void ds1501_reset(struct ds1501 *ds1501)
{
    // Get system time and set ds1501 register

}

unsigned char ds1501_get_seconds(struct ds1501 *ds1501)
{
    struct tm *now_tm;
    if (ds1501->seconds_delta_from_system==0)
        {
        now_tm=ds1501_get_system_time(ds1501->seconds_delta_from_system);
        return ds1501_to_bcd(now_tm->tm_sec);
        }
    else
        return ds1501->seconds_delta_from_system;
    
}

unsigned char ds1501_get_minutes(struct ds1501 *ds1501)
{
    struct tm *now_tm;
    if (ds1501->minutes_delta_from_system==0)
        {
        now_tm=ds1501_get_system_time(ds1501->minutes_delta_from_system);
        return ds1501_to_bcd(now_tm->tm_min);
        }
    else
        return ds1501->minutes_delta_from_system;
    
}

unsigned char ds1501_get_hours(struct ds1501 *ds1501)
{
    struct tm *now_tm;
    if (ds1501->hours_delta_from_system==0)
        {
        now_tm=ds1501_get_system_time(ds1501->hours_delta_from_system);
        return ds1501_to_bcd(now_tm->tm_hour);
        }
    else
        return ds1501->hours_delta_from_system;
}

unsigned char ds1501_get_date(struct ds1501 *ds1501)
{
    struct tm *now_tm;
    if (ds1501->date_delta_from_system==0)
        {
        now_tm=ds1501_get_system_time(ds1501->date_delta_from_system);
        return ds1501_to_bcd(now_tm->tm_mday);
        }
    else
        return ds1501->date_delta_from_system;
}

unsigned char ds1501_get_day(struct ds1501 *ds1501)
{
    struct tm *now_tm;
    if (ds1501->day_delta_from_system==0)
    {
        now_tm=ds1501_get_system_time(ds1501->day_delta_from_system);
        return ds1501_to_bcd(now_tm->tm_wday+1); // Because ds1501 return month between 1 and 12, but time.h returns 0 to 11
    }
    else
        return ds1501->day_delta_from_system;
}

unsigned char ds1501_get_month(struct ds1501 *ds1501)
{
    struct tm *now_tm;
    if (ds1501->month_delta_from_system==0)
    {
        now_tm=ds1501_get_system_time(ds1501->month_delta_from_system);
        return ds1501_to_bcd(now_tm->tm_mon+1); // Because ds1501 return month between 1 and 12, but time.h returns 0 to 11
    }
    else
        return ds1501->month_delta_from_system;
}

unsigned char ds1501_get_year(struct ds1501 *ds1501)
{
    struct tm *now_tm;
    unsigned char year;
    if (ds1501->year_delta_from_system==0)
    {    
        now_tm=ds1501_get_system_time(ds1501->year_delta_from_system);
        year=now_tm->tm_year & 0xFF;
        return ds1501_to_bcd(year); 
    }
    else
        return ds1501->year_delta_from_system;
}

unsigned char ds1501_get_century(struct ds1501 *ds1501)
{
    struct tm *now_tm;
    unsigned char century;
    if (ds1501->century_delta_from_system==0)
    {    
        now_tm=ds1501_get_system_time(ds1501->century_delta_from_system);
        century=now_tm->tm_year/100;
        return ds1501_to_bcd(century); // Because ds1501 return month between 1 and 12, but time.h returns 0 to 11
    }
    else 
        return ds1501->century_delta_from_system;

}

//write
void ds1501_write_seconds(struct ds1501 *ds1501,unsigned char seconds)
{
    ds1501->seconds_delta_from_system=seconds;
}

void ds1501_write_minutes(struct ds1501 *ds1501,unsigned char minutes)
{
    ds1501->seconds_delta_from_system=minutes;
}

void ds1501_write_hours(struct ds1501 *ds1501,unsigned char hours)
{
    ds1501->hours_delta_from_system=hours;
}

void ds1501_write_date(struct ds1501 *ds1501,unsigned char date)
{
    ds1501->date_delta_from_system=date;
}

void ds1501_write_day(struct ds1501 *ds1501,unsigned char day)
{
    ds1501->day_delta_from_system=day;
}

void ds1501_write_month(struct ds1501 *ds1501,unsigned char month)
{
    ds1501->month_delta_from_system=month;
}

void ds1501_write_year(struct ds1501 *ds1501,unsigned char year)
{
    ds1501->year_delta_from_system=year;
}

void ds1501_write_century(struct ds1501 *ds1501,unsigned char century)
{
    ds1501->century_delta_from_system=century;
}

void ds1501_set_battery_state(struct ds1501 *ds1501,unsigned char battery_state)
{
    if (battery_state==DS1501_BATTERY_FULL)
        ds1501->ctrlA_register=ds1501->ctrlA_register|0x80;
    else
        ds1501->ctrlA_register=ds1501->ctrlA_register&0x7F;
}


void ds1501_destroy(struct ds1501 *ds1501)
{

}
