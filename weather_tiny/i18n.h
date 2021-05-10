#ifndef _i18n_h
#define _i18n_h


#define LANG_EN 0 

int LANG = LANG_EN;

const char* LANGS[] = { "en" };

const char* WEEKDAYS[][7] = {
    { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" },
};

const char* get_weekday(int day) {
    return WEEKDAYS[LANG][day];
}

#endif
