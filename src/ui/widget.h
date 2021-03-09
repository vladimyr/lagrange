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

#pragma once

/* Base class for UI widgets. */

#include "metrics.h"
#include "util.h"

#include <the_Foundation/object.h>
#include <the_Foundation/objectlist.h>
#include <the_Foundation/rect.h>
#include <the_Foundation/string.h>
#include <SDL_events.h>

#define iDeclareWidgetClass(className) \
    iDeclareType(className); \
    typedef iWidgetClass i##className##Class; \
    extern i##className##Class Class_##className;

iDeclareType(Widget)
iBeginDeclareClass(Widget)
    iBool (*processEvent)   (iWidget *, const SDL_Event *);
    void  (*draw)           (const iWidget *);
    void  (*sizeChanged)    (iWidget *); /* optional, defaults to NULL */
iEndDeclareClass(Widget)

enum iWidgetFlag {
    hidden_WidgetFlag             = iBit(1),
    disabled_WidgetFlag           = iBit(2),
    hover_WidgetFlag              = iBit(3), /* eligible for mouse hover */
    selected_WidgetFlag           = iBit(4),
    pressed_WidgetFlag            = iBit(5),
    alignLeft_WidgetFlag          = iBit(6),
    alignRight_WidgetFlag         = iBit(7),
    frameless_WidgetFlag          = iBit(8),
    commandOnClick_WidgetFlag     = iBit(9),
    commandOnMouseMiss_WidgetFlag = iBit(10),
    drawKey_WidgetFlag            = iBit(11),
    focusable_WidgetFlag          = iBit(12),
    tight_WidgetFlag              = iBit(13), /* smaller padding */
    keepOnTop_WidgetFlag          = iBit(14), /* gets events first; drawn last */
    mouseModal_WidgetFlag         = iBit(15), /* eats all unprocessed mouse events */
    radio_WidgetFlag              = iBit(16), /* select on click; deselect siblings */
    /* arrangement */
    fixedPosition_WidgetFlag               = iBit(17),
    arrangeHorizontal_WidgetFlag           = iBit(18), /* arrange children horizontally */
    arrangeVertical_WidgetFlag             = iBit(19), /* arrange children vertically */
    arrangeWidth_WidgetFlag                = iBit(20), /* area of children becomes parent size */
    arrangeHeight_WidgetFlag               = iBit(21), /* area of children becomes parent size */
    resizeWidthOfChildren_WidgetFlag       = iBit(22),
    resizeHeightOfChildren_WidgetFlag      = iBit(23),
    expand_WidgetFlag                      = iBit(24),
    fixedWidth_WidgetFlag                  = iBit(25),
    fixedHeight_WidgetFlag                 = iBit(26),
    resizeChildrenToWidestChild_WidgetFlag = iBit(27),
    resizeToParentWidth_WidgetFlag         = iBit(28),
    resizeToParentHeight_WidgetFlag        = iBit(29),
    collapse_WidgetFlag                    = iBit(30), /* if hidden, arrange size to zero */
    /* combinations */
    arrangeSize_WidgetFlag    = arrangeWidth_WidgetFlag | arrangeHeight_WidgetFlag,
    resizeChildren_WidgetFlag = resizeWidthOfChildren_WidgetFlag | resizeHeightOfChildren_WidgetFlag,
    fixedSize_WidgetFlag      = fixedWidth_WidgetFlag | fixedHeight_WidgetFlag,
};

/* 64-bit extended flags */
#define wasCollapsed_WidgetFlag             iBit64(32)
#define centerHorizontal_WidgetFlag         iBit64(33)
#define moveToParentLeftEdge_WidgetFlag     iBit64(34)
#define moveToParentRightEdge_WidgetFlag    iBit64(35)
#define wrapText_WidgetFlag                 iBit64(36)
#define borderTop_WidgetFlag                iBit64(37)
#define overflowScrollable_WidgetFlag       iBit64(38)
#define focusRoot_WidgetFlag                iBit64(39)
#define unhittable_WidgetFlag               iBit64(40)
#define touchDrag_WidgetFlag                iBit64(41) /* touch event behavior: immediate drag */
#define noBackground_WidgetFlag             iBit64(42)
#define drawBackgroundToHorizontalSafeArea_WidgetFlag   iBit64(43)
#define drawBackgroundToVerticalSafeArea_WidgetFlag     iBit64(44)
#define visualOffset_WidgetFlag             iBit64(45)
#define parentCannotResize_WidgetFlag       iBit64(46)
#define noTopFrame_WidgetFlag               iBit64(47)
#define unpadded_WidgetFlag                 iBit64(48) /* ignore parent's padding */
#define extraPadding_WidgetFlag             iBit64(49)
#define borderBottom_WidgetFlag             iBit64(50)
#define horizontalOffset_WidgetFlag         iBit64(51) /* default is vertical offset */
#define chevron_WidgetFlag                  iBit64(52)

enum iWidgetAddPos {
    back_WidgetAddPos,
    front_WidgetAddPos,
};

enum iWidgetFocusDir {
    forward_WidgetFocusDir,
    backward_WidgetFocusDir,
};

struct Impl_Widget {
    iObject      object;
    iString      id;
    int64_t      flags;
    iRect        rect;
    int          padding[4]; /* left, top, right, bottom */
    iAnim        visualOffset;
    int          bgColor;
    int          frameColor;
    iObjectList *children;
    iWidget *    parent;
    iBool (*commandHandler)(iWidget *, const char *);
};

iDeclareObjectConstruction(Widget)

