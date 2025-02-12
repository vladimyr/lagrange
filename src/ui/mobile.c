/* Copyright 2021 Jaakko Keränen <jaakko.keranen@iki.fi>

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

#include "mobile.h"

#include "app.h"
#include "command.h"
#include "defs.h"
#include "inputwidget.h"
#include "labelwidget.h"
#include "root.h"
#include "text.h"
#include "widget.h"
#include "window.h"

#if defined (iPlatformAppleMobile)
#   include "ios.h"
#endif

static iBool useMobileSheetLayout_(void) {
    return deviceType_App() != desktop_AppDeviceType;
}

static iBool isSideBySideLayout_(void) {
    if (deviceType_App() == phone_AppDeviceType) {
        return isLandscape_App();
    }
    return numRoots_Window(get_Window()) == 1;
}

static enum iFontId labelFont_(void) {
    return deviceType_App() == phone_AppDeviceType ? defaultBig_FontId : defaultMedium_FontId;
}

static enum iFontId labelBoldFont_(void) {
    return deviceType_App() == phone_AppDeviceType ? defaultBigBold_FontId : defaultMediumBold_FontId;
}

static void updatePanelSheetMetrics_(iWidget *sheet) {
    iWidget *navi       = findChild_Widget(sheet, "panel.navi");
    iWidget *naviPad    = child_Widget(navi, 0);
    int      naviHeight = lineHeight_Text(labelFont_()) + 4 * gap_UI;
#if defined (iPlatformAppleMobile)
    float left, right, top, bottom;
    safeAreaInsets_iOS(&left, &top, &right, &bottom);
    setPadding_Widget(sheet, left, 0, right, 0);
    navi->rect.pos = init_I2(left, top);
    iConstForEach(PtrArray, i, findChildren_Widget(sheet, "panel.toppad")) {
        iWidget *pad = *i.value;
        setFixedSize_Widget(pad, init1_I2(naviHeight));
    }
#endif
    setFixedSize_Widget(navi, init_I2(-1, naviHeight));
}

static iWidget *findDetailStack_(iWidget *topPanel) {
    return findChild_Widget(parent_Widget(topPanel), "detailstack");
}

static void unselectAllPanelButtons_(iWidget *topPanel) {
    iForEach(ObjectList, i, children_Widget(topPanel)) {
        if (isInstance_Object(i.object, &Class_LabelWidget)) {
            iLabelWidget *label = i.object;
            if (!cmp_String(command_LabelWidget(label), "panel.open")) {
                setFlags_Widget(i.object, selected_WidgetFlag, iFalse);
            }
        }
    }
}

static iBool mainDetailSplitHandler_(iWidget *mainDetailSplit, const char *cmd) {
    if (equal_Command(cmd, "window.resized")) {
        const iBool isPortrait = (deviceType_App() == phone_AppDeviceType && isPortrait_App());
        const iRect safeRoot   = safeRect_Root(mainDetailSplit->root);
        setPos_Widget(mainDetailSplit, topLeft_Rect(safeRoot));
        setFixedSize_Widget(mainDetailSplit, safeRoot.size);
        iWidget *    sheet        = parent_Widget(mainDetailSplit);
        iWidget *    navi         = findChild_Widget(sheet, "panel.navi");
        iWidget *    detailStack  = findChild_Widget(mainDetailSplit, "detailstack");
        const size_t numPanels    = childCount_Widget(detailStack);
        const iBool  isSideBySide = isSideBySideLayout_() && numPanels > 0;
        setFlags_Widget(mainDetailSplit, arrangeHorizontal_WidgetFlag, isSideBySide);
        setFlags_Widget(detailStack, expand_WidgetFlag, isSideBySide);
        setFlags_Widget(detailStack, hidden_WidgetFlag, numPanels == 0);
        iWidget *topPanel = findChild_Widget(mainDetailSplit, "panel.top");
        const int pad = isPortrait ? 0 : 3 * gap_UI;
        if (isSideBySide) {
            iAssert(topPanel);
            topPanel->rect.size.x = (deviceType_App() == phone_AppDeviceType ?
                                     safeRoot.size.x * 2 / 5 : (safeRoot.size.x / 3));
        }
        if (deviceType_App() == tablet_AppDeviceType) {
            setPadding_Widget(topPanel, pad, 0, pad, pad);
            if (numPanels == 0) {
                setFlags_Widget(sheet, centerHorizontal_WidgetFlag, iTrue);
                const int sheetWidth = iMin(safeRoot.size.x, safeRoot.size.y);
                mainDetailSplit->rect.size.x = sheetWidth;
                setFixedSize_Widget(sheet, init_I2(sheetWidth, -1));
                setFixedSize_Widget(navi, init_I2(sheetWidth, -1));
            }
        }
        iForEach(ObjectList, i, children_Widget(detailStack)) {
            iWidget *panel = i.object;
            setFlags_Widget(panel, edgeDraggable_WidgetFlag, !isSideBySide);
            if (isSideBySide) {
                setVisualOffset_Widget(panel, 0, 0, 0);
            }
            setPadding_Widget(panel, pad, 0, pad, pad);
        }
        arrange_Widget(mainDetailSplit);
    }
    return iFalse;
}

static iBool topPanelHandler_(iWidget *topPanel, const char *cmd) {
    const iBool isPortrait = !isSideBySideLayout_();
    if (equal_Command(cmd, "panel.open")) {
        iWidget *button = pointer_Command(cmd);
        iWidget *panel = userData_Object(button);
//        openMenu_Widget(panel, innerToWindow_Widget(panel, zero_I2()));
//        setFlags_Widget(panel, hidden_WidgetFlag, iFalse);
        unselectAllPanelButtons_(topPanel);
        iForEach(ObjectList, i, children_Widget(findDetailStack_(topPanel))) {
            iWidget *child = i.object;
            setFlags_Widget(child, hidden_WidgetFlag | disabled_WidgetFlag, child != panel);
            /* Animate the current panel in. */
            if (child == panel && isPortrait) {
                setupSheetTransition_Mobile(panel, iTrue);
            }
        }
        setFlags_Widget(button, selected_WidgetFlag, iTrue);
        return iTrue;
    }
    if (equal_Command(cmd, "mouse.clicked") && arg_Command(cmd) &&
        argLabel_Command(cmd, "button") == SDL_BUTTON_X1) {
        postCommand_App("panel.close");
        return iTrue;
    }
    if (equal_Command(cmd, "panel.close")) {
        iBool wasClosed = iFalse;
        if (isPortrait) {
            iForEach(ObjectList, i, children_Widget(findDetailStack_(topPanel))) {
                iWidget *child = i.object;
                if (!cmp_String(id_Widget(child), "panel") && isVisible_Widget(child)) {
    //                closeMenu_Widget(child);
                    setupSheetTransition_Mobile(child, iFalse);
                    setFlags_Widget(child, hidden_WidgetFlag | disabled_WidgetFlag, iTrue);
                    setFocus_Widget(NULL);
                    updateTextCStr_LabelWidget(findWidget_App("panel.back"), "Back");
                    wasClosed = iTrue;
                }
            }
        }
        unselectAllPanelButtons_(topPanel);
        if (!wasClosed) {
            postCommand_App("prefs.dismiss");
        }
        return iTrue;
    }
    if (equal_Command(cmd, "document.changed")) {
        postCommand_App("prefs.dismiss");
        return iFalse;
    }
    if (equal_Command(cmd, "window.resized")) {
        // sheet > mdsplit > panel.top
        updatePanelSheetMetrics_(parent_Widget(parent_Widget(topPanel)));
    }
    return iFalse;
}

