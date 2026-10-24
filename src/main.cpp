// FrameSnap v0.1.1 - sin probar
// Modo Regular: no toca nada, el juego funciona como siempre.
// Modo Frame perfect: cada click se guarda con su hora real y se aplica
// en el frame exacto donde ocurrio (hasta la ventana configurada).
// Sin indicador visual.
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <chrono>
#include <deque>

using namespace geode::prelude;
using Clock = std::chrono::steady_clock;
using FSec = std::chrono::duration<float>;

struct QueuedClick {
    bool down;
    int button;
    bool isPlayer1;
    Clock::time_point time;
};

static std::deque<QueuedClick> g_queue;
static bool g_applying = false;

static bool framePerfect() {
    return Mod::get()->getSettingValue<std::string>("mode") == "Frame perfect";
}

static float windowSeconds() {
    return Mod::get()->getSettingValue<int64_t>("window-ms") / 1000.f;
}

// Definida en input_hook.cpp: hora real del ultimo click/tecla de Windows.
Clock::time_point lastInputTime();

static Clock::time_point eventTime() {
    return lastInputTime();
}

// Solo actuamos dentro de un nivel, no en el editor
static bool isPlaying(GJBaseGameLayer* layer) {
    auto pl = PlayLayer::get();
    return pl && static_cast<GJBaseGameLayer*>(pl) == layer;
}

class $modify(FrameSnapLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        bool active = framePerfect() && !g_applying && isPlaying(this);
        if (!active) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }
        g_queue.push_back({down, button, isPlayer1, eventTime()});
    }

    void processCommands(float dt) {
        if (!framePerfect() || !isPlaying(this) || g_queue.empty()) {
            if (!framePerfect()) g_queue.clear();
            return GJBaseGameLayer::processCommands(dt);
        }

        auto frameStart = Clock::now() - std::chrono::duration_cast<Clock::duration>(FSec(dt));
        float consumed = 0.f;
        float win = windowSeconds();

        while (!g_queue.empty()) {
            auto click = g_queue.front();
            g_queue.pop_front();

            float offset = FSec(click.time - frameStart).count();

            // click mas viejo que la ventana: se aplica al inicio del frame
            if (offset < -win) offset = 0.f;
            offset = std::clamp(offset, consumed, std::max(consumed, dt));

            float run = offset - consumed;
            if (run > 0.f) {
                GJBaseGameLayer::processCommands(run);
                consumed += run;
            }

            g_applying = true;
            GJBaseGameLayer::handleButton(click.down, click.button, click.isPlayer1);
            g_applying = false;
        }

        if (dt - consumed > 0.f) {
            GJBaseGameLayer::processCommands(dt - consumed);
        }
    }
};
