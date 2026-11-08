#include "scene/dimension.h"
#include "shape/primitives.h"
#include "shape/mutate.h"
#include "scene/props.h"
#include <cmath>
#include <numbers>
#include <algorithm>
#include <cstring>

namespace chaos {

static const float PI = std::numbers::pi_v<float>;
static const char* MOOD_NAMES[] = { "CALM", "SOFT", "LIVELY", "WILD" };
static const float MOOD_STOPS[] = { 0.05f, 0.35f, 0.65f, 0.95f };
static int moodIndex(float v) { return std::min(3, int(v * 4)); }

const char* moodNameOf(float v) { return MOOD_NAMES[moodIndex(v)]; }
const char* Dimension::moodName() const { return moodNameOf(moodV); }

static glm::vec3 seededColor(Xoshiro256& r) {
    float hue = float(r.uniform()), sat = 0.65f + 0.35f * float(r.uniform()), light = 0.40f + 0.45f * float(r.uniform());
    return hslToRgb(hue, sat, light);
}

SeedProps deriveProps(const Seed32& seed, Xoshiro256* shapeRngOut) {
    SeedProps p;
    auto rarityRng = subStream(seed, "rarity");
    auto modeRng = subStream(seed, "mode");
    auto shapeRng = subStream(seed, "shape");
    p.tier = rollTier(rarityRng);
    p.unlocks = unlocksFor(p.tier);
    p.nativeMode = p.unlocks.forceBleed ? Mode::Bleed : rollMode(modeRng);
    p.geo = rollGeoParams(shapeRng);
    p.shapeType = p.unlocks.forceCompound ? 40 : p.geo.type;
    p.family = familyOf(p.shapeType);
    std::memcpy(&p.mutationSeed, seed.data(), 4);
    if (shapeRngOut) *shapeRngOut = shapeRng;
    return p;
}

Dimension rollDimension(const Seed32& seed, int forcedShape, float forcedMood) {
    Dimension d;
    d.seed = seed;
    Xoshiro256 shapeRng = Xoshiro256::fromU64(0);
    SeedProps P = deriveProps(seed, &shapeRng);
    auto mutRng = subStream(seed, "mutation");
    auto skinRng = subStream(seed, "skin");
    auto palRng = subStream(seed, "palette");
    auto lightRng = subStream(seed, "lights");
    auto motionRng = subStream(seed, "motion");
    auto satRng = subStream(seed, "satellites");
    auto foldRng = subStream(seed, "planes");
    auto debRng = subStream(seed, "debris");
    auto partRng = subStream(seed, "particles");
    auto ringRng = subStream(seed, "rings");
    auto bgRng = subStream(seed, "background");
    auto moodRng = subStream(seed, "mood");

    // ---- rarity first: the tier feeds everything after it ----
    d.tier = P.tier;
    d.unlocks = P.unlocks;
    const Unlocks& U = d.unlocks;
    d.runtimeRng = subStream(seed, "runtime");

    // ---- mood ----
    d.moodV = std::pow(float(moodRng.uniform()), 1.6f);
    if (forcedMood >= 0) { d.moodV = std::clamp(forcedMood, 0.f, 1.f); d.moodLocked = true; }
    d.moodSeeded = d.moodV;
    d.moodTarget = d.moodV;

    // ---- mode ----
    d.nativeMode = P.nativeMode;
    d.displayMode = d.nativeMode;

    // ---- palette ----
    PalettePair first = makePair(palRng, U.acidOnly ? 4 : -1);
    d.palC = makeThirdStop(palRng, first);
    d.pal.a = d.pal.fromA = d.pal.toA = first.a;
    d.pal.b = d.pal.fromB = d.pal.toB = first.b;
    d.pal.next = 2.5f + 6 * float(palRng.uniform());
    d.paletteTempo = palRng.range(0.6f, 1.6f);
    d.duoExposure = palRng.range(0.45f, 0.85f);
    d.duoContrast = palRng.range(1.0f, 1.5f);
    d.duoSplit = palRng.range(0.4f, 0.6f);

    // ---- motion ----
    Motion& m = d.motion;
    m.rotX = motionRng.range(-1.2f, 1.2f); m.rotY = motionRng.range(-1.2f, 1.2f); m.rotZ = motionRng.range(-1.2f, 1.2f);
    m.orbitSpeed = motionRng.range(0.1f, 2.5f); m.bobSpeed = motionRng.range(0.1f, 2.5f); m.bobAmp = motionRng.range(0.1f, 1.2f);
    m.waveSpeed = motionRng.range(0.1f, 2.0f); m.waveAmp = motionRng.range(0.05f, 1.0f);
    m.particleSpeed = motionRng.range(0.05f, 1.5f); m.particleSpread = motionRng.range(2.5f, 14);
    m.pulseSpeed = motionRng.range(0.2f, 2.5f); m.pulseAmp = motionRng.range(0.02f, 0.6f);
    m.chaosFactor = motionRng.range(0.1f, 3.0f); m.invertChance = float(motionRng.uniform()); m.brightnessOffset = motionRng.range(-0.3f, 0.5f);

    // ---- lights ----
    glm::vec3 lightPos(std::sin(lightRng.range(0, 2 * PI)) * lightRng.range(7, 13), lightRng.range(8, 16), std::cos(lightRng.range(0, 2 * PI)) * lightRng.range(7, 13));
    d.lightInt[0] = lightRng.range(0.9f, 1.5f);
    glm::vec3 fillPos(-lightPos.x * lightRng.range(0.7f, 1.3f), lightRng.range(-3, 5), -lightPos.z * lightRng.range(0.7f, 1.3f));
    d.lightInt[1] = lightRng.range(0.2f, 0.45f);
    glm::vec3 rimPos(lightRng.range(-4, 4), lightRng.range(2, 6), -lightRng.range(8, 14));
    d.lightInt[2] = lightRng.range(0.4f, 0.8f);
    d.lightDir[0] = glm::normalize(lightPos); d.lightDir[1] = glm::normalize(fillPos); d.lightDir[2] = glm::normalize(rimPos);

    // ---- backdrop ----
    d.bgDensity = bgRng.range(1.5f, 4.0f);
    d.bgSeed = float(bgRng.next() & 0xffffff);

    // ---- shape + lineage ----
    d.geo = P.geo;
    d.shapeType = forcedShape >= 0 ? forcedShape % SHAPE_COUNT : P.shapeType;
    d.family = familyOf(d.shapeType);
    Mesh g = buildShape(d.shapeType, d.geo, shapeRng, 0);
    g = weld(g, 1e-4f);                                   // merged shapes come back with seams, weld before displacing
    d.lineage = mutateLineage(g, mutRng, U.extraGens);
    g.center();
    g.computeVertexNormals();
    d.mainRadius = g.boundingRadius();
    d.mainMesh = std::move(g);
    d.wireAlpha = skinRng.range(0.04f, 0.16f);

    // ---- skin ----
    Skin& s = d.skin;
    s.chaos = m.chaosFactor;
    s.noiseScale = skinRng.range(0.6f, 2.4f);
    s.dispStrength = skinRng.range(0.1f, 0.4f);
    s.cellScale = skinRng.range(1.6f, 7.5f) * (3.0f / std::max(1.5f, d.mainRadius));
    s.domeR = skinRng.range(0.45f, 0.72f);
    s.domeAmp = skinRng.range(0.05f, 0.28f) * std::max(0.8f, d.mainRadius / 3);
    s.transDepth = skinRng.range(0.1f, 0.45f);
    s.bead = skinRng.range(0.6f, 1.6f);
    s.seeThrough = skinRng.range(0.3f, 1.2f);
    s.seedAmt = skinRng.chance(0.6) ? skinRng.range(0.1f, 0.5f) : 0.f;
    s.roughness = skinRng.range(0.08f, 0.45f);
    s.f0 = std::pow((1.33f - 1) / (1.33f + 1), 2.f) * skinRng.range(0.8f, 2.5f);
    s.specular = skinRng.range(0.6f, 1.6f);
    s.sheen = skinRng.range(0.1f, 0.9f);
    s.poreScale = skinRng.range(18, 70);
    s.poreStrength = skinRng.range(0.05f, 0.5f);
    s.bloom = skinRng.chance(0.65) ? skinRng.range(0.2f, 0.9f) : 0.f;
    s.bloomScale = skinRng.range(0.4f, 3.0f);
    s.seedOff = glm::vec3(skinRng.range(0, 100), skinRng.range(0, 100), skinRng.range(0, 100));
    d.mutRate = mutRng.range(0.005f, 0.02f);

    // ---- satellites ----
    int satCount = satRng.rangeInt(2, 14);
    for (int i = 0; i < satCount; i++) {
        Satellite sat;
        sat.color = seededColor(satRng);
        sat.brightness = 0.3f + 0.7f * float(satRng.uniform());
        sat.invert = satRng.uniform() > 0.5 ? 1.f : 0.f;
        float size = satRng.range(0.08f, 0.7f);
        switch (satRng.rangeInt(0, 5)) {
        case 0: sat.mesh = icosahedronGeo(size, satRng.rangeInt(1, 3)); break;
        case 1: sat.mesh = octahedronGeo(size, satRng.rangeInt(0, 2)); break;
        case 2: sat.mesh = dodecahedronGeo(size, satRng.rangeInt(0, 2)); break;
        case 3: sat.mesh = boxGeo(size, size * satRng.range(0.5f, 2), size * satRng.range(0.5f, 2), 6, 6, 6); break;
        case 4: sat.mesh = tetrahedronGeo(size, satRng.rangeInt(0, 2)); break;
        default: sat.mesh = torusKnotGeo(size * 0.6f, size * 0.2f, 80, 12, satRng.rangeInt(1, 4), satRng.rangeInt(2, 5)); break;
        }
        sat.berry = satRng.uniform() < U.satBerryChance;
        sat.glitchAmt = 0; sat.speed = 1; sat.mode = 0;
        if (sat.berry) {
            Skin& k = sat.skin;
            k.chaos = m.chaosFactor;
            k.noiseScale = satRng.range(1.5f, 4); k.dispStrength = satRng.range(0.01f, 0.06f);
            k.cellScale = satRng.range(6, 18); k.domeR = satRng.range(0.45f, 0.7f); k.domeAmp = satRng.range(0.01f, 0.05f);
            k.transDepth = satRng.range(0.05f, 0.2f); k.bead = satRng.range(0.6f, 1.5f); k.seeThrough = satRng.range(0.3f, 1); k.seedAmt = satRng.range(0, 0.4f);
            k.roughness = satRng.range(0.08f, 0.4f); k.f0 = 0.02f * satRng.range(0.8f, 2.5f); k.specular = satRng.range(0.7f, 1.5f); k.sheen = satRng.range(0.1f, 0.8f);
            k.poreScale = satRng.range(40, 120); k.poreStrength = satRng.range(0.05f, 0.4f);
            k.bloom = satRng.uniform() < 0.6 ? satRng.range(0.2f, 0.9f) : 0.f; k.bloomScale = satRng.range(2, 8);
            k.seedOff = glm::vec3(satRng.range(0, 100), satRng.range(0, 100), satRng.range(0, 100));
        } else {
            sat.glitchAmt = satRng.range(0.2f, 0.8f);
            sat.speed = satRng.range(1.0f, 4.0f);
            sat.mode = satRng.rangeInt(0, 4);
        }
        sat.angle = satRng.range(0, 2 * PI);
        sat.radius = satRng.range(d.mainRadius * 0.9f, d.mainRadius * 2.2f);
        sat.heightOffset = satRng.range(-3, 3);
        sat.orbitSpeed = satRng.range(0.1f, 2.0f);
        sat.phase = satRng.range(0, 2 * PI);
        sat.orbitTilt = satRng.range(-0.8f, 0.8f);
        d.sats.push_back(std::move(sat));
    }

    if (U.secondBerry) {
        // rare and up: a second berry, a different shape at a third the size, its own skin, orbiting wide
        auto twinRng = subStream(seed, "twin");
        GeoParams tg = rollGeoParams(twinRng);
        tg.radius *= 0.35f; tg.height *= 0.35f; tg.width *= 0.35f; tg.depth *= 0.35f; tg.tube *= 0.35f;
        int tt = twinRng.rangeInt(0, SHAPE_COUNT - 2);
        if (tt == 27 || tt == 28 || tt == 36 || tt == 38) tt = twinRng.rangeInt(0, 26);
        Satellite tw;
        tw.mesh = weld(buildShape(tt, tg, twinRng, 1), 1e-4f);
        tw.mesh.center(); tw.mesh.computeVertexNormals();
        tw.berry = true;
        Skin& k = tw.skin;
        float tr = std::max(0.5f, tw.mesh.boundingRadius());
        k.chaos = m.chaosFactor;
        k.noiseScale = twinRng.range(0.6f, 2.4f); k.dispStrength = twinRng.range(0.05f, 0.2f);
        k.cellScale = twinRng.range(1.6f, 7.5f) * (3.0f / std::max(1.5f, tr)); k.domeR = twinRng.range(0.45f, 0.72f);
        k.domeAmp = twinRng.range(0.03f, 0.15f) * std::max(0.8f, tr / 3);
        k.transDepth = twinRng.range(0.1f, 0.45f); k.bead = twinRng.range(0.6f, 1.6f); k.seeThrough = twinRng.range(0.3f, 1.2f); k.seedAmt = twinRng.range(0, 0.5f);
        k.roughness = twinRng.range(0.08f, 0.45f); k.f0 = 0.02f * twinRng.range(0.8f, 2.5f); k.specular = twinRng.range(0.6f, 1.6f); k.sheen = twinRng.range(0.1f, 0.9f);
        k.poreScale = twinRng.range(18, 70); k.poreStrength = twinRng.range(0.05f, 0.5f);
        k.bloom = twinRng.uniform() < 0.65 ? twinRng.range(0.2f, 0.9f) : 0.f; k.bloomScale = twinRng.range(0.4f, 3.0f);
        k.seedOff = glm::vec3(twinRng.range(0, 100), twinRng.range(0, 100), twinRng.range(0, 100));
        tw.color = glm::vec3(1); tw.invert = 0; tw.brightness = 1; tw.glitchAmt = 0; tw.speed = 1; tw.mode = 0;
        tw.angle = twinRng.range(0, 2 * PI);
        tw.radius = d.mainRadius * twinRng.range(1.4f, 2.0f);
        tw.heightOffset = twinRng.range(-2, 2);
        tw.orbitSpeed = twinRng.range(0.1f, 0.6f);
        tw.phase = twinRng.range(0, 2 * PI);
        tw.orbitTilt = twinRng.range(-0.5f, 0.5f);
        d.sats.push_back(std::move(tw));
    }

    // ---- folding planes ----
    int foldCount = foldRng.rangeInt(1, 8);
    for (int i = 0; i < foldCount; i++) {
        FoldGroup grp;
        int per = foldRng.rangeInt(3, 8);
        for (int j = 0; j < per; j++) {
            FoldPlane p;
            p.w = foldRng.range(4, 14); p.h = foldRng.range(4, 18);
            p.ws = foldRng.rangeInt(2, 6); p.hs = foldRng.rangeInt(2, 6);
            p.baseBrightness = 0.2f + float(foldRng.uniform()) * 0.6f;
            p.color = seededColor(foldRng);
            p.opacity = 0.45f + 0.35f * float(foldRng.uniform());
            p.shininess = 50 + float(foldRng.uniform()) * 150;
            p.emissive = 0.2f + float(foldRng.uniform()) * 0.3f;
            p.pos = glm::vec3(foldRng.range(-36, 36), foldRng.range(-24, 24), foldRng.range(-36, 36));
            p.foldPhase = foldRng.range(0, PI * 4);
            foldRng.uniform(); foldRng.uniform();   // colorShift, hueSpeed: rolled, unused by the reference frame loop
            grp.planes.push_back(p);
        }
        d.folds.push_back(std::move(grp));
    }

    // ---- debris ----
    int debCount = debRng.rangeInt(12, 36);
    for (int i = 0; i < debCount; i++) {
        Debris b;
        auto rr = [&](float a, float c) { return debRng.range(a, c); };
        auto ri = [&](int a, int c) { return debRng.rangeInt(a, c); };
        switch (ri(0, 6)) {
        case 0: b.mesh = coneGeo(rr(1, 5), rr(3, 12), ri(3, 8)); break;
        case 1: b.mesh = boxGeo(rr(2, 7), rr(1, 5), rr(4, 14)); break;
        case 2: b.mesh = cylinderGeo(rr(0.5f, 3), rr(1, 5), rr(4, 14), ri(4, 10)); break;
        case 3: b.mesh = torusGeo(rr(1, 5), rr(0.2f, 1.2f), ri(8, 24), ri(8, 24)); break;
        case 4: b.mesh = icosahedronGeo(rr(1, 5), ri(0, 2)); break;
        case 5: b.mesh = dodecahedronGeo(rr(1, 5), 0); break;
        default: b.mesh = torusKnotGeo(rr(1, 3), rr(0.2f, 0.8f), 64, 10, ri(1, 4), ri(2, 7)); break;
        }
        b.baseBrightness = 0.3f + float(debRng.uniform()) * 0.4f;
        b.color = seededColor(debRng);
        b.opacity = 0.6f + 0.4f * float(debRng.uniform());
        b.shininess = 50 + float(debRng.uniform()) * 150;
        b.emissive = 0.1f + float(debRng.uniform()) * 0.3f;
        b.pos = glm::vec3(rr(-48, 48), rr(-34, 34), rr(-48, 48));
        b.rotSpeed = glm::vec3(rr(-0.03f, 0.03f), rr(-0.03f, 0.03f), rr(-0.03f, 0.03f));
        b.oscPhase = rr(0, 2 * PI);
        b.rot = glm::vec3(0);
        debRng.uniform(); debRng.uniform();   // hueOffset, hueSpeed
        d.debris.push_back(std::move(b));
    }

    // ---- particles ----
    int pc = partRng.rangeInt(400, 900);
    d.particles.spread = m.particleSpread;
    for (int i = 0; i < pc; i++) {
        float th = partRng.range(0, 2 * PI), ph = std::acos(partRng.range(-1, 1)), rad = partRng.range(0.4f, m.particleSpread * 1.8f);
        d.particles.pos.emplace_back(rad * std::sin(ph) * std::cos(th), rad * std::cos(ph), rad * std::sin(ph) * std::sin(th));
        d.particles.bright.push_back(0.2f + 0.8f * float(partRng.uniform()));
    }
    d.particles.opacity = 0.7f + 0.3f * float(partRng.uniform());

    // ---- rings ----
    int ringCount = ringRng.rangeInt(3, 14) * U.ringMult;
    for (int i = 0; i < ringCount; i++) {
        Ring rg;
        rg.r = ringRng.range(0.8f, d.mainRadius * 1.4f);
        rg.t = ringRng.range(0.02f, 0.2f);
        rg.seg = ringRng.rangeInt(12, 64);
        rg.bright = 0.3f + 0.5f * float(ringRng.uniform());
        rg.opacity = 0.1f + 0.35f * float(ringRng.uniform());
        rg.wire = ringRng.uniform() > 0.5;
        rg.rot = glm::vec3(ringRng.range(0, 2 * PI), ringRng.range(0, 2 * PI), 0);
        rg.rotX = ringRng.range(-0.8f, 0.8f); rg.rotY = ringRng.range(-0.8f, 0.8f); rg.rotZ = ringRng.range(-0.8f, 0.8f);
        rg.orbitSpeed = ringRng.range(0.1f, 0.9f); rg.orbitRadius = ringRng.range(0.3f, 3.0f); rg.phase = ringRng.range(0, 2 * PI);
        d.rings.push_back(rg);
    }
    d.setDisplayMode(d.nativeMode);
    d.score = rarityScore(d.tier, int(d.lineage.size()), int(d.sats.size()), d.moodSeeded);
    return d;
}

void Dimension::setDisplayMode(Mode m) {
    displayMode = m;
    const ModePreset& p = preset();
    if (!moodLocked) moodTarget = p.moodPin >= 0 ? p.moodPin : std::min(moodSeeded, p.moodCap);
}

void Dimension::cycleMood() {
    // a manual mood press always wins over the preset, like the web's M key
    moodTarget = MOOD_STOPS[(moodIndex(moodTarget) + 1) % 4];
    moodLocked = true;
}

void Dimension::update(float now, float dt) {
    const ModePreset& p = preset();
    moodV += (moodTarget - moodV) * std::min(1.f, dt * 1.5f);
    if (p.mutation) mutV = std::min(mutMax, mutV + dt * mutRate * (0.25f + mutV) * ml(0.6f, 1.4f));
    if (p.paletteJumps && now >= pal.next) {
        PalettePair pair = makePair(runtimeRng);
        pal.fromA = pal.a; pal.fromB = pal.b; pal.toA = pair.a; pal.toB = pair.b;
        pal.t0 = now;
        float roll = float(runtimeRng.uniform());
        pal.hard = roll < (p.hardCut >= 0 ? p.hardCut : ml(0.0f, 0.3f));
        pal.dur = pal.hard ? 0.0001f : (ml(1.6f, 0.4f) + 3.2f * float(runtimeRng.uniform()));
        pal.next = now + (2.5f + 11 * float(runtimeRng.uniform())) * ml(2.0f, 1.0f) / paletteTempo;
        pal.kick = 1;
        pal.jumps++;
        mutV = std::min(mutMax, mutV + 0.03f);
    }
    float k = std::min(1.f, (now - pal.t0) / pal.dur);
    k = k * k * (3 - 2 * k);
    pal.a = glm::mix(pal.fromA, pal.toA, k);
    pal.b = glm::mix(pal.fromB, pal.toB, k);
    pal.kick = std::max(0.f, 1 - (now - pal.t0) / 0.6f);
}

void applyModel(Dimension& d, Mesh mesh, int skin, glm::vec3 colour) {
    if (mesh.indices.empty()) return;
    d.mainRadius = mesh.boundingRadius();
    d.mainMesh = std::move(mesh);
    d.sats.clear(); d.folds.clear(); d.debris.clear(); d.rings.clear();
    d.wireAlpha = 0;                                    // no wire ghost: on a model it read as triangles flying off it
    d.particles.pos.clear(); d.particles.bright.clear();   // no dust through it either: just the model
    d.modelSkin = skin < 0 ? MODEL_BERRY : skin > 3 ? MODEL_OWN : skin;
    if (d.modelSkin == MODEL_OWN && !d.mainMesh.hasOwnLook()) d.modelSkin = MODEL_MONO;   // nothing came with it: one colour
    d.modelColor = colour;
    tameSkinForModel(d);
}

// the berry skin displaces the surface by amounts tuned for fat berries; on a thin model (a tube, a blade, lettering)
// that tears the shape. a model keeps the skin's colour and pores but a fraction of its push
void tameSkinForModel(Dimension& d) {
    if (d.modelSkin < 0) return;
    d.skin.dispStrength *= 0.25f;
    d.skin.domeAmp *= 0.3f;
}

void copyLook(Dimension& d, const Dimension& from) {
    d.skin = from.skin;
    d.pal = from.pal; d.palC = from.palC; d.paletteTempo = from.paletteTempo;
    d.duoExposure = from.duoExposure; d.duoContrast = from.duoContrast; d.duoSplit = from.duoSplit;
    for (int i = 0; i < 3; i++) { d.lightDir[i] = from.lightDir[i]; d.lightInt[i] = from.lightInt[i]; }
    d.unlocks.thirdStop = from.unlocks.thirdStop; d.unlocks.acidOnly = from.unlocks.acidOnly;
    d.wireAlpha = 0;
    tameSkinForModel(d);
}

} // namespace chaos