static iBool isTwoColumnPage_(iWidget *d) {
    if (cmp_String(id_Widget(d), "dialogbuttons") == 0 ||
        cmp_String(id_Widget(d), "prefs.tabs") == 0) {
        return iFalse;
    }
    if (class_Widget(d) == &Class_Widget && childCount_Widget(d) == 2) {
        return class_Widget(child_Widget(d, 0)) == &Class_Widget &&
               class_Widget(child_Widget(d, 1)) == &Class_Widget;
    }
    return iFalse;
}

static iBool isOmittedPref_(const iString *id) {
    static const char *omittedPrefs[] = {
        "prefs.smoothscroll",
        "prefs.imageloadscroll",
        "prefs.pinsplit",
        "prefs.retainwindow",
        "prefs.ca.file",
        "prefs.ca.path",
    };
    iForIndices(i, omittedPrefs) {
        if (cmp_String(id, omittedPrefs[i]) == 0) {
            return iTrue;
        }
    }
    return iFalse;
}

enum iPrefsElement {
    panelTitle_PrefsElement,
    heading_PrefsElement,
    toggle_PrefsElement,
    dropdown_PrefsElement,
    radioButton_PrefsElement,
    textInput_PrefsElement,
};

static iAnyObject *addPanelChild_(iWidget *panel, iAnyObject *child, int64_t flags,
                                  enum iPrefsElement elementType,
                                  enum iPrefsElement precedingElementType) {
    /* Erase redundant/unused headings. */
    if (precedingElementType == heading_PrefsElement &&
        (!child || (elementType == heading_PrefsElement || elementType == radioButton_PrefsElement))) {
        iRelease(removeChild_Widget(panel, lastChild_Widget(panel)));
        if (!cmp_String(id_Widget(constAs_Widget(lastChild_Widget(panel))), "padding")) {
            iRelease(removeChild_Widget(panel, lastChild_Widget(panel)));
        }
    }
    if (child) {
        /* Insert padding between different element types. */
        if (precedingElementType != panelTitle_PrefsElement) {
            if (elementType == heading_PrefsElement ||
                (elementType == toggle_PrefsElement &&
                 precedingElementType != toggle_PrefsElement &&
                 precedingElementType != heading_PrefsElement) ||
                (elementType == dropdown_PrefsElement &&
                 precedingElementType != dropdown_PrefsElement &&
                 precedingElementType != heading_PrefsElement) ||
                (elementType == textInput_PrefsElement &&
                 precedingElementType != textInput_PrefsElement &&
                 precedingElementType != heading_PrefsElement)) {
                addChild_Widget(panel, iClob(makePadding_Widget(lineHeight_Text(labelFont_()))));
            }
        }
        if ((elementType == toggle_PrefsElement && precedingElementType != toggle_PrefsElement) ||
            (elementType == textInput_PrefsElement && precedingElementType != textInput_PrefsElement) ||
            (elementType == dropdown_PrefsElement && precedingElementType != dropdown_PrefsElement) ||
            (elementType == radioButton_PrefsElement && precedingElementType == heading_PrefsElement)) {
            flags |= borderTop_WidgetFlag;
        }
        return addChildFlags_Widget(panel, child, flags);
    }
    return NULL;
}

