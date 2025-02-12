/* Copyright 2020 Jaakko Keränen <jaakko.keranen@iki.fi>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "util.h"

#include "app.h"
#include "bindingswidget.h"
#include "bookmarks.h"
#include "color.h"
#include "command.h"
#include "defs.h"
#include "documentwidget.h"
#include "feeds.h"
#include "gmutil.h"
#include "inputwidget.h"
#include "keys.h"
#include "labelwidget.h"
#include "root.h"
#include "text.h"
#include "touch.h"
#include "widget.h"
#include "window.h"

#if defined (iPlatformAppleMobile)
#   include "../ios.h"
#endif

#include <the_Foundation/math.h>
#include <the_Foundation/path.h>
#include <SDL_timer.h>

iBool isCommand_SDLEvent(const SDL_Event *d) {
    return d->type == SDL_USEREVENT && d->user.code == command_UserEventCode;
}

iBool isCommand_UserEvent(const SDL_Event *d, const char *cmd) {
    return d->type == SDL_USEREVENT && d->user.code == command_UserEventCode &&
           equal_Command(d->user.data1, cmd);
}

const char *command_UserEvent(const SDL_Event *d) {
    if (d->type == SDL_USEREVENT && d->user.code == command_UserEventCode) {
        return d->user.data1;
    }
    return "";
}

static void removePlus_(iString *str) {
    if (endsWith_String(str, "+")) {
        removeEnd_String(str, 1);
        appendCStr_String(str, " ");
    }
}

void toString_Sym(int key, int kmods, iString *str) {
#if defined (iPlatformApple)
    if (kmods & KMOD_CTRL) {
        appendChar_String(str, 0x2303);
    }
    if (kmods & KMOD_ALT) {
        appendChar_String(str, 0x2325);
    }
    if (kmods & KMOD_SHIFT) {
        appendCStr_String(str, shift_Icon);
    }
    if (kmods & KMOD_GUI) {
        appendChar_String(str, 0x2318);
    }
#else
    if (kmods & KMOD_CTRL) {
        appendCStr_String(str, "Ctrl+");
    }
    if (kmods & KMOD_ALT) {
        appendCStr_String(str, "Alt+");
    }
    if (kmods & KMOD_SHIFT) {
        appendCStr_String(str, shift_Icon "+");
    }
    if (kmods & KMOD_GUI) {
        appendCStr_String(str, "Meta+");
    }
#endif
    if (kmods & KMOD_CAPS) {
        appendCStr_String(str, "Caps+");
    }
    if (key == 0x20) {
        appendCStr_String(str, "Space");
    }
    else if (key == SDLK_ESCAPE) {
        appendCStr_String(str, "Esc");
    }
    else if (key == SDLK_LEFT) {
        removePlus_(str);
        appendChar_String(str, 0x2190);
    }
    else if (key == SDLK_RIGHT) {
        removePlus_(str);
        appendChar_String(str, 0x2192);
    }
    else if (key == SDLK_UP) {
        removePlus_(str);
        appendChar_String(str, 0x2191);
    }
    else if (key == SDLK_DOWN) {
        removePlus_(str);
        appendChar_String(str, 0x2193);
    }
    else if (key < 128 && (isalnum(key) || ispunct(key))) {
        if (ispunct(key)) removePlus_(str);
        appendChar_String(str, upper_Char(key));
    }
    else if (key == SDLK_BACKSPACE) {
        removePlus_(str);
        appendChar_String(str, 0x232b); /* Erase to the Left */
    }
    else if (key == SDLK_DELETE) {
        removePlus_(str);
        appendChar_String(str, 0x2326); /* Erase to the Right */
    }
    else if (key == SDLK_RETURN) {
        removePlus_(str);
        appendCStr_String(str, return_Icon); /* Leftwards arrow with a hook */
    }
    else {
        appendCStr_String(str, SDL_GetKeyName(key));
    }
}

iBool isMod_Sym(int key) {
    return key == SDLK_LALT || key == SDLK_RALT || key == SDLK_LCTRL || key == SDLK_RCTRL ||
           key == SDLK_LGUI || key == SDLK_RGUI || key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
           key == SDLK_CAPSLOCK;
}

int normalizedMod_Sym(int key) {
    if (key == SDLK_RSHIFT) key = SDLK_LSHIFT;
    if (key == SDLK_RCTRL) key = SDLK_LCTRL;
    if (key == SDLK_RALT) key = SDLK_LALT;
    if (key == SDLK_RGUI) key = SDLK_LGUI;
    return key;
}

int keyMods_Sym(int kmods) {
    kmods &= (KMOD_SHIFT | KMOD_ALT | KMOD_CTRL | KMOD_GUI | KMOD_CAPS);
    /* Don't treat left/right modifiers differently. */
    if (kmods & KMOD_SHIFT) kmods |= KMOD_SHIFT;
    if (kmods & KMOD_ALT)   kmods |= KMOD_ALT;
    if (kmods & KMOD_CTRL)  kmods |= KMOD_CTRL;
    if (kmods & KMOD_GUI)   kmods |= KMOD_GUI;
    return kmods;
}

int openTabMode_Sym(int kmods) {
    const int km = keyMods_Sym(kmods);
    return (km == KMOD_SHIFT ? otherRoot_OpenTabFlag : 0) | /* open to the side */
           (((km & KMOD_PRIMARY) && (km & KMOD_SHIFT)) ? new_OpenTabFlag :
            (km & KMOD_PRIMARY) ? newBackground_OpenTabFlag : 0);
}

iRangei intersect_Rangei(iRangei a, iRangei b) {
    if (a.end < b.start || a.start > b.end) {
        return (iRangei){ 0, 0 };
    }
    return (iRangei){ iMax(a.start, b.start), iMin(a.end, b.end) };
}

iRangei union_Rangei(iRangei a, iRangei b) {
    if (isEmpty_Rangei(a)) return b;
    if (isEmpty_Rangei(b)) return a;
    return (iRangei){ iMin(a.start, b.start), iMax(a.end, b.end) };
}

iBool isSelectionBreaking_Char(iChar c) {
    return isSpace_Char(c) || (c == '@' || c == '-' || c == '/' || c == '\\' || c == ',');
}

static const char *moveBackward_(const char *pos, iRangecc bounds, int mode) {
    iChar ch;
    while (pos > bounds.start) {
        int len = decodePrecedingBytes_MultibyteChar(pos, bounds.start, &ch);
        if (len > 0) {
            if (mode & word_RangeExtension && isSelectionBreaking_Char(ch)) break;
            if (mode & line_RangeExtension && ch == '\n') break;
            pos -= len;
        }
        else break;
    }
    return pos;
}

static const char *moveForward_(const char *pos, iRangecc bounds, int mode) {
    iChar ch;
    while (pos < bounds.end) {
        int len = decodeBytes_MultibyteChar(pos, bounds.end, &ch);
        if (len > 0) {
            if (mode & word_RangeExtension && isSelectionBreaking_Char(ch)) break;
            if (mode & line_RangeExtension && ch == '\n') break;
            pos += len;
        }
        else break;
    }
    return pos;
}

void extendRange_Rangecc(iRangecc *d, iRangecc bounds, int mode) {
    if (!d->start) return;
    if (d->end >= d->start) {
        if (mode & moveStart_RangeExtension) {
            d->start = moveBackward_(d->start, bounds, mode);
        }
        if (mode & moveEnd_RangeExtension) {
            d->end = moveForward_(d->end, bounds, mode);
        }
    }
    else {
        if (mode & moveStart_RangeExtension) {
            d->start = moveForward_(d->start, bounds, mode);
        }
        if (mode & moveEnd_RangeExtension) {
            d->end = moveBackward_(d->end, bounds, mode);
        }
    }
}

/*----------------------------------------------------------------------------------------------*/

iBool isFinished_Anim(const iAnim *d) {
    return d->from == d->to || frameTime_Window(get_Window()) >= d->due;
}

void init_Anim(iAnim *d, float value) {
    d->due = d->when = SDL_GetTicks();
    d->from = d->to = value;
    d->bounce = 0.0f;
    d->flags = 0;
}

iLocalDef float pos_Anim_(const iAnim *d, uint32_t now) {
    return (float) (now - d->when) / (float) (d->due - d->when);
}

iLocalDef float easeIn_(float t) {
    return t * t;
}

iLocalDef float easeOut_(float t) {
    return t * (2.0f - t);
}

iLocalDef float easeBoth_(float t) {
    if (t < 0.5f) {
        return easeIn_(t * 2.0f) * 0.5f;
    }
    return 0.5f + easeOut_((t - 0.5f) * 2.0f) * 0.5f;
}

static float valueAt_Anim_(const iAnim *d, const uint32_t now) {
    if (now >= d->due) {
        return d->to;
    }
    if (now <= d->when) {
        return d->from;
    }
    float t = pos_Anim_(d, now);
    const iBool isSoft     = (d->flags & softer_AnimFlag) != 0;
    const iBool isVerySoft = (d->flags & muchSofter_AnimFlag) != 0;
    if ((d->flags & easeBoth_AnimFlag) == easeBoth_AnimFlag) {
        t = easeBoth_(t);
        if (isSoft) t = easeBoth_(t);
        if (isVerySoft) t = easeBoth_(easeBoth_(t));
    }
    else if (d->flags & easeIn_AnimFlag) {
        t = easeIn_(t);
        if (isSoft) t = easeIn_(t);
        if (isVerySoft) t = easeIn_(easeIn_(t));
    }
    else if (d->flags & easeOut_AnimFlag) {
        t = easeOut_(t);
        if (isSoft) t = easeOut_(t);
        if (isVerySoft) t = easeOut_(easeOut_(t));
    }
    float value = d->from * (1.0f - t) + d->to * t;
    if (d->flags & bounce_AnimFlag) {
        t = (1.0f - easeOut_(easeOut_(t))) * easeOut_(t);
        value += d->bounce * t;
    }
    return value;
}

void setValue_Anim(iAnim *d, float to, uint32_t span) {
    if (span == 0) {
        d->from = d->to = to;
        d->when = d->due = frameTime_Window(get_Window()); /* effectively in the past */
    }
    else if (fabsf(to - d->to) > 0.00001f) {
        const uint32_t now = SDL_GetTicks();
        d->from = valueAt_Anim_(d, now);
        d->to   = to;
        d->when = now;
        d->due  = now + span;
    }
    d->bounce = 0;
}

void setValueSpeed_Anim(iAnim *d, float to, float unitsPerSecond) {
    if (iAbs(d->to - to) > 0.0001f) {
        const uint32_t now   = SDL_GetTicks();
        const float    from  = valueAt_Anim_(d, now);
        const float    delta = to - from;
        const uint32_t span  = (fabsf(delta) / unitsPerSecond) * 1000;
        d->from              = from;
        d->to                = to;
        d->when              = now;
        d->due               = d->when + span;
        d->bounce            = 0;
    }
}

void setValueEased_Anim(iAnim *d, float to, uint32_t span) {
    if (fabsf(to - d->to) <= 0.00001f) {
        d->to = to; /* Pretty much unchanged. */
        return;
    }
    const uint32_t now = SDL_GetTicks();
    if (isFinished_Anim(d)) {
        d->from  = d->to;
        d->flags = easeBoth_AnimFlag;
    }
    else {
        d->from  = valueAt_Anim_(d, now);
        d->flags = easeOut_AnimFlag;
    }
    d->to     = to;
    d->when   = now;
    d->due    = now + span;
    d->bounce = 0;
}

void setFlags_Anim(iAnim *d, int flags, iBool set) {
    iChangeFlags(d->flags, flags, set);
}

void stop_Anim(iAnim *d) {
    d->from = d->to = value_Anim(d);
    d->when = d->due = SDL_GetTicks();
}

float pos_Anim(const iAnim *d) {
    return pos_Anim_(d, frameTime_Window(get_Window()));
}

float value_Anim(const iAnim *d) {
    return valueAt_Anim_(d, frameTime_Window(get_Window()));
}

/*-----------------------------------------------------------------------------------------------*/

void init_Click(iClick *d, iAnyObject *widget, int button) {
    d->isActive = iFalse;
    d->button   = button;
    d->bounds   = as_Widget(widget);
    d->minHeight = 0;
    d->startPos = zero_I2();
    d->pos      = zero_I2();
}

iBool contains_Click(const iClick *d, iInt2 coord) {
    if (d->minHeight) {
        iRect rect = bounds_Widget(d->bounds);
        rect.size.y = iMax(d->minHeight, rect.size.y);
        return contains_Rect(rect, coord);
    }
    return contains_Widget(d->bounds, coord);
}

