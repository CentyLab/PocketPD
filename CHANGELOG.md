# Changelog

All notable changes to PocketPD v2 firmware.

## 2.1.0

### Added

- **Cable voltage-drop compensation on PPS profiles.** PocketPD learns an offset that counters IR drop across the cable, holding device-end voltage at the setpoint. It is off by default. User can enable it in Settings by going to Menu -> Settings. 
- **non-PD passthrough.** When connected to non-PD sources, PocketPD enters passthrough mode and outputs source VBUS with no PD target instead of displaying "No profile detected". This makes PocketPD still usable as monitoring device.
- **Skip-picker-on-boot.** With this enabled, PocketPD boots straight to the normal view using the first PDO profile (5V) instead of showing the profile picker. User can enable this option in Settings page. 
- **Menu and Settings** Menu and Settings are new screens. Menu replaces L-LONG to expand PocketPD capabilities.

### Under the hood

- AP33772 PD negotiation timeout default lowered from 10s to 5s; `begin()` takes the poll budget as a parameter.
- `PreferencesStore` persists settings to EEPROM (layout-version byte + CRC8).
- Shared `TableView` for the Menu and Settings list screens.
- Nav back-stack lives on the Conductor; stages receive `now_ms` at `on_enter`/`on_exit` and arm timers there.
- `make` target to copy the built `.uf2` to `dist/`.

## 2.0.1

- Extend AP33772 `begin()` poll timeout to 10s for slow chargers.
