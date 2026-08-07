// Ensures leon_core has at least one TU on PS2 lean builds (no GLM host sources).

namespace leon {

namespace {
int g_ps2LeanCoreAnchor = 0;
}

int Ps2LeanCoreAnchor() {
    return g_ps2LeanCoreAnchor;
}

} // namespace leon
