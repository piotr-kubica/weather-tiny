#ifndef _fmt_h
#define _fmt_h


#include <time.h>
#include "i18n.h"


String capitalize(String str) {
    String first_letter = str.substring(0, 1);
    first_letter.toUpperCase();
    String capitalized = first_letter + str.substring(1, str.length());
    return capitalized;
}


String right_pad(String text, const int size, char pad_char=' ') {
    if (text.length() > size) {
        return text.substring(0, size);
    }
    char buff[size+1];
    String fmt = String("%-" + String(size) + "s");
    sprintf(buff, fmt.c_str(), text);
    memset(buff+text.length(), pad_char, size-text.length());
    return String(buff);
}


String left_pad(String text, const int size, char pad_char=' ') {
    if (text.length() > size) {
        return text.substring(0, size);
    }
    char buff[size+1];
    String fmt = String("%" + String(size) + "s");
    sprintf(buff, fmt.c_str(), text);
    memset(buff, pad_char, size-text.length());
    return String(buff);
}


String fmt_2f1(float f) {
	char c[4];
    sprintf(c, "%2.1f", f);
    return String(c);
}


String header_datetime(time_t* dt, bool updated) {
    struct tm *t = localtime(dt);
    char date[9+1];  // last char is '\0'
    sprintf(date, "%s %02u/%02u", get_weekday(t->tm_wday), t->tm_mday, t->tm_mon+1);
    char curr_time[5+1];  // last char is '\0'
    sprintf(curr_time, "%02u:%02u", t->tm_hour, t->tm_min);
    if (updated) {
        return String(curr_time) + " " + String(date);
    } else {
        return String(curr_time) + "!" + String(date);
    }
}


String ts2weekday(int timestamp) {
    time_t tm = timestamp;
    struct tm *now = localtime(&tm);
    return String(get_weekday(now->tm_wday));
}

// TODO refactor/extract all function below

String ts2date(int timestamp) {
    time_t tm = timestamp;
    struct tm *now = localtime(&tm);
    char ts_str[20];
    strftime(ts_str, sizeof(ts_str), "%H:%M %d/%m/%y", now);
    return String(ts_str);
}


String ts2H(int timestamp) {
    time_t tm = timestamp;
    struct tm *now = localtime(&tm);
    char ts_str[5];
    strftime(ts_str, sizeof(ts_str), "%H", now);
    return String(ts_str);
}


String ts2HM(int timestamp) {
    time_t tm = timestamp;
    struct tm *now = localtime(&tm);
    char ts_str[10];
    strftime(ts_str, sizeof(ts_str), "%H:%M", now);
    return String(ts_str);
}


String ts2dm(int timestamp) {
    time_t tm = timestamp;
    struct tm *now = localtime(&tm);
    char ts_str[10];
    strftime(ts_str, sizeof(ts_str), "%d/%m", now);
    return String(ts_str);
}


#endif