enum iClickResult processEvent_Click(iClick *d, const SDL_Event *event) {
    if (event->type == SDL_MOUSEMOTION) {
        const iInt2 pos = init_I2(event->motion.x, event->motion.y);
        if (d->isActive) {
            d->pos = pos;
            return drag_ClickResult;
        }
    }
    if (event->type != SDL_MOUSEBUTTONDOWN && event->type != SDL_MOUSEBUTTONUP) {
        return none_ClickResult;
    }
    const SDL_MouseButtonEvent *mb = &event->button;
    if (mb->button != d->button) {
        return none_ClickResult;
    }
    const iInt2 pos = init_I2(mb->x, mb->y);
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        d->count = mb->clicks;
    }
    if (!d->isActive) {
        if (mb->state == SDL_PRESSED) {
            if (contains_Click(d, pos)) {
                d->isActive = iTrue;
                d->startPos = d->pos = pos;
                setMouseGrab_Widget(d->bounds);
                return started_ClickResult;
            }
        }
    }
    else { /* Active. */
        if (mb->state == SDL_RELEASED) {
            enum iClickResult result = contains_Click(d, pos)
                                           ? finished_ClickResult
                                           : aborted_ClickResult;
            d->isActive = iFalse;
            d->pos = pos;
            setMouseGrab_Widget(NULL);
            return result;
        }
    }
    return none_ClickResult;
}

void cancel_Click(iClick *d) {
    if (d->isActive) {
        d->isActive = iFalse;
        setMouseGrab_Widget(NULL);
    }
}

iBool isMoved_Click(const iClick *d) {
    return dist_I2(d->startPos, d->pos) > 2;
}

iInt2 pos_Click(const iClick *d) {
    return d->pos;
}

iRect rect_Click(const iClick *d) {
    return initCorners_Rect(min_I2(d->startPos, d->pos), max_I2(d->startPos, d->pos));
}

iInt2 delta_Click(const iClick *d) {
    return sub_I2(d->pos, d->startPos);
}

/*----------------------------------------------------------------------------------------------*/

void init_SmoothScroll(iSmoothScroll *d, iWidget *owner, iSmoothScrollNotifyFunc notify) {
    reset_SmoothScroll(d);
    d->widget = owner;
    d->notify = notify;
}

void reset_SmoothScroll(iSmoothScroll *d) {
    init_Anim(&d->pos, 0);
    d->max = 0;
    d->overscroll = (deviceType_App() != desktop_AppDeviceType ? 100 * gap_UI : 0);
}

void setMax_SmoothScroll(iSmoothScroll *d, int max) {
    max = iMax(0, max);
    if (max != d->max) {
        d->max = max;
        if (targetValue_Anim(&d->pos) > d->max) {
            d->pos.to = d->max;
        }
    }
}

static int overscroll_SmoothScroll_(const iSmoothScroll *d) {
    if (d->overscroll) {
        const int y = value_Anim(&d->pos);
        if (y <= 0) {
            return y;
        }
        if (y >= d->max) {
            return y - d->max;
        }
    }
    return 0;
}

float pos_SmoothScroll(const iSmoothScroll *d) {
    return value_Anim(&d->pos) - overscroll_SmoothScroll_(d) * 0.667f;
}

iBool isFinished_SmoothScroll(const iSmoothScroll *d) {
    return isFinished_Anim(&d->pos);
}

void moveSpan_SmoothScroll(iSmoothScroll *d, int offset, uint32_t span) {
#if !defined (iPlatformMobile)
    if (!prefs_App()->smoothScrolling) {
        span = 0; /* always instant */
    }
#endif
    int destY = targetValue_Anim(&d->pos) + offset;
    if (destY < -d->overscroll) {
        destY = -d->overscroll;
    }
    if (d->max > 0) {
        if (destY >= d->max + d->overscroll) {
            destY = d->max + d->overscroll;
        }
    }
    else {
        destY = 0;
    }
    if (span) {
        setValueEased_Anim(&d->pos, destY, span);
    }
    else {
        setValue_Anim(&d->pos, destY, 0);
    }
    if (d->overscroll && widgetMode_Touch(d->widget) == momentum_WidgetTouchMode) {
        const int osDelta = overscroll_SmoothScroll_(d);
        if (osDelta) {
            const float remaining = stopWidgetMomentum_Touch(d->widget);
            span = iMini(1000, 50 * sqrt(remaining / gap_UI));
            setValue_Anim(&d->pos, osDelta < 0 ? 0 : d->max, span);
            d->pos.flags = bounce_AnimFlag | easeOut_AnimFlag | softer_AnimFlag;
            //            printf("remaining: %f  dur: %d\n", remaining, duration);
            d->pos.bounce = (osDelta < 0 ? -1 : 1) *
                            iMini(5 * d->overscroll, remaining * remaining * 0.00005f);
        }
    }
    if (d->notify) {
        d->notify(d->widget, offset, span);
    }
}

void move_SmoothScroll(iSmoothScroll *d, int offset) {
    moveSpan_SmoothScroll(d, offset, 0 /* instantly */);
}

iBool processEvent_SmoothScroll(iSmoothScroll *d, const SDL_Event *ev) {
    if (ev->type == SDL_USEREVENT && ev->user.code == widgetTouchEnds_UserEventCode) {
        const int osDelta = overscroll_SmoothScroll_(d);
        if (osDelta) {
            moveSpan_SmoothScroll(d, -osDelta, 100 * sqrt(iAbs(osDelta) / gap_UI));
            d->pos.flags = easeOut_AnimFlag | muchSofter_AnimFlag;
        }
        return iTrue;
    }
    return iFalse;
}

/*-----------------------------------------------------------------------------------------------*/

iWidget *makePadding_Widget(int size) {
    iWidget *pad = new_Widget();
    setId_Widget(pad, "padding");
    setFixedSize_Widget(pad, init1_I2(size));
    return pad;
}

iLabelWidget *makeHeading_Widget(const char *text) {
    iLabelWidget *heading = new_LabelWidget(text, NULL);
    setFlags_Widget(as_Widget(heading), frameless_WidgetFlag | alignLeft_WidgetFlag, iTrue);
    setBackgroundColor_Widget(as_Widget(heading), none_ColorId);
    return heading;
}

iWidget *makeVDiv_Widget(void) {
    iWidget *div = new_Widget();
    setFlags_Widget(div, resizeChildren_WidgetFlag | arrangeVertical_WidgetFlag | unhittable_WidgetFlag, iTrue);
    return div;
}

iWidget *makeHDiv_Widget(void) {
    iWidget *div = new_Widget();
    setFlags_Widget(div, resizeChildren_WidgetFlag | arrangeHorizontal_WidgetFlag | unhittable_WidgetFlag, iTrue);
    return div;
}

iWidget *addAction_Widget(iWidget *parent, int key, int kmods, const char *command) {
    iLabelWidget *action = newKeyMods_LabelWidget("", key, kmods, command);
    setFixedSize_Widget(as_Widget(action), zero_I2());
    addChildFlags_Widget(parent, iClob(action), hidden_WidgetFlag);
    return as_Widget(action);
}

iBool isAction_Widget(const iWidget *d) {
    return isInstance_Object(d, &Class_LabelWidget) && isEqual_I2(d->rect.size, zero_I2());
}

/*-----------------------------------------------------------------------------------------------*/

static iBool isCommandIgnoredByMenus_(const char *cmd) {
    /* TODO: Perhaps a common way of indicating which commands are notifications and should not
       be reacted to by menus? */
    return equal_Command(cmd, "media.updated") ||
           equal_Command(cmd, "media.player.update") ||
           startsWith_CStr(cmd, "feeds.update.") ||
           equal_Command(cmd, "bookmarks.request.started") ||
           equal_Command(cmd, "bookmarks.request.finished") ||
           equal_Command(cmd, "bookmarks.changed") ||
           equal_Command(cmd, "document.autoreload") ||
           equal_Command(cmd, "document.reload") ||
           equal_Command(cmd, "document.request.started") ||
           equal_Command(cmd, "document.request.updated") ||
           equal_Command(cmd, "document.request.finished") ||
           equal_Command(cmd, "document.changed") ||
           equal_Command(cmd, "scrollbar.fade") ||
           equal_Command(cmd, "visited.changed") ||
           (deviceType_App() == desktop_AppDeviceType && equal_Command(cmd, "window.resized")) ||
           equal_Command(cmd, "widget.overflow") ||
           equal_Command(cmd, "window.reload.update") ||
           equal_Command(cmd, "window.mouse.exited") ||
           equal_Command(cmd, "window.mouse.entered") ||
           (equal_Command(cmd, "mouse.clicked") && !arg_Command(cmd)); /* button released */
}

static iLabelWidget *parentMenuButton_(const iWidget *menu) {
    if (isInstance_Object(menu->parent, &Class_LabelWidget)) {
        iLabelWidget *button = (iLabelWidget *) menu->parent;
        if (!cmp_String(command_LabelWidget(button), "menu.open")) {
            return button;
        }
    }
    return NULL;
}

static iBool menuHandler_(iWidget *menu, const char *cmd) {
    if (isVisible_Widget(menu)) {
        if (equalWidget_Command(cmd, menu, "menu.opened")) {
            return iFalse;
        }
        if (equal_Command(cmd, "menu.open") && pointer_Command(cmd) == menu->parent) {
            /* Don't reopen self; instead, root will close the menu. */
            return iFalse;
        }
        if ((equal_Command(cmd, "mouse.clicked") || equal_Command(cmd, "mouse.missed")) &&
            arg_Command(cmd)) {
            if (hitChild_Window(get_Window(), coord_Command(cmd)) == parentMenuButton_(menu)) {
                return iFalse;
            }
            /* Dismiss open menus when clicking outside them. */
            closeMenu_Widget(menu);
            return iTrue;
        }
        if (!isCommandIgnoredByMenus_(cmd)) {
            closeMenu_Widget(menu);
        }
    }
    return iFalse;
}

static iWidget *makeMenuSeparator_(void) {
    iWidget *sep = new_Widget();
    setBackgroundColor_Widget(sep, uiSeparator_ColorId);
    sep->rect.size.y = gap_UI / 3;
    if (deviceType_App() != desktop_AppDeviceType) {
        sep->rect.size.y = gap_UI / 2;
    }
    setFlags_Widget(sep, hover_WidgetFlag | fixedHeight_WidgetFlag, iTrue);
    return sep;
}

iWidget *makeMenu_Widget(iWidget *parent, const iMenuItem *items, size_t n) {
    iWidget *menu = new_Widget();
    setBackgroundColor_Widget(menu, uiBackgroundMenu_ColorId);
    if (deviceType_App() != desktop_AppDeviceType) {
        setPadding1_Widget(menu, 2 * gap_UI);
    }
    else {
        setPadding1_Widget(menu, gap_UI / 2);
    }
    const iBool isPortraitPhone = (deviceType_App() == phone_AppDeviceType && isPortrait_App());
    int64_t itemFlags = (deviceType_App() != desktop_AppDeviceType ? 0 : 0) |
                        (isPortraitPhone ? extraPadding_WidgetFlag : 0);
    setFlags_Widget(menu,
                    keepOnTop_WidgetFlag | collapse_WidgetFlag | hidden_WidgetFlag |
                        arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag |
                        resizeChildrenToWidestChild_WidgetFlag | overflowScrollable_WidgetFlag |
                        (isPortraitPhone ? drawBackgroundToVerticalSafeArea_WidgetFlag : 0),
                    iTrue);
    if (!isPortraitPhone) {
        setFrameColor_Widget(menu, uiSeparator_ColorId);
    }
    iBool haveIcons = iFalse;
    for (size_t i = 0; i < n; ++i) {
        const iMenuItem *item = &items[i];
        if (equal_CStr(item->label, "---")) {
            addChild_Widget(menu, iClob(makeMenuSeparator_()));
        }
        else {
            iBool isInfo = iFalse;
            const char *labelText = item->label;
            if (startsWith_CStr(labelText, "```")) {
                labelText += 3;
                isInfo = iTrue;
            }
            iLabelWidget *label = addChildFlags_Widget(
                menu,
                iClob(newKeyMods_LabelWidget(labelText, item->key, item->kmods, item->command)),
                noBackground_WidgetFlag | frameless_WidgetFlag | alignLeft_WidgetFlag |
                drawKey_WidgetFlag | (isInfo ? wrapText_WidgetFlag : 0) | itemFlags);
            haveIcons |= checkIcon_LabelWidget(label);
            updateSize_LabelWidget(label); /* drawKey was set */
            if (isInfo) {
                setTextColor_LabelWidget(label, uiTextAction_ColorId);
            }
        }
    }
    if (deviceType_App() == phone_AppDeviceType) {
        addChild_Widget(menu, iClob(makeMenuSeparator_()));
        addChildFlags_Widget(menu,
                             iClob(new_LabelWidget("${cancel}", "cancel")),
                             itemFlags | noBackground_WidgetFlag | frameless_WidgetFlag |
                             alignLeft_WidgetFlag);
    }
    if (haveIcons) {
        /* All items must have icons if at least one of them has. */
        iForEach(ObjectList, i, children_Widget(menu)) {
            if (isInstance_Object(i.object, &Class_LabelWidget)) {
                iLabelWidget *label = i.object;
                if (icon_LabelWidget(label) == 0) {
                    setIcon_LabelWidget(label, ' ');
                }
            }
        }
    }
    addChild_Widget(parent, menu);
    iRelease(menu); /* owned by parent now */
    setCommandHandler_Widget(menu, menuHandler_);
    iWidget *cancel = addAction_Widget(menu, SDLK_ESCAPE, 0, "cancel");
    setId_Widget(cancel, "menu.cancel");
    setFlags_Widget(cancel, disabled_WidgetFlag, iTrue);
    return menu;
}

