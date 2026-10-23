#pragma once

#include <Preferences.h>

namespace VersionController
{
    inline Preferences prefs;

    inline void saveVersion(const String &version)
    {
        prefs.begin("firmware", false);
        prefs.putString("version", version);
        prefs.end();
    }

    inline String getVersion()
    {
        prefs.begin("firmware", true);
        String version = prefs.getString("version", "");
        prefs.end();

        return version;
    }

    inline void clearVersion()
    {
        prefs.begin("firmware", false);
        prefs.remove("version");
        prefs.end();
    }
}