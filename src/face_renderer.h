#pragma once

#include <stdint.h>

#include "face_layer.h"

struct Expression {
    const FaceLayer* eyes;
    const FaceLayer* mouth;
};

struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;

    bool empty() const { return w <= 0 || h <= 0; }
};

class FaceRenderer {
public:
    explicit FaceRenderer(const FaceLayer& background);

    bool begin();
    void setExpression(const Expression& expression);
    // nullptr restores the expression's own layer
    void overrideEyes(const FaceLayer* eyes);
    void overrideMouth(const FaceLayer* mouth);

    const Expression& expression() const { return expression_; }

private:
    const FaceLayer* currentEyes() const;
    const FaceLayer* currentMouth() const;
    void render(Rect area);
    void blend(const FaceLayer* layer, const Rect& area);

    const FaceLayer& background_;
    Expression expression_{};
    const FaceLayer* eyesOverride_ = nullptr;
    const FaceLayer* mouthOverride_ = nullptr;
    uint16_t* buffer_ = nullptr;
};