void openMenu_Widget(iWidget *d, iInt2 windowCoord) {
    openMenuFlags_Widget(d, windowCoord, iTrue);
}

void openMenuFlags_Widget(iWidget *d, iInt2 windowCoord, iBool postCommands) {
    const iRect rootRect        = rect_Root(d->root);
    const iInt2 rootSize        = rootRect.size;
    const iBool isPortraitPhone = (deviceType_App() == phone_AppDeviceType && isPortrait_App());
    const iBool isSlidePanel    = (flags_Widget(d) & horizontalOffset_WidgetFlag) != 0;
    if (postCommands) {
        postCommand_App("cancel"); /* dismiss any other menus */
    }
    /* Menu closes when commands are emitted, so handle any pending ones beforehand. */
    processEvents_App(postedEventsOnly_AppEventMode);
    setFlags_Widget(d, hidden_WidgetFlag, iFalse);
    setFlags_Widget(d, commandOnMouseMiss_WidgetFlag, iTrue);
    raise_Widget(d);
    setFlags_Widget(findChild_Widget(d, "menu.cancel"), disabled_WidgetFlag, iFalse);
    if (isPortraitPhone) {
        setFlags_Widget(d, arrangeWidth_WidgetFlag | resizeChildrenToWidestChild_WidgetFlag, iFalse);
        setFlags_Widget(d, resizeWidthOfChildren_WidgetFlag | drawBackgroundToBottom_WidgetFlag, iTrue);
        if (!isSlidePanel) {
            setFlags_Widget(d, borderTop_WidgetFlag, iTrue);
        }
        d->rect.size.x = rootSize.x;
    }
    /* Update item fonts. */ {
        iForEach(ObjectList, i, children_Widget(d)) {
            if (isInstance_Object(i.object, &Class_LabelWidget)) {
                iLabelWidget *label = i.object;
                const iBool isCaution = startsWith_String(text_LabelWidget(label), uiTextCaution_ColorEscape);
                if (flags_Widget(as_Widget(label)) & wrapText_WidgetFlag) {
                    continue;
                }
                if (deviceType_App() == desktop_AppDeviceType) {
                    setFont_LabelWidget(label, isCaution ? uiLabelBold_FontId : uiLabel_FontId);
                }
                else if (isPortraitPhone) {
                    if (!isSlidePanel) {
                        setFont_LabelWidget(label, isCaution ? defaultBigBold_FontId : defaultBig_FontId);
                    }
                }
                else {
                    setFont_LabelWidget(label, isCaution ? uiContentBold_FontId : uiContent_FontId);
                }
            }
        }
    }
    arrange_Widget(d);
    if (isPortraitPhone) {
        if (isSlidePanel) {
            d->rect.pos = zero_I2(); //neg_I2(bounds_Widget(parent_Widget(d)).pos);
        }
        else {
            d->rect.pos = init_I2(0, rootSize.y);
        }
    }
    else {
        d->rect.pos = windowToLocal_Widget(d, windowCoord);
    }
    /* Ensure the full menu is visible. */
    const iRect bounds       = bounds_Widget(d);
    int         leftExcess   = left_Rect(rootRect) - left_Rect(bounds);
    int         rightExcess  = right_Rect(bounds) - right_Rect(rootRect);
    int         topExcess    = top_Rect(rootRect) - top_Rect(bounds);
    int         bottomExcess = bottom_Rect(bounds) - bottom_Rect(rootRect);
#if defined (iPlatformAppleMobile)
    /* Reserve space for the system status bar. */ {
        float l, t, r, b;
        safeAreaInsets_iOS(&l, &t, &r, &b);
        topExcess    += t;
        bottomExcess += iMax(b, get_Window()->keyboardHeight);
        leftExcess   += l;
        rightExcess  += r;
    }
#endif
    if (bottomExcess > 0 && (!isPortraitPhone || !isSlidePanel)) {
        d->rect.pos.y -= bottomExcess;
    }
    if (topExcess > 0) {
        d->rect.pos.y += topExcess;
    }
    if (rightExcess > 0) {
        d->rect.pos.x -= rightExcess;
    }
    if (leftExcess > 0) {
        d->rect.pos.x += leftExcess;
    }
    postRefresh_App();
    if (postCommands) {
        postCommand_Widget(d, "menu.opened");
    }
    setupMenuTransition_Mobile(d, iTrue);
}

void closeMenu_Widget(iWidget *d) {
    if (d == NULL || flags_Widget(d) & hidden_WidgetFlag) {
        return; /* Already closed. */
    }
    setFlags_Widget(d, hidden_WidgetFlag, iTrue);
    setFlags_Widget(findChild_Widget(d, "menu.cancel"), disabled_WidgetFlag, iTrue);
    postRefresh_App();
    postCommand_Widget(d, "menu.closed");
    setupMenuTransition_Mobile(d, iFalse);
}

iLabelWidget *findMenuItem_Widget(iWidget *menu, const char *command) {
    iForEach(ObjectList, i, children_Widget(menu)) {
        if (isInstance_Object(i.object, &Class_LabelWidget)) {
            iLabelWidget *menuItem = i.object;
            if (!cmp_String(command_LabelWidget(menuItem), command)) {
                return menuItem;
            }
        }
    }
    return NULL;
}

int checkContextMenu_Widget(iWidget *menu, const SDL_Event *ev) {
    if (menu && ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_RIGHT) {
        if (isVisible_Widget(menu)) {
            closeMenu_Widget(menu);
            return 0x1;
        }
        const iInt2 mousePos = init_I2(ev->button.x, ev->button.y);
        if (contains_Widget(menu->parent, mousePos)) {
            openMenu_Widget(menu, mousePos);
            return 0x2;
        }
    }
    return 0;
}

iLabelWidget *makeMenuButton_LabelWidget(const char *label, const iMenuItem *items, size_t n) {
    iLabelWidget *button = new_LabelWidget(label, "menu.open");
    iWidget *menu = makeMenu_Widget(as_Widget(button), items, n);
    setId_Widget(menu, "menu");
    return button;
}

/*-----------------------------------------------------------------------------------------------*/

static iBool isTabPage_Widget_(const iWidget *tabs, const iWidget *page) {
    return page && page->parent == findChild_Widget(tabs, "tabs.pages");
}

static void unfocusFocusInsideTabPage_(const iWidget *page) {
    iWidget *focus = focus_Widget();
    if (page && focus && hasParent_Widget(focus, page)) {
//        printf("unfocus inside page: %p\n", focus);
        setFocus_Widget(NULL);
    }
}

static iBool tabSwitcher_(iWidget *tabs, const char *cmd) {
    if (equal_Command(cmd, "tabs.switch")) {
        iWidget *target = pointerLabel_Command(cmd, "page");
        if (!target) {
            target = findChild_Widget(tabs, cstr_Rangecc(range_Command(cmd, "id")));
        }
        if (!target) return iFalse;
        unfocusFocusInsideTabPage_(currentTabPage_Widget(tabs));
        if (flags_Widget(target) & focusable_WidgetFlag) {
            setFocus_Widget(target);
        }
        if (isTabPage_Widget_(tabs, target)) {
            showTabPage_Widget(tabs, target);
            return iTrue;
        }
        else if (hasParent_Widget(target, tabs)) {
            /* Some widget on a page. */
            while (target && !isTabPage_Widget_(tabs, target)) {
                target = target->parent;
            }
            showTabPage_Widget(tabs, target);
            return iTrue;
        }
    }
    else if (equal_Command(cmd, "tabs.next") || equal_Command(cmd, "tabs.prev")) {
        unfocusFocusInsideTabPage_(currentTabPage_Widget(tabs));
        iWidget *pages = findChild_Widget(tabs, "tabs.pages");
        int tabIndex = 0;
        iConstForEach(ObjectList, i, pages->children) {
            const iWidget *child = constAs_Widget(i.object);
            if (isVisible_Widget(child)) break;
            tabIndex++;
        }
        const int dir = (equal_Command(cmd, "tabs.next") ? +1 : -1);
        /* If out of tabs, rotate to the next set of tabs if one is available. */
        if ((tabIndex == 0 && dir < 0) || (tabIndex == childCount_Widget(pages) - 1 && dir > 0)) {
            iWidget *nextTabs = findChild_Widget(otherRoot_Window(get_Window(), tabs->root)->widget,
                                                 "doctabs");
            iWidget *nextPages = findChild_Widget(nextTabs, "tabs.pages");
            tabIndex = (dir < 0 ? childCount_Widget(nextPages) - 1 : 0);
            showTabPage_Widget(nextTabs, child_Widget(nextPages, tabIndex));
            postCommand_App("keyroot.next");
        }
        else {
            showTabPage_Widget(tabs, child_Widget(pages, tabIndex + dir));
        }
        refresh_Widget(tabs);
        return iTrue;
    }
    return iFalse;
}

iWidget *makeTabs_Widget(iWidget *parent) {
    iWidget *tabs = makeVDiv_Widget();
    iWidget *buttons = addChild_Widget(tabs, iClob(new_Widget()));
    setFlags_Widget(buttons,
                    resizeWidthOfChildren_WidgetFlag | arrangeHorizontal_WidgetFlag |
                        arrangeHeight_WidgetFlag,
                    iTrue);
    setId_Widget(buttons, "tabs.buttons");
    iWidget *content = addChildFlags_Widget(tabs, iClob(makeHDiv_Widget()), expand_WidgetFlag);
    setId_Widget(content, "tabs.content");
    iWidget *pages = addChildFlags_Widget(
        content, iClob(new_Widget()), expand_WidgetFlag | resizeChildren_WidgetFlag);
    setId_Widget(pages, "tabs.pages");
    addChild_Widget(parent, iClob(tabs));
    setCommandHandler_Widget(tabs, tabSwitcher_);
    return tabs;
}

static void addTabPage_Widget_(iWidget *tabs, enum iWidgetAddPos addPos, iWidget *page,
                               const char *label, int key, int kmods) {
    iWidget *   pages   = findChild_Widget(tabs, "tabs.pages");
    const iBool isSel   = childCount_Widget(pages) == 0;
    iWidget *   buttons = findChild_Widget(tabs, "tabs.buttons");
    iWidget *   button  = addChildPos_Widget(
        buttons,
        iClob(newKeyMods_LabelWidget(label, key, kmods, format_CStr("tabs.switch page:%p", page))),
        addPos);
    setFlags_Widget(button, selected_WidgetFlag, isSel);
    setFlags_Widget(
        button, noTopFrame_WidgetFlag | commandOnClick_WidgetFlag | expand_WidgetFlag, iTrue);
    addChildPos_Widget(pages, page, addPos);
    if (tabCount_Widget(tabs) > 1) {
        setFlags_Widget(buttons, hidden_WidgetFlag, iFalse);
    }
    setFlags_Widget(page, hidden_WidgetFlag | disabled_WidgetFlag, !isSel);
}

void appendTabPage_Widget(iWidget *tabs, iWidget *page, const char *label, int key, int kmods) {
    addTabPage_Widget_(tabs, back_WidgetAddPos, page, label, key, kmods);
}

void prependTabPage_Widget(iWidget *tabs, iWidget *page, const char *label, int key, int kmods) {
    addTabPage_Widget_(tabs, front_WidgetAddPos, page, label, key, kmods);
}

