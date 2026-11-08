#pragma once
// everything one seed rolls. CPU only, no GL. the renderer uploads it.
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "seed/seed.h"
#include "shape/parametric.h"
#include "shape/generators.h"
#include "scene/palette.h"
#include "scene/mode.h"
#include "seed/rarity.h"

namespace chaos {

struct Skin {
    float chaos, noiseScale, dispStrength, cellScale, domeR, domeAmp, transDepth, bead, seeThrough, seedAmt;
    float roughness, f0, specular, sheen, poreScale, poreStrength, bloom, bloomScale;
    glm::vec3 seedOff;
};

// rolled in the reference's order. the fields nothing reads any more (orbitSpeed, waveSpeed, waveAmp, particleSpeed,
// pulseAmp, invertChance, brightnessOffset) are still drawn so the stream stays in step: removing a draw would move
// every seed's dimension.
struct Motion {
    float rotX, rotY, rotZ, orbitSpeed, bobSpeed, bobAmp, waveSpeed, waveAmp, particleSpeed, particleSpread;
    float pulseSpeed, pulseAmp, chaosFactor, invertChance, brightnessOffset;
};

struct Satellite {
    Mesh mesh;
    bool berry;
    Skin skin;                                       // when berry
    glm::vec3 color; float invert, brightness, glitchAmt, speed; int mode;   // when glitch
    float angle, radius, heightOffset, orbitSpeed, phase, orbitTilt;
};

struct FoldPlane { float w, h; int ws, hs; glm::vec3 color; float opacity, shininess, emissive, baseBrightness, foldPhase; glm::vec3 pos; };
struct FoldGroup { std::vector<FoldPlane> planes; };

struct Debris { Mesh mesh; glm::vec3 color, pos, rotSpeed, rot; float opacity, shininess, emissive, baseBrightness, oscPhase; };

struct Ring { float r, t; int seg; float bright, opacity; bool wire; glm::vec3 rot; float rotX, rotY, rotZ, orbitSpeed, orbitRadius, phase; };

struct Particles { std::vector<glm::vec3> pos; std::vector<float> bright; float opacity, spread; };

struct PaletteState {
    glm::vec3 a, b, fromA, fromB, toA, toB;
    float t0 = 0, dur = 1, next = 0, kick = 0;
    bool hard = false;
    int jumps = 0;
};

const char* moodNameOf(float v);

struct Dimension {
    Seed32 seed;
    // rarity
    Tier tier;
    int family;
    Unlocks unlocks;
    float score;
    // shape
    GeoParams geo;
    int shapeType;
    Mesh mainMesh;
    float mainRadius;
    std::vector<std::string> lineage;
    Skin skin;
    float wireAlpha;
    // colour
    PaletteState pal;
    glm::vec3 palC;                     // third stop, used when unlocks.thirdStop
    float paletteTempo;
    Xoshiro256 runtimeRng;
    float duoExposure, duoContrast, duoSplit;
    // mood: 0 calm .. 1 wild
    float moodV, moodTarget;
    float moodSeeded;                   // what the seed rolled, before any mode touches it
    bool moodLocked = false;
    // mode: native is what the seed rolled and what gets valued, display is what this install shows
    Mode nativeMode, displayMode;
    void setDisplayMode(Mode m);
    const ModePreset& preset() const { return modePreset(displayMode); }
    // lights: world directions towards the light, intensities
    glm::vec3 lightDir[3]; float lightInt[3];
    Motion motion;
    // the swarm
    std::vector<Satellite> sats;
    std::vector<FoldGroup> folds;
    std::vector<Debris> debris;
    Particles particles;
    std::vector<Ring> rings;
    // backdrop
    float bgDensity, bgSeed;
    // mutation clock
    float mutV = 0, mutRate, mutMax = 1.5f;
    // a model on the stage (see applyModel). -1 = none, the seed's shape
    int modelSkin = -1;
    glm::vec3 modelColor{ 0.9f, 0.9f, 0.9f };

    float ml(float calm, float wild) const { return calm + (wild - calm) * moodV; }
    const char* moodName() const;
    void cycleMood();
    void update(float t, float dt);     // mood ease, mutation clock, palette schedule
};

// forcedShape -1 = seeded. forcedMood < 0 = seeded.
Dimension rollDimension(const Seed32& seed, int forcedShape = -1, float forcedMood = -1);

// your own model on the stage (a .cheng): it replaces the main shape, the swarm goes, the rest of the dimension
// (palette, mood, mode, backdrop) stays the seed's. skin: 0 berry (the seed's skin), 1 mono (one colour), 2 wire,
// 3 own (the textures and colours the model came with). this is a display choice on this install: the seed, its
// value and its file do not change.
enum ModelSkin { MODEL_BERRY = 0, MODEL_MONO = 1, MODEL_WIRE = 2, MODEL_OWN = 3 };
void applyModel(Dimension& d, Mesh mesh, int skin, glm::vec3 colour);
// the look of one seed on another: skin, palette (and its third stop), grading, lights, palette tempo. the geometry,
// mood, mode and everything else stay. used so a model can wear the skin of a seed you chose (one from your vault)
void copyLook(Dimension& d, const Dimension& from);
void tameSkinForModel(Dimension& d);   // less displacement on a model than on a berry

} // namespace chaos
