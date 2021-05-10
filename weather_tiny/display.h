#ifndef _display_h
#define _display_h

#include <GxEPD.h>
#include "view.h"


struct WindArrow {
    int x = 0, y = 0;
    int scale = 1;
    int offset = -3;
    float deg2rad = 0.01745;

    // 2 triangle coords. Center is (3, 3)
    float x0 = 1 +offset, y0 = 0 +offset;  // top left
    float x1 = 3 +offset, y1 = 2 +offset;  // bottom
    float x2 = 3 +offset, y2 = 6 +offset;  // top
    float x3 = 5 +offset, y3 = 0 +offset;  // top right

    void rotate(int alpha) {
        float rad = alpha * deg2rad;
        Serial.printf("WindArrow rotate by %d deg (%.2f rad)\n", alpha, rad);
        _rotate_point(rad, &x0, &y0);
        _rotate_point(rad, &x1, &y1);
        _rotate_point(rad, &x2, &y2);
        _rotate_point(rad, &x3, &y3);
    }
    
    void draw(int x, int y, GxEPD_Class& display) {
        this->x = x;
        this->y = y;
        display.fillTriangle(x+x0*scale, y+y0*scale, x+x1*scale, y+y1*scale, x+x2*scale, y+y2*scale, GxEPD_BLACK);
        display.fillTriangle(x+x1*scale, y+y1*scale, x+x2*scale, y+y2*scale, x+x3*scale, y+y3*scale, GxEPD_BLACK);
    }


    private:
    
    void _rotate_point(float rad, float *x, float *y) {
        float xc = *x - this->x;
        float yc = *y - this->y;
        
        // p'x = cos(theta) * (px-ox) - sin(theta) * (py-oy) + ox
        // p'y = sin(theta) * (px-ox) + cos(theta) * (py-oy) + oy
        *x = cos(rad) * xc - sin(rad) * yc + this->x;
        *y = sin(rad) * xc + cos(rad) * yc + this->y;

        // Serial.printf("rot %.2f: A(%.2f, %.2f)  B(%.2f, %.2f)  C(%.2f, %.2f)  D(%.2f, %.2f)\n", rad, x0, y0, x1, y1, x2, y2, x3, y3);
    }
} ;


int display_battery_icon(int x, int y, GxEPD_Class& display, int percent) {
    const int icon_width = 24;
    const int icon_height = 12;
    const int bar_width = 6;
    const int bar_height = 8;
    const int bar_margin = 2;
    const int plus_rectangle_width = 4;

    // icon rectangle
    display.drawRect(x, y, icon_width, icon_height, GxEPD_BLACK);

    // the small battery plus-side rectangle
    display.fillRoundRect(x+icon_width-1, y+3, plus_rectangle_width, icon_height-2*3, 2, GxEPD_BLACK);

    // draw charging icon
    if (percent > 100) {
        const int margin = 2;
        const int half_w = icon_width / 2;
        const int half_h = icon_height / 2;

        display.fillTriangle(x-margin+half_w, y+half_h, x-margin+half_w, y+half_h/2, x+half_w+half_w/2, y+half_h, GxEPD_BLACK);
        display.fillTriangle(x+margin+half_w, y+half_h, x+margin+half_w, y+half_h+half_h/2, x+half_w-half_w/2, y+half_h, GxEPD_BLACK);
        return icon_width + plus_rectangle_width;
    }

    // 3-level percent bar
    if (percent > 5) {
        display.fillRect(x+bar_margin+0*(bar_width+1), y+bar_margin, bar_width, bar_height, GxEPD_BLACK);
    }
    if (percent > 35) {
        display.fillRect(x+bar_margin+1*(bar_width+1), y+bar_margin, bar_width, bar_height, GxEPD_BLACK);
    }
    if (percent > 70) {
        display.fillRect(x+bar_margin+2*(bar_width+1), y+bar_margin, bar_width, bar_height, GxEPD_BLACK);
    }
    return icon_width + plus_rectangle_width;
}


int get_text_width(String text) {
    uint16_t width, height;
    int16_t  xb, yb;
    display.getTextBounds(text, display.getCursorX(), display.getCursorY(), &xb, &yb, &width, &height);
    return width;
}


int get_text_height(String text) {
    uint16_t width, height;
    int16_t  xb, yb;
    display.getTextBounds(text, display.getCursorX(), display.getCursorY(), &xb, &yb, &width, &height);
    return height;
}


int position_text(int x, int y, String text) {
    uint16_t width, height;
    int16_t  xb, yb;
    display.getTextBounds(text, x, y, &xb, &yb, &width, &height);
    display.setTextWrap(false);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x, y + height);
    return width;
}


void print_text(int x, int y, String text) {
    position_text(x, y, text);
    display.print(text);
}