void moveTabButtonToEnd_Widget(iWidget *tabButton) {
    iWidget *buttons = tabButton->parent;
    iWidget *tabs    = buttons->parent;
    removeChild_Widget(buttons, tabButton);
    addChild_Widget(buttons, iClob(tabButton));
    arrange_Widget(tabs);
}

iWidget *tabPage_Widget(iWidget *tabs, size_t index) {
    iWidget *pages = findChild_Widget(tabs, "tabs.pages");
    return child_Widget(pages, index);
}

iWidget *removeTabPage_Widget(iWidget *tabs, size_t index) {
    iWidget *buttons = findChild_Widget(tabs, "tabs.buttons");
    iWidget *pages   = findChild_Widget(tabs, "tabs.pages");
    iWidget *button  = removeChild_Widget(buttons, child_Widget(buttons, index));
    iRelease(button);
    iWidget *page = child_Widget(pages, index);
    setFlags_Widget(page, hidden_WidgetFlag | disabled_WidgetFlag, iFalse);
    removeChild_Widget(pages, page); /* `page` is now ours */
    if (tabCount_Widget(tabs) <= 1 && flags_Widget(buttons) & collapse_WidgetFlag) {
        setFlags_Widget(buttons, hidden_WidgetFlag, iTrue);
    }
    return page;
}

void resizeToLargestPage_Widget(iWidget *tabs) {
//    puts("RESIZE TO LARGEST PAGE ...");
    iWidget *pages = findChild_Widget(tabs, "tabs.pages");
    iForEach(ObjectList, i, children_Widget(pages)) {
        setMinSize_Widget(i.object, zero_I2());
//        resetSize_Widget(i.object);
    }
    arrange_Widget(tabs);
    iInt2 largest = zero_I2();
    iConstForEach(ObjectList, j, children_Widget(pages)) {
        const iWidget *page = constAs_Widget(j.object);
        largest = max_I2(largest, page->rect.size);
    }
    iForEach(ObjectList, k, children_Widget(pages)) {
        setMinSize_Widget(k.object, largest);
    }
    setFixedSize_Widget(tabs, addY_I2(largest, height_Widget(findChild_Widget(tabs, "tabs.buttons"))));
//    puts("... DONE WITH RESIZE TO LARGEST PAGE");
}

iLabelWidget *tabButtonForPage_Widget_(iWidget *tabs, const iWidget *page) {
    iWidget *buttons = findChild_Widget(tabs, "tabs.buttons");
    iForEach(ObjectList, i, buttons->children) {
        iAssert(isInstance_Object(i.object, &Class_LabelWidget));
        iAny *label = i.object;
        if (pointerLabel_Command(cstr_String(command_LabelWidget(label)), "page") == page) {
            return label;
        }
    }
    return NULL;
}

void showTabPage_Widget(iWidget *tabs, const iWidget *page) {
    if (!page) {
        return;
    }
    /* Select the corresponding button. */ {
        iWidget *buttons = findChild_Widget(tabs, "tabs.buttons");
        iForEach(ObjectList, i, buttons->children) {
            iAssert(isInstance_Object(i.object, &Class_LabelWidget));
            iAny *label = i.object;
            const iBool isSel =
                (pointerLabel_Command(cstr_String(command_LabelWidget(label)), "page") == page);
            setFlags_Widget(label, selected_WidgetFlag, isSel);
        }
    }
    /* Show/hide pages. */ {
        iWidget *pages = findChild_Widget(tabs, "tabs.pages");
        iForEach(ObjectList, i, pages->children) {
            iWidget *child = as_Widget(i.object);
            setFlags_Widget(child, hidden_WidgetFlag | disabled_WidgetFlag, child != page);
        }
    }
    /* Notify. */
    if (!isEmpty_String(id_Widget(page))) {
        postCommandf_Root(page->root, "tabs.changed id:%s", cstr_String(id_Widget(page)));
    }
}

iLabelWidget *tabPageButton_Widget(iWidget *tabs, const iAnyObject *page) {
    return tabButtonForPage_Widget_(tabs, page);
}

iBool isTabButton_Widget(const iWidget *d) {
    return d->parent && cmp_String(id_Widget(d->parent), "tabs.buttons") == 0;
}

void setTabPageLabel_Widget(iWidget *tabs, const iAnyObject *page, const iString *label) {
    iLabelWidget *button = tabButtonForPage_Widget_(tabs, page);
    setText_LabelWidget(button, label);
    arrange_Widget(tabs);
}

size_t tabPageIndex_Widget(const iWidget *tabs, const iAnyObject *page) {
    iWidget *pages = findChild_Widget(tabs, "tabs.pages");
    return childIndex_Widget(pages, page);
}

const iWidget *currentTabPage_Widget(const iWidget *tabs) {
    iWidget *pages = findChild_Widget(tabs, "tabs.pages");
    iConstForEach(ObjectList, i, pages->children) {
        if (isVisible_Widget(i.object)) {
            return constAs_Widget(i.object);
        }
    }
    return NULL;
}

size_t tabCount_Widget(const iWidget *tabs) {
    return childCount_Widget(findChild_Widget(tabs, "tabs.pages"));
}

/*-----------------------------------------------------------------------------------------------*/


iWidget *makeSheet_Widget(const char *id) {
    iWidget *sheet = new_Widget();
    setId_Widget(sheet, id);
    setPadding1_Widget(sheet, 3 * gap_UI);
    setFrameColor_Widget(sheet, uiSeparator_ColorId);
    setBackgroundColor_Widget(sheet, uiBackground_ColorId);
    setFlags_Widget(sheet,
                    parentCannotResize_WidgetFlag |
                        focusRoot_WidgetFlag | mouseModal_WidgetFlag | keepOnTop_WidgetFlag |
                        arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag |
                        centerHorizontal_WidgetFlag | overflowScrollable_WidgetFlag,
                    iTrue);
    return sheet;
}

static void acceptValueInput_(iWidget *dlg) {
    const iInputWidget *input = findChild_Widget(dlg, "input");
    if (!isEmpty_String(id_Widget(dlg))) {
        const iString *val = text_InputWidget(input);
        postCommandf_App("%s arg:%d value:%s",
                         cstr_String(id_Widget(dlg)),
                         toInt_String(val),
                         cstr_String(val));
    }
}

static void updateValueInputWidth_(iWidget *dlg) {
    const iRect safeRoot = safeRect_Root(dlg->root);
    const iInt2 rootSize = safeRoot.size;
    iWidget *   title    = findChild_Widget(dlg, "valueinput.title");
    iWidget *   prompt   = findChild_Widget(dlg, "valueinput.prompt");
    if (deviceType_App() == phone_AppDeviceType) {
        dlg->rect.size.x = rootSize.x;
    }
    else {
        dlg->rect.size.x =
            iMin(rootSize.x, iMaxi(iMaxi(100 * gap_UI, title->rect.size.x), prompt->rect.size.x));
    }
}

iBool valueInputHandler_(iWidget *dlg, const char *cmd) {
    iWidget *ptr = as_Widget(pointer_Command(cmd));
    if (equal_Command(cmd, "window.resized")) {
        if (isVisible_Widget(dlg)) {
            updateValueInputWidth_(dlg);
            arrange_Widget(dlg);
        }
        return iFalse;
    }
    if (equal_Command(cmd, "input.ended")) {
        if (argLabel_Command(cmd, "enter") && hasParent_Widget(ptr, dlg)) {
            if (arg_Command(cmd)) {
                acceptValueInput_(dlg);
            }
            else {
                postCommandf_App("valueinput.cancelled id:%s", cstr_String(id_Widget(dlg)));
                setId_Widget(dlg, ""); /* no further commands to emit */
            }
            setupSheetTransition_Mobile(dlg, iFalse);
            destroy_Widget(dlg);
            return iTrue;
        }
        return iFalse;
    }
    else if (equal_Command(cmd, "cancel")) {
        postCommandf_App("valueinput.cancelled id:%s", cstr_String(id_Widget(dlg)));
        setId_Widget(dlg, ""); /* no further commands to emit */
        setupSheetTransition_Mobile(dlg, iFalse);
        destroy_Widget(dlg);
        return iTrue;
    }
    else if (equal_Command(cmd, "valueinput.accept")) {
        acceptValueInput_(dlg);
        setupSheetTransition_Mobile(dlg, iFalse);        
        destroy_Widget(dlg);
        return iTrue;
    }
    return iFalse;
}

iWidget *makeDialogButtons_Widget(const iMenuItem *actions, size_t numActions) {
    iWidget *div = new_Widget();
    setId_Widget(div, "dialogbuttons");
    setFlags_Widget(div,
                    arrangeHorizontal_WidgetFlag | arrangeHeight_WidgetFlag |
                        resizeToParentWidth_WidgetFlag |
                        resizeWidthOfChildren_WidgetFlag,
                    iTrue);
    /* If there is no separator, align everything to the right. */
    iBool haveSep = iFalse;
    for (size_t i = 0; i < numActions; i++) {
        if (!iCmpStr(actions[i].label, "---")) {
            haveSep = iTrue;
            break;
        }
    }
    if (!haveSep) {
        addChildFlags_Widget(div, iClob(new_Widget()), expand_WidgetFlag);
    }
    int fonts[2] = { uiLabel_FontId, uiLabelBold_FontId };
    if (deviceType_App() == phone_AppDeviceType) {
        fonts[0] = defaultMedium_FontId;
        fonts[1] = defaultMediumBold_FontId;
    }
    for (size_t i = 0; i < numActions; i++) {
        const char *label     = actions[i].label;
        const char *cmd       = actions[i].command;
        int         key       = actions[i].key;
        int         kmods     = actions[i].kmods;
        const iBool isDefault = (i == numActions - 1);
        if (*label == '*' || *label == '&') {
            continue; /* Special value selection items for a Question dialog. */
        }
        if (startsWith_CStr(label, "```")) {
            /* Annotation. */
            iLabelWidget *annotation = addChild_Widget(div, iClob(new_LabelWidget(label + 3, NULL)));
            setTextColor_LabelWidget(annotation, uiTextAction_ColorId);
            continue;
        }
        if (!iCmpStr(label, "---")) {
            /* Separator.*/
            addChildFlags_Widget(div, iClob(new_Widget()), expand_WidgetFlag);
            continue;
        }
        if (!iCmpStr(label, "${cancel}") && !cmd) {
            cmd = "cancel";
            key = SDLK_ESCAPE;
            kmods = 0;
        }
        if (isDefault) {
            if (!key) {
                key = SDLK_RETURN;
                kmods = 0;
            }
            if (label == NULL) {
                label = format_CStr(uiTextAction_ColorEscape "%s", cstr_Lang("dlg.default"));
            }
        }
        iLabelWidget *button =
            addChild_Widget(div, iClob(newKeyMods_LabelWidget(label, key, kmods, cmd)));
        if (isDefault) {
            setId_Widget(as_Widget(button), "default");
        }
        setFlags_Widget(as_Widget(button), alignLeft_WidgetFlag | drawKey_WidgetFlag, isDefault);
        setFont_LabelWidget(button, isDefault ? fonts[1] : fonts[0]);
    }
    return div;
}

iWidget *makeValueInput_Widget(iWidget *parent, const iString *initialValue, const char *title,
                               const char *prompt, const char *acceptLabel, const char *command) {
    if (parent) {
        setFocus_Widget(NULL);
    }
    iWidget *dlg = makeSheet_Widget(command);
    setCommandHandler_Widget(dlg, valueInputHandler_);
    if (parent) {
        addChild_Widget(parent, iClob(dlg));
    }
    setId_Widget(
        addChildFlags_Widget(dlg, iClob(new_LabelWidget(title, NULL)), frameless_WidgetFlag),
        "valueinput.title");
    setId_Widget(
        addChildFlags_Widget(dlg, iClob(new_LabelWidget(prompt, NULL)), frameless_WidgetFlag),
        "valueinput.prompt");
    iInputWidget *input = addChildFlags_Widget(dlg, iClob(new_InputWidget(0)),
                                               resizeToParentWidth_WidgetFlag);
    setContentPadding_InputWidget(input, 0.5f * gap_UI, 0.5f * gap_UI);
    if (deviceType_App() == phone_AppDeviceType) {
        setFont_InputWidget(input, defaultBig_FontId);
        setBackgroundColor_Widget(dlg, uiBackgroundSidebar_ColorId);
        setContentPadding_InputWidget(input, gap_UI, gap_UI);
    }
    if (initialValue) {
        setText_InputWidget(input, initialValue);
    }
    setId_Widget(as_Widget(input), "input");
    updateValueInputWidth_(dlg);
    addChild_Widget(dlg, iClob(makePadding_Widget(gap_UI)));
    addChild_Widget(
        dlg,
        iClob(makeDialogButtons_Widget(
            (iMenuItem[]){ { "${cancel}", 0, 0, NULL }, { acceptLabel, 0, 0, "valueinput.accept" } },
            2)));
    finalizeSheet_Mobile(dlg);
    if (parent) {
        setFocus_Widget(as_Widget(input));
    }
    return dlg;
}

