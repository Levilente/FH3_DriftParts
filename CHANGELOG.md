# Changelog

## 1.2.0

- Faster steering with the Drift Suspension: turning 210, straightening 230 and counter-steering 280 degrees per second (every FH3 car has 105), so the bigger lock is reached and counter-steer kicks in quicker.

## 1.1.0

- Drift Suspension now has an angle kit: **60° steering lock** on the car it's fitted to (FH3's stock cars have 30-42°, its drift Horizon Editions 50°). Both the simulation and the normal steering angle are raised; AI cars, loaners and the shop preview follow the part the car is built with.
- FH6-style drift defaults on the Drift Suspension: camber front -5.0° / rear -1.0°, toe front 0.5° out / rear 0.2° in, caster 7.0°, lowered, softer springs and dampers (front 0.90x/0.85x, rear 0.80x/0.80x).
- Drift Differential: decel lock lowered to 15 % of the (locked) accel lock, so the car stays loose off the throttle.

## 1.0.0

First release of **Drift Suspension and Differential for Forza Horizon 3**, on the [FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform).

- **Garage > Upgrades > Platform and Handling > Springs and Dampers** gets **Drift Suspension** on every car that has Race suspension: the car's Race coilovers (same tuning ranges) with a stiffer front, a softer rear and 7.0° front caster.
- **Garage > Upgrades > Drivetrain > Differential** gets **Drift Differential** on every drivetrain that has a Race differential: a near-locked diff (4x the Race lock, at least 6000 Nm on power / 3000 Nm off power); AWD cars keep half the Race front lock and send at least 80 % of the torque to the rear.
- Same price and weight as the Race parts.
- Pure data (`add_sql`), only new rows; one small code hook so the shop's comparison panel names the new differential level correctly.
