enum { SAGE_THEME, OCEAN_THEME, VIOLET_THEME, AMBER_THEME, CUSTOM_THEME, THEME_COUNT };
enum { BASE_PAPER, BASE_EDGE, BASE_INK, BASE_DIM, BASE_DARK, BASE_NAV, BASE_NAVEDGE,
       BASE_PALE, BASE_MINT, BASE_GREEN, BASE_TEAL, BASE_WASH, COLOR_COUNT };
#define SAGE_COLORS {C(0xf7f8f3),C(0xdce3da),C(0x1c302b),C(0x53685f),C(0x192d29),C(0x263d37),C(0x486055),C(0xc3d2c9),C(0xa2edbe),C(0x216947),C(0x185c42),C(0xe0efe4)}
static const U presetColors[][COLOR_COUNT] = {
    SAGE_COLORS,
    {C(0xf3f7fb),C(0xd3dfeb),C(0x192e46),C(0x4d647a),C(0x122a40),C(0x203b54),C(0x455f75),C(0xc6d9e5),C(0x96dcff),C(0x17668b),C(0x195b83),C(0xdeedf7)},
    {C(0xf8f5fb),C(0xe2d9eb),C(0x352343),C(0x695672),C(0x2b203b),C(0x3f304f),C(0x645273),C(0xded0e8),C(0xddbdff),C(0x75499a),C(0x674184),C(0xeee4f7)},
    {C(0xfaf7f0),C(0xe7dfcf),C(0x392e22),C(0x6d5b47),C(0x30271f),C(0x43382a),C(0x6f5c45),C(0xe4d6bf),C(0xffcf86),C(0x885918),C(0x785021),C(0xf7ebd4)}
};
static const U WHITE = C(0xffffff);
static U activePalette[COLOR_COUNT] = SAGE_COLORS;
#define PAPER activePalette[BASE_PAPER]
#define EDGE activePalette[BASE_EDGE]
#define INK activePalette[BASE_INK]
#define DIM activePalette[BASE_DIM]
#define DARK activePalette[BASE_DARK]
#define NAV activePalette[BASE_NAV]
#define NAVEDGE activePalette[BASE_NAVEDGE]
#define PALE activePalette[BASE_PALE]
#define MINT activePalette[BASE_MINT]
#define GREEN activePalette[BASE_GREEN]
#define TEAL activePalette[BASE_TEAL]
#define themeWash activePalette[BASE_WASH]
static int themeIndex;
static U customColor = 0x3c9977;
static const int themeNames[] = {M_SAGE, M_OCEAN, M_VIOLET, M_AMBER, M_CUSTOM};
static U tint(U a, U b, U part) {
    U out = C(0);
    for (int shift=0; shift<=16; shift+=8) {
        U c = (((a>>shift)&255)*(100-part) + ((b>>shift)&255)*part + 50)/100;
        out |= c<<shift;
    }
    return out;
}
static void setPalette(int index, U rgb) {
    themeIndex = index >= 0 && index < THEME_COUNT ? index : SAGE_THEME;
    customColor = rgb & 0xffffffu;
    if (themeIndex < CUSTOM_THEME) {
        for (int i=0; i<COLOR_COUNT; i++) activePalette[i] = presetColors[themeIndex][i];
        return;
    }
    /* Bounded mixes maintain contrast with very dark, very bright and neutral inputs. */
    PAPER=tint(rgb,WHITE,96); EDGE=tint(rgb,C(0xdfdfdf),88);
    INK=tint(rgb,C(0x17191c),89); DIM=tint(rgb,C(0x45494d),82);
    DARK=tint(rgb,C(0x15191c),89); NAV=tint(rgb,C(0x292c31),84);
    NAVEDGE=tint(rgb,C(0x596067),84); PALE=tint(rgb,C(0xe2e5e8),84);
    MINT=tint(rgb,WHITE,52); GREEN=tint(rgb,C(0x172224),71);
    TEAL=tint(rgb,C(0x192226),73); themeWash=tint(rgb,WHITE,88);
}
static U colorRef(U rgb) {
    return (rgb & 0xff00) | ((rgb >> 16) & 255) | ((rgb & 255) << 16);
}