void updateValueInput_Widget(iWidget *d, const char *title, const char *prompt) {
    setTextCStr_LabelWidget(findChild_Widget(d, "valueinput.title"), title);
    setTextCStr_LabelWidget(findChild_Widget(d, "valueinput.prompt"), prompt);
    updateValueInputWidth_(d);
}

static iBool messageHandler_(iWidget *msg, const char *cmd) {
    /* Almost any command dismisses the sheet. */
    /* TODO: Use a "notification" prefix (like `) to ignore all types of commands line this? */
    if (!(equal_Command(cmd, "media.updated") ||
          equal_Command(cmd, "media.player.update") ||
          equal_Command(cmd, "bookmarks.request.finished") ||
          equal_Command(cmd, "document.autoreload") ||
          equal_Command(cmd, "document.reload") ||
          equal_Command(cmd, "document.request.updated") ||
          equal_Command(cmd, "scrollbar.fade") ||
          equal_Command(cmd, "widget.overflow") ||
          startsWith_CStr(cmd, "window."))) {
        setupSheetTransition_Mobile(msg, iFalse);
        destroy_Widget(msg);
    }
    return iFalse;
}

iWidget *makeSimpleMessage_Widget(const char *title, const char *msg) {
    return makeMessage_Widget(title,
                              msg,
                              (iMenuItem[]){ { "${dlg.message.ok}", 0, 0, "message.ok" } },
                              1);
}

iWidget *makeMessage_Widget(const char *title, const char *msg, const iMenuItem *items,
                            size_t numItems) {
    iWidget *dlg = makeQuestion_Widget(title, msg, items, numItems);
    addAction_Widget(dlg, SDLK_ESCAPE, 0, "message.ok");
    addAction_Widget(dlg, SDLK_SPACE, 0, "message.ok");
    return dlg;
}

iWidget *makeQuestion_Widget(const char *title, const char *msg,
                             const iMenuItem *items, size_t numItems) {
    processEvents_App(postedEventsOnly_AppEventMode);
    iWidget *dlg = makeSheet_Widget("");
    setCommandHandler_Widget(dlg, messageHandler_);
    addChildFlags_Widget(dlg, iClob(new_LabelWidget(title, NULL)), frameless_WidgetFlag);
    addChildFlags_Widget(dlg, iClob(new_LabelWidget(msg, NULL)), frameless_WidgetFlag);
    /* Check for value selections. */
    for (size_t i = 0; i < numItems; i++) {
        const iMenuItem *item = &items[i];
        const char first = item->label[0];
        if (first == '*' || first == '&') {
            iLabelWidget *option =
                addChildFlags_Widget(dlg,
                                 iClob(newKeyMods_LabelWidget(item->label + 1,
                                                              item->key,
                                                              item->kmods,
                                                              item->command)),
                                 resizeToParentWidth_WidgetFlag |
                                 (first == '&' ? selected_WidgetFlag : 0));
            if (deviceType_App() != desktop_AppDeviceType) {
                setFont_LabelWidget(option, defaultBig_FontId);
            }
        }
    }
    addChild_Widget(dlg, iClob(makePadding_Widget(gap_UI)));
    addChild_Widget(dlg, iClob(makeDialogButtons_Widget(items, numItems)));
    addChild_Widget(dlg->root->widget, iClob(dlg));
    arrange_Widget(dlg); /* BUG: This extra arrange shouldn't be needed but the dialog won't
                            be arranged correctly unless it's here. */
    finalizeSheet_Mobile(dlg);
    return dlg;
}

void setToggle_Widget(iWidget *d, iBool active) {
    if (d) {
        setFlags_Widget(d, selected_WidgetFlag, active);
        iLabelWidget *label = (iLabelWidget *) d;
        if (!cmp_String(text_LabelWidget(label), cstr_Lang("toggle.yes")) ||
            !cmp_String(text_LabelWidget(label), cstr_Lang("toggle.no"))) {
            updateText_LabelWidget(
                (iLabelWidget *) d,
                collectNewCStr_String(isSelected_Widget(d) ? "${toggle.yes}" : "${toggle.no}"));
        }
        else {
            refresh_Widget(d);
        }
    }
}

static iBool toggleHandler_(iWidget *d, const char *cmd) {
    if (equal_Command(cmd, "toggle") && pointer_Command(cmd) == d) {
        setToggle_Widget(d, (flags_Widget(d) & selected_WidgetFlag) == 0);
        postCommand_Widget(d,
                           format_CStr("%s.changed arg:%d",
                                       cstr_String(id_Widget(d)),
                                       isSelected_Widget(d) ? 1 : 0));
        return iTrue;
    }
    else if (equal_Command(cmd, "lang.changed")) {
        /* TODO: Measure labels again. */
    }
    return iFalse;
}

iWidget *makeToggle_Widget(const char *id) {
    iWidget *toggle = as_Widget(new_LabelWidget("${toggle.yes}", "toggle")); /* "YES" for sizing */
    setId_Widget(toggle, id);
    /* TODO: Measure both labels and use the larger of the two. */
    updateTextCStr_LabelWidget((iLabelWidget *) toggle, "${toggle.no}"); /* actual initial value */
    setFlags_Widget(toggle, fixedWidth_WidgetFlag, iTrue);
    setCommandHandler_Widget(toggle, toggleHandler_);
    return toggle;
}

static void appendFramelessTabPage_(iWidget *tabs, iWidget *page, const char *title, int shortcut,
                                    int kmods) {
    appendTabPage_Widget(tabs, page, title, shortcut, kmods);
    setFlags_Widget(
        (iWidget *) back_ObjectList(children_Widget(findChild_Widget(tabs, "tabs.buttons"))),
        frameless_WidgetFlag | noBackground_WidgetFlag,
        iTrue);
}

static iWidget *makeTwoColumnWidget_(iWidget **headings, iWidget **values) {
    iWidget *page = new_Widget();
    setFlags_Widget(page, arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag, iTrue);
    *headings = addChildFlags_Widget(
        page, iClob(new_Widget()), arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag);
    *values = addChildFlags_Widget(
        page, iClob(new_Widget()), arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag);
    return page;
}

