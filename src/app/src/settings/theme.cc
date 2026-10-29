#include "theme.h"

namespace theme {

namespace {

const Palette *gActive = &kDark;

}  // namespace

const Palette &colors() {
    return *gActive;
}

void setDark(bool dark) {
    gActive = dark ? &kDark : &kLight;
}

}  // namespace theme
