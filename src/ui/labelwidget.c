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

#include "labelwidget.h"
#include "text.h"
#include "defs.h"
#include "color.h"
#include "paint.h"
#include "app.h"
#include "util.h"
#include "keys.h"
#include "touch.h"

struct Impl_LabelWidget {
    iWidget widget;
    iString srcLabel;
    iString label;
    int     font;
    int     key;
    int     kmods;
    iChar   icon;
    int     forceFg;
    iString command;
    iClick  click;
    struct {
        uint8_t alignVisual     : 1; /* align according to visible bounds, not font metrics */
        uint8_t noAutoMinHeight : 1; /* minimum height is not set automatically */
    } flags;
};

static iBool isHover_LabelWidget_(const iLabelWidget *d) {
#if defined (iPlatformMobile)
    if (!isHovering_Touch()) {
        return iFalse;
    }
#endif
    return isHover_Widget(d);
}

static iInt2 padding_LabelWidget_(const iLabelWidget *d, int corner) {
    const iWidget *w = constAs_Widget(d);
    const int64_t flags = flags_Widget(w);
    const iInt2 widgetPad = (corner   == 0 ? init_I2(w->padding[0], w->padding[1])
                             : corner == 1 ? init_I2(w->padding[2], w->padding[1])
                             : corner == 2 ? init_I2(w->padding[2], w->padding[3])
                             : init_I2(w->padding[0], w->padding[3]));
#if defined (iPlatformAppleMobile)
    return add_I2(widgetPad,
                  init_I2(flags & tight_WidgetFlag ? 2 * gap_UI : (4 * gap_UI),
                          (flags & extraPadding_WidgetFlag ? 1.5f : 1.0f) * 3 * gap_UI / 2));
#else
    return add_I2(widgetPad,
                  init_I2(flags & tight_WidgetFlag ? 3 * gap_UI / 2 : (3 * gap_UI),
                          gap_UI));
#endif
}

iDefineObjectConstructionArgs(LabelWidget,
                              (const char *label, const char *cmd),
                              label, cmd)

static iBool checkModifiers_(int have, int req) {
    return keyMods_Sym(req) == keyMods_Sym(have);
}

static void trigger_LabelWidget_(const iLabelWidget *d) {
    const iWidget *w = constAs_Widget(d);
    postCommand_Widget(&d->widget, "%s", cstr_String(&d->command));
    if (flags_Widget(w) & radio_WidgetFlag) {
        iForEach(ObjectList, i, children_Widget(w->parent)) {
            setFlags_Widget(i.object, selected_WidgetFlag, d == i.object);
        }
    }
}

static void updateKey_LabelWidget_(iLabelWidget *d) {
    if (!isEmpty_String(&d->command)) {
        const iBinding *bind = findCommand_Keys(cstr_String(&d->command));
        if (bind && bind->id < builtIn_BindingId) {
            d->key   = bind->key;
            d->kmods = bind->mods;
        }
    }
}

static iBool processEvent_LabelWidget_(iLabelWidget *d, const SDL_Event *ev) {
    iWidget *w = &d->widget;
    if (isMetricsChange_UserEvent(ev)) {
        updateSize_LabelWidget(d);
    }
    else if (isCommand_UserEvent(ev, "lang.changed")) {
        const iChar oldIcon = d->icon; /* icon will be retained */
        setText_LabelWidget(d, &d->srcLabel);
        checkIcon_LabelWidget(d); /* strip it */
        d->icon = oldIcon;
        return iFalse;
    }
    else if (isCommand_UserEvent(ev, "bindings.changed")) {
        /* Update the key used to trigger this label. */
        updateKey_LabelWidget_(d);
        return iFalse;
    }
    if (!isEmpty_String(&d->command)) {
#if 0 && defined (iPlatformAppleMobile)
        /* Touch allows activating any button on release. */
        switch (ev->type) {
            case SDL_MOUSEBUTTONUP: {
                const iInt2 mouse = init_I2(ev->button.x, ev->button.y);
                if (contains_Widget(w, mouse)) {
                    trigger_LabelWidget_(d);
                    refresh_Widget(w);
                }
                break;
            }
        }
#endif
        switch (processEvent_Click(&d->click, ev)) {
            case started_ClickResult:
                setFlags_Widget(w, pressed_WidgetFlag, iTrue);
                refresh_Widget(w);
                return iTrue;
            case aborted_ClickResult:
                setFlags_Widget(w, pressed_WidgetFlag, iFalse);
                refresh_Widget(w);
                return iTrue;
//            case double_ClickResult:
            case finished_ClickResult:
                setFlags_Widget(w, pressed_WidgetFlag, iFalse);
                trigger_LabelWidget_(d);
                refresh_Widget(w);
                return iTrue;
            default:
                break;
        }
        switch (ev->type) {
            case SDL_KEYDOWN: {
                const int mods = ev->key.keysym.mod;
                if (d->key && ev->key.keysym.sym == d->key && checkModifiers_(mods, d->kmods)) {
                    trigger_LabelWidget_(d);
                    return iTrue;
                }
                break;
            }
        }
    }
    return processEvent_Widget(&d->widget, ev);
}

