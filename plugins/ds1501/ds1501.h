struct ds1501 *ds1501_create(unsigned char battery_state);
unsigned char ds1501_get_seconds(struct ds1501 *ds1501);
unsigned char ds1501_get_minutes(struct ds1501 *ds1501);
unsigned char ds1501_get_hours(struct ds1501 *ds1501);
unsigned char ds1501_get_date(struct ds1501 *ds1501);
unsigned char ds1501_get_day(struct ds1501 *ds1501);
unsigned char ds1501_get_month(struct ds1501 *ds1501);
unsigned char ds1501_get_century(struct ds1501 *ds1501);
unsigned char ds1501_get_year(struct ds1501 *ds1501);
void ds1501_write_seconds(struct ds1501 *ds1501,unsigned char seconds);
void ds1501_write_minutes(struct ds1501 *ds1501,unsigned char minutes);
void ds1501_write_hours(struct ds1501 *ds1501,unsigned char hours);
void ds1501_write_date(struct ds1501 *ds1501,unsigned char date);
void ds1501_write_day(struct ds1501 *ds1501,unsigned char day);
void ds1501_write_month(struct ds1501 *ds1501,unsigned char month);
void ds1501_write_year(struct ds1501 *ds1501,unsigned char year);
void ds1501_write_century(struct ds1501 *ds1501,unsigned char century);
void ds1501_set_battery_state(struct ds1501 *ds1501,unsigned char battery_state);
void ds1501_set_internal_ram_adress_register(struct ds1501 *ds1501,unsigned char address);
unsigned char ds1501_get_internal_ram(struct ds1501 *ds1501);
void ds1501_set_internal_ram(struct ds1501 *ds1501,unsigned char value);

#define DS1501_BATTERY_FULL  0
#define DS1501_BATTERY_EMPTY 1


struct ds1501
{
    unsigned char  ctrlA_register;
    unsigned char  ctrlB_register;
    unsigned char  seconds_delta_from_system;
    unsigned char  minutes_delta_from_system;
    unsigned char  hours_delta_from_system;
    unsigned char  day_delta_from_system;
    unsigned char  date_delta_from_system;
    unsigned char  month_delta_from_system;
    unsigned char  year_delta_from_system;
    unsigned char  century_delta_from_system;
    unsigned char  internal_ram_adress_register;
    unsigned char  internal_ram[256];

};