static void stripTrailingColon_(iLabelWidget *label) {
    const iString *text = text_LabelWidget(label);
    if (endsWith_String(text, ":")) {
        iString *mod = copy_String(text);
        removeEnd_String(mod, 1);
        updateText_LabelWidget(label, mod);
        delete_String(mod);
    }
}

static iLabelWidget *makePanelButton_(const char *text, const char *command) {
    iLabelWidget *btn = new_LabelWidget(text, command);
    setFlags_Widget(as_Widget(btn),
                    borderBottom_WidgetFlag | alignLeft_WidgetFlag |
                    frameless_WidgetFlag | extraPadding_WidgetFlag,
                    iTrue);
    checkIcon_LabelWidget(btn);
    setFont_LabelWidget(btn, labelFont_());
    setTextColor_LabelWidget(btn, uiTextStrong_ColorId);
    setBackgroundColor_Widget(as_Widget(btn), uiBackgroundSidebar_ColorId);
    return btn;
}

static iWidget *makeValuePadding_(iWidget *value) {
    iInputWidget *input = isInstance_Object(value, &Class_InputWidget) ? (iInputWidget *) value : NULL;
    if (input) {
        setFont_InputWidget(input, labelFont_());
        setContentPadding_InputWidget(input, 3 * gap_UI, 3 * gap_UI);
    }
    iWidget *pad = new_Widget();
    setBackgroundColor_Widget(pad, uiBackgroundSidebar_ColorId);
    setPadding_Widget(pad, 0, 1 * gap_UI, 0, 1 * gap_UI);
    addChild_Widget(pad, iClob(value));
    setFlags_Widget(pad,
                    borderBottom_WidgetFlag |
                    arrangeVertical_WidgetFlag |
                    resizeToParentWidth_WidgetFlag |
                    resizeWidthOfChildren_WidgetFlag |
                    arrangeHeight_WidgetFlag,
                    iTrue);
    return pad;
}

static iWidget *makeValuePaddingWithHeading_(iLabelWidget *heading, iWidget *value) {
    const iBool isInput = isInstance_Object(value, &Class_InputWidget);
    iWidget *div = new_Widget();
    setFlags_Widget(div,
                    borderBottom_WidgetFlag | arrangeHeight_WidgetFlag |
                    resizeWidthOfChildren_WidgetFlag |
                    arrangeHorizontal_WidgetFlag, iTrue);
    setBackgroundColor_Widget(div, uiBackgroundSidebar_ColorId);
    setPadding_Widget(div, gap_UI, gap_UI, 4 * gap_UI, gap_UI);
    addChildFlags_Widget(div, iClob(heading), 0);
    setPadding1_Widget(as_Widget(heading), 0);
    //setFixedSize_Widget(as_Widget(heading), init_I2(-1, height_Widget(value)));
    setFont_LabelWidget(heading, labelFont_());
    setTextColor_LabelWidget(heading, uiTextStrong_ColorId);
    if (isInput) {
        addChildFlags_Widget(div, iClob(value), expand_WidgetFlag);
    }
    else if (isInstance_Object(value, &Class_LabelWidget) &&
             cmp_String(command_LabelWidget((iLabelWidget *) value), "toggle")) {
        addChildFlags_Widget(div, iClob(value), expand_WidgetFlag);
        /* TODO: This doesn't work? */
//        setCommand_LabelWidget(heading,
//                               collectNewFormat_String("!%s ptr:%p",
//                                           cstr_String(command_LabelWidget((iLabelWidget *) value)),
//                                           value));
    }
    else {
        addChildFlags_Widget(div, iClob(new_Widget()), expand_WidgetFlag);
        addChild_Widget(div, iClob(value));
    }
    printTree_Widget(div);
    return div;
}