static void keyStr_LabelWidget_(const iLabelWidget *d, iString *str) {
    toString_Sym(d->key, d->kmods, str);
}

static void getColors_LabelWidget_(const iLabelWidget *d, int *bg, int *fg, int *frame1, int *frame2) {
    const iWidget *w           = constAs_Widget(d);
    const int64_t  flags       = flags_Widget(w);
    const iBool    isPress     = (flags & pressed_WidgetFlag) != 0;
    const iBool    isSel       = (flags & selected_WidgetFlag) != 0;
    const iBool    isFrameless = (flags & frameless_WidgetFlag) != 0;
    const iBool    isButton    = d->click.button != 0;
    const iBool    isKeyRoot   = (w->root == get_Window()->keyRoot);
    /* Default color state. */
    *bg     = isButton && ~flags & noBackground_WidgetFlag ? (d->widget.bgColor != none_ColorId ?
                                                              d->widget.bgColor : uiBackground_ColorId)
                                                           : none_ColorId;
    *fg     = uiText_ColorId;
    *frame1 = isButton ? uiEmboss1_ColorId : d->widget.frameColor;
    *frame2 = isButton ? uiEmboss2_ColorId : *frame1;
    if (flags & disabled_WidgetFlag && isButton) {
        *fg = uiTextDisabled_ColorId;
    }
    if (isSel) {
        *bg = uiBackgroundSelected_ColorId;
//        if (!isKeyRoot) {
//            *bg = uiEmbossSelected1_ColorId; //uiBackgroundUnfocusedSelection_ColorId;
//        }
        if (!isKeyRoot) {
            *bg = isDark_ColorTheme(colorTheme_App()) ? uiBackgroundUnfocusedSelection_ColorId
                : uiMarked_ColorId ;
        }
        *fg = uiTextSelected_ColorId;
        if (isButton) {
            *frame1 = uiEmbossSelected1_ColorId;
            *frame2 = uiEmbossSelected2_ColorId;
            if (!isKeyRoot) {
                *frame1 = *bg;
            }
        }
    }
    int colorEscape = none_ColorId;
    if (startsWith_String(&d->label, "\r")) {
        colorEscape = cstr_String(&d->label)[1] - asciiBase_ColorEscape; /* TODO: can be two bytes long */
    }
    if (isHover_LabelWidget_(d)) {
        if (isFrameless) {
            *bg = uiBackgroundFramelessHover_ColorId;
            *fg = uiTextFramelessHover_ColorId;
        }
        else {
            /* Frames matching color escaped text. */
            if (colorEscape != none_ColorId) {
                if (isDark_ColorTheme(colorTheme_App())) {
                    *frame1 = colorEscape;
                    *frame2 = darker_Color(*frame1);
                }
                else {
                    *bg = *frame1 = *frame2 = colorEscape;
                    *fg = white_ColorId | permanent_ColorId;
                }
            }
            else if (isSel) {
                *frame1 = uiEmbossSelectedHover1_ColorId;
                *frame2 = uiEmbossSelectedHover2_ColorId;
            }
            else {
                if (isButton) *bg = uiBackgroundHover_ColorId;
                *frame1 = uiEmbossHover1_ColorId;
                *frame2 = uiEmbossHover2_ColorId;
            }
        }
    }
    if (isPress) {
        *bg = uiBackgroundPressed_ColorId | permanent_ColorId;
        if (isButton) {
            *frame1 = uiEmbossPressed1_ColorId;
            *frame2 = colorEscape != none_ColorId ? colorEscape : uiEmbossPressed2_ColorId;
        }
        if (colorEscape == none_ColorId || colorEscape == uiTextAction_ColorId) {
            *fg = uiTextPressed_ColorId | permanent_ColorId;
        }
        else {
            *fg = isDark_ColorTheme(colorTheme_App()) ? white_ColorId : black_ColorId;
        }
    }
    if (d->forceFg >= 0) {
        *fg = d->forceFg;
    }
}

