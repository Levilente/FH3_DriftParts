// Drift Suspension and Drift Differential for Forza Horizon 3 (FH3 Mod Platform mod).
//
// Forza Horizon 6 has a "Drift Suspension" (Platform and Handling > Springs and Dampers) and a "Drift Differential"
// (Drivetrain > Differential) next to the Race parts. FH3 has neither (its levels are Stock, Street, Sport, Race, plus Offroad
// suspension from Blizzard Mountain). This mod adds both, on every car, as a new choice in the game's own upgrade lists:
//
//   - Drift Suspension: a copy of the car's own Race springs and dampers (fully adjustable in Tuning, same ranges) with an FH6-style
//     drift baseline: 60 degree steering lock (angle kit), front camber -5.0, rear -1.0, front toe 0.5 out, rear 0.2 in, caster
//     7.0, lowered (15 % up the ride-height range), softer springs (front 0.90x, rear 0.80x) and dampers (0.85x / 0.80x).
//   - Drift Differential: a copy of the car's Race differential, accel lock 4x Race (at least 6000 Nm, "100 %"), decel lock 15 %
//     of that, so both rear wheels spin together on power and the car stays loose off power. FWD/RWD cars lock the front the same
//     way; AWD cars keep half of the Race front lock and send at least 80 % of the torque to the rear.
//
// How it works: pure data, like Drag and Drift Tires. The mod queues SQL for the platform (add_sql), which applies it to the copy of
// gamedbRC.slt the game reads (mediaoverride; the game's own files are never touched). Only new rows are added: a new level in
// List_UpgradeSpringDamper (5) and List_UpgradeDrivetrainDifferential (4), new List_SpringDamperPhysics rows, new Upgrades rows
// and new texts. One small code hook: the shop's part comparison names the differential level from a 4-entry table (Stock, Street,
// Sport, Race); for the new level 4 it would read past that table, so the hook makes it use the Race entry.
#include "fh3mod.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int kSpringLevel     = 5;         // List_UpgradeSpringDamper.Level of the drift suspension (Race = 3, Offroad = 4)
constexpr int kDiffLevel       = 4;         // List_UpgradeDrivetrainDifferential.Level of the drift differential (Race = 3)
constexpr int kSpringUpgradeId = 9000012;   // Upgrades.id rows of this mod (Drag and Drift Tires uses 9000010 and 9000011)
constexpr int kDiffUpgradeId   = 9000013;
constexpr int kTypeSprings     = 13;        // UpgradeTypes.id of Springs and Dampers (part enum 5)
constexpr int kTypeDiff        = 19;        // UpgradeTypes.id of Differential (part enum 33)

constexpr uint32_t kKeySpringName = 0xD5F70101u;   // keys of the new texts in Upgrades.str
constexpr uint32_t kKeySpringDesc = 0xD5F70102u;
constexpr uint32_t kKeyDiffName   = 0xD5F70103u;
constexpr uint32_t kKeyDiffDesc   = 0xD5F70104u;

const char kSpringIcon[]  = "GAME:\\Media\\UI\\Textures\\Data_Bound\\Upgrade_Parts\\Platform_Handling\\springs_dampers_3.swatchbin";
const char kSpringImage[] = "GAME:\\Media\\UI\\Textures\\Data_Bound\\Upgrade_Parts\\Platform_Handling\\springs_dampers_LG_3.swatchbin";
const char kDiffIcon[]    = "GAME:\\Media\\UI\\Textures\\Data_Bound\\Upgrade_Parts\\Drivetrain\\diff_3.swatchbin";
const char kDiffImage[]   = "GAME:\\Media\\UI\\Textures\\Data_Bound\\Upgrade_Parts\\Drivetrain\\diff_LG_3.swatchbin";

// The shop's comparison panel (game+0x16CF7EF): `mov r8d, [rsp+rbx*4+0x70]` picks the differential's level name from a 4-entry
// table, rbx = the new part's level.
constexpr uintptr_t kDiffNameAt = 0x16CF7EF;
const unsigned char kDiffNameBytes[] = {0x44, 0x8B, 0x44, 0x9C, 0x70};