static iWidget *appendTwoColumnPage_(iWidget *tabs, const char *title, int shortcut, iWidget **headings,
                                     iWidget **values) {
    /* TODO: Use `makeTwoColumnWidget_()`, see above. */
    iWidget *page = new_Widget();
    setFlags_Widget(page, arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag, iTrue);
    addChildFlags_Widget(page, iClob(new_Widget()), expand_WidgetFlag);
    setPadding_Widget(page, 0, gap_UI, 0, gap_UI);
    iWidget *columns = new_Widget();
    addChildFlags_Widget(page, iClob(columns), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
    *headings = addChildFlags_Widget(
        columns, iClob(new_Widget()), arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag);
    *values = addChildFlags_Widget(
        columns, iClob(new_Widget()), arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag);
    addChildFlags_Widget(page, iClob(new_Widget()), expand_WidgetFlag);
    appendFramelessTabPage_(tabs, iClob(page), title, shortcut, shortcut ? KMOD_PRIMARY : 0);
    return page;
}

static void makeTwoColumnHeading_(const char *title, iWidget *headings, iWidget *values) {
    addChildFlags_Widget(headings,
                         iClob(makeHeading_Widget(format_CStr(uiHeading_ColorEscape "%s", title))),
                         ignoreForParentWidth_WidgetFlag);
    addChild_Widget(values, iClob(makeHeading_Widget("")));
}

static void expandInputFieldWidth_(iInputWidget *input) {
    if (!input) return;
    iWidget *page = as_Widget(input)->parent->parent->parent->parent; /* tabs > page > values > input */
    as_Widget(input)->rect.size.x =
        right_Rect(bounds_Widget(page)) - left_Rect(bounds_Widget(constAs_Widget(input)));
}

static void addRadioButton_(iWidget *parent, const char *id, const char *label, const char *cmd) {
    setId_Widget(
        addChildFlags_Widget(parent, iClob(new_LabelWidget(label, cmd)), radio_WidgetFlag),
        id);
}

static void addFontButtons_(iWidget *parent, const char *id) {
    const struct {
        const char *   name;
        enum iTextFont cfgId;
    } fonts[] = { { "Nunito", nunito_TextFont },
                  { "Source Sans 3", sourceSans3_TextFont },
                  { "Fira Sans", firaSans_TextFont },
                  { "---", -1 },
                  { "Literata", literata_TextFont },
                  { "Tinos", tinos_TextFont },
                  { "---", -1 },
                  { "Iosevka", iosevka_TextFont } };
    iArray *items = new_Array(sizeof(iMenuItem));
    iForIndices(i, fonts) {
        pushBack_Array(items,
                       &(iMenuItem){ fonts[i].name,
                                     0,
                                     0,
                                     fonts[i].cfgId >= 0
                                         ? format_CStr("!%s.set arg:%d", id, fonts[i].cfgId)
                                         : NULL });
    }
    iLabelWidget *button = makeMenuButton_LabelWidget("Source Sans 3", data_Array(items), size_Array(items));
    setBackgroundColor_Widget(findChild_Widget(as_Widget(button), "menu"), uiBackgroundMenu_ColorId);
    setId_Widget(as_Widget(button), format_CStr("prefs.%s", id));
    addChildFlags_Widget(parent, iClob(button), alignLeft_WidgetFlag);
    delete_Array(items);
}

#if 0
static int cmp_MenuItem_(const void *e1, const void *e2) {
    const iMenuItem *a = e1, *b = e2;
    return iCmpStr(a->label, b->label);
}
#endif

void updatePreferencesLayout_Widget(iWidget *prefs) {
    if (!prefs || deviceType_App() != desktop_AppDeviceType) {
        return;
    }
    /* Doing manual layout here because the widget arranging logic isn't sophisticated enough. */
    /* TODO: Make the arranging more sophisticated to automate this. */
    static const char *inputIds[] = {
        "prefs.searchurl",
        "prefs.downloads",
        "prefs.userfont",
        "prefs.ca.file",
        "prefs.ca.path",
        "prefs.proxy.gemini",
        "prefs.proxy.gopher",
        "prefs.proxy.http"
    };
    iWidget *tabs = findChild_Widget(prefs, "prefs.tabs");
    /* Input fields expand to the right edge. */
    /* TODO: Add an arrangement flag for this. */
    iForIndices(i, inputIds) {
        iInputWidget *input = findChild_Widget(tabs, inputIds[i]);
        if (input) {
            as_Widget(input)->rect.size.x = 0;
        }
    }
    iWidget *bindings = findChild_Widget(prefs, "bindings");
    if (bindings) {
        bindings->rect.size.x = 0;
    }
    resizeToLargestPage_Widget(tabs);
    arrange_Widget(prefs);
    iForIndices(i, inputIds) {
        expandInputFieldWidth_(findChild_Widget(tabs, inputIds[i]));
    }
}

static void addDialogInputWithHeadingAndFlags_(iWidget *headings, iWidget *values, const char *labelText,
                                               const char *inputId, iInputWidget *input, int64_t flags) {
    iLabelWidget *head = addChild_Widget(headings, iClob(makeHeading_Widget(labelText)));
#if defined (iPlatformMobile)
    /* On mobile, inputs have 2 gaps of extra padding. */
    setFixedSize_Widget(as_Widget(head), init_I2(-1, height_Widget(input)));
    setPadding_Widget(as_Widget(head), 0, gap_UI, 0, 0);
#endif
    setId_Widget(addChild_Widget(values, input), inputId);
    if (deviceType_App() != phone_AppDeviceType) {
        /* Ensure that the label has the same height as the input widget. */
        as_Widget(head)->sizeRef = as_Widget(input);
    }
    setFlags_Widget(as_Widget(head), flags, iTrue);
    setFlags_Widget(as_Widget(input), flags, iTrue);
}

static void addDialogInputWithHeading_(iWidget *headings, iWidget *values, const char *labelText,
                                       const char *inputId, iInputWidget *input) {
    addDialogInputWithHeadingAndFlags_(headings, values, labelText, inputId, input, 0);
}

iInputWidget *addTwoColumnDialogInputField_Widget(iWidget *headings, iWidget *values,
                                                  const char *labelText, const char *inputId,
                                                  iInputWidget *input) {
    addDialogInputWithHeading_(headings, values, labelText, inputId, input);
    return input;
}

static void addPrefsInputWithHeading_(iWidget *headings, iWidget *values,
                                      const char *id, iInputWidget *input) {
    addDialogInputWithHeading_(headings, values, format_CStr("${%s}", id), id, input);
}

iWidget *makePreferences_Widget(void) {
    iWidget *dlg = makeSheet_Widget("prefs");
    addChildFlags_Widget(dlg,
                         iClob(new_LabelWidget(uiHeading_ColorEscape "${heading.prefs}", NULL)),
                         frameless_WidgetFlag);
    iWidget *tabs = makeTabs_Widget(dlg);
    setBackgroundColor_Widget(findChild_Widget(tabs, "tabs.buttons"), uiBackgroundSidebar_ColorId);
    setId_Widget(tabs, "prefs.tabs");
    iWidget *headings, *values;
    const int bigGap = lineHeight_Text(uiLabel_FontId) * 3 / 4;
    /* General preferences. */ {
        appendTwoColumnPage_(tabs, "${heading.prefs.general}", '1', &headings, &values);
#if defined (LAGRANGE_ENABLE_DOWNLOAD_EDIT)
        //addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.downloads}")));
        //setId_Widget(addChild_Widget(values, iClob(new_InputWidget(0))), "prefs.downloads");
        addPrefsInputWithHeading_(headings, values, "prefs.downloads", iClob(new_InputWidget(0)));
#endif
        iInputWidget *searchUrl;
        addPrefsInputWithHeading_(headings, values, "prefs.searchurl", iClob(searchUrl = new_InputWidget(0)));
        setUrlContent_InputWidget(searchUrl, iTrue);
        addChild_Widget(headings, iClob(makePadding_Widget(bigGap)));
        addChild_Widget(values, iClob(makePadding_Widget(bigGap)));
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.collapsepreonload}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.collapsepreonload")));
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.plaintext.wrap}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.plaintext.wrap")));
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.centershort}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.centershort")));
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.hoverlink}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.hoverlink")));
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.archive.openindex}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.archive.openindex")));
        if (deviceType_App() != phone_AppDeviceType) {
            addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.pinsplit}")));
            iWidget *pinSplit = new_Widget();
            /* Split mode document pinning. */ {
                addRadioButton_(pinSplit, "prefs.pinsplit.0", "${prefs.pinsplit.none}", "pinsplit.set arg:0");
                addRadioButton_(pinSplit, "prefs.pinsplit.1", "${prefs.pinsplit.left}", "pinsplit.set arg:1");
                addRadioButton_(pinSplit, "prefs.pinsplit.2", "${prefs.pinsplit.right}", "pinsplit.set arg:2");
            }
            addChildFlags_Widget(values, iClob(pinSplit), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
        }
        addChild_Widget(headings, iClob(makePadding_Widget(bigGap)));
        addChild_Widget(values, iClob(makePadding_Widget(bigGap)));
        /* UI languages. */ {
            iArray *uiLangs = collectNew_Array(sizeof(iMenuItem));
            const iMenuItem langItems[] = {
                { "${lang.de} - de", 0, 0, "uilang id:de" },
                { "${lang.en} - en", 0, 0, "uilang id:en" },
                { "${lang.es} - es", 0, 0, "uilang id:es" },
                { "${lang.fi} - fi", 0, 0, "uilang id:fi" },
                { "${lang.fr} - fr", 0, 0, "uilang id:fr" },
                { "${lang.ia} - ia", 0, 0, "uilang id:ia" },
                { "${lang.ie} - ie", 0, 0, "uilang id:ie" },
                { "${lang.pl} - pl", 0, 0, "uilang id:pl" },
                { "${lang.ru} - ru", 0, 0, "uilang id:ru" },
                { "${lang.sr} - sr", 0, 0, "uilang id:sr" },
                { "${lang.tok} - tok", 0, 0, "uilang id:tok" },
                { "${lang.zh.hans} - zh", 0, 0, "uilang id:zh_Hans" },
                { "${lang.zh.hant} - zh", 0, 0, "uilang id:zh_Hant" },
            };
            pushBackN_Array(uiLangs, langItems, iElemCount(langItems));
            //sort_Array(uiLangs, cmp_MenuItem_);
            /* TODO: Add an arrange flag for resizing parent to widest child. */
            int widest = 0;
            size_t widestPos = iInvalidPos;
            iConstForEach(Array, i, uiLangs) {
                const int width =
                    advance_Text(uiLabel_FontId,
                                 translateCStr_Lang(((const iMenuItem *) i.value)->label))
                        .x;
                if (widestPos == iInvalidPos || width > widest) {
                    widest = width;
                    widestPos = index_ArrayConstIterator(&i);
                }
            }
            addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.uilang}")));
            setId_Widget(addChildFlags_Widget(values,
                                              iClob(makeMenuButton_LabelWidget(
                                                  value_Array(uiLangs, widestPos, iMenuItem).label,
                                                  data_Array(uiLangs),
                                                  size_Array(uiLangs))),
                                              alignLeft_WidgetFlag),
                         "prefs.uilang");
        }
    }
    /* User Interface. */ {
        appendTwoColumnPage_(tabs, "${heading.prefs.interface}", '2', &headings, &values);
#if defined (iPlatformApple) || defined (iPlatformMSys)
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.ostheme}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.ostheme")));
#endif
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.theme}")));
        iWidget *themes = new_Widget();
        /* Themes. */ {
            setId_Widget(addChild_Widget(themes, iClob(new_LabelWidget("${prefs.theme.black}", "theme.set arg:0"))), "prefs.theme.0");
            setId_Widget(addChild_Widget(themes, iClob(new_LabelWidget("${prefs.theme.dark}", "theme.set arg:1"))), "prefs.theme.1");
            setId_Widget(addChild_Widget(themes, iClob(new_LabelWidget("${prefs.theme.light}", "theme.set arg:2"))), "prefs.theme.2");
            setId_Widget(addChild_Widget(themes, iClob(new_LabelWidget("${prefs.theme.white}", "theme.set arg:3"))), "prefs.theme.3");
        }
        addChildFlags_Widget(values, iClob(themes), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
        /* Accents. */
        iWidget *accent = new_Widget(); {
            setId_Widget(addChild_Widget(accent, iClob(new_LabelWidget("${prefs.accent.teal}", "accent.set arg:0"))), "prefs.accent.0");
            setId_Widget(addChild_Widget(accent, iClob(new_LabelWidget("${prefs.accent.orange}", "accent.set arg:1"))), "prefs.accent.1");
        }
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.accent}")));
        addChildFlags_Widget(values, iClob(accent), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
#if defined (LAGRANGE_ENABLE_CUSTOM_FRAME)
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.customframe}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.customframe")));
#endif
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.animate}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.animate")));
        makeTwoColumnHeading_("${heading.prefs.scrolling}", headings, values);
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.smoothscroll}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.smoothscroll")));
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.imageloadscroll}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.imageloadscroll")));
        if (deviceType_App() == phone_AppDeviceType) {
            addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.hidetoolbarscroll}")));
            addChild_Widget(values, iClob(makeToggle_Widget("prefs.hidetoolbarscroll")));
        }
        makeTwoColumnHeading_("${heading.prefs.sizing}", headings, values);
        addPrefsInputWithHeading_(headings, values, "prefs.uiscale", iClob(new_InputWidget(8)));
        if (deviceType_App() == desktop_AppDeviceType) {
            addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.retainwindow}")));
            addChild_Widget(values, iClob(makeToggle_Widget("prefs.retainwindow")));
        }
    }
    /* Colors. */ {
        appendTwoColumnPage_(tabs, "${heading.prefs.colors}", '3', &headings, &values);
        makeTwoColumnHeading_("${heading.prefs.pagecontent}", headings, values);
        for (int i = 0; i < 2; ++i) {
            const iBool isDark = (i == 0);
            const char *mode = isDark ? "dark" : "light";
            const iMenuItem themes[] = {
                { "${prefs.doctheme.name.colorfuldark}", 0, 0, format_CStr("doctheme.%s.set arg:%d", mode, colorfulDark_GmDocumentTheme) },
                { "${prefs.doctheme.name.colorfullight}", 0, 0, format_CStr("doctheme.%s.set arg:%d", mode, colorfulLight_GmDocumentTheme) },
                { "${prefs.doctheme.name.black}", 0, 0, format_CStr("doctheme.%s.set arg:%d", mode, black_GmDocumentTheme) },
                { "${prefs.doctheme.name.gray}", 0, 0, format_CStr("doctheme.%s.set arg:%d", mode, gray_GmDocumentTheme) },
                { "${prefs.doctheme.name.white}", 0, 0, format_CStr("doctheme.%s.set arg:%d", mode, white_GmDocumentTheme) },
                { "${prefs.doctheme.name.sepia}", 0, 0, format_CStr("doctheme.%s.set arg:%d", mode, sepia_GmDocumentTheme) },
                { "${prefs.doctheme.name.highcontrast}", 0, 0, format_CStr("doctheme.%s.set arg:%d", mode, highContrast_GmDocumentTheme) },
            };
            addChild_Widget(headings, iClob(makeHeading_Widget(isDark ? "${prefs.doctheme.dark}" : "${prefs.doctheme.light}")));
            iLabelWidget *button =
                makeMenuButton_LabelWidget(themes[1].label, themes, iElemCount(themes));
//            setFrameColor_Widget(findChild_Widget(as_Widget(button), "menu"),
//                                 uiBackgroundSelected_ColorId);
            setBackgroundColor_Widget(findChild_Widget(as_Widget(button), "menu"), uiBackgroundMenu_ColorId);
            setId_Widget(addChildFlags_Widget(values, iClob(button), alignLeft_WidgetFlag),
                         format_CStr("prefs.doctheme.%s", mode));
        }
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.saturation}")));
        iWidget *sats = new_Widget();
        /* Saturation levels. */ {
            addRadioButton_(sats, "prefs.saturation.3", "100 %", "saturation.set arg:100");
            addRadioButton_(sats, "prefs.saturation.2", "66 %", "saturation.set arg:66");
            addRadioButton_(sats, "prefs.saturation.1", "33 %", "saturation.set arg:33");
            addRadioButton_(sats, "prefs.saturation.0", "0 %", "saturation.set arg:0");
        }
        addChildFlags_Widget(values, iClob(sats), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
    }
    /* Layout. */ {
        setId_Widget(appendTwoColumnPage_(tabs, "${heading.prefs.style}", '4', &headings, &values), "prefs.page.style");
        makeTwoColumnHeading_("${heading.prefs.fonts}", headings, values);
        /* Fonts. */ {
            addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.headingfont}")));
            addFontButtons_(values, "headingfont");
            addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.font}")));
            addFontButtons_(values, "font");
            addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.mono}")));
            iWidget *mono = new_Widget(); {
                iWidget *tog;
                setTextCStr_LabelWidget(
                    addChild_Widget(mono, tog = iClob(makeToggle_Widget("prefs.mono.gemini"))),
                    "${prefs.mono.gemini}");
                setFlags_Widget(tog, fixedWidth_WidgetFlag, iFalse);
                updateSize_LabelWidget((iLabelWidget *) tog);
                setTextCStr_LabelWidget(
                    addChild_Widget(mono, tog = iClob(makeToggle_Widget("prefs.mono.gopher"))),
                    "${prefs.mono.gopher}");
                setFlags_Widget(tog, fixedWidth_WidgetFlag, iFalse);
                updateSize_LabelWidget((iLabelWidget *) tog);
            }
            addChildFlags_Widget(values, iClob(mono), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
            addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.boldlink}")));
            iWidget *boldLink = new_Widget(); {
                /* TODO: Add a utility function for this type of toggles? (also for above) */
                iWidget *tog;
                setTextCStr_LabelWidget(
                    addChild_Widget(boldLink, tog = iClob(makeToggle_Widget("prefs.boldlink.dark"))),
                    "${prefs.boldlink.dark}");
                setFlags_Widget(tog, fixedWidth_WidgetFlag, iFalse);
                updateSize_LabelWidget((iLabelWidget *) tog);
                setTextCStr_LabelWidget(
                    addChild_Widget(boldLink, tog = iClob(makeToggle_Widget("prefs.boldlink.light"))),
                    "${prefs.boldlink.light}");
                setFlags_Widget(tog, fixedWidth_WidgetFlag, iFalse);
                updateSize_LabelWidget((iLabelWidget *) tog);
            }
            addChildFlags_Widget(values, iClob(boldLink), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
            addPrefsInputWithHeading_(headings, values, "prefs.userfont", iClob(new_InputWidget(0)));
        }
        makeTwoColumnHeading_("${heading.prefs.paragraph}", headings, values);
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.linewidth}")));
        iWidget *widths = new_Widget();
        /* Line widths. */ {
            addRadioButton_(widths, "prefs.linewidth.30", "\u20132", "linewidth.set arg:30");
            addRadioButton_(widths, "prefs.linewidth.34", "\u20131", "linewidth.set arg:34");
            addRadioButton_(widths, "prefs.linewidth.38", "${prefs.linewidth.normal}", "linewidth.set arg:38");
            addRadioButton_(widths, "prefs.linewidth.43", "+1", "linewidth.set arg:43");
            addRadioButton_(widths, "prefs.linewidth.48", "+2", "linewidth.set arg:48");
            addRadioButton_(widths, "prefs.linewidth.1000", "${prefs.linewidth.fill}", "linewidth.set arg:1000");
        }
        addChildFlags_Widget(values, iClob(widths), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.quoteicon}")));
        iWidget *quote = new_Widget(); {
            addRadioButton_(quote, "prefs.quoteicon.1", "${prefs.quoteicon.icon}", "quoteicon.set arg:1");
            addRadioButton_(quote, "prefs.quoteicon.0", "${prefs.quoteicon.line}", "quoteicon.set arg:0");
        }
        addChildFlags_Widget(values, iClob(quote), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.biglede}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.biglede")));
