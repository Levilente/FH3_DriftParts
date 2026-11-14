# Changelog

## 1.0.0

First release of **Drift Suspension and Differential for Forza Horizon 3**, on the [FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform).

- **Garage > Upgrades > Platform and Handling > Springs and Dampers** gets **Drift Suspension** on every car that has Race suspension: the car's Race coilovers (same tuning ranges) with a stiffer front, a softer rear and 7.0° front caster.
- **Garage > Upgrades > Drivetrain > Differential** gets **Drift Differential** on every drivetrain that has a Race differential: a near-locked diff (4x the Race lock, at least 6000 Nm on power / 3000 Nm off power); AWD cars keep half the Race front lock and send at least 80 % of the torque to the rear.
- Same price and weight as the Race parts.
- Pure data (`add_sql`), only new rows; one small code hook so the shop's comparison panel names the new differential level correctly.
