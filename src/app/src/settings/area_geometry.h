#pragma once

#include <string>

#include "config.h"

namespace settings {

// Halves round toward +∞.
double roundTo(double value, int digits);

std::string formatRounded(double value, int digits);

double normalizeDegrees(double degrees);

double keepCentre(double centre, double extent, double total, int digits);

// mm.
macosu_pad::Size rotatedExtent(const macosu_pad::TrackpadArea &area);

macosu_pad::Size trackpadLimits(const macosu_pad::TrackpadArea &area, macosu_pad::Size trackpad);

void fitDisplay(macosu_pad::DisplayArea &area, macosu_pad::Size screen);

void fitTrackpad(macosu_pad::TrackpadArea &area, macosu_pad::Size trackpad);

// OpenTabletDriver "Lock aspect ratio"; no-op unless `config.lockAspectRatio`.
void lockAspect(macosu_pad::Config &config, macosu_pad::Size trackpad, bool keepHeight);

}  // namespace settings
