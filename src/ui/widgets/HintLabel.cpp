#include "ui/widgets/HintLabel.h"

#include "ui/UiThemeColors.h"

#include <QEvent>

HintLabel::HintLabel(const QString& text, QWidget* parent, Tone tone, int fontSizePx)
    : QLabel(text, parent)
    , m_tone(tone)
    , m_fontSizePx(fontSizePx) {
    setWordWrap(true);
    applyColors();
}

void HintLabel::applyColors() {
    QFont labelFont = font();
    labelFont.setPixelSize(m_fontSizePx);
    setFont(labelFont);

    const QColor color =
        m_tone == Tone::Warning ? warningNoteTextColor(palette()) : secondaryHintTextColor(palette());
    applyLabelTextColor(this, color);
}

void HintLabel::changeEvent(QEvent* event) {
    QLabel::changeEvent(event);
    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        applyColors();
        break;
    default:
        break;
    }
}
