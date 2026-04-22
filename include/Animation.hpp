#pragma once
#include <cmath>
#include <chrono>

namespace Animation {

static float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float EaseOutCubic(float t) {
    return 1.0f - std::pow(1.0f - t, 3);
}

static float EaseOutQuart(float t) {
    return 1.0f - std::pow(1.0f - t, 4);
}

static float EaseOutExpo(float t) {
    return (t >= 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

static float EaseInOutCubic(float t) {
    return (t < 0.5f) ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3) * 0.5f;
}

static float EaseOutBack(float t) {
    float c1 = 1.70158f;
    float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3) + c1 * std::pow(t - 1.0f, 2);
}

static float BounceOut(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

class SmoothFloat {
private:
    float current;
    float target;
    float speed;
    bool dirty;

public:
    SmoothFloat(float initial = 0.0f, float animSpeed = 8.0f)
        : current(initial), target(initial), speed(animSpeed), dirty(false) {}

    void SetTarget(float value) {
        if (target != value) {
            target = value;
            dirty = true;
        }
    }

    void Set(float value) {
        current = target = value;
        dirty = false;
    }

    float Update(float dt = 0.016f) {
        if (!dirty) return current;
        float diff = target - current;
        if ((diff < 0.001f && diff > -0.001f)) {
            current = target;
            dirty = false;
        } else {
            current += diff * speed * dt;
        }
        return current;
    }

    float Get() const { return current; }
    float GetTarget() const { return target; }
    bool IsAnimating() const { return dirty; }
};

class SmoothVec2 {
private:
    float cx, cy, tx, ty, speed;
    bool dirty;

public:
    SmoothVec2(float ix = 0, float iy = 0, float animSpeed = 8.0f)
        : cx(ix), cy(iy), tx(ix), ty(iy), speed(animSpeed), dirty(false) {}

    void SetTarget(float x, float y) {
        if (tx != x || ty != y) {
            tx = x; ty = y; dirty = true;
        }
    }

    void Set(float x, float y) {
        cx = tx = x; cy = ty = y; dirty = false;
    }

    void Update(float dt = 0.016f) {
        if (!dirty) return;
        float dx = tx - cx, dy = ty - cy;
        if ((dx < 0.01f && dx > -0.01f) && (dy < 0.01f && dy > -0.01f)) {
            cx = tx; cy = ty; dirty = false;
        } else {
            cx += dx * speed * dt;
            cy += dy * speed * dt;
        }
    }

    float X() const { return cx; }
    float Y() const { return cy; }
};

class Timer {
private:
    std::chrono::steady_clock::time_point start;

public:
    Timer() : start(std::chrono::steady_clock::now()) {}

    void Reset() {
        start = std::chrono::steady_clock::now();
    }

    float Elapsed() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<float>(now - start).count();
    }

    float Progress(float duration) const {
        float p = Elapsed() / duration;
        return (p > 1.0f) ? 1.0f : p;
    }
};

}