// Steering lock. The central car loader (game+0x19E6A80) fills the car's spec (r14): the steering values from Data_Car at
// spec+0x5C (SteerMaxAngle, degrees) and spec+0x60 (SteerMaxAngleFiltered) first (call at 0x19E7823), then the springs
// and dampers (call at 0x19E8023 into 0x19F4B00, which reads the installed part's FrontSpringDamperPhysicsID with the call
// that returns at 0x19F4BA2). The drift suspension's physics rows are the only ones numbered Ordinal*1000+5, so:
//   - at 0x19F4BA2 (eax = the front physics id) the mod notes, for this thread, whether the car being built has it;
//   - at 0x19E8028 (right after the springs are built, r14 = the spec) it raises both steering angles to kDriftLock and the
//     steering speeds (spec+0x64 turning, +0x68 straightening, +0x6C counter-steering, degrees per second).
// The answer comes from the part the game is building the car with, so the shop preview, AI cars and loaners are all right.
constexpr uintptr_t kFrontIdAt   = 0x19F4BA2;
const unsigned char kFrontIdBytes[] = {0x44, 0x8B, 0xC0, 0x48, 0x8D, 0x54, 0x24, 0x58};
constexpr uintptr_t kAfterSpringsAt = 0x19E8028;
const unsigned char kAfterSpringsBytes[] = {0x48, 0x8B, 0x9D, 0xC0, 0x07, 0x00, 0x00};
constexpr float kDriftLock = 60.0f;   // degrees; FH3's stock cars have 30-42, its drift Horizon Editions 50, the largest any car has is 60
// Steering speed (degrees per second). Every FH3 car has 105 for turning, straightening and counter-steering, which with the
// 60 degree lock makes full lock take ~0.57 s instead of ~0.40 s: the drift suspension roughly doubles them so the wheels snap
// into counter-steer faster than on a stock car.
constexpr float kDriftTurnRate        = 210.0f;   // spec+0x5C+0x08 SteerMaxAngVelTurning
constexpr float kDriftStraightenRate  = 230.0f;   // +0x0C SteerMaxAngVelStraighten
constexpr float kDriftCountersteerRate = 280.0f;  // +0x10 SteerAngVelCountersteer
thread_local bool t_drift_springs = false;

const FH3ModApi* g_api = nullptr;
FH3Mod* g_mod          = nullptr;

std::string text_ref(uint32_t key) {   // how the database refers to a text in Upgrades.str
    return "_&" + std::to_string((static_cast<unsigned long long>(FH3_TABLE_UPGRADES) << 32) | key);
}

std::string upgrade_row(int id, int type, int level, uint32_t name, uint32_t desc, const char* icon, const char* image) {
    char b[1000];
    std::snprintf(b, sizeof b,
                  "INSERT OR REPLACE INTO Upgrades (id,TypeId,IsException,Name,Level,Description,IconPath,ImagePath) "
                  "VALUES (%d,'%d',0,'%s',%d,'%s','%s','%s');",
                  id, type, text_ref(name).c_str(), level, text_ref(desc).c_str(), icon, image);
    return b;
}

// Drift suspension: per car, new front/rear List_SpringDamperPhysics rows (ids Ordinal*1000+5 / +105, the game's own numbering)
// copied from the car's Race rows, and a List_UpgradeSpringDamper row at level 5 that uses them. Every statement only adds rows
// that don't exist yet, so running it again changes nothing.
const char kSpringSql[] = R"SQL(
DROP TABLE IF EXISTS temp.driftparts_p;
CREATE TEMP TABLE driftparts_p AS SELECT p.* FROM List_SpringDamperPhysics p JOIN List_UpgradeSpringDamper u ON p.SpringDamperPhysicsID=u.FrontSpringDamperPhysicsID
  WHERE u.Level=3 AND NOT EXISTS (SELECT 1 FROM List_UpgradeSpringDamper x WHERE x.Ordinal=u.Ordinal AND x.Level=5);
UPDATE driftparts_p SET SpringDamperPhysicsID=Ordinal*1000+5, Caster=7.0, StaticCamber=-5.0, StaticToe=0.5,
  DefRideHeight=MinRideHeight+(MaxRideHeight-MinRideHeight)*0.15,
  DefSpringRate=MIN(MaxSpringRate,MAX(MinSpringRate,DefSpringRate*0.90)),
  DefDampenBumpRate=MIN(MaxDampenBumpRate,MAX(MinDampenBumpRate,DefDampenBumpRate*0.85)),
  DefDampenReboundRate=MIN(MaxDampenReboundRate,MAX(MinDampenReboundRate,DefDampenReboundRate*0.85));
INSERT OR IGNORE INTO List_SpringDamperPhysics SELECT * FROM driftparts_p;
DELETE FROM driftparts_p;
INSERT INTO driftparts_p SELECT p.* FROM List_SpringDamperPhysics p JOIN List_UpgradeSpringDamper u ON p.SpringDamperPhysicsID=u.RearSpringDamperPhysicsID
  WHERE u.Level=3 AND NOT EXISTS (SELECT 1 FROM List_UpgradeSpringDamper x WHERE x.Ordinal=u.Ordinal AND x.Level=5);
