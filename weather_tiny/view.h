#ifndef _view_h
#define _view_h


# define PERCIP_SIZE 5


struct View {

    View() {
        for (int i = 0; i < PERCIP_SIZE; i++) {
            percip[i] = "---";
            percip_time[i] = "--";
            percic_pop[i] = "---";
            percip_icon[i] = ")";
        }
    }
    
    String location = "Unknown";
    String datetime = "00:00  --- 00/00";
    unsigned int battery_percent = 0;
    String battery_percent_display = "---";

    String weather_icon = ")";
    String weather_desc = "Unknown";

    String temp_high = "--";
    String temp_low = "--";
    String temp_feel = "--";
    String temp_curr = "--";
    String temp_unit = "*";

    String pressure = "----";
    String pressure_unit = "hPa";

    String wind = "-";
    int wind_deg = 0;
    String wind_unit = "Bft";

    String aq_pm25 = "---";
    String aq_pm25_unit = "PM2.5";

    String percip_time_unit = "godz";
    String percip_unit = "mm";
    String percic_pop_unit = "%";

    String percip_time[PERCIP_SIZE];
    String percip_icon[PERCIP_SIZE];
    String percip[PERCIP_SIZE];
    String percic_pop[PERCIP_SIZE];
} ;


char meteo_font[1+9] = {
    ')',   //-1 N/A
    'B',   // 0 clear sky
    'H',   // 1 few clouds    
    'N',   // 2 scattered clouds
    'Y',   // 3 broken clouds
    'Q',   // 4 shower rain
    'R',   // 5 rain
    'O',   // 6 thunderstorm
    'W',   // 7 snow
    'L'    // 8 mist
};


char icon2meteo_font(String icon) {
    int i = 8;
    for (; i >= -1; i--) {
        if (icon.equals(openweather_icons[i])) break;
    }
    // Serial.println("Meteo font index: " + String(i+1));
    return meteo_font[i+1];
}


#endif
