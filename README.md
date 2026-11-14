# Drift Suspension and Differential (FH3)

The **Drift Suspension** and **Drift Differential** of Forza Horizon 6, added to **Forza Horizon 3** (Xbox One build on PC, through Azimuth) on **every car**, on the **[FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform)**.

> Version **1.0.0**. Needs the FH3 Mod Platform **1.2.0** or newer.

## What it adds

| Where | Choice | What it is |
|---|---|---|
| Platform and Handling > Springs and Dampers | **Drift Suspension** | the car's own Race coilovers (same tuning ranges), front springs/dampers 1.10x, rear springs 0.75x, rear bump 0.85x, rear rebound 0.80x, front caster 7.0° |
| Drivetrain > Differential | **Drift Differential** | the car's Race diff, near-locked: rear 4x the Race lock (at least 6000 Nm accel / 3000 Nm decel); FWD/RWD front the same; AWD keeps half the Race front lock and at least 80 % rear torque split |

Price and weight are the same as the car's Race part.

## Install

Put `DriftParts.dll` into the `fh3-mods` folder and start the game **twice** (the first start builds the changed database and the texts, the game reads them at the next start). No settings file.

## Build

    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=platform/cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build build

with the FH3 Mod Platform checkout at `./platform`.

## How it works

Pure data: SQL queued with `add_sql`, applied by the platform to the database copy in `mediaoverride`. Only new rows are added (level 5 in `List_UpgradeSpringDamper` with new `List_SpringDamperPhysics` rows, level 4 in `List_UpgradeDrivetrainDifferential`, two `Upgrades` rows, four texts). One instruction hook (`game+0x16CF7EF`) keeps the shop's comparison panel inside its 4-entry differential level-name table.