void print_text(String text) {
    print_text(display.getCursorX(), display.getCursorY(), text);
}


void print_text_y(int y, String text) {
    print_text(display.getCursorX(), y, text);
}


void print_text_x(int x, String text) {
    print_text(x, display.getCursorY(), text);
}


void init_display() {
    Serial.print("init_display...");
    SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
    // enable diagnostic output on Serial 
    display.init(115200);
    display.setRotation(3);
    display.setTextWrap(false);
    display.fillScreen(GxEPD_WHITE);
    Serial.println(" completed");
}


void display_config_mode(String network, String pass, String ip) {
    display.setFont(&monofonto18pt7b);
    print_text(50, 0, "Welcome!");
    display.setFont(&Cousine_Regular6pt7b);
    print_text(0, 8, String("Connect to weather station...") + "\nSSID: " + network + "\nPass: " + pass);
    // print_text(0, 65, String("and configure... \n") + "http://" + ip + "/config");
}


void display_validating_mode() {
    display.setFont(&monofonto10pt7b);
    print_text(35, 0, "Validating");
    print_text_x(35, "configuration...");
    display.setFont(&Cousine_Regular6pt7b);
    print_text(35, display.getCursorY()+10, "Please, wait!");
}


void display_header(View& view) {
    display.setFont(&monofonto10pt7b);
    print_text(0, -3, view.location);
    print_text(SCREEN_WIDTH - get_text_width(view.datetime)-3, -12, view.datetime);
    int batt_x = SCREEN_WIDTH - get_text_width(view.datetime) - 33;
    display_battery_icon(batt_x, 3, display, view.battery_percent);
    // display_battery_percentage
    // print_text(batt_x, 3, view.battery_percent_display);  
}


void display_weather(View& view) {
    display.setFont(&meteocons_webfont10pt7b);
    print_text(2, 21, view.weather_icon);
    
    display.setFont(&Cousine_Regular6pt7b);
    print_text(30, 24, view.weather_desc);

    display.setFont(&monofonto18pt7b);
    print_text(30, 45, view.temp_curr);

    display.setFont(&meteocons_webfont8pt7b);
    print_text(78, 48, view.temp_unit);
    
    display.setFont(&Cousine_Regular6pt7b);
    print_text(0, 45, view.temp_high);
    print_text(0, 55, view.temp_low);
    print_text(0, 65, view.temp_feel);

    // pressure
    display.setFont(&monofonto12pt7b);
    print_text(100, 45, view.pressure);
    
    display.setFont(&Cousine_Regular6pt7b);
    print_text(125, 65, view.pressure_unit);

    // wind
    display.setFont(&monofonto12pt7b);    
    print_text(180, 45, view.wind);
    WindArrow wind_arrow;
    wind_arrow.rotate(view.wind_deg);
    wind_arrow.scale = 4;
    wind_arrow.draw(172, 55, display);

    display.setFont(&Cousine_Regular6pt7b);
    print_text(183, 65, view.wind_unit);
    
    // hourly rain forecast
    display.setFont(&Cousine_Regular6pt7b);
    print_text(0, 82, view.percip_time_unit);
    print_text(0, 102, view.percip_unit);
    print_text(0, 112, view.percic_pop_unit);

    print_text(50-7, 85, view.percip_time[0]);
    print_text(90-5, 85, view.percip_time[1]);
    print_text(130-3, 85, view.percip_time[2]);
    print_text(170-1, 85, view.percip_time[3]);
    print_text(210+1, 85, view.percip_time[4]);

    print_text(50-7, 102, view.percip[0]);
    print_text(90-5, 102, view.percip[1]);
    print_text(130-3, 102, view.percip[2]);
    print_text(170-1, 102, view.percip[3]);
    print_text(210+1, 102, view.percip[4]);

    print_text(50-7, 112, view.percic_pop[0]);
    print_text(90-5, 112, view.percic_pop[1]);
    print_text(130-3, 112, view.percic_pop[2]);
    print_text(170-1, 112, view.percic_pop[3]);
    print_text(210+1, 112, view.percic_pop[4]);

    display.setFont(&meteocons_webfont10pt7b);
    print_text(61, 82, view.percip_icon[0]);
    print_text(103, 82, view.percip_icon[1]);
    print_text(145, 82, view.percip_icon[2]);
    print_text(187, 82, view.percip_icon[3]);
    print_text(229, 82, view.percip_icon[4]);
}


void display_air_quality(View& view) {
    display.setFont(&monofonto12pt7b);
    print_text(215, 45, view.aq_pm25);

    display.setFont(&Cousine_Regular6pt7b);
    print_text(215, 65, view.aq_pm25_unit);
}


#endif