UPDATE driftparts_p SET SpringDamperPhysicsID=Ordinal*1000+105, StaticCamber=-1.0, StaticToe=-0.2,
  DefRideHeight=MinRideHeight+(MaxRideHeight-MinRideHeight)*0.15,
  DefSpringRate=MIN(MaxSpringRate,MAX(MinSpringRate,DefSpringRate*0.80)),
  DefDampenBumpRate=MIN(MaxDampenBumpRate,MAX(MinDampenBumpRate,DefDampenBumpRate*0.80)),
  DefDampenReboundRate=MIN(MaxDampenReboundRate,MAX(MinDampenReboundRate,DefDampenReboundRate*0.80));
INSERT OR IGNORE INTO List_SpringDamperPhysics SELECT * FROM driftparts_p;
DROP TABLE driftparts_p;
INSERT OR IGNORE INTO List_UpgradeSpringDamper (Id,Ordinal,Level,ManufacturerID,MassDiff,DragScale,WindInstabilityScale,Price,IsStock,
  SuspensionDamageModDefID,TiresDamageModDefID,FrontSpringDamperPhysicsID,RearSpringDamperPhysicsID)
  SELECT u.Ordinal*1000+5,u.Ordinal,5,u.ManufacturerID,u.MassDiff,u.DragScale,u.WindInstabilityScale,u.Price,0,
  u.SuspensionDamageModDefID,u.TiresDamageModDefID,u.Ordinal*1000+5,u.Ordinal*1000+105
  FROM List_UpgradeSpringDamper u WHERE u.Level=3
  AND NOT EXISTS (SELECT 1 FROM List_UpgradeSpringDamper x WHERE x.Ordinal=u.Ordinal AND x.Level=5)
  AND NOT EXISTS (SELECT 1 FROM List_UpgradeSpringDamper y WHERE y.Id=u.Ordinal*1000+5)
  AND EXISTS (SELECT 1 FROM List_SpringDamperPhysics f WHERE f.SpringDamperPhysicsID=u.Ordinal*1000+5)
  AND EXISTS (SELECT 1 FROM List_SpringDamperPhysics r WHERE r.SpringDamperPhysicsID=u.Ordinal*1000+105);
)SQL";

// Drift differential: per drivetrain, a level 4 row copied from the Race (level 3) row.
const char kDiffSql[] = R"SQL(
INSERT OR IGNORE INTO List_UpgradeDrivetrainDifferential (Id,DrivetrainID,Level,IsStock,ManufacturerID,MassDiff,Price,
  FrontLimitedSlipTorqueAccel,FrontLimitedSlipTorqueDecel,FrontLimitedSlipRelVelClamp,FrontLimitedSlipAccelDefInputTorque,
  RearLimitedSlipTorqueAccel,RearLimitedSlipTorqueDecel,RearLimitedSlipRelVelClamp,RearLimitedSlipAccelDefInputTorque,
  CenterLimitedSlipTorqueAccel,CenterLimitedSlipTorqueDecel,CenterLimitedSlipRelVelClamp,CenterLimitedSlipAccelDefInputTorque,RearToqueSplit)
  SELECT d.DrivetrainID*1000+4,d.DrivetrainID,4,0,d.ManufacturerID,d.MassDiff,d.Price,
  CASE WHEN d.CenterLimitedSlipTorqueAccel>0 THEN d.FrontLimitedSlipTorqueAccel*0.5 ELSE MAX(d.FrontLimitedSlipTorqueAccel*4,6000) END,
  CASE WHEN d.CenterLimitedSlipTorqueAccel>0 THEN d.FrontLimitedSlipTorqueDecel*0.5 ELSE MAX(d.FrontLimitedSlipTorqueAccel*4,6000)*0.15 END,
  d.FrontLimitedSlipRelVelClamp,d.FrontLimitedSlipAccelDefInputTorque,
  MAX(d.RearLimitedSlipTorqueAccel*4,6000),MAX(d.RearLimitedSlipTorqueAccel*4,6000)*0.15,d.RearLimitedSlipRelVelClamp,d.RearLimitedSlipAccelDefInputTorque,
  d.CenterLimitedSlipTorqueAccel,d.CenterLimitedSlipTorqueDecel,d.CenterLimitedSlipRelVelClamp,d.CenterLimitedSlipAccelDefInputTorque,
  CASE WHEN d.CenterLimitedSlipTorqueAccel>0 THEN MAX(d.RearToqueSplit,0.80) ELSE d.RearToqueSplit END
  FROM List_UpgradeDrivetrainDifferential d WHERE d.Level=3
  AND NOT EXISTS (SELECT 1 FROM List_UpgradeDrivetrainDifferential x WHERE x.DrivetrainID=d.DrivetrainID AND x.Level=4)
  AND NOT EXISTS (SELECT 1 FROM List_UpgradeDrivetrainDifferential y WHERE y.Id=d.DrivetrainID*1000+4);
)SQL";