static iWidget *addChildPanel_(iWidget *parent, iLabelWidget *panelButton,
                               const iString *titleText) {
    iWidget *panel = new_Widget();
    setId_Widget(panel, "panel");
    setUserData_Object(panelButton, panel);
    setBackgroundColor_Widget(panel, uiBackground_ColorId);
    setId_Widget(addChild_Widget(panel, iClob(makePadding_Widget(0))), "panel.toppad");
    if (titleText) {
        iLabelWidget *title =
            addChildFlags_Widget(panel,
                                 iClob(new_LabelWidget(cstr_String(titleText), NULL)),
                                 alignLeft_WidgetFlag | frameless_WidgetFlag);
        setFont_LabelWidget(title, uiLabelLargeBold_FontId);
        setTextColor_LabelWidget(title, uiHeading_ColorId);
    }
    addChildFlags_Widget(parent,
                         iClob(panel),
                         focusRoot_WidgetFlag | hidden_WidgetFlag | disabled_WidgetFlag |
                             arrangeVertical_WidgetFlag | resizeWidthOfChildren_WidgetFlag |
                             arrangeHeight_WidgetFlag | overflowScrollable_WidgetFlag |
                             drawBackgroundToBottom_WidgetFlag |
                             horizontalOffset_WidgetFlag | commandOnClick_WidgetFlag);
    return panel;
}

