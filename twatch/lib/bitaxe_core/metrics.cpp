#include "metrics.h"

#include <stdio.h>

bool metrics_efficiency(const BitaxeStatus &s, float &joulesPerTerahash)
{
    if (s.hashRate10m < METRICS_MIN_HASHRATE_GHS) {
        return false;
    }
    // A API entrega Gh/s e watts; J/TH = W / (Gh/s / 1000).
    joulesPerTerahash = s.power / (s.hashRate10m / 1000.0f);
    return true;
}

Freshness metrics_freshness(uint32_t ageSeconds)
{
    if (ageSeconds < 60) {
        return FRESHNESS_NOW;
    }
    if (ageSeconds < 600) {
        return FRESHNESS_AGING;
    }
    return FRESHNESS_STALE;
}

void metrics_format_hashrate(float ghs, char *buf, size_t cap)
{
    if (ghs >= 1000.0f) {
        snprintf(buf, cap, "%.2f Th/s", ghs / 1000.0f);
    } else {
        snprintf(buf, cap, "%.1f Gh/s", ghs);
    }
}

void metrics_format_hashrate_parts(float ghs, char *value, size_t vcap,
                                   char *unit, size_t ucap)
{
    if (ghs >= 1000.0f) {
        snprintf(value, vcap, "%.2f", ghs / 1000.0f);
        snprintf(unit, ucap, "Th/s");
    } else {
        snprintf(value, vcap, "%.1f", ghs);
        snprintf(unit, ucap, "Gh/s");
    }
}
