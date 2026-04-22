#pragma once

struct Vector2 {
    float x, y;
    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}
};

struct Vector3 {
    float x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct Vector4 {
    float x, y, z, w;
};

struct Matrix3x4 {
    float m[3][4];
};

struct Matrix4x4 {
    float m[16];
};

namespace Bones {
    enum BoneID {
        Pelvis = 0,
        Spine = 4,
        Neck = 5,
        Head = 6,
        ShoulderLeft = 8,
        ElbowLeft = 9,
        HandLeft = 11,
        ShoulderRight = 13,
        ElbowRight = 14,
        HandRight = 16,
        HipLeft = 22,
        KneeLeft = 23,
        FootLeft = 24,
        HipRight = 25,
        KneeRight = 26,
        FootRight = 27
    };
}