void finalizeSheet_Mobile(iWidget *sheet) {
    /* The sheet contents are completely rearranged and restyled on a phone.
       We'll set up a linear fullscreen arrangement of the widgets. Sheets are already
       scrollable so they can be taller than the display. In hindsight, it may have been
       easier to create phone versions of each dialog, but at least this works with any
       future changes to the UI (..."works"). At least this way it is possible to enforce
       a consistent styling. */
    if (useMobileSheetLayout_() && parent_Widget(sheet) == root_Widget(sheet)) {
        if (~flags_Widget(sheet) & keepOnTop_WidgetFlag) {
            /* Already finalized. */
            arrange_Widget(sheet);
            postRefresh_App();
            return;
        }
        /* TODO: In portrait, top panel and detail stack are all stacked together.
         
                 Landscape Layout                 Portrait Layout
                                              
        ┌─────────┬──────Detail─Stack─────┐    ┌─────────┬ ─ ─ ─ ─ ┐
        │         │┌───────────────────┐  │    │         │Detail
        │         ││┌──────────────────┴┐ │    │         │Stack    │
        │         │││┌──────────────────┴┐│    │         │┌──────┐
        │         ││││                   ││    │         ││┌─────┴┐│
        │         ││││                   ││    │         │││      │
        │Top Panel││││                   ││    │Top Panel│││      ││
        │         ││││      Panels       ││    │         │││Panels│
        │         ││││                   ││    │         │││      ││
        │         │└┤│                   ││    │         │││      │
        │         │ └┤                   ││    │         │└┤      ││
        │         │  └───────────────────┘│    │         │ └──────┘
        └─────────┴───────────────────────┘    └─────────┴ ─ ─ ─ ─ ┘
                                                          offscreen
        */
        /* Modify the top sheet to act as a fullscreen background. */
        setPadding1_Widget(sheet, 0);
        setBackgroundColor_Widget(sheet, uiBackground_ColorId);
        setFlags_Widget(sheet,
                        keepOnTop_WidgetFlag |
                        parentCannotResize_WidgetFlag |
                        arrangeSize_WidgetFlag |
                        centerHorizontal_WidgetFlag |
                        arrangeVertical_WidgetFlag |
                        arrangeHorizontal_WidgetFlag |
                        overflowScrollable_WidgetFlag,
                        iFalse);
        setFlags_Widget(sheet,
                        frameless_WidgetFlag |
                        //resizeWidthOfChildren_WidgetFlag |
                        edgeDraggable_WidgetFlag |
                        commandOnClick_WidgetFlag,
                        iTrue);
        iPtrArray *   contents         = collect_PtrArray(new_PtrArray()); /* two-column pages */
        iPtrArray *   panelButtons     = collect_PtrArray(new_PtrArray());
        iWidget *     prefsTabs        = findChild_Widget(sheet, "prefs.tabs");
        iWidget *     dialogHeading    = (prefsTabs ? NULL : child_Widget(sheet, 0));
        const iBool   isPrefs          = (prefsTabs != NULL);
        const int64_t panelButtonFlags = borderBottom_WidgetFlag | alignLeft_WidgetFlag |
                                         frameless_WidgetFlag | extraPadding_WidgetFlag;
        iWidget *mainDetailSplit = makeHDiv_Widget();
        setCommandHandler_Widget(mainDetailSplit, mainDetailSplitHandler_);
        setFlags_Widget(mainDetailSplit, resizeHeightOfChildren_WidgetFlag, iFalse);
        setId_Widget(mainDetailSplit, "mdsplit");
        iWidget *topPanel = new_Widget(); {
            setId_Widget(topPanel, "panel.top");
            setCommandHandler_Widget(topPanel, topPanelHandler_);
            setFlags_Widget(topPanel,
                            arrangeVertical_WidgetFlag |
                            resizeWidthOfChildren_WidgetFlag |
                            arrangeHeight_WidgetFlag |
                            overflowScrollable_WidgetFlag |
                            commandOnClick_WidgetFlag,
                            iTrue);
            addChild_Widget(mainDetailSplit, iClob(topPanel));
        }
        iWidget *detailStack = new_Widget(); {
            setId_Widget(detailStack, "detailstack");
            setFlags_Widget(detailStack, collapse_WidgetFlag | resizeWidthOfChildren_WidgetFlag, iTrue);
            addChild_Widget(mainDetailSplit, iClob(detailStack));
        }
        addChild_Widget(topPanel, iClob(makePadding_Widget(lineHeight_Text(labelFont_()))));
        /* Slide top panel with detail panels. */ {
            setFlags_Widget(topPanel, refChildrenOffset_WidgetFlag, iTrue);
            topPanel->offsetRef = detailStack;
        }
        if (prefsTabs) {
            iRelease(removeChild_Widget(sheet, child_Widget(sheet, 0))); /* heading */
            iRelease(removeChild_Widget(sheet, findChild_Widget(sheet, "dialogbuttons")));
            /* Pull out the pages and make them panels. */
            iWidget *pages = findChild_Widget(prefsTabs, "tabs.pages");
            size_t pageCount = tabCount_Widget(prefsTabs);
            for (size_t i = 0; i < pageCount; i++) {
                iString *text = copy_String(text_LabelWidget(tabPageButton_Widget(prefsTabs, tabPage_Widget(prefsTabs, 0))));
                iWidget *page = removeTabPage_Widget(prefsTabs, 0);
                iWidget *pageContent = child_Widget(page, 1); /* surrounded by padding widgets */
                pushBack_PtrArray(contents, ref_Object(pageContent));
                iLabelWidget *panelButton;
                pushBack_PtrArray(panelButtons,
                                  addChildFlags_Widget(topPanel,
                                                       iClob(panelButton = makePanelButton_(
                                                             i == 1 ? "${heading.prefs.userinterface}" : cstr_String(text),
                                                             "panel.open")),
                                                       (i == 0 ? borderTop_WidgetFlag : 0) |
                                                       chevron_WidgetFlag));
                const iChar icons[] = {
                    0x02699, /* gear */
                    0x1f4f1, /* mobile phone */
                    0x1f3a8, /* palette */
                    0x1f523,
                    0x1f5a7, /* computer network */
                };
                setIcon_LabelWidget(panelButton, icons[i]);
//                setFont_LabelWidget(panelButton, labelFont_());
//                setBackgroundColor_Widget(as_Widget(panelButton), uiBackgroundSidebar_ColorId);
                iRelease(page);
                delete_String(text);
            }
            destroy_Widget(prefsTabs);
        }
        iForEach(ObjectList, i, children_Widget(sheet)) {
            iWidget *child = i.object;
            if (isTwoColumnPage_(child)) {
                pushBack_PtrArray(contents, removeChild_Widget(sheet, child));
            }
            else {
                removeChild_Widget(sheet, child);
                addChild_Widget(topPanel, child);
                iRelease(child);
            }
        }
        const iBool useSlidePanels = (size_PtrArray(contents) == size_PtrArray(panelButtons));
        addChild_Widget(sheet, iClob(mainDetailSplit));
        iForEach(PtrArray, j, contents) {
            iWidget *owner = topPanel;
            if (useSlidePanels) {
                /* Create a new child panel. */
                iLabelWidget *button = at_PtrArray(panelButtons, index_PtrArrayIterator(&j));
                owner = addChildPanel_(detailStack, button,
                                       collect_String(upper_String(text_LabelWidget(button))));
            }
            iWidget *pageContent = j.ptr;
            iWidget *headings = child_Widget(pageContent, 0);
            iWidget *values   = child_Widget(pageContent, 1);
            enum iPrefsElement prevElement = panelTitle_PrefsElement;
            /* Identify the types of controls in the dialog and restyle/organize them. */
            while (!isEmpty_ObjectList(children_Widget(headings))) {
                iWidget *heading = child_Widget(headings, 0);
                iWidget *value   = child_Widget(values, 0);
                removeChild_Widget(headings, heading);
                removeChild_Widget(values, value);
                /* Can we ignore these widgets? */
                if (isOmittedPref_(id_Widget(value)) ||
                    (class_Widget(heading) == &Class_Widget &&
                     class_Widget(value) == &Class_Widget) /* just padding */) {
                    iRelease(heading);
                    iRelease(value);
                    continue;
                }
                enum iPrefsElement element = toggle_PrefsElement;
                iLabelWidget *headingLabel = NULL;
                iLabelWidget *valueLabel = NULL;
                iInputWidget *valueInput = NULL;
                const iBool isMenuButton = findChild_Widget(value, "menu") != NULL;
                if (isInstance_Object(heading, &Class_LabelWidget)) {
                    headingLabel = (iLabelWidget *) heading;
                    stripTrailingColon_(headingLabel);
                }
                if (isInstance_Object(value, &Class_LabelWidget)) {
                    valueLabel = (iLabelWidget *) value;
                    setFont_LabelWidget(valueLabel, labelFont_());
                }
                if (isInstance_Object(value, &Class_InputWidget)) {
                    valueInput = (iInputWidget *) value;
                    setFlags_Widget(value, borderBottom_WidgetFlag, iFalse);
                    element = textInput_PrefsElement;
                }
                if (childCount_Widget(value) >= 2) {
                    if (isInstance_Object(child_Widget(value, 0), &Class_InputWidget)) {
                        element = textInput_PrefsElement;
                        setPadding_Widget(value, 0, 0, gap_UI, 0);
                        valueInput = child_Widget(value, 0);
                    }
                }
                if (valueInput) {
                    setFont_InputWidget(valueInput, labelFont_());
                    setContentPadding_InputWidget(valueInput, 3 * gap_UI, 0);
                }
                /* Toggles have the button on the right. */
                if (valueLabel && cmp_String(command_LabelWidget(valueLabel), "toggle") == 0) {
                    element = toggle_PrefsElement;
                    addPanelChild_(owner,
                                   iClob(makeValuePaddingWithHeading_(headingLabel, value)),
                                   0,
                                   element,
                                   prevElement);
                }
                else if (valueLabel && isEmpty_String(text_LabelWidget(valueLabel))) {
                    element = heading_PrefsElement;
                    iRelease(value);
                    addPanelChild_(owner, iClob(heading), 0, element, prevElement);
                    setFont_LabelWidget(headingLabel, uiLabel_FontId);
                }
                else if (isMenuButton) {
                    element = dropdown_PrefsElement;
                    setFlags_Widget(value,
                                    alignRight_WidgetFlag | noBackground_WidgetFlag |
                                    frameless_WidgetFlag, iTrue);
                    setFlags_Widget(value, alignLeft_WidgetFlag, iFalse);
                    iWidget *pad = addPanelChild_(owner, iClob(makeValuePaddingWithHeading_(headingLabel, value)), 0,
                                                  element, prevElement);
                    pad->padding[2] = gap_UI;
                }
                else if (valueInput) {
                    addPanelChild_(owner, iClob(makeValuePaddingWithHeading_(headingLabel, value)), 0,
                                       element, prevElement);
                }
                else {
                    if (childCount_Widget(value) >= 2) {
                        element = radioButton_PrefsElement;
                        /* Always padding before radio buttons. */
                        addChild_Widget(owner, iClob(makePadding_Widget(lineHeight_Text(labelFont_()))));
                    }
                    addChildFlags_Widget(owner, iClob(heading), borderBottom_WidgetFlag);
                    if (headingLabel) {
                        setTextColor_LabelWidget(headingLabel, uiSubheading_ColorId);
                        setText_LabelWidget(headingLabel,
                                            collect_String(upper_String(text_LabelWidget(headingLabel))));
                    }
                    addPanelChild_(owner, iClob(value), 0, element, prevElement);
                    /* Radio buttons expand to fill the space. */
                    if (element == radioButton_PrefsElement) {
                        setBackgroundColor_Widget(value, uiBackgroundSidebar_ColorId);
                        setPadding_Widget(value, 4 * gap_UI, 2 * gap_UI, 4 * gap_UI, 2 * gap_UI);
                        setFlags_Widget(value, arrangeWidth_WidgetFlag, iFalse);
                        setFlags_Widget(value,
                                        borderBottom_WidgetFlag |
                                        resizeToParentWidth_WidgetFlag |
                                        resizeWidthOfChildren_WidgetFlag,
                                        iTrue);
                        iForEach(ObjectList, sub, children_Widget(value)) {
                            if (isInstance_Object(sub.object, &Class_LabelWidget)) {
                                iLabelWidget *opt = sub.object;
                                setFont_LabelWidget(opt, defaultMedium_FontId);
                                setFlags_Widget(as_Widget(opt), noBackground_WidgetFlag, iTrue);
                            }
                        }
                    }
                }
                prevElement = element;
            }
            addPanelChild_(owner, NULL, 0, 0, prevElement);
            destroy_Widget(pageContent);
        }
        destroyPending_Root(sheet->root);
        /* Additional elements for preferences. */
        if (isPrefs) {
            addChild_Widget(topPanel, iClob(makePadding_Widget(lineHeight_Text(labelFont_()))));
            iLabelWidget *aboutButton = addChildFlags_Widget(topPanel,
                                 iClob(makePanelButton_(planet_Icon " ${menu.about}", "panel.open")),
                                 chevron_WidgetFlag | borderTop_WidgetFlag);
            addChildFlags_Widget(topPanel,
                                 iClob(makePanelButton_(info_Icon " ${menu.help}", "!open url:about:help")), 0);
            /* The About panel. */ {
                iWidget *panel = addChildPanel_(detailStack, aboutButton, NULL);
                iString *msg = collectNew_String();
                setCStr_String(msg, "Lagrange " LAGRANGE_APP_VERSION);
#if defined (iPlatformAppleMobile)
                appendCStr_String(msg, " (" LAGRANGE_IOS_VERSION ")");
#endif
                addChildFlags_Widget(panel, iClob(new_LabelWidget(cstr_String(msg), NULL)),
                                     frameless_WidgetFlag);
                addChildFlags_Widget(panel,
                                     iClob(makePanelButton_(globe_Icon " By @jk@skyjake.fi",
                                                            "!open url:https://skyjake.fi/@jk")),
                                     borderTop_WidgetFlag);
                addChildFlags_Widget(panel,
                                     iClob(makePanelButton_(clock_Icon " ${menu.releasenotes}",
                                                            "!open url:about:version")),
                                     0);
                addChildFlags_Widget(panel,
                                     iClob(makePanelButton_(info_Icon " ${menu.aboutpages}",
                                                            "!open url:about:about")),
                                     0);
                addChildFlags_Widget(panel,
                                     iClob(makePanelButton_(bug_Icon " ${menu.debug}",
                                                            "!open url:about:debug")),
                                     0);
            }
        }
        else {
            setFlags_Widget(topPanel, overflowScrollable_WidgetFlag, iTrue);
            /* Update heading style. */
            setFont_LabelWidget((iLabelWidget *) dialogHeading, uiLabelLargeBold_FontId);
            setFlags_Widget(dialogHeading, alignLeft_WidgetFlag, iTrue);
        }
        if (findChild_Widget(sheet, "valueinput.prompt")) {
            iWidget *prompt = findChild_Widget(sheet, "valueinput.prompt");
            setFlags_Widget(prompt, alignLeft_WidgetFlag, iTrue);
            iInputWidget *input = findChild_Widget(sheet, "input");
            removeChild_Widget(parent_Widget(input), input);
            addChild_Widget(topPanel, iClob(makeValuePadding_(as_Widget(input))));
        }
        /* Top padding for each panel, to account for the overlaid navbar. */ {
            setId_Widget(addChildPos_Widget(topPanel,
                                            iClob(makePadding_Widget(0)), front_WidgetAddPos),
                         "panel.toppad");
        }
        /* Navbar. */ {
            iWidget *navi = new_Widget();
            setId_Widget(navi, "panel.navi");
            setBackgroundColor_Widget(navi, uiBackground_ColorId);
            addChild_Widget(navi, iClob(makePadding_Widget(0)));
            iLabelWidget *back = addChildFlags_Widget(navi,
                                                      iClob(new_LabelWidget(leftAngle_Icon " ${panel.back}", "panel.close")),
                                                      noBackground_WidgetFlag | frameless_WidgetFlag |
                                                      alignLeft_WidgetFlag | extraPadding_WidgetFlag);
            checkIcon_LabelWidget(back);
            setId_Widget(as_Widget(back), "panel.back");
            setFont_LabelWidget(back, labelFont_());
            if (!isPrefs) {
                /* Pick up the dialog buttons for the navbar. */
                iWidget *buttons = findChild_Widget(sheet, "dialogbuttons");
                iLabelWidget *cancel = findMenuItem_Widget(buttons, "cancel");
//                if (!cancel) {
//                    cancel = findMenuItem_Widget(buttons, "translation.cancel");
//                }
                if (cancel) {
                    updateText_LabelWidget(back, text_LabelWidget(cancel));
                    setCommand_LabelWidget(back, command_LabelWidget(cancel));
                }
                iLabelWidget *def = (iLabelWidget *) lastChild_Widget(buttons);
                if (def && !cancel) {
                    updateText_LabelWidget(back, text_LabelWidget(def));
                    setCommand_LabelWidget(back, command_LabelWidget(def));
                    setFlags_Widget(as_Widget(back), alignLeft_WidgetFlag, iFalse);
                    setFlags_Widget(as_Widget(back), alignRight_WidgetFlag, iTrue);
                    setIcon_LabelWidget(back, 0);
                    setFont_LabelWidget(back, labelBoldFont_());
                }
                else if (def != cancel) {
                    removeChild_Widget(buttons, def);
                    setFont_LabelWidget(def, labelBoldFont_());
                    setFlags_Widget(as_Widget(def),
                                    frameless_WidgetFlag | extraPadding_WidgetFlag |
                                    noBackground_WidgetFlag, iTrue);
                    addChildFlags_Widget(as_Widget(back), iClob(def), moveToParentRightEdge_WidgetFlag);
                    updateSize_LabelWidget(def);
                }
                /* Action buttons are added in the bottom as extra buttons. */ {
                    iBool isFirstAction = iTrue;
                    iForEach(ObjectList, i, children_Widget(buttons)) {
                        if (isInstance_Object(i.object, &Class_LabelWidget) &&
                            i.object != cancel && i.object != def) {
                            iLabelWidget *item = i.object;
                            setBackgroundColor_Widget(i.object, uiBackgroundSidebar_ColorId);
                            setFont_LabelWidget(item, labelFont_());
                            removeChild_Widget(buttons, item);
                            addChildFlags_Widget(topPanel, iClob(item), panelButtonFlags |
                                                 (isFirstAction ? borderTop_WidgetFlag : 0));
                            updateSize_LabelWidget(item);
                            isFirstAction = iFalse;
                        }
                    }
                }
                iRelease(removeChild_Widget(parent_Widget(buttons), buttons));
                /* Styling for remaining elements. */
                iForEach(ObjectList, i, children_Widget(topPanel)) {
                    if (isInstance_Object(i.object, &Class_LabelWidget) &&
                        isEmpty_String(command_LabelWidget(i.object)) &&
                        isEmpty_String(id_Widget(i.object))) {
                        setFlags_Widget(i.object, alignLeft_WidgetFlag, iTrue);
                        if (font_LabelWidget(i.object) == uiLabel_FontId) {
                            setFont_LabelWidget(i.object, uiContent_FontId);
                        }
                    }
                }
            }
            addChildFlags_Widget(sheet, iClob(navi),
                                 drawBackgroundToVerticalSafeArea_WidgetFlag |
                                 arrangeHeight_WidgetFlag | resizeWidthOfChildren_WidgetFlag |
                                 resizeToParentWidth_WidgetFlag | arrangeVertical_WidgetFlag);
        }
        if (isPrefs && isSideBySideLayout_()) {
            /* Show the General panel. */
            postCommand_Widget(at_PtrArray(panelButtons, 0), "panel.open");
        }
        mainDetailSplitHandler_(mainDetailSplit, "window.resized"); /* make it resize the split */
        updatePanelSheetMetrics_(sheet);
        iAssert(sheet->parent);
        arrange_Widget(sheet->parent);
        postCommand_App("widget.overflow"); /* with the correct dimensions */
    }
    else {
        arrange_Widget(sheet);
    }
    if (!useMobileSheetLayout_()) {
        setupSheetTransition_Mobile(sheet, iTrue);
    }
    postRefresh_App();
}

