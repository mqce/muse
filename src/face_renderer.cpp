#include "face_renderer.h"

#include <esp_heap_caps.h>
#include <string.h>

#include <algorithm>

#include "display.h"

namespace {

Rect rectOf(const FaceLayer* layer) {
    if (layer == nullptr) {
        return {0, 0, 0, 0};
    }
    return {layer->x, layer->y, layer->w, layer->h};
}

Rect unite(const Rect& a, const Rect& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    int16_t left = std::min(a.x, b.x);
    int16_t top = std::min(a.y, b.y);
    int16_t right = std::max(a.x + a.w, b.x + b.w);
    int16_t bottom = std::max(a.y + a.h, b.y + b.h);
    return {left, top, static_cast<int16_t>(right - left), static_cast<int16_t>(bottom - top)};
}

Rect intersect(const Rect& a, const Rect& b) {
    int16_t left = std::max(a.x, b.x);
    int16_t top = std::max(a.y, b.y);
    int16_t right = std::min(a.x + a.w, b.x + b.w);
    int16_t bottom = std::min(a.y + a.h, b.y + b.h);
    return {left, top, static_cast<int16_t>(right - left), static_cast<int16_t>(bottom - top)};
}

// CO5300 requires even start coordinates and even window sizes.
Rect alignEven(const Rect& r, int16_t maxW, int16_t maxH) {
    int16_t left = r.x & ~1;
    int16_t top = r.y & ~1;
    int16_t right = std::min<int16_t>((r.x + r.w + 1) & ~1, maxW);
    int16_t bottom = std::min<int16_t>((r.y + r.h + 1) & ~1, maxH);
    return {left, top, static_cast<int16_t>(right - left), static_cast<int16_t>(bottom - top)};
}

uint16_t blend565(uint16_t dst, uint16_t src, uint8_t alpha) {
    uint32_t inv = 255 - alpha;
    uint32_t r = (((src >> 11) & 0x1F) * alpha + ((dst >> 11) & 0x1F) * inv) / 255;
    uint32_t g = (((src >> 5) & 0x3F) * alpha + ((dst >> 5) & 0x3F) * inv) / 255;
    uint32_t b = ((src & 0x1F) * alpha + (dst & 0x1F) * inv) / 255;
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

}  // namespace

FaceRenderer::FaceRenderer(const FaceLayer& background) : background_(background) {}

bool FaceRenderer::begin() {
    size_t bytes = static_cast<size_t>(background_.w) * background_.h * sizeof(uint16_t);
    buffer_ = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
    return buffer_ != nullptr;
}

void FaceRenderer::setExpression(const Expression& expression) {
    expression_ = expression;
    eyesOverride_ = nullptr;
    mouthOverride_ = nullptr;
    render({0, 0, background_.w, background_.h});
}

void FaceRenderer::overrideEyes(const FaceLayer* eyes) {
    const FaceLayer* before = currentEyes();
    eyesOverride_ = eyes;
    const FaceLayer* after = currentEyes();
    if (before == after) {
        return;
    }
    render(unite(rectOf(before), rectOf(after)));
}

void FaceRenderer::overrideMouth(const FaceLayer* mouth) {
    const FaceLayer* before = currentMouth();
    mouthOverride_ = mouth;
    const FaceLayer* after = currentMouth();
    if (before == after) {
        return;
    }
    render(unite(rectOf(before), rectOf(after)));
}

const FaceLayer* FaceRenderer::currentEyes() const {
    return eyesOverride_ != nullptr ? eyesOverride_ : expression_.eyes;
}

const FaceLayer* FaceRenderer::currentMouth() const {
    return mouthOverride_ != nullptr ? mouthOverride_ : expression_.mouth;
}

void FaceRenderer::render(Rect area) {
    area = alignEven(intersect(area, rectOf(&background_)), background_.w, background_.h);
    if (area.empty() || buffer_ == nullptr) {
        return;
    }

    for (int16_t row = 0; row < area.h; ++row) {
        const uint16_t* src = background_.pixels + (area.y + row) * background_.w + area.x;
        memcpy(buffer_ + row * area.w, src, area.w * sizeof(uint16_t));
    }
    blend(currentEyes(), area);
    blend(currentMouth(), area);

    display::push(area.x, area.y, buffer_, area.w, area.h);
}

void FaceRenderer::blend(const FaceLayer* layer, const Rect& area) {
    Rect overlap = intersect(rectOf(layer), area);
    if (overlap.empty()) {
        return;
    }

    for (int16_t y = overlap.y; y < overlap.y + overlap.h; ++y) {
        int32_t srcRow = (y - layer->y) * layer->w;
        uint16_t* dstRow = buffer_ + (y - area.y) * area.w;
        for (int16_t x = overlap.x; x < overlap.x + overlap.w; ++x) {
            int32_t srcIndex = srcRow + (x - layer->x);
            uint8_t alpha = layer->alpha != nullptr ? layer->alpha[srcIndex] : 255;
            if (alpha == 0) {
                continue;
            }
            uint16_t& dst = dstRow[x - area.x];
            uint16_t src = layer->pixels[srcIndex];
            dst = alpha == 255 ? src : blend565(dst, src, alpha);
        }
    }
}