iLocalDef int iconPadding_LabelWidget_(const iLabelWidget *d) {
    const float amount = flags_Widget(constAs_Widget(d)) & extraPadding_WidgetFlag ? 1.5f : 1.15f;
    return d->icon ? iRound(lineHeight_Text(d->font) * amount) : 0;
}

static void draw_LabelWidget_(const iLabelWidget *d) {
    const iWidget *w = constAs_Widget(d);
    draw_Widget(w);
    const iBool   isButton = d->click.button != 0;
    const int64_t flags    = flags_Widget(w);
    const iRect   bounds   = bounds_Widget(w);
    iRect         rect     = bounds;
    const iBool   isHover  = isHover_LabelWidget_(d);
    if (isButton) {
        shrink_Rect(&rect, divi_I2(gap2_UI, 4));
        adjustEdges_Rect(&rect, gap_UI / 8, 0, -gap_UI / 8, 0);
    }
    iPaint p;
    init_Paint(&p);
    int bg, fg, frame, frame2;
    getColors_LabelWidget_(d, &bg, &fg, &frame, &frame2);
    const iBool isCaution = startsWith_String(&d->label, uiTextCaution_ColorEscape);
    if (bg >= 0) {
        fillRect_Paint(&p, rect, isCaution && isHover ? uiMarked_ColorId : bg);
    }
    if (~flags & frameless_WidgetFlag) {
        iRect frameRect = adjusted_Rect(rect, zero_I2(), init1_I2(-1));
        if (isButton) {
            iInt2 points[] = {
                bottomLeft_Rect(frameRect), topLeft_Rect(frameRect), topRight_Rect(frameRect),
                bottomRight_Rect(frameRect), bottomLeft_Rect(frameRect)
            };
            drawLines_Paint(&p, points + 2, 3, frame2);
            drawLines_Paint(
                &p, points, !isHover && flags & noTopFrame_WidgetFlag ? 2 : 3, frame);
        }
    }
    setClip_Paint(&p, rect);
    const int iconPad = iconPadding_LabelWidget_(d);
    if (d->icon && d->icon != 0x20) { /* no need to draw an empty icon */
        iString str;
        initUnicodeN_String(&str, &d->icon, 1);
        drawCentered_Text(
            d->font,
            (iRect){
                /* The icon position is fine-tuned; c.f. high baseline of Source Sans Pro. */
                add_I2(add_I2(bounds.pos, padding_LabelWidget_(d, 0)),
                       init_I2((flags & extraPadding_WidgetFlag ? -2 : -1.20f) * gap_UI +
                               (deviceType_App() == tablet_AppDeviceType ? -gap_UI : 0),
                               -gap_UI / 8)),
                init_I2(iconPad, lineHeight_Text(d->font)) },
            iTrue,
            isCaution                                            ? uiTextCaution_ColorId
            : flags & (disabled_WidgetFlag | pressed_WidgetFlag) ? fg
            : isHover                                            ? uiIconHover_ColorId
                                                                 : uiIcon_ColorId,
            "%s",
            cstr_String(&str));
        deinit_String(&str);
    }
    if (flags & wrapText_WidgetFlag) {
        const iRect inner = adjusted_Rect(innerBounds_Widget(w), init_I2(iconPad, 0), zero_I2());
        const int   wrap  = inner.size.x;
        drawWrapRange_Text(d->font, topLeft_Rect(inner), wrap, fg, range_String(&d->label));
    }
    else if (flags & alignLeft_WidgetFlag) {
        draw_Text(d->font, add_I2(bounds.pos, addX_I2(padding_LabelWidget_(d, 0), iconPad)),
                  fg, cstr_String(&d->label));
        if ((flags & drawKey_WidgetFlag) && d->key) {
            iString str;
            init_String(&str);
            keyStr_LabelWidget_(d, &str);
            drawAlign_Text(uiShortcuts_FontId,
                           add_I2(topRight_Rect(bounds),
                                  addX_I2(negX_I2(padding_LabelWidget_(d, 1)),
                                          deviceType_App() == tablet_AppDeviceType ? gap_UI : 0)),
                           flags & pressed_WidgetFlag ? fg
                           : isCaution                ? uiTextCaution_ColorId
                                                      : uiTextShortcut_ColorId,
                           right_Alignment,
                           cstr_String(&str));
            deinit_String(&str);
        }
    }
    else if (flags & alignRight_WidgetFlag) {
        drawAlign_Text(
            d->font,
            add_I2(topRight_Rect(bounds), negX_I2(padding_LabelWidget_(d, 1))),
            fg,
            right_Alignment,
            cstr_String(&d->label));
    }
    else {
        drawCentered_Text(d->font,
                          adjusted_Rect(bounds,
                                        add_I2(zero_I2(), init_I2(iconPad, 0)),
                                        neg_I2(zero_I2())),
                          d->flags.alignVisual,
                          fg,
                          "%s",
                          cstr_String(&d->label));
    }
    if (flags & chevron_WidgetFlag) {
        const iRect chRect = rect;
        const int chSize = lineHeight_Text(d->font);
        drawCentered_Text(d->font,
                          (iRect){ addX_I2(topRight_Rect(chRect), -iconPad),
                                   init_I2(chSize, height_Rect(chRect)) },
                          iTrue, uiSeparator_ColorId, rightAngle_Icon);
    }
    unsetClip_Paint(&p);
}

