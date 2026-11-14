// Drift Suspension and Drift Differential for Forza Horizon 3 (FH3 Mod Platform mod).
//
// Forza Horizon 6 has a "Drift Suspension" (Platform and Handling > Springs and Dampers) and a "Drift Differential"
// (Drivetrain > Differential) next to the Race parts. FH3 has neither (its levels are Stock, Street, Sport, Race, plus Offroad
// suspension from Blizzard Mountain). This mod adds both, on every car, as a new choice in the game's own upgrade lists:
//
//   - Drift Suspension: a copy of the car's own Race springs and dampers (fully adjustable in Tuning, same ranges), set up for
//     sliding: stiffer front, softer rear (springs 0.75x, rebound 0.80x), and 7.0 degrees of front caster (the most the game's
//     cars use) for stronger self-centring and more usable lock while counter-steering.
//   - Drift Differential: a copy of the car's Race differential with a near-locked rear (4x the Race lock, at least 6000 Nm on
//     power and 3000 Nm off power), so both rear wheels spin together and the car holds its angle. FWD/RWD cars lock the front
//     the same way; AWD cars keep half of the Race front lock and send at least 80 % of the torque to the rear.
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
UPDATE driftparts_p SET SpringDamperPhysicsID=Ordinal*1000+5, Caster=7.0,
  DefSpringRate=MIN(MaxSpringRate,MAX(MinSpringRate,DefSpringRate*1.10)),
  DefDampenBumpRate=MIN(MaxDampenBumpRate,MAX(MinDampenBumpRate,DefDampenBumpRate*1.10)),
  DefDampenReboundRate=MIN(MaxDampenReboundRate,MAX(MinDampenReboundRate,DefDampenReboundRate*1.10));
INSERT OR IGNORE INTO List_SpringDamperPhysics SELECT * FROM driftparts_p;
DELETE FROM driftparts_p;
INSERT INTO driftparts_p SELECT p.* FROM List_SpringDamperPhysics p JOIN List_UpgradeSpringDamper u ON p.SpringDamperPhysicsID=u.RearSpringDamperPhysicsID
  WHERE u.Level=3 AND NOT EXISTS (SELECT 1 FROM List_UpgradeSpringDamper x WHERE x.Ordinal=u.Ordinal AND x.Level=5);
UPDATE driftparts_p SET SpringDamperPhysicsID=Ordinal*1000+105,
  DefSpringRate=MIN(MaxSpringRate,MAX(MinSpringRate,DefSpringRate*0.75)),
  DefDampenBumpRate=MIN(MaxDampenBumpRate,MAX(MinDampenBumpRate,DefDampenBumpRate*0.85)),
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
  CASE WHEN d.CenterLimitedSlipTorqueAccel>0 THEN d.FrontLimitedSlipTorqueDecel*0.5 ELSE MAX(d.FrontLimitedSlipTorqueDecel*4,3000) END,
  d.FrontLimitedSlipRelVelClamp,d.FrontLimitedSlipAccelDefInputTorque,
  MAX(d.RearLimitedSlipTorqueAccel*4,6000),MAX(d.RearLimitedSlipTorqueDecel*4,3000),d.RearLimitedSlipRelVelClamp,d.RearLimitedSlipAccelDefInputTorque,
  d.CenterLimitedSlipTorqueAccel,d.CenterLimitedSlipTorqueDecel,d.CenterLimitedSlipRelVelClamp,d.CenterLimitedSlipAccelDefInputTorque,
  CASE WHEN d.CenterLimitedSlipTorqueAccel>0 THEN MAX(d.RearToqueSplit,0.80) ELSE d.RearToqueSplit END
  FROM List_UpgradeDrivetrainDifferential d WHERE d.Level=3
  AND NOT EXISTS (SELECT 1 FROM List_UpgradeDrivetrainDifferential x WHERE x.DrivetrainID=d.DrivetrainID AND x.Level=4)
  AND NOT EXISTS (SELECT 1 FROM List_UpgradeDrivetrainDifferential y WHERE y.Id=d.DrivetrainID*1000+4);
)SQL";

void add_texts_and_sql() {
    g_api->add_string_key(g_mod, "Upgrades.str", kKeySpringName, L"Drift Suspension");
    g_api->add_string_key(g_mod, "Upgrades.str", kKeySpringDesc,
                          L"Fully adjustable coilovers set up for drifting: a stiffer front, a softer rear and more front caster, so the car "
                          L"breaks traction easily, holds its angle and steers back smoothly. Tune it like the Race suspension.");
    g_api->add_string_key(g_mod, "Upgrades.str", kKeyDiffName, L"Drift Differential");
    g_api->add_string_key(g_mod, "Upgrades.str", kKeyDiffDesc,
                          L"A near-locked differential made for drifting: both driven wheels turn together on and off the throttle, so the "
                          L"car keeps its slide and answers the throttle predictably. On AWD cars most of the power goes to the rear.");

    std::string sql = upgrade_row(kSpringUpgradeId, kTypeSprings, kSpringLevel, kKeySpringName, kKeySpringDesc, kSpringIcon, kSpringImage);
    sql += upgrade_row(kDiffUpgradeId, kTypeDiff, kDiffLevel, kKeyDiffName, kKeyDiffDesc, kDiffIcon, kDiffImage);
    sql += kSpringSql;
    sql += kDiffSql;
    g_api->add_sql(g_mod, sql.c_str());
}

void on_diff_name(FH3Context* ctx, void*) {
    if (ctx->rbx > 3) ctx->rbx = 3;   // the drift differential (level 4) is named like the Race one in the comparison panel
}

DWORD WINAPI install_hook(void*) {
    char* game = static_cast<char*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!game) return 0;
    void* at = game + kDiffNameAt;
    if (std::memcmp(at, kDiffNameBytes, sizeof kDiffNameBytes) != 0) {
        g_api->log(g_mod, "comparison-panel hook: unexpected game bytes, skipped (the drift differential still works)");
        return 0;
    }
    FH3Hook* h = g_api->hook_instruction(g_mod, "diff level name", at, on_diff_name, nullptr);
    if (!h || !g_api->enable_hooks(g_mod, h)) g_api->log(g_mod, "comparison-panel hook: could not be switched on");
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
    if (HANDLE t = CreateThread(nullptr, 0, install_hook, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
