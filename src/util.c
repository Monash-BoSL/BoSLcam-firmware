
#include "util.h"
#include "errno.h"

void unix_date(struct tm* cal, int32_t unixtime) {
  uint32_t seconds, minutes, hours, days, year, month;
  uint32_t day_of_week;
  seconds = unixtime;

  minutes  = seconds / 60;
  hours    = minutes / 60;
  days     = hours   / 24;

  seconds = seconds % 60;
  minutes = minutes % 60;
  hours   = hours % 24;

  /* Unix time starts in 1970 on a Thursday */
  year      = 1970;
  day_of_week = 4;

  while(1)
  {
    int     leap_year   = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint16_t days_in_year = leap_year ? 366 : 365;
    if (days >= days_in_year)
    {
      day_of_week += leap_year ? 2 : 1;
      days      -= days_in_year;
      if (day_of_week >= 7)
        day_of_week -= 7;
      ++year;
    }
    else
    {
      cal->tm_yday = days;
      day_of_week  += days;
      day_of_week  %= 7;

      /* calculate the month and day */
      static const uint8_t days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for(month = 0; month < 12; ++month)
      {
        uint8_t dim = days_in_month[month];

        /* add a day to feburary if this is a leap year */
        if (month == 1 && leap_year)
          ++dim;

        if (days >= dim)
          days -= dim;
        else
          break;
      }
      break;
    }
  }

  cal->tm_sec  = seconds;
  cal->tm_min  = minutes;
  cal->tm_hour = hours;
  cal->tm_mday = days + 1;
  cal->tm_mon  = month;
  cal->tm_year = year-1900;//for correct callender epoch
  cal->tm_wday = day_of_week;
}


int strfstatus(char* buf, const size_t len, const struct status_t* const status, const struct capture_task_t* const capture_task) {
    struct tm cal;

    unix_date(&cal, status->time_wall);
    const size_t timelen = strftime(buf, len, "%Y/%m/%d-%H:%M:%S UTC", &cal);
    if (!timelen){return -ERANGE;}
    const int printret = snprintf(
                buf+timelen, 
                len-timelen,
                ",%s,%d,%d,%s,%d,%d,%s\n",
                strftimesource(status->time_src),
                status->captures,
                status->battery_voltage,
                status->mccmnc,
                status->rsrq,
                status->rsrp,
                strftrigger(capture_task->trigger)
            );
    if (printret < 0){ return printret; }
    else if(printret >= len){ return -ERANGE; }
    else { return 0; }
}