static void sizeChanged_LabelWidget_(iLabelWidget *d) {
    iWidget *w = as_Widget(d);
    if (flags_Widget(w) & wrapText_WidgetFlag) {
        if (flags_Widget(w) & fixedHeight_WidgetFlag) {
            /* Calculate a new height based on the wrapping. */
            w->rect.size.y = advanceWrapRange_Text(
                                 d->font, innerBounds_Widget(w).size.x, range_String(&d->label))
                                 .y;
        }
    }
}

iInt2 defaultSize_LabelWidget(const iLabelWidget *d) {
    const iWidget *w = constAs_Widget(d);
    const int64_t flags = flags_Widget(w);
    iInt2 size = add_I2(measure_Text(d->font, cstr_String(&d->label)),
                        add_I2(padding_LabelWidget_(d, 0), padding_LabelWidget_(d, 2)));
    if ((flags & drawKey_WidgetFlag) && d->key) {
        iString str;
        init_String(&str);
        keyStr_LabelWidget_(d, &str);
        size.x += 2 * gap_UI + measure_Text(uiShortcuts_FontId, cstr_String(&str)).x;
        deinit_String(&str);
    }
    size.x += iconPadding_LabelWidget_(d);
    return size;
}

int font_LabelWidget(const iLabelWidget *d) {
    return d->font;
}

void updateSize_LabelWidget(iLabelWidget *d) {
    iWidget *w = as_Widget(d);
    const int64_t flags = flags_Widget(w);
    const iInt2 size = defaultSize_LabelWidget(d);
    if (!d->flags.noAutoMinHeight) {
        w->minSize.y = size.y; /* vertically text must remain visible */
    }
    /* Wrapped text implies that width must be defined by arrangement. */
    if (!(flags & (fixedWidth_WidgetFlag | wrapText_WidgetFlag))) {
        w->rect.size.x = size.x;
    }
    if (~flags & fixedHeight_WidgetFlag) {
        w->rect.size.y = size.y;
    }
}

static void replaceVariables_LabelWidget_(iLabelWidget *d) {
    translate_Lang(&d->label);
}

void init_LabelWidget(iLabelWidget *d, const char *label, const char *cmd) {
    iWidget *w = &d->widget;
    init_Widget(w);
    d->font = uiLabel_FontId;
    d->forceFg = none_ColorId;
    d->icon = 0;
    initCStr_String(&d->srcLabel, label);
    initCopy_String(&d->label, &d->srcLabel);
    replaceVariables_LabelWidget_(d);
    if (cmd) {
        initCStr_String(&d->command, cmd);
    }
    else {
        setFrameColor_Widget(w, uiFrame_ColorId);
        init_String(&d->command);
    }
    d->key   = 0;
    d->kmods = 0;
    init_Click(&d->click, d, !isEmpty_String(&d->command) ? SDL_BUTTON_LEFT : 0);
    setFlags_Widget(w, hover_WidgetFlag, d->click.button != 0);
    d->flags.alignVisual = iFalse;
    d->flags.noAutoMinHeight = iFalse;
    updateSize_LabelWidget(d);
    updateKey_LabelWidget_(d); /* could be bound to another key */
}

