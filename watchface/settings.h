#ifndef SETTINGS_H
#define SETTINGS_H

//Weather Settings
//Location: Cambridge, Cambridgeshire, UK (city centre)
//You can use CITY_ID (https://openweathermap.org/current#cityid) or LAT,LON, but not both
#define LAT "52.2053"
#define LON "0.1218"

#ifdef CITY_ID
    #define OPENWEATHERMAP_URL "http://api.openweathermap.org/data/2.5/weather?id={cityID}&lang={lang}&units={units}&appid={apiKey}" //open weather api using city ID
#else
    #define OPENWEATHERMAP_URL "http://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&lang={lang}&units={units}&appid={apiKey}" //open weather api using lat lon
#endif

#define OPENWEATHERMAP_APIKEY "f058fe1cad2afe8e2ddc5d063a64cecb" //use your own API key :)
#define TEMP_UNIT "metric" //metric = Celsius , imperial = Fahrenheit
#define TEMP_LANG "en"
#define WEATHER_UPDATE_INTERVAL 30 //must be greater than 5, measured in minutes
//NTP Settings
#define NTP_SERVER "pool.ntp.org"
//UK time: GMT (UTC+0) in winter, BST (UTC+1) from the last Sunday of March at 01:00
//to the last Sunday of October at 02:00. Applied whenever the watch syncs its time.
#define TIMEZONE "GMT0BST,M3.5.0/1,M10.5.0"
#define GMT_OFFSET_SEC 0 //used only until the first time sync; TIMEZONE takes over after that

//WiFi Settings
//Your network name and password go in wifi_secrets.h (copy wifi_secrets.example.h).
//That file is never committed, because this repository is public.
//Without it, the watch uses the network saved through the "Setup WiFi" menu.
#if __has_include("wifi_secrets.h")
    #include "wifi_secrets.h"
#endif
#ifndef WIFI_SSID
    #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
    #define WIFI_PASSWORD ""
#endif

watchySettings settings{
    #ifdef CITY_ID
        .cityID = CITY_ID,
    #else
        .cityID = "",
        .lat = LAT,
        .lon = LON,
    #endif
    .weatherAPIKey = OPENWEATHERMAP_APIKEY,
    .weatherURL = OPENWEATHERMAP_URL,
    .weatherUnit = TEMP_UNIT,
    .weatherLang = TEMP_LANG,
    .weatherUpdateInterval = WEATHER_UPDATE_INTERVAL,
    .ntpServer = NTP_SERVER,
    .gmtOffset = GMT_OFFSET_SEC,
    .vibrateOClock = true,
    .wifiSSID = WIFI_SSID,
    .wifiPassword = WIFI_PASSWORD,
    .timezone = TIMEZONE,
};

#endif