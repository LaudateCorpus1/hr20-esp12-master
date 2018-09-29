#include "config.h"

void ICACHE_FLASH_ATTR Config::begin()
{
#ifdef MQTT
    sprintf(mqtt_client_id, "%s%08x", mqtt_client_id_prefix, ESP.getChipId());
#endif
}
bool ICACHE_FLASH_ATTR Config::jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0)
    {
        return true;
    }
    return false;
}
uint8_t ICACHE_FLASH_ATTR Config::hex2int(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}
char *ICACHE_FLASH_ATTR Config::get_rfm_pass_value()
{
    for (int i = 0; i < 8; i++)
    {
        sprintf(rfmPassValue + 2 * i, "%02X", rfm_pass[i]);
    }
    return rfmPassValue;
}
bool ICACHE_FLASH_ATTR Config::set_rfm_pass(const char *rfmPass)
{
    uint8_t tmp[8];
    for (int j = 0; j < 16; j = j + 2)
    {
        uint8_t x0 = hex2int(rfmPass[j]);
        uint8_t x1 = hex2int(rfmPass[j + 1]);
        if (x0 >= 0 && x1 >= 0)
        {
            tmp[j / 2] = x0 * 0x10 + x1;
        }
        else
        {
            ERR("Invalid char in RFM password");
            return false;
        }
    }
    memcpy(rfm_pass, tmp, 8 * sizeof(uint8_t));
    return true;
}
bool ICACHE_FLASH_ATTR Config::save(const char *filename)
{
    SPIFFS.begin();
    File file = SPIFFS.open(filename, "w");
    if (!file)
    {
        ERR("Config file open failed");
        SPIFFS.end();
        return false;
    }
    else
    {
        DBG("writing to file: %s", filename);
        file.printf("{"
#ifdef NTP_CLIENT
                    "\"ntp_server\":\"%s\","
#endif
#ifdef MQTT
                    "\"mqtt_server\":\"%s\","
                    "\"mqtt_port\":%u,"
                    "\"mqtt_user\":\"%s\","
                    "\"mqtt_pass\":\"%s\","
                    "\"mqtt_topic_prefix\":\"%s\","
#endif
                    "\"rfm_pass\":\"%s\""
                    "}\n",
#ifdef NTP_CLIENT
                    ntp_server,
#endif
#ifdef MQTT
                    mqtt_server, mqtt_port, mqtt_user, mqtt_pass, mqtt_topic_prefix,
#endif
                    get_rfm_pass_value());

        DBG("Configuration written: {"
#ifdef NTP_CLIENT
            "\"ntp_server\":\"%s\","
#endif
#ifdef MQTT
            "\"mqtt_server\":\"%s\","
            "\"mqtt_port\":%u,"
            "\"mqtt_user\":\"%s\","
            "\"mqtt_pass\":\"%s\","
            "\"mqtt_topic_prefix\":\"%s\","
#endif
            "\"rfm_pass\":\"%s\""
            "}",
#ifdef NTP_CLIENT
            ntp_server,
#endif
#ifdef MQTT
            mqtt_server, mqtt_port, mqtt_user, mqtt_pass, mqtt_topic_prefix,
#endif
            get_rfm_pass_value());

        file.close();
        SPIFFS.end();
    }

    return true;
}

bool ICACHE_FLASH_ATTR Config::load(const char *filename)
{
    const char *JSON_STRING;
    String json;
    DBG("Reading config");
    SPIFFS.begin();
    File file = SPIFFS.open(filename, "r");
    if (file)
    {
        json = file.readStringUntil('\n');
        file.close();
        SPIFFS.end();
    }
    else
    {
        ERR("Config file not found.");
        SPIFFS.end();
        return false;
    }

    DBG("Parsing JSON");
    JSON_STRING = json.c_str();
    int i;
    int r;
    jsmn_parser p;
    jsmntok_t t[32]; /* We expect no more than 32 tokens */

    jsmn_init(&p);
    r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t, sizeof(t) / sizeof(t[0]));
    if (r < 0)
    {
        ERR("Failed to parse JSON: %d", r);
        return false;
    }
    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_OBJECT)
    {
        ERR("Invalid JSON. Object expected");
        return false;
    }

    /* Loop over all keys of the root object */
    for (i = 1; i < r; i++)
    {
        uint16_t offset = t[i + 1].start;
        uint16_t len = t[i + 1].end - t[i + 1].start;
        if (jsoneq(JSON_STRING, &t[i], "rfm_pass"))
        {
            if (len != 16)
            {
                ERR("Invalid RFM password length");
                return false;
            }
            if (!set_rfm_pass(JSON_STRING + offset))
            {
                return false;
            }
            i++;
        }
#ifdef NTP_CLIENT
        else if (jsoneq(JSON_STRING, &t[i], "ntp_server"))
        {
            strncpy(ntp_server, JSON_STRING + offset, len);
            ntp_server[len] = 0;
            i++;
        }
#endif
#ifdef MQTT
        else if (jsoneq(JSON_STRING, &t[i], "mqtt_server"))
        {
            strncpy(mqtt_server, JSON_STRING + offset, len);
            mqtt_server[len] = 0;

            i++;
        }
        else if (jsoneq(JSON_STRING, &t[i], "mqtt_port"))
        {
            char val[6];
            strncpy(val, JSON_STRING + offset, len);
            val[len] = 0;
            mqtt_port = (uint16_t)atoi(val);
            i++;
        }
        else if (jsoneq(JSON_STRING, &t[i], "mqtt_user"))
        {
            strncpy(mqtt_user, JSON_STRING + offset, len);
            mqtt_user[len] = 0;
            i++;
        }
        else if (jsoneq(JSON_STRING, &t[i], "mqtt_pass"))
        {
            strncpy(mqtt_pass, JSON_STRING + offset, len);
            mqtt_pass[len] = 0;
            i++;
        }
        else if (jsoneq(JSON_STRING, &t[i], "mqtt_topic_prefix"))
        {
            strncpy(mqtt_topic_prefix, JSON_STRING + offset, len);
            mqtt_topic_prefix[len] = 0;
            i++;
        }
#endif
        else
        {
            DBG("Unexpected key: %.*s", len, JSON_STRING + offset);
        }
    }
    DBG("Configuration loaded");
    return true;
}