void add_texts_and_sql() {
    g_api->add_string_key(g_mod, "Upgrades.str", kKeySpringName, L"Drift Suspension");
    g_api->add_string_key(g_mod, "Upgrades.str", kKeySpringDesc,
                          L"Fully adjustable drift coilovers with an angle kit: far more steering lock, lots of negative front camber, maximum caster "
                          L"and a lowered, softer setup, so the car holds big angles and steers back smoothly. Tune it like the Race suspension.");
    g_api->add_string_key(g_mod, "Upgrades.str", kKeyDiffName, L"Drift Differential");
    g_api->add_string_key(g_mod, "Upgrades.str", kKeyDiffDesc,
                          L"A drift differential: fully locked on the throttle so both driven wheels spin together, and loose off the throttle so "
                          L"the car keeps its slide. On AWD cars most of the power goes to the rear.");

    std::string sql = upgrade_row(kSpringUpgradeId, kTypeSprings, kSpringLevel, kKeySpringName, kKeySpringDesc, kSpringIcon, kSpringImage);
    sql += upgrade_row(kDiffUpgradeId, kTypeDiff, kDiffLevel, kKeyDiffName, kKeyDiffDesc, kDiffIcon, kDiffImage);
    sql += kSpringSql;
    sql += kDiffSql;
    g_api->add_sql(g_mod, sql.c_str());
}

void on_diff_name(FH3Context* ctx, void*) {
    if (ctx->rbx > 3) ctx->rbx = 3;   // the drift differential (level 4) is named like the Race one in the comparison panel
}

void on_front_physics_id(FH3Context* ctx, void*) {
    const uint32_t id = static_cast<uint32_t>(ctx->rax);
    t_drift_springs = id >= 1000 && id % 1000 == 5;
}

void on_springs_built(FH3Context* ctx, void*) {
    if (!t_drift_springs) return;
    t_drift_springs = false;
    float* steer = reinterpret_cast<float*>(ctx->r14 + 0x5C);
    if (steer[0] < kDriftLock) steer[0] = kDriftLock;   // SteerMaxAngle
    if (steer[1] < kDriftLock) steer[1] = kDriftLock;   // SteerMaxAngleFiltered
    if (steer[2] < kDriftTurnRate) steer[2] = kDriftTurnRate;
    if (steer[3] < kDriftStraightenRate) steer[3] = kDriftStraightenRate;
    if (steer[4] < kDriftCountersteerRate) steer[4] = kDriftCountersteerRate;
}

void hook_at(char* game, uintptr_t rva, const unsigned char* bytes, size_t n, const char* name, FH3InstructionFn fn) {
    void* at = game + rva;
    if (std::memcmp(at, bytes, n) != 0) {
        char b[160];
        std::snprintf(b, sizeof b, "%s: unexpected game bytes, hook skipped", name);
        g_api->log(g_mod, b);
        return;
    }
    FH3Hook* h = g_api->hook_instruction(g_mod, name, at, fn, nullptr);
    if (!h || !g_api->enable_hooks(g_mod, h)) {
        char b[160];
        std::snprintf(b, sizeof b, "%s: hook could not be switched on", name);
        g_api->log(g_mod, b);
    }
}

DWORD WINAPI install_hooks(void*) {
    char* game = static_cast<char*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!game) return 0;
    hook_at(game, kDiffNameAt, kDiffNameBytes, sizeof kDiffNameBytes, "diff level name", on_diff_name);
    hook_at(game, kFrontIdAt, kFrontIdBytes, sizeof kFrontIdBytes, "drift suspension check", on_front_physics_id);
    hook_at(game, kAfterSpringsAt, kAfterSpringsBytes, sizeof kAfterSpringsBytes, "drift steering lock", on_springs_built);
    return 0;
}

}  // namespace

FH3MOD_EXPORT int fh3mod_init(const FH3ModApi* api) {
    if (api->api_version < 10) return 1;   // add_sql (2), hook_instruction (7), add_string_key (10)
    g_api = api;

    FH3ModInfo info{};
    info.size             = sizeof info;
    info.name             = "Drift Suspension and Differential";
    info.version          = DRIFTPARTS_VERSION;
    info.github_repo      = "Levilente/FH3_DriftParts";
    info.platform_version = FH3MOD_PLATFORM_VERSION;
    info.check_updates    = 1;
    info.description      = "FH6-style Drift Suspension (Springs and Dampers) and Drift Differential for every car.";
    info.offline_only     = 1;
    g_mod                 = api->register_mod(&info);
    if (!g_mod) return 1;

    add_texts_and_sql();
    if (HANDLE t = CreateThread(nullptr, 0, install_hooks, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
