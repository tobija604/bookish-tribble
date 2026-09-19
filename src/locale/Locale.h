#pragma once
// i18n string table (spec section 49). Primary: Slovenian + English now;
// German/Italian/Croatian/Serbian are stubbed with English fallback so the
// lookup never returns an empty string — add their tables as translations
// become available, the keys are already all any screen needs.
#include <Arduino.h>
#include <map>
#include "../config/ConfigManager.h"

class Locale {
 public:
  static Locale& instance() {
    static Locale l;
    return l;
  }

  String t(const String& key) {
    const String& lang = ConfigManager::instance().get().locale.language;
    auto langIt = table_.find(lang);
    if (langIt != table_.end()) {
      auto it = langIt->second.find(key);
      if (it != langIt->second.end()) return it->second;
    }
    auto en = table_.find("en");
    if (en != table_.end()) {
      auto it = en->second.find(key);
      if (it != en->second.end()) return it->second;
    }
    return key; // last resort: show the key itself rather than a blank label
  }

 private:
  Locale() { seed(); }
  std::map<String, std::map<String, String>> table_;

  void seed() {
    table_["sl"] = {
      {"home.locked", "ZAKLENJENO"}, {"home.unlocked", "ODKLENJENO"},
      {"home.closed", "ZAPRTO"}, {"home.open", "ODPRTO"},
      {"menu.title", "MENI"},
      {"menu.status", "STATUS"}, {"menu.unlock", "ODKLENI"}, {"menu.lock", "ZAKLENI"},
      {"menu.rfid", "RFID"}, {"menu.users", "UPORABNIKI"}, {"menu.events", "DOGODKI"},
      {"menu.settings", "NASTAVITVE"}, {"menu.network", "OMREŽJE"}, {"menu.security", "VARNOST"},
      {"menu.device", "NAPRAVA"}, {"menu.diagnostics", "DIAGNOSTIKA"}, {"menu.about", "O NAPRAVI"},
      {"wifi.setup", "NASTAVITEV WIFI"}, {"wifi.connected", "WIFI POVEZAN"},
      {"web.locked", "WEB ZAKLENJEN"}, {"web.holdbutton1", "DRŽI GUMB 1"},
      {"error.lock", "NAPAKA ZAKLEPA"}, {"scan.card", "PRIBLIŽAJ KARTICO"},
    };
    table_["en"] = {
      {"home.locked", "LOCKED"}, {"home.unlocked", "UNLOCKED"},
      {"home.closed", "CLOSED"}, {"home.open", "OPEN"},
      {"menu.title", "MENU"},
      {"menu.status", "STATUS"}, {"menu.unlock", "UNLOCK"}, {"menu.lock", "LOCK"},
      {"menu.rfid", "RFID"}, {"menu.users", "USERS"}, {"menu.events", "EVENTS"},
      {"menu.settings", "SETTINGS"}, {"menu.network", "NETWORK"}, {"menu.security", "SECURITY"},
      {"menu.device", "DEVICE"}, {"menu.diagnostics", "DIAGNOSTICS"}, {"menu.about", "ABOUT"},
      {"wifi.setup", "WIFI SETUP"}, {"wifi.connected", "WIFI CONNECTED"},
      {"web.locked", "WEB LOCKED"}, {"web.holdbutton1", "HOLD BUTTON 1"},
      {"error.lock", "LOCK ERROR"}, {"scan.card", "SCAN CARD"},
    };
    // DE/IT/HR/SR: intentionally left out of `table_` for now so lookups
    // fall through to the English table above (see t()); fill these in as
    // real translations land instead of shipping machine-guessed strings.
  }
};
