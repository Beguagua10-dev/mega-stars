#include "mega/Brawlers.h"

namespace mega {
namespace {

std::vector<BrawlerDef> buildRoster() {
    std::vector<BrawlerDef> roster;

    BrawlerDef faisca;
    faisca.id = "faisca";
    faisca.displayName = "Faisca";
    faisca.attack = AttackKind::SingleShot;
    faisca.maxHealth = 3600;
    faisca.moveSpeed = 4.4f;
    faisca.attackRange = 8.0f;
    faisca.reloadSeconds = 0.9f;
    faisca.ammoCapacity = 3;
    faisca.projectilesPerShot = 1;
    faisca.damagePerProjectile = 1100;
    faisca.projectileSpeed = 13.0f;
    faisca.colorRgb = 0xF2C14E;
    roster.push_back(faisca);

    BrawlerDef bruto;
    bruto.id = "bruto";
    bruto.displayName = "Bruto";
    bruto.attack = AttackKind::Spread;
    bruto.maxHealth = 5600;
    bruto.moveSpeed = 4.0f;
    bruto.attackRange = 4.5f;
    bruto.reloadSeconds = 1.4f;
    bruto.ammoCapacity = 3;
    bruto.projectilesPerShot = 5;
    bruto.spreadDegrees = 34.0f;
    bruto.damagePerProjectile = 420;
    bruto.projectileSpeed = 11.0f;
    bruto.superChargePerHit = 8;
    bruto.colorRgb = 0xE2574C;
    roster.push_back(bruto);

    BrawlerDef estopim;
    estopim.id = "estopim";
    estopim.displayName = "Estopim";
    estopim.attack = AttackKind::Lobbed;
    estopim.maxHealth = 3800;
    estopim.moveSpeed = 4.2f;
    estopim.attackRange = 6.5f;
    estopim.reloadSeconds = 1.1f;
    estopim.ammoCapacity = 3;
    estopim.projectilesPerShot = 1;
    estopim.damagePerProjectile = 1500;
    estopim.projectileSpeed = 9.0f;
    estopim.superChargePerHit = 20;
    estopim.colorRgb = 0x8E6BD6;
    roster.push_back(estopim);

    BrawlerDef rajada;
    rajada.id = "rajada";
    rajada.displayName = "Rajada";
    rajada.attack = AttackKind::Burst;
    rajada.maxHealth = 3200;
    rajada.moveSpeed = 5.0f;
    rajada.attackRange = 6.0f;
    rajada.reloadSeconds = 0.7f;
    rajada.ammoCapacity = 4;
    rajada.projectilesPerShot = 3;
    rajada.spreadDegrees = 10.0f;
    rajada.damagePerProjectile = 420;
    rajada.projectileSpeed = 15.0f;
    rajada.superChargePerHit = 9;
    rajada.colorRgb = 0x4ECD8F;
    roster.push_back(rajada);

    BrawlerDef mira;
    mira.id = "mira";
    mira.displayName = "Mira";
    mira.attack = AttackKind::Beam;
    mira.maxHealth = 2600;
    mira.moveSpeed = 4.1f;
    mira.attackRange = 12.0f;
    mira.reloadSeconds = 1.8f;
    mira.ammoCapacity = 3;
    mira.projectilesPerShot = 1;
    mira.damagePerProjectile = 1800;
    mira.projectileSpeed = 20.0f;
    mira.superChargePerHit = 25;
    mira.colorRgb = 0x4FA8E8;
    roster.push_back(mira);

    BrawlerDef pilha;
    pilha.id = "pilha";
    pilha.displayName = "Pilha";
    pilha.attack = AttackKind::Healing;
    pilha.maxHealth = 3400;
    pilha.moveSpeed = 4.3f;
    pilha.attackRange = 7.0f;
    pilha.reloadSeconds = 1.0f;
    pilha.ammoCapacity = 3;
    pilha.projectilesPerShot = 1;
    pilha.damagePerProjectile = 900;
    pilha.projectileSpeed = 12.0f;
    pilha.superChargePerHit = 15;
    pilha.colorRgb = 0xF08BC8;
    roster.push_back(pilha);

    return roster;
}

}  // namespace

const std::vector<BrawlerDef>& brawlerRoster() {
    static const std::vector<BrawlerDef> roster = buildRoster();
    return roster;
}

const BrawlerDef& findBrawler(const std::string& id) {
    const std::vector<BrawlerDef>& roster = brawlerRoster();
    for (const BrawlerDef& def : roster) {
        if (def.id == id) {
            return def;
        }
    }
    return roster.front();
}

}  // namespace mega
