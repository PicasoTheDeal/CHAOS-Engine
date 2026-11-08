#pragma once
// the command line: parsing, the commands that never open a window, and the registry probe.
// main.cpp keeps only the window loop and the offscreen render.
#include <string>
#include <cstdint>
#include "seed/seed.h"
#include "scene/mode.h"
#include "chaos_file/format.h"
#include "wallpaper.h"

namespace chaos::cli {

struct Options {
    Seed32 seed{};                       // random unless --seed / --roll set it
    std::string seedArg, modeArg, registryArg;
    std::string shotPath, uiShotPath, ticketPath, revealPath, commitPath, claimPath, loginToken, loginDev;
    int bench = 0;
    std::string chengSrc;          // --cheng <file>: convert to <models>/<stem>.cheng and exit
    std::string modelArg;          // --model <file>: put this model on the stage                 // --bench N: render N frames offscreen at 1080p, print ms per frame and memory, exit
    int forcedShape = -1; float forcedMood = -1;
    Mode forcedMode = Mode::Chaos; bool hasMode = false;
    bool keep = false, noPanel = false, noPoster = false, wallpaper = false, x11 = false, noLibdecor = false, devOffline = false;
    bool doRoll = false, doSync = false, doLogout = false, doWhoami = false;
    WallpaperArgs wa;
    // filled by --roll
    cf::Ticket ticket{}; bool hasTicket = false; uint8_t clientRandom[32] = {};
};

struct RegistryState {
    std::string url; bool online = false; bool devOffline = false;
    bool signedIn = false;        // a session token that the registry accepted
    bool canGenerate() const { return devOffline || (online && signedIn); }   // signup is a must
};

void usage();
// parses argv. returns true when the program is done (a command ran, --version, usage error): rc holds the exit code.
bool parse(int argc, char** argv, Options& o, int& rc);
// GET /api/health with the url from --registry, settings.json or the build default. prints one line.
RegistryState probeRegistry(const Options& o);
// --logout, --login-*, --whoami, --claim, --sync, --roll, --commit, --wallpaper. returns true when done (rc set).
// --roll fills o.seed / o.ticket and returns false so the window opens on it.
bool runCommands(Options& o, const RegistryState& reg, int& rc);

} // namespace chaos::cli