//        makeTwoColumnHeading_("${heading.prefs.widelayout}", headings, values);
        addChild_Widget(headings, iClob(makePadding_Widget(bigGap)));
        addChild_Widget(values, iClob(makePadding_Widget(bigGap)));
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.sideicon}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.sideicon")));
    }
    /* Network. */ {
        appendTwoColumnPage_(tabs, "${heading.prefs.network}", '5', &headings, &values);
        addChild_Widget(headings, iClob(makeHeading_Widget("${prefs.decodeurls}")));
        addChild_Widget(values, iClob(makeToggle_Widget("prefs.decodeurls")));
        /* Cache size. */ {
            iInputWidget *cache = new_InputWidget(4);
            setSelectAllOnFocus_InputWidget(cache, iTrue);
            addPrefsInputWithHeading_(headings, values, "prefs.cachesize", iClob(cache));
            iWidget *unit =
                addChildFlags_Widget(as_Widget(cache),
                                     iClob(new_LabelWidget("${mb}", NULL)),
                                     frameless_WidgetFlag | moveToParentRightEdge_WidgetFlag |
                                     resizeToParentHeight_WidgetFlag);
            setContentPadding_InputWidget(cache, 0, width_Widget(unit) - 4 * gap_UI);
        }
        makeTwoColumnHeading_("${heading.prefs.certs}", headings, values);
        addPrefsInputWithHeading_(headings, values, "prefs.ca.file", iClob(new_InputWidget(0)));
        addPrefsInputWithHeading_(headings, values, "prefs.ca.path", iClob(new_InputWidget(0)));
        makeTwoColumnHeading_("${heading.prefs.proxies}", headings, values);
        addPrefsInputWithHeading_(headings, values, "prefs.proxy.gemini", iClob(new_InputWidget(0)));
        addPrefsInputWithHeading_(headings, values, "prefs.proxy.gopher", iClob(new_InputWidget(0)));
        addPrefsInputWithHeading_(headings, values, "prefs.proxy.http", iClob(new_InputWidget(0)));
    }
    /* Keybindings. */
    if (deviceType_App() == desktop_AppDeviceType) {
        iBindingsWidget *bind = new_BindingsWidget();
        appendFramelessTabPage_(tabs, iClob(bind), "${heading.prefs.keys}", '6', KMOD_PRIMARY);
    }
    addChild_Widget(dlg, iClob(makePadding_Widget(gap_UI)));
    updatePreferencesLayout_Widget(dlg);
    addChild_Widget(dlg,
                    iClob(makeDialogButtons_Widget(
                        (iMenuItem[]){ { "${close}", SDLK_ESCAPE, 0, "prefs.dismiss" } }, 1)));
    addChild_Widget(dlg->root->widget, iClob(dlg));
    finalizeSheet_Mobile(dlg);
    setupSheetTransition_Mobile(dlg, iTrue);
//    printTree_Widget(dlg);
    return dlg;
}

iWidget *makeBookmarkEditor_Widget(void) {
    iWidget *dlg = makeSheet_Widget("bmed");
    setId_Widget(addChildFlags_Widget(
                     dlg,
                     iClob(new_LabelWidget(uiHeading_ColorEscape "${heading.bookmark.edit}", NULL)),
                     frameless_WidgetFlag),
                 "bmed.heading");
    iWidget *headings, *values;
    addChild_Widget(dlg, iClob(makeTwoColumnWidget_(&headings, &values)));
    iInputWidget *inputs[4];
    addDialogInputWithHeading_(headings, values, "${dlg.bookmark.title}", "bmed.title", iClob(inputs[0] = new_InputWidget(0)));
    addDialogInputWithHeading_(headings, values, "${dlg.bookmark.url}",   "bmed.url",   iClob(inputs[1] = new_InputWidget(0)));
    setUrlContent_InputWidget(inputs[1], iTrue);
    addDialogInputWithHeading_(headings, values, "${dlg.bookmark.tags}",  "bmed.tags",  iClob(inputs[2] = new_InputWidget(0)));
    addDialogInputWithHeading_(headings, values, "${dlg.bookmark.icon}",  "bmed.icon",  iClob(inputs[3] = new_InputWidget(1)));
    /* Buttons for special tags. */
    addChild_Widget(dlg, iClob(makePadding_Widget(gap_UI)));
    addChild_Widget(dlg, iClob(makeTwoColumnWidget_(&headings, &values)));
    makeTwoColumnHeading_("SPECIAL TAGS", headings, values);
    addChild_Widget(headings, iClob(makeHeading_Widget("${bookmark.tag.home}")));
    addChild_Widget(values, iClob(makeToggle_Widget("bmed.tag.home")));
    addChild_Widget(headings, iClob(makeHeading_Widget("${bookmark.tag.remote}")));
    addChild_Widget(values, iClob(makeToggle_Widget("bmed.tag.remote")));
    addChild_Widget(headings, iClob(makeHeading_Widget("${bookmark.tag.linksplit}")));
    addChild_Widget(values, iClob(makeToggle_Widget("bmed.tag.linksplit")));
    arrange_Widget(dlg);
    for (int i = 0; i < 3; ++i) {
        as_Widget(inputs[i])->rect.size.x = 100 * gap_UI - headings->rect.size.x;
    }
    addChild_Widget(dlg, iClob(makePadding_Widget(gap_UI)));
    addChild_Widget(
        dlg,
        iClob(makeDialogButtons_Widget((iMenuItem[]){ { "${cancel}", 0, 0, NULL },
                                                { uiTextCaution_ColorEscape "${dlg.bookmark.save}",
                                                  SDLK_RETURN,
                                                  KMOD_PRIMARY,
                                                  "bmed.accept" } },
                                 2)));
    addChild_Widget(get_Root()->widget, iClob(dlg));
    finalizeSheet_Mobile(dlg);
    return dlg;
}

static iBool handleBookmarkCreationCommands_SidebarWidget_(iWidget *editor, const char *cmd) {
    if (equal_Command(cmd, "bmed.accept") || equal_Command(cmd, "cancel")) {
        if (equal_Command(cmd, "bmed.accept")) {
            const iString *title = text_InputWidget(findChild_Widget(editor, "bmed.title"));
            const iString *url   = text_InputWidget(findChild_Widget(editor, "bmed.url"));
            const iString *tags  = text_InputWidget(findChild_Widget(editor, "bmed.tags"));
            const iString *icon  = collect_String(trimmed_String(text_InputWidget(findChild_Widget(editor, "bmed.icon"))));
            const uint32_t id    = add_Bookmarks(bookmarks_App(), url, title, tags, first_String(icon));
            iBookmark *    bm    = get_Bookmarks(bookmarks_App(), id);
            if (!isEmpty_String(icon)) {
                addTagIfMissing_Bookmark(bm, userIcon_BookmarkTag);
            }
            if (isSelected_Widget(findChild_Widget(editor, "bmed.tag.home"))) {
                addTag_Bookmark(bm, homepage_BookmarkTag);
            }
            if (isSelected_Widget(findChild_Widget(editor, "bmed.tag.remote"))) {
                addTag_Bookmark(bm, remoteSource_BookmarkTag);
            }
            if (isSelected_Widget(findChild_Widget(editor, "bmed.tag.linksplit"))) {
                addTag_Bookmark(bm, linkSplit_BookmarkTag);
            }
            postCommand_App("bookmarks.changed");
        }
        setupSheetTransition_Mobile(editor, iFalse);
        destroy_Widget(editor);
        return iTrue;
    }
    return iFalse;
}

iWidget *makeBookmarkCreation_Widget(const iString *url, const iString *title, iChar icon) {
    iWidget *dlg = makeBookmarkEditor_Widget();
    setId_Widget(dlg, "bmed.create");
    setTextCStr_LabelWidget(findChild_Widget(dlg, "bmed.heading"),
                            uiHeading_ColorEscape "${heading.bookmark.add}");
    iUrl parts;
    init_Url(&parts, url);
    setTextCStr_InputWidget(findChild_Widget(dlg, "bmed.title"),
                            title ? cstr_String(title) : cstr_Rangecc(parts.host));
    setText_InputWidget(findChild_Widget(dlg, "bmed.url"), url);
    setId_Widget(
        addChildFlags_Widget(
            dlg,
            iClob(new_LabelWidget(cstrCollect_String(newUnicodeN_String(&icon, 1)), NULL)),
            collapse_WidgetFlag | hidden_WidgetFlag | disabled_WidgetFlag),
        "bmed.icon");
    setCommandHandler_Widget(dlg, handleBookmarkCreationCommands_SidebarWidget_);
    return dlg;
}


static iBool handleFeedSettingCommands_(iWidget *dlg, const char *cmd) {
    if (equal_Command(cmd, "cancel")) {
        setupSheetTransition_Mobile(dlg, iFalse);
        destroy_Widget(dlg);
        return iTrue;
    }
    if (equal_Command(cmd, "feedcfg.accept")) {
        iString *feedTitle =
            collect_String(copy_String(text_InputWidget(findChild_Widget(dlg, "feedcfg.title"))));
        trim_String(feedTitle);
        if (isEmpty_String(feedTitle)) {
            return iTrue;
        }
        int id = argLabel_Command(cmd, "bmid");
        const iBool headings = isSelected_Widget(findChild_Widget(dlg, "feedcfg.type.headings"));
        const iString *tags = collectNewFormat_String("subscribed%s", headings ? " headings" : "");
        if (!id) {
            const size_t numSubs = numSubscribed_Feeds();
            const iString *url   = url_DocumentWidget(document_App());
            add_Bookmarks(bookmarks_App(),
                          url,
                          feedTitle,
                          tags,
                          siteIcon_GmDocument(document_DocumentWidget(document_App())));
            if (numSubs == 0) {
                /* Auto-refresh after first addition. */
                postCommand_App("feeds.refresh");
            }
        }
        else {
            iBookmark *bm = get_Bookmarks(bookmarks_App(), id);
            if (bm) {
                set_String(&bm->title, feedTitle);
                set_String(&bm->tags, tags);
            }
        }
        postCommand_App("bookmarks.changed");
        setupSheetTransition_Mobile(dlg, iFalse);
        destroy_Widget(dlg);
        return iTrue;
    }
    return iFalse;
}

