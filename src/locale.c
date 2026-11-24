enum { ITALIAN, ENGLISH, SPANISH, FRENCH, LANG_COUNT };
enum {
#define MSG(key, it, en, es, fr) M_##key,
#include "messages.def"
#undef MSG
    M_COUNT
};
static const S messages[M_COUNT][LANG_COUNT] = {
#define MSG(key, it, en, es, fr) {T(it), T(en), T(es), T(fr)},
#include "messages.def"
#undef MSG
};
static const S languageNames[] = {T("Italiano"), T("English"), T("Español"), T("Français")};
static int language;
static S tr(int key) {
    return messages[key][language];
}
static S fileWord(int count) {
    return tr(count == 1 ? M_FILE_ONE : M_FILES);
}