void deinit_LabelWidget(iLabelWidget *d) {
    deinit_String(&d->label);
    deinit_String(&d->srcLabel);
    deinit_String(&d->command);
}

void setFont_LabelWidget(iLabelWidget *d, int fontId) {
    d->font = fontId;
    updateSize_LabelWidget(d);
}

void setTextColor_LabelWidget(iLabelWidget *d, int color) {
    if (d && d->forceFg != color) {
        d->forceFg = color;
        refresh_Widget(d);
    }
}

void setText_LabelWidget(iLabelWidget *d, const iString *text) {
    updateText_LabelWidget(d, text);
    updateSize_LabelWidget(d);
}

void setAlignVisually_LabelWidget(iLabelWidget *d, iBool alignVisual) {
    d->flags.alignVisual = alignVisual;
}

void setNoAutoMinHeight_LabelWidget(iLabelWidget *d, iBool noAutoMinHeight) {
    /* By default all labels use a minimum height determined by the text dimensions. */
    d->flags.noAutoMinHeight = noAutoMinHeight;
    if (noAutoMinHeight) {
        d->widget.minSize.y = 0;
    }
}

void updateText_LabelWidget(iLabelWidget *d, const iString *text) {
    set_String(&d->label, text);
    set_String(&d->srcLabel, text);
    replaceVariables_LabelWidget_(d);
    refresh_Widget(&d->widget);
}

void updateTextCStr_LabelWidget(iLabelWidget *d, const char *text) {
    setCStr_String(&d->label, text);
    set_String(&d->srcLabel, &d->label);
    replaceVariables_LabelWidget_(d);
    refresh_Widget(&d->widget);
}

void setTextCStr_LabelWidget(iLabelWidget *d, const char *text) {
    setCStr_String(&d->label, text);
    set_String(&d->srcLabel, &d->label);
    replaceVariables_LabelWidget_(d);
    updateSize_LabelWidget(d);
}

void setCommand_LabelWidget(iLabelWidget *d, const iString *command) {
    set_String(&d->command, command);
}

void setIcon_LabelWidget(iLabelWidget *d, iChar icon) {
    if (d->icon != icon) {
        d->icon = icon;
        updateSize_LabelWidget(d);
    }
}

iBool checkIcon_LabelWidget(iLabelWidget *d) {
    if (isEmpty_String(&d->label)) {
        d->icon = 0;
        return iFalse;
    }
    iStringConstIterator iter;
    init_StringConstIterator(&iter, &d->label);
    const iChar icon = iter.value;
    next_StringConstIterator(&iter);
    if (iter.value == ' ' && icon >= 0x100) {
        d->icon = icon;
        remove_Block(&d->label.chars, 0, iter.next - constBegin_String(&d->label));
        return iTrue;
    }
    else {
        d->icon = 0;
    }
    return iFalse;
}

iChar icon_LabelWidget(const iLabelWidget *d) {
    return d->icon;
}

const iString *text_LabelWidget(const iLabelWidget *d) {
    if (!d) return collectNew_String();
    return &d->label;
}

const iString *sourceText_LabelWidget(const iLabelWidget *d) {
    if (!d) return collectNew_String();
    return &d->srcLabel;
}

const iString *command_LabelWidget(const iLabelWidget *d) {
    return &d->command;
}

iLabelWidget *newKeyMods_LabelWidget(const char *label, int key, int kmods, const char *command) {
    iLabelWidget *d = new_LabelWidget(label, command);
    d->key = key;
    d->kmods = kmods;
    updateKey_LabelWidget_(d); /* could be bound to a different key */
    return d;
}

iLabelWidget *newColor_LabelWidget(const char *text, int color) {
    iLabelWidget *d = new_LabelWidget(format_CStr("%s%s", escape_Color(color), text), NULL);
    setFlags_Widget(as_Widget(d), frameless_WidgetFlag, iTrue);
    return d;
}

iBeginDefineSubclass(LabelWidget, Widget)
    .processEvent = (iAny *) processEvent_LabelWidget_,
    .draw         = (iAny *) draw_LabelWidget_,
    .sizeChanged  = (iAny *) sizeChanged_LabelWidget_,
iEndDefineSubclass(LabelWidget)