void setupMenuTransition_Mobile(iWidget *sheet, iBool isIncoming) {
    if (!useMobileSheetLayout_()) {
        return;    
    }
    const iBool isSlidePanel = (flags_Widget(sheet) & horizontalOffset_WidgetFlag) != 0;
    if (isSlidePanel && isLandscape_App()) {
        return;
    }
    if (isIncoming) {
        setVisualOffset_Widget(sheet, isSlidePanel ? width_Widget(sheet) : height_Widget(sheet), 0, 0);
        setVisualOffset_Widget(sheet, 0, 330, easeOut_AnimFlag | softer_AnimFlag);
    }
    else {
        const iBool wasDragged = iAbs(value_Anim(&sheet->visualOffset) - 0) > 1;
        setVisualOffset_Widget(sheet,
                               isSlidePanel ? width_Widget(sheet) : height_Widget(sheet),
                               wasDragged ? 100 : 200,
                               wasDragged ? 0 : easeIn_AnimFlag | softer_AnimFlag);
    }
}

void setupSheetTransition_Mobile(iWidget *sheet, iBool isIncoming) {
    if (!useMobileSheetLayout_()) {
        if (prefs_App()->uiAnimations) {
            setFlags_Widget(sheet, horizontalOffset_WidgetFlag, iFalse);
            if (isIncoming) {
                setVisualOffset_Widget(sheet, -height_Widget(sheet), 0, 0);
                setVisualOffset_Widget(sheet, 0, 200, easeOut_AnimFlag | softer_AnimFlag);
            }
            else {
                setVisualOffset_Widget(sheet, -height_Widget(sheet), 200, easeIn_AnimFlag);
            }
        }
        return;
    }
    if(isSideBySideLayout_()) {
        return;
    }
    setFlags_Widget(sheet, horizontalOffset_WidgetFlag, iTrue);
    if (isIncoming) {
        setVisualOffset_Widget(sheet, size_Root(sheet->root).x, 0, 0);
        setVisualOffset_Widget(sheet, 0, 200, easeOut_AnimFlag);
    }
    else {
        const iBool wasDragged = iAbs(value_Anim(&sheet->visualOffset)) > 0;
        setVisualOffset_Widget(sheet, size_Root(sheet->root).x, wasDragged ? 100 : 200,
                               wasDragged ? 0 : easeIn_AnimFlag);
    }
}
