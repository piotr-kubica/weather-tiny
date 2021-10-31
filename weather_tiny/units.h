#ifndef _units_h
#define _units_h


int wind_ms2bft(float ms) {
    //return Math.ceil(Math.cbrt(Math.pow(ms/0.836, 2)));
    const float third = 0.33333333f;
    return ceil(pow(pow(ms/0.836,2), third));
}


int kelv2cels(float kelvin) {
    return round(kelvin -273.15);
}

float kelv2cels1(float kelvin) {
    return round((kelvin -273.15)*10)/10.0;
}

#endif
