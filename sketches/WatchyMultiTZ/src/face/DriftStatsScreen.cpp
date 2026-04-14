#include "DriftStatsScreen.h"
#include "DriftTracker.h"

#include "../fonts/MyCompactFont.h"
#include "../fonts/Seven_Segment10pt7b.h"

#include <stdio.h>
#include <math.h>

namespace wmt {

namespace {

// Cursor-advance width (sum of xAdvance) for the given GFX font + string.
// Mirrors TimeZoneCard's helper — bitmap-bbox right-alignment clips the
// trailing glyph for fonts with non-zero per-glyph xOffset (DSEG7 family),
// so we always right-align by cursor advance instead.
int16_t advanceWidth(const GFXfont &f, const char *s) {
    int16_t w = 0;
    for (; *s; ++s) {
        const uint8_t c = (uint8_t)*s;
        if (c < f.first || c > f.last) continue;
        w += (int16_t)pgm_read_byte(&f.glyph[c - f.first].xAdvance);
    }
    return w;
}

void drawTextRightAdv(IDisplay *d, const GFXfont &f,
                      int16_t rightX, int16_t y, const char *s) {
    const int16_t w = advanceWidth(f, s);
    d->drawText((int16_t)(rightX - w), y, s);
}

void drawTextCentered(IDisplay *d, const GFXfont &f,
                      int16_t cx, int16_t y, const char *s) {
    const int16_t w = advanceWidth(f, s);
    d->drawText((int16_t)(cx - w / 2), y, s);
}

const char *profileName(int8_t p) {
    switch (p) {
        case 0: return "OFF";
        case 1: return "STATIC+DITH";
        case 2: return "QUAD";
        case 3: return "CUBIC-CONS";
        case 4: return "FINETUNED";
        default: return "UNK";
    }
}

void formatLastSync(int64_t deltaSec, char *out, size_t cap) {
    if (deltaSec < 0) deltaSec = 0;
    if (deltaSec < 24 * 3600) {
        const int h = (int)(deltaSec / 3600);
        const int m = (int)((deltaSec % 3600) / 60);
        snprintf(out, cap, "%d h %02d m", h, m);
    } else {
        const int d = (int)(deltaSec / 86400);
        const int h = (int)((deltaSec % 86400) / 3600);
        snprintf(out, cap, "%d d %02d h", d, h);
    }
}

void drawOverview(IDisplay *d, IClock * /*clock*/, IThermometer *thermo,
                  const DriftTracker &dt, int64_t nowUtc) {
    char buf[24];

    d->setTextColour(Ink::Fg);
    d->setFont((const Font *)&MyCompactFont7pt);
    drawTextCentered(d, MyCompactFont7pt, 100, 12, "DRIFT STATS  1/2");

    // Two columns: labels left in MyCompactFont7pt, values right-aligned in
    // Seven_Segment10pt7b. Row pitch 22 px gives 7 rows in [34..166].
    constexpr int16_t LABEL_X = 8;
    constexpr int16_t VALUE_RIGHT_X = 192;
    constexpr int16_t ROW0_Y = 34;
    constexpr int16_t ROW_DY = 22;

    auto label = [&](int row, const char *txt) {
        d->setFont((const Font *)&MyCompactFont7pt);
        d->drawText(LABEL_X, (int16_t)(ROW0_Y + row * ROW_DY), txt);
    };
    auto value = [&](int row, const char *txt) {
        d->setFont((const Font *)&Seven_Segment10pt7b);
        drawTextRightAdv(d, Seven_Segment10pt7b, VALUE_RIGHT_X,
                         (int16_t)(ROW0_Y + row * ROW_DY), txt);
    };

    // FC ppm
    const int16_t fcX100 = dt.fcPpmX100();
    snprintf(buf, sizeof buf, "%+.2f PPM", fcX100 / 100.0f);
    label(0, "FC");
    value(0, buf);

    // Samples — show "fold" until N_FOLD reached, then "EMA".
    const uint8_t samples = dt.sampleCount();
    snprintf(buf, sizeof buf, "%u / %d %s",
             (unsigned)samples, (int)DriftTracker::N_FOLD,
             samples >= (uint8_t)DriftTracker::N_FOLD ? "EMA" : "FOLD");
    label(1, "SAMPLES");
    value(1, buf);

    // Last sync
    const int64_t lastUtc = dt.lastSyncUtc();
    if (lastUtc == 0) {
        snprintf(buf, sizeof buf, "NEVER");
    } else {
        formatLastSync(nowUtc - lastUtc, buf, sizeof buf);
    }
    label(2, "LAST SYNC");
    value(2, buf);

    // Temp now
    if (thermo == nullptr) {
        snprintf(buf, sizeof buf, "N/A");
    } else {
        snprintf(buf, sizeof buf, "%d C", (int)thermo->readCelsius());
    }
    label(3, "TEMP NOW");
    value(3, buf);

    // Center T (turnover)
    snprintf(buf, sizeof buf, "%.1f C", dt.centerTempX100() / 100.0f);
    label(4, "CENTER T");
    value(4, buf);

    // Profile
    label(5, "PROFILE");
    value(5, profileName(dt.profile()));

    // Residual estimate. Pre-convergence we honestly don't know; once the EMA
    // takes over the plan's success criterion is sub-second/day so we publish
    // a fixed conservative bound rather than back-propagating |FC| (which is
    // what we're correcting AWAY, not the residual).
    label(6, "RESIDUAL");
    if (samples < (uint8_t)DriftTracker::N_FOLD) {
        value(6, "LEARNING");
    } else {
        value(6, "<1 S/DAY");
    }
}

void drawGraph(IDisplay *d, const DriftTracker &dt) {
    d->setTextColour(Ink::Fg);
    d->setFont((const Font *)&MyCompactFont7pt);
    drawTextCentered(d, MyCompactFont7pt, 100, 12, "DRIFT STATS  2/2");

    // Plot rectangle. 1-px border; sample columns inside.
    constexpr int16_t PLOT_X = 6;
    constexpr int16_t PLOT_Y = 24;
    constexpr int16_t PLOT_W = 188;   // 6..193 inclusive
    constexpr int16_t PLOT_H = 148;   // 24..171 inclusive
    d->drawRect({PLOT_X, PLOT_Y, PLOT_W, PLOT_H}, Ink::Fg);

    DriftTracker::Sample samples[DriftTracker::RING_SIZE];
    const int n = dt.history(samples, DriftTracker::RING_SIZE);

    if (n == 0) {
        drawTextCentered(d, MyCompactFont7pt, 100,
                         (int16_t)(PLOT_Y + PLOT_H / 2 + 3), "NO DATA");
        return;
    }

    // Axis half-range. Clamp to >= 5 ppm so a single small-magnitude sample
    // doesn't make the axis impossibly twitchy on early renders.
    float maxAbs = 5.0f;
    for (int i = 0; i < n; ++i) {
        const float v = fabsf(samples[i].inst_ppm_x100 / 100.0f);
        if (v > maxAbs) maxAbs = v;
        const float e = fabsf(samples[i].ema_ppm_x100 / 100.0f);
        if (e > maxAbs) maxAbs = e;
    }

    const int16_t plotInnerX = (int16_t)(PLOT_X + 1);
    const int16_t plotInnerY = (int16_t)(PLOT_Y + 1);
    const int16_t plotInnerW = (int16_t)(PLOT_W - 2);
    const int16_t plotInnerH = (int16_t)(PLOT_H - 2);
    const int16_t midY       = (int16_t)(plotInnerY + plotInnerH / 2);
    const float   halfH      = (float)plotInnerH * 0.5f;
    const float   dx         = (float)plotInnerW / (float)DriftTracker::RING_SIZE;

    // Dashed zero baseline (2 on / 2 off).
    for (int16_t x = plotInnerX; x < plotInnerX + plotInnerW; ++x) {
        if (((x - plotInnerX) & 0x03) < 2) {
            d->drawPixel(x, midY, Ink::Fg);
        }
    }

    auto sampleX = [&](int i) -> int16_t {
        return (int16_t)(plotInnerX + (int)((i + 0.5f) * dx));
    };
    auto valueY = [&](float ppm) -> int16_t {
        float yOff = (ppm / maxAbs) * halfH;
        if (yOff >  halfH) yOff =  halfH;
        if (yOff < -halfH) yOff = -halfH;
        // Positive ppm = RTC fast = bar goes UP (smaller y).
        return (int16_t)lroundf((float)midY - yOff);
    };

    // Bars: instantaneous samples from baseline to value.
    for (int i = 0; i < n; ++i) {
        const int16_t bx = sampleX(i);
        const int16_t by = valueY(samples[i].inst_ppm_x100 / 100.0f);
        if (by < midY) {
            d->drawVLine(bx, by, (int16_t)(midY - by + 1), Ink::Fg);
        } else if (by > midY) {
            d->drawVLine(bx, midY, (int16_t)(by - midY + 1), Ink::Fg);
        } else {
            d->drawPixel(bx, midY, Ink::Fg);
        }
    }

    // EMA polyline.
    int16_t prevX = 0, prevY = 0;
    for (int i = 0; i < n; ++i) {
        const int16_t cx = sampleX(i);
        const int16_t cy = valueY(samples[i].ema_ppm_x100 / 100.0f);
        if (i > 0) {
            d->drawLine(prevX, prevY, cx, cy, Ink::Fg);
        }
        prevX = cx;
        prevY = cy;
    }

    // Axis labels in MyCompactFont7pt. Top-right shows +maxAbs, bottom-right
    // -maxAbs, bottom-left the live EMA value.
    char buf[16];
    d->setFont((const Font *)&MyCompactFont7pt);

    snprintf(buf, sizeof buf, "+%.1f", maxAbs);
    drawTextRightAdv(d, MyCompactFont7pt,
                     (int16_t)(PLOT_X + PLOT_W - 3),
                     (int16_t)(PLOT_Y + 9), buf);

    snprintf(buf, sizeof buf, "-%.1f", maxAbs);
    drawTextRightAdv(d, MyCompactFont7pt,
                     (int16_t)(PLOT_X + PLOT_W - 3),
                     (int16_t)(PLOT_Y + PLOT_H - 3), buf);

    const float emaNow = samples[n - 1].ema_ppm_x100 / 100.0f;
    snprintf(buf, sizeof buf, "EMA %+.2f", emaNow);
    d->drawText((int16_t)(PLOT_X + 3),
                (int16_t)(PLOT_Y + PLOT_H - 3), buf);
}

} // namespace

DriftStatsScreen::DriftStatsScreen(IDisplay *display,
                                   IButtons *buttons,
                                   IPower   *power,
                                   IClock   *clock,
                                   IThermometer *thermo,
                                   const DriftTracker &tracker)
    : d_(display), b_(buttons), p_(power),
      c_(clock), t_(thermo), dt_(tracker) {}

void DriftStatsScreen::renderPage(IDisplay *display,
                                  IClock   *clock,
                                  IThermometer *thermo,
                                  const DriftTracker &tracker,
                                  int page,
                                  int64_t nowUtc) {
    if (display == nullptr) return;
    display->clear(Ink::Bg);
    if (page == 1) {
        drawGraph(display, tracker);
    } else {
        drawOverview(display, clock, thermo, tracker, nowUtc);
    }
    display->drawRect({0, 0, display->width(), display->height()}, Ink::Fg);
}

void DriftStatsScreen::repaint() {
    renderPage(d_, c_, t_, dt_, page_, c_ ? c_->nowUtc() : 0);
    if (d_) d_->commit(/*partialRefresh=*/true);
}

DriftStatsScreen::ExitReason DriftStatsScreen::run() {
    repaint();

    constexpr uint32_t IDLE_TIMEOUT_MS = 10000;
    constexpr uint32_t POLL_MS         = 20;

    if (p_ == nullptr || b_ == nullptr) {
        // No way to poll — behave as if idle.
        return ExitReason::IdleTimeout;
    }

    uint32_t lastActivity = p_->millisNow();

    for (;;) {
        const uint32_t now = p_->millisNow();
        if (now - lastActivity >= IDLE_TIMEOUT_MS) {
            return ExitReason::IdleTimeout;
        }

        if (b_->isPressed(Button::Back)) {
            while (b_->isPressed(Button::Back)) p_->delayMs(5);
            return ExitReason::Back;
        }
        if (b_->isPressed(Button::Menu)) {
            while (b_->isPressed(Button::Menu)) p_->delayMs(5);
            return ExitReason::ToLibraryMenu;
        }
        if (b_->isPressed(Button::Up)) {
            while (b_->isPressed(Button::Up)) p_->delayMs(5);
            page_ = (page_ + 1) % NUM_PAGES;
            repaint();
            lastActivity = p_->millisNow();
            continue;
        }
        if (b_->isPressed(Button::Down)) {
            while (b_->isPressed(Button::Down)) p_->delayMs(5);
            page_ = (page_ + NUM_PAGES - 1) % NUM_PAGES;
            repaint();
            lastActivity = p_->millisNow();
            continue;
        }

        p_->delayMs(POLL_MS);
    }
}

} // namespace wmt
