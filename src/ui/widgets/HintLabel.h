#pragma once

#include <QLabel>

class HintLabel : public QLabel {
public:
    enum class Tone { Secondary, Warning };

    explicit HintLabel(const QString& text,
                       QWidget* parent = nullptr,
                       Tone tone = Tone::Secondary,
                       int fontSizePx = 11);

protected:
    void changeEvent(QEvent* event) override;

private:
    void applyColors();

    Tone m_tone = Tone::Secondary;
    int m_fontSizePx = 11;
};