iLocalDef iWidget *as_Widget(iAnyObject *d) {
#if !defined (NDEBUG)
    if (d) {
        iAssertIsObject(d);
        iAssert(isInstance_Object(d, &Class_Widget));
    }
#endif
    return (iWidget *) d;
}

iLocalDef const iWidget *constAs_Widget(const iAnyObject *d) {
#if !defined (NDEBUG)
    if (d) {
        iAssertIsObject(d);
        iAssert(isInstance_Object(d, &Class_Widget));
    }
#endif
    return (const iWidget *) d;
}

void    destroy_Widget          (iWidget *); /* widget removed and deleted later */
void    destroyPending_Widget   (void);
void    releaseChildren_Widget  (iWidget *);

const iString *id_Widget    (const iWidget *);
int64_t flags_Widget        (const iWidget *);
iRect   bounds_Widget       (const iWidget *); /* outer bounds */
iRect   innerBounds_Widget  (const iWidget *);
iInt2   localCoord_Widget   (const iWidget *, iInt2 coord);
iBool   contains_Widget     (const iWidget *, iInt2 coord);
iAny *  hitChild_Widget     (const iWidget *, iInt2 coord);
iAny *  findChild_Widget    (const iWidget *, const char *id);
iAny *  findParentClass_Widget(const iWidget *, const iAnyClass *class);
iAny *  findFocusable_Widget(const iWidget *startFrom, enum iWidgetFocusDir focusDir);
size_t  childCount_Widget   (const iWidget *);
void    draw_Widget         (const iWidget *);
void    drawBackground_Widget(const iWidget *);
void    drawChildren_Widget (const iWidget *);

iLocalDef int width_Widget(const iAnyObject *d) {
    if (d) {
        iAssert(isInstance_Object(d, &Class_Widget));
        return ((const iWidget *) d)->rect.size.x;
    }
    return 0;
}
iLocalDef int height_Widget(const iAnyObject *d) {
    if (d) {
        iAssert(isInstance_Object(d, &Class_Widget));
        return ((const iWidget *) d)->rect.size.y;
    }
    return 0;
}
iLocalDef iObjectList *children_Widget(iAnyObject *d) {
    iAssert(isInstance_Object(d, &Class_Widget));
    return ((iWidget *) d)->children;
}
iLocalDef iWidget *parent_Widget(const iAnyObject *d) {
    if (d) {
        iAssert(isInstance_Object(d, &Class_Widget));
        return ((iWidget *) d)->parent;
    }
    return NULL;
}

iBool   isVisible_Widget            (const iAnyObject *);
iBool   isDisabled_Widget           (const iAnyObject *);
iBool   isFocused_Widget            (const iAnyObject *);
iBool   isHover_Widget              (const iAnyObject *);
iBool   isSelected_Widget           (const iAnyObject *);
iBool   isCommand_Widget            (const iWidget *d, const SDL_Event *ev, const char *cmd);
iBool   hasParent_Widget            (const iWidget *d, const iWidget *someParent);
void    setId_Widget                (iWidget *, const char *id);
void    setFlags_Widget             (iWidget *, int64_t flags, iBool set);
void    setPos_Widget               (iWidget *, iInt2 pos);
void    setSize_Widget              (iWidget *, iInt2 size);
void    setPadding_Widget           (iWidget *, int left, int top, int right, int bottom);
iLocalDef void setPadding1_Widget   (iWidget *d, int padding) { setPadding_Widget(d, padding, padding, padding, padding); }
void    setVisualOffset_Widget      (iWidget *d, int value, uint32_t span, int animFlags);
void    setBackgroundColor_Widget   (iWidget *, int bgColor);
void    setFrameColor_Widget        (iWidget *, int frameColor);
void    setCommandHandler_Widget    (iWidget *, iBool (*handler)(iWidget *, const char *));
iAny *  addChild_Widget             (iWidget *, iAnyObject *child); /* holds a ref */
iAny *  addChildPos_Widget          (iWidget *, iAnyObject *child, enum iWidgetAddPos addPos);
iAny *  addChildFlags_Widget        (iWidget *, iAnyObject *child, int64_t childFlags); /* holds a ref */
iAny *  insertChildAfter_Widget     (iWidget *, iAnyObject *child, size_t afterIndex);
iAny *  insertChildAfterFlags_Widget(iWidget *, iAnyObject *child, size_t afterIndex, int64_t childFlags);
iAny *  removeChild_Widget          (iWidget *, iAnyObject *child); /* returns a ref */
iAny *  child_Widget                (iWidget *, size_t index); /* O(n) */
size_t  childIndex_Widget           (const iWidget *, const iAnyObject *child); /* O(n) */
void    arrange_Widget              (iWidget *);
iBool   dispatchEvent_Widget        (iWidget *, const SDL_Event *);
iBool   processEvent_Widget         (iWidget *, const SDL_Event *);
void    postCommand_Widget          (const iAnyObject *, const char *cmd, ...);
void    refresh_Widget              (const iAnyObject *);

iBool   equalWidget_Command (const char *cmd, const iWidget *widget, const char *checkCommand);

void        setFocus_Widget         (iWidget *);
iWidget *   focus_Widget            (void);
void        setHover_Widget         (iWidget *);
iWidget *   hover_Widget            (void);
void        unhover_Widget          (void);
void        setMouseGrab_Widget     (iWidget *);
iWidget *   mouseGrab_Widget        (void);
iBool       hasVisibleChildOnTop_Widget
                                    (const iWidget *parent);
void        printTree_Widget        (const iWidget *);
