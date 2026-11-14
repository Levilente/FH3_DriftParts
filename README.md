# Drift Suspension and Differential (FH3)

The **Drift Suspension** and **Drift Differential** of Forza Horizon 6, added to **Forza Horizon 3** (Xbox One build on PC, through Azimuth) on **every car**, on the **[FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform)**.

> Version **1.2.0**. Needs the FH3 Mod Platform **1.2.0** or newer.

## What it adds

| Where | Choice | What it is |
|---|---|---|
| Platform and Handling > Springs and Dampers | **Drift Suspension** | the car's own Race coilovers (same tuning ranges) with an angle kit and an FH6-style drift baseline: **60° steering lock** (stock cars 30-42°) and about 2x faster steering (210/230/280 °/s turning/straightening/counter-steering instead of 105), camber front -5.0° / rear -1.0°, toe front 0.5° out / rear 0.2° in, caster 7.0°, lowered (15 % up the ride-height range), springs front 0.90x / rear 0.80x, dampers front 0.85x / rear 0.80x |
| Drivetrain > Differential | **Drift Differential** | the car's Race diff: accel lock 4x Race (at least 6000 Nm, locked), decel lock 15 % of that (loose off throttle); FWD/RWD front the same; AWD keeps half the Race front lock and at least 80 % rear torque split |

Price and weight are the same as the car's Race part.

## Install

Put `DriftParts.dll` into the `fh3-mods` folder and start the game **twice** (the first start builds the changed database and the texts, the game reads them at the next start). No settings file.

## Build

    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=platform/cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build build

with the FH3 Mod Platform checkout at `./platform`.

## How it works

Pure data: SQL queued with `add_sql`, applied by the platform to the database copy in `mediaoverride`. Only new rows are added (level 5 in `List_UpgradeSpringDamper` with new `List_SpringDamperPhysics` rows, level 4 in `List_UpgradeDrivetrainDifferential`, two `Upgrades` rows, four texts). Code hooks: the steering lock (the car loader `game+0x19E6A80` stores Data_Car's steering angles at spec `+0x5C`/`+0x60` before it builds the springs; the mod notes at `0x19F4BA2` whether the front spring physics id is a drift suspension one (`Ordinal*1000+5`) and raises both angles at `0x19E8028`), and `game+0x16CF7EF`, which keeps the shop's comparison panel inside its 4-entry differential level-name table.