iWidget *makeFeedSettings_Widget(uint32_t bookmarkId) {
    iWidget *dlg = makeSheet_Widget("feedcfg");
    setId_Widget(addChildFlags_Widget(
                     dlg,
                     iClob(new_LabelWidget(bookmarkId ? uiHeading_ColorEscape "${heading.feedcfg}"
                                                      : uiHeading_ColorEscape "${heading.subscribe}",
                                           NULL)),
                     frameless_WidgetFlag),
                 "feedcfg.heading");
    iWidget *headings, *values;
    addChild_Widget(dlg, iClob(makeTwoColumnWidget_(&headings, &values)));
    iInputWidget *input = new_InputWidget(0);
    addDialogInputWithHeading_(headings, values, "${dlg.feed.title}", "feedcfg.title", iClob(input));
    addChild_Widget(headings, iClob(makeHeading_Widget("${dlg.feed.entrytype}")));
    iWidget *types = new_Widget(); {
        addRadioButton_(types, "feedcfg.type.gemini", "${dlg.feed.type.gemini}", "feedcfg.type arg:0");
        addRadioButton_(types, "feedcfg.type.headings", "${dlg.feed.type.headings}", "feedcfg.type arg:1");
    }
    addChildFlags_Widget(values, iClob(types), arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag);
    iWidget *buttons =
        addChild_Widget(dlg,
                        iClob(makeDialogButtons_Widget(
                            (iMenuItem[]){ { "${cancel}", 0, 0, NULL },
                                           { bookmarkId ? uiTextCaution_ColorEscape "${dlg.feed.save}"
                                                        : uiTextCaution_ColorEscape "${dlg.feed.sub}",
                                             SDLK_RETURN,
                                             KMOD_PRIMARY,
                                             format_CStr("feedcfg.accept bmid:%d", bookmarkId) } },
                            2)));
    setId_Widget(child_Widget(buttons, childCount_Widget(buttons) - 1), "feedcfg.save");
    arrange_Widget(dlg);
    as_Widget(input)->rect.size.x = 100 * gap_UI - headings->rect.size.x;
    addChild_Widget(get_Root()->widget, iClob(dlg));
    finalizeSheet_Mobile(dlg);
    /* Initialize. */ {
        const iBookmark *bm  = bookmarkId ? get_Bookmarks(bookmarks_App(), bookmarkId) : NULL;
        setText_InputWidget(findChild_Widget(dlg, "feedcfg.title"),
                            bm ? &bm->title : feedTitle_DocumentWidget(document_App()));
        setFlags_Widget(findChild_Widget(dlg,
                                         hasTag_Bookmark(bm, headings_BookmarkTag) ? "feedcfg.type.headings"
                                                                         : "feedcfg.type.gemini"),
                        selected_WidgetFlag,
                        iTrue);
        setCommandHandler_Widget(dlg, handleFeedSettingCommands_);
    }
    return dlg;
}

iWidget *makeIdentityCreation_Widget(void) {
    iWidget *dlg = makeSheet_Widget("ident");
    setId_Widget(addChildFlags_Widget(
                     dlg,
                     iClob(new_LabelWidget(uiHeading_ColorEscape "${heading.newident}", NULL)),
                     frameless_WidgetFlag),
                 "ident.heading");
    iWidget *page = new_Widget();
    addChildFlags_Widget(
        dlg, iClob(new_LabelWidget("${dlg.newident.rsa.selfsign}", NULL)), frameless_WidgetFlag);
    /* TODO: Use makeTwoColumnWidget_? */
    addChild_Widget(dlg, iClob(page));
    setFlags_Widget(page, arrangeHorizontal_WidgetFlag | arrangeSize_WidgetFlag, iTrue);
    iWidget *headings = addChildFlags_Widget(
        page, iClob(new_Widget()), arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag);
    iWidget *values = addChildFlags_Widget(
        page, iClob(new_Widget()), arrangeVertical_WidgetFlag | arrangeSize_WidgetFlag);
    setId_Widget(headings, "headings");
    setId_Widget(values, "values");
    iInputWidget *inputs[6];
    /* Where will the new identity be active on? */ {
        addChild_Widget(headings, iClob(makeHeading_Widget("${dlg.newident.scope}")));
        const iMenuItem items[] = {
            { "${dlg.newident.scope.domain}", 0, 0, "ident.scope arg:0" },
            { "${dlg.newident.scope.page}",   0, 0, "ident.scope arg:1" },
            { "${dlg.newident.scope.none}",   0, 0, "ident.scope arg:2" },
        };
        setId_Widget(addChild_Widget(values,
                                     iClob(makeMenuButton_LabelWidget(
                                         items[0].label, items, iElemCount(items)))),
                     "ident.scope");
    }
    addDialogInputWithHeading_(headings,
                               values,
                               "${dlg.newident.until}",
                               "ident.until",
                               iClob(newHint_InputWidget(19, "${hint.newident.date}")));
    addDialogInputWithHeading_(headings,
                               values,
                               "${dlg.newident.commonname}",
                               "ident.common",
                               iClob(inputs[0] = new_InputWidget(0)));
    /* Temporary? */ {
        addChild_Widget(headings, iClob(makeHeading_Widget("${dlg.newident.temp}")));
        iWidget *tmpGroup = new_Widget();
        setFlags_Widget(tmpGroup, arrangeSize_WidgetFlag | arrangeHorizontal_WidgetFlag, iTrue);
        addChild_Widget(tmpGroup, iClob(makeToggle_Widget("ident.temp")));
        setId_Widget(
            addChildFlags_Widget(tmpGroup,
                                 iClob(new_LabelWidget(uiTextCaution_ColorEscape warning_Icon
                                                       "  ${dlg.newident.notsaved}",
                                                       NULL)),
                                 hidden_WidgetFlag | frameless_WidgetFlag),
            "ident.temp.note");
        addChild_Widget(values, iClob(tmpGroup));
    }
    addChildFlags_Widget(headings, iClob(makePadding_Widget(gap_UI)), collapse_WidgetFlag | hidden_WidgetFlag);
    addChildFlags_Widget(values, iClob(makePadding_Widget(gap_UI)), collapse_WidgetFlag | hidden_WidgetFlag);
    addDialogInputWithHeadingAndFlags_(headings, values, "${dlg.newident.email}",   "ident.email",   iClob(inputs[1] = newHint_InputWidget(0, "${hint.newident.optional}")), collapse_WidgetFlag | hidden_WidgetFlag);
    addDialogInputWithHeadingAndFlags_(headings, values, "${dlg.newident.userid}",  "ident.userid",  iClob(inputs[2] = newHint_InputWidget(0, "${hint.newident.optional}")), collapse_WidgetFlag | hidden_WidgetFlag);
    addDialogInputWithHeadingAndFlags_(headings, values, "${dlg.newident.domain}",  "ident.domain",  iClob(inputs[3] = newHint_InputWidget(0, "${hint.newident.optional}")), collapse_WidgetFlag | hidden_WidgetFlag);
    addDialogInputWithHeadingAndFlags_(headings, values, "${dlg.newident.org}",     "ident.org",     iClob(inputs[4] = newHint_InputWidget(0, "${hint.newident.optional}")), collapse_WidgetFlag | hidden_WidgetFlag);
    addDialogInputWithHeadingAndFlags_(headings, values, "${dlg.newident.country}", "ident.country", iClob(inputs[5] = newHint_InputWidget(0, "${hint.newident.optional}")), collapse_WidgetFlag | hidden_WidgetFlag);
    arrange_Widget(dlg);
    for (size_t i = 0; i < iElemCount(inputs); ++i) {
        as_Widget(inputs[i])->rect.size.x = 100 * gap_UI - headings->rect.size.x;
    }
    addChild_Widget(dlg,
                    iClob(makeDialogButtons_Widget(
                        (iMenuItem[]){ { "${dlg.newident.more}", 0, 0, "ident.showmore" },
                                       { "---", 0, 0, NULL },
                                       { "${cancel}", SDLK_ESCAPE, 0, "ident.cancel" },
                                       { uiTextAction_ColorEscape "${dlg.newident.create}",
                                         SDLK_RETURN,
                                         KMOD_PRIMARY,
                                         "ident.accept" } },
                        4)));
    addChild_Widget(get_Root()->widget, iClob(dlg));
    finalizeSheet_Mobile(dlg);
    return dlg;
}

static const iMenuItem languages[] = {
    { "${lang.ar}", 0, 0, "xlt.lang id:ar" },
    { "${lang.zh}", 0, 0, "xlt.lang id:zh" },
    { "${lang.en}", 0, 0, "xlt.lang id:en" },
    { "${lang.fr}", 0, 0, "xlt.lang id:fr" },
    { "${lang.de}", 0, 0, "xlt.lang id:de" },
    { "${lang.hi}", 0, 0, "xlt.lang id:hi" },
    { "${lang.it}", 0, 0, "xlt.lang id:it" },
    { "${lang.ja}", 0, 0, "xlt.lang id:ja" },
    { "${lang.pt}", 0, 0, "xlt.lang id:pt" },
    { "${lang.ru}", 0, 0, "xlt.lang id:ru" },
    { "${lang.es}", 0, 0, "xlt.lang id:es" },
};

static iBool translationHandler_(iWidget *dlg, const char *cmd) {
    iUnused(dlg);
    if (equal_Command(cmd, "xlt.lang")) {
        iLabelWidget *menuItem = pointer_Command(cmd);
        iWidget *button = parent_Widget(parent_Widget(menuItem));
        iAssert(isInstance_Object(button, &Class_LabelWidget));
        updateText_LabelWidget((iLabelWidget *) button, text_LabelWidget(menuItem));
        return iTrue;
    }
    return iFalse;
}

const char *languageId_String(const iString *menuItemLabel) {
    iForIndices(i, languages) {
        if (!cmp_String(menuItemLabel, translateCStr_Lang(languages[i].label))) {
            return cstr_Rangecc(range_Command(languages[i].command, "id"));
        }
    }
    return "";
}

int languageIndex_CStr(const char *langId) {
    iForIndices(i, languages) {
        if (equal_Rangecc(range_Command(languages[i].command, "id"), langId)) {
            return (int) i;
        }
    }
    return -1;
}

iWidget *makeTranslation_Widget(iWidget *parent) {
    iWidget *dlg = makeSheet_Widget("xlt");
    setFlags_Widget(dlg, keepOnTop_WidgetFlag, iFalse);
    dlg->minSize.x = 70 * gap_UI;
    setCommandHandler_Widget(dlg, translationHandler_);
    addChildFlags_Widget(dlg,
                         iClob(new_LabelWidget(uiHeading_ColorEscape "${heading.translate}", NULL)),
                         frameless_WidgetFlag);
    addChild_Widget(dlg, iClob(makePadding_Widget(lineHeight_Text(uiLabel_FontId))));
    iWidget *headings, *values;
    iWidget *page;
    addChild_Widget(dlg, iClob(page = makeTwoColumnWidget_(&headings, &values)));
    setId_Widget(page, "xlt.langs");
    iLabelWidget *fromLang, *toLang;
    /* Source language. */ {
        addChild_Widget(headings, iClob(makeHeading_Widget("${dlg.translate.from}")));
        setId_Widget(
            addChildFlags_Widget(values,
                                 iClob(fromLang = makeMenuButton_LabelWidget(
                                           "${lang.pt}", languages, iElemCount(languages))),
                                 alignLeft_WidgetFlag),
            "xlt.from");
        iWidget *langMenu = findChild_Widget(as_Widget(fromLang), "menu");
        updateText_LabelWidget(fromLang,
                               text_LabelWidget(child_Widget(langMenu, prefs_App()->langFrom)));
        setBackgroundColor_Widget(langMenu, uiBackgroundMenu_ColorId);
    }
    /* Target language. */ {
        addChild_Widget(headings, iClob(makeHeading_Widget("${dlg.translate.to}")));
        setId_Widget(addChildFlags_Widget(values,
                                          iClob(toLang = makeMenuButton_LabelWidget(
                                                    "${lang.pt}", languages, iElemCount(languages))),
                                          alignLeft_WidgetFlag),
                     "xlt.to");
        iWidget *langMenu = findChild_Widget(as_Widget(toLang), "menu");
        setBackgroundColor_Widget(langMenu, uiBackgroundMenu_ColorId);
        updateText_LabelWidget(toLang,
                               text_LabelWidget(child_Widget(langMenu, prefs_App()->langTo)));
    }
    addChild_Widget(dlg, iClob(makePadding_Widget(lineHeight_Text(uiLabel_FontId))));
    addChild_Widget(
        dlg,
        iClob(makeDialogButtons_Widget(
            (iMenuItem[]){
                { "${cancel}", SDLK_ESCAPE, 0, "translation.cancel" },
                { uiTextAction_ColorEscape "${dlg.translate}", SDLK_RETURN, 0, "translation.submit" } },
            2)));
    addChild_Widget(parent, iClob(dlg));
    arrange_Widget(dlg);
    finalizeSheet_Mobile(dlg);
    return dlg;
}
