#include "QrScreen.h"
#include "../assets/qr_codes.h"
#include "../fonts/RobotoCondBold13.h"
#include "../fonts/MyCompactFont.h"

namespace wmt {

namespace {

// Layout:
//   y =   0 .. HEADER_H-1   : big bold label (e.g. "WhatsApp") — so the
//                             user can tell WHICH QR is showing without
//                             scanning it
//   y = HEADER_H .. rest    : the QR grid itself, centered, scaled to fit
//                             above the footer
//   y = 200-FOOTER_H .. 199 : "1/2" index at the very bottom
// Screen is 200×200 1-bit. Module scale = floor(available / N); all
// codes in assets/qr_codes.h share the same N (gen_qr_codes.py picks a
// shared version), so they all render at identical physical size.
constexpr int16_t  SCREEN_W    = 200;
constexpr int16_t  SCREEN_H    = 200;
constexpr int16_t  HEADER_H    = 22;   // bold label at the top
constexpr int16_t  FOOTER_H    = 12;   // "N/total" at the bottom
constexpr int16_t  QUIET_PX    = 6;    // quiet zone around the modules
constexpr int16_t  POLL_MS     = 20;
constexpr uint32_t DEBOUNCE_MS = 5;

inline bool moduleOn(const QrCode &qr, int row, int col) {
    const int bytes_per_row = (qr.size + 7) / 8;
    const uint8_t byte = qr.data[row * bytes_per_row + (col >> 3)];
    return (byte & (0x80u >> (col & 7))) != 0;
}

void paintQr(IDisplay *d, int qrIndex) {
    if (d == nullptr) return;
    if (qrIndex < 0 || qrIndex >= QR_COUNT) qrIndex = 0;
    const QrCode &qr = QR_CODES[qrIndex];

    // 1. Blank canvas in Bg (white).
    d->fillRect({0, 0, SCREEN_W, SCREEN_H}, Ink::Bg);

    // 2. Big bold label at the top. RobotoCondBold13 is already linked
    //    (EventCard uses it), so reusing it costs zero extra flash.
    d->setTextColour(Ink::Fg);
    d->setFont((const Font *)&RobotoCondBold13);
    const char *label = qr.label ? qr.label : "";
    int16_t lblW = 0, lblH = 0;
    d->measureText(label, lblW, lblH);
    const int16_t labelX = (int16_t)((SCREEN_W - lblW) / 2);
    const int16_t labelBaseline = (int16_t)(HEADER_H - 4);
    d->drawText(labelX, labelBaseline, label);

    // 3. Fit the module grid into the area between header and footer,
    //    with a quiet-zone margin. Scale is floored so corners land on
    //    pixel boundaries.
    const int16_t availW   = (int16_t)(SCREEN_W - 2 * QUIET_PX);
    const int16_t availH   = (int16_t)(SCREEN_H - HEADER_H - FOOTER_H - 2 * QUIET_PX);
    const int16_t minAvail = availW < availH ? availW : availH;
    const int16_t scale    = (int16_t)(minAvail / qr.size);
    const int16_t gridW    = (int16_t)(scale * qr.size);
    const int16_t originX  = (int16_t)((SCREEN_W - gridW) / 2);
    const int16_t originY  = (int16_t)(HEADER_H + QUIET_PX +
                                        (availH - gridW) / 2);

    // 4. Paint "on" modules as fg rectangles at `scale` pixels each.
    for (int row = 0; row < qr.size; ++row) {
        for (int col = 0; col < qr.size; ++col) {
            if (!moduleOn(qr, row, col)) continue;
            d->fillRect({
                (int16_t)(originX + col * scale),
                (int16_t)(originY + row * scale),
                scale, scale,
            }, Ink::Fg);
        }
    }

    // 5. Footer: small "N/total" index in MyCompactFont7pt (already
    //    linked, 4×7 pixels). Hand-rolled formatting avoids pulling in
    //    more of printf — we just need "1/2".
    d->setFont((const Font *)&MyCompactFont7pt);
    char idx[8];
    {
        int i = 0;
        idx[i++] = (char)('0' + (qrIndex + 1));
        idx[i++] = '/';
        idx[i++] = (char)('0' + QR_COUNT);
        idx[i] = '\0';
    }
    int16_t iw = 0, ih = 0;
    d->measureText(idx, iw, ih);
    const int16_t ix = (int16_t)((SCREEN_W - iw) / 2);
    const int16_t iy = (int16_t)(SCREEN_H - 2);
    d->drawText(ix, iy, idx);
}

} // namespace

void QrScreen::renderOne(IDisplay *d, int qrIndex) {
    paintQr(d, qrIndex);
}

void QrScreen::repaint() {
    paintQr(d_, index_);
    if (d_) d_->commit(/*partialRefresh=*/true);
}

QrScreen::ExitReason QrScreen::run() {
    index_ = 0;
    repaint();

    if (p_ == nullptr || b_ == nullptr) {
        return ExitReason::IdleTimeout;
    }

    uint32_t lastActivity = p_->millisNow();
    for (;;) {
        const uint32_t now = p_->millisNow();
        if (now - lastActivity >= IDLE_MS) {
            return ExitReason::IdleTimeout;
        }

        if (b_->isPressed(Button::Back)) {
            while (b_->isPressed(Button::Back)) p_->delayMs(DEBOUNCE_MS);
            ++index_;
            if (index_ >= QR_COUNT) {
                return ExitReason::CycledPastEnd;
            }
            repaint();
            lastActivity = p_->millisNow();
            continue;
        }

        p_->delayMs(POLL_MS);
    }
}

} // namespace wmt
