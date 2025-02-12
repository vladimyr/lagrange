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

#include "bookmarks.h"
#include "visited.h"
#include "gmrequest.h"
#include "app.h"

#include <the_Foundation/file.h>
#include <the_Foundation/hash.h>
#include <the_Foundation/mutex.h>
#include <the_Foundation/path.h>
#include <the_Foundation/regexp.h>
#include <the_Foundation/stringset.h>

void init_Bookmark(iBookmark *d) {
    init_String(&d->url);
    init_String(&d->title);
    init_String(&d->tags);
    iZap(d->when);
    d->sourceId = 0;
}

void deinit_Bookmark(iBookmark *d) {
    deinit_String(&d->tags);
    deinit_String(&d->title);
    deinit_String(&d->url);
}

iBool hasTag_Bookmark(const iBookmark *d, const char *tag) {
    if (!d) return iFalse;
    iRegExp *pattern = new_RegExp(format_CStr("\\b%s\\b", tag), caseSensitive_RegExpOption);
    iRegExpMatch m;
    init_RegExpMatch(&m);
    const iBool found = matchString_RegExp(pattern, &d->tags, &m);
    iRelease(pattern);
    return found;
}

void addTag_Bookmark(iBookmark *d, const char *tag) {
    if (!isEmpty_String(&d->tags)) {
        appendChar_String(&d->tags, ' ');
    }
    appendCStr_String(&d->tags, tag);
}

void removeTag_Bookmark(iBookmark *d, const char *tag) {
    const size_t pos = indexOfCStr_String(&d->tags, tag);
    if (pos != iInvalidPos) {
        remove_Block(&d->tags.chars, pos, strlen(tag));
        trim_String(&d->tags);
    }
}

iDefineTypeConstruction(Bookmark)

static int cmpTimeDescending_Bookmark_(const iBookmark **a, const iBookmark **b) {
    return iCmp(seconds_Time(&(*b)->when), seconds_Time(&(*a)->when));
}

static int cmpTitleAscending_Bookmark_(const iBookmark **a, const iBookmark **b) {
    return cmpStringCase_String(&(*a)->title, &(*b)->title);
}

/*----------------------------------------------------------------------------------------------*/

static const char *fileName_Bookmarks_ = "bookmarks.txt";

struct Impl_Bookmarks {
    iMutex *  mtx;
    int       idEnum;
    iHash     bookmarks; /* bookmark ID is the hash key */
    iPtrArray remoteRequests;
};

iDefineTypeConstruction(Bookmarks)

void init_Bookmarks(iBookmarks *d) {
    d->mtx = new_Mutex();
    d->idEnum = 0;
    init_Hash(&d->bookmarks);
    init_PtrArray(&d->remoteRequests);
}

void deinit_Bookmarks(iBookmarks *d) {
    iForEach(PtrArray, i, &d->remoteRequests) {
        cancel_GmRequest(i.ptr);
        free(userData_Object(i.ptr));
        iRelease(i.ptr);
    }
    deinit_PtrArray(&d->remoteRequests);
    clear_Bookmarks(d);
    deinit_Hash(&d->bookmarks);
    delete_Mutex(d->mtx);
}

void clear_Bookmarks(iBookmarks *d) {
    lock_Mutex(d->mtx);
    iForEach(Hash, i, &d->bookmarks) {
        delete_Bookmark((iBookmark *) i.value);
    }
    clear_Hash(&d->bookmarks);
    d->idEnum = 0;
    unlock_Mutex(d->mtx);
}

static void insert_Bookmarks_(iBookmarks *d, iBookmark *bookmark) {
    lock_Mutex(d->mtx);
    bookmark->node.key = ++d->idEnum;
    insert_Hash(&d->bookmarks, &bookmark->node);
    unlock_Mutex(d->mtx);
}

void load_Bookmarks(iBookmarks *d, const char *dirPath) {
    clear_Bookmarks(d);
    iFile *f = newCStr_File(concatPath_CStr(dirPath, fileName_Bookmarks_));
    if (open_File(f, readOnly_FileMode | text_FileMode)) {
        const iRangecc src = range_Block(collect_Block(readAll_File(f)));
        iRangecc line = iNullRange;
        while (nextSplit_Rangecc(src, "\n", &line)) {
            /* Skip empty lines. */ {
                iRangecc ln = line;
                trim_Rangecc(&ln);
                if (isEmpty_Range(&ln)) {
                    continue;
                }
            }
            iBookmark *bm = new_Bookmark();
            bm->icon = strtoul(line.start, NULL, 16);
            line.start += 9;
            char *endPos;
            initSeconds_Time(&bm->when, strtod(line.start, &endPos));
            line.start = skipSpace_CStr(endPos);
            setRange_String(&bm->url, line);
            /* Clean up the URL. */ {
                iUrl parts;
                init_Url(&parts, &bm->url);
                if (isEmpty_Range(&parts.path) && isEmpty_Range(&parts.query)) {
                    appendChar_String(&bm->url, '/');
                }
                stripDefaultUrlPort_String(&bm->url);
            }
            nextSplit_Rangecc(src, "\n", &line);
            setRange_String(&bm->title, line);
            nextSplit_Rangecc(src, "\n", &line);
            setRange_String(&bm->tags, line);
            insert_Bookmarks_(d, bm);
        }
    }
    iRelease(f);
}

void save_Bookmarks(const iBookmarks *d, const char *dirPath) {
    lock_Mutex(d->mtx);
    iRegExp *remotePattern = iClob(new_RegExp("\\bremote\\b", caseSensitive_RegExpOption));
    iFile *f = newCStr_File(concatPath_CStr(dirPath, fileName_Bookmarks_));
    if (open_File(f, writeOnly_FileMode | text_FileMode)) {
        iString *str = collectNew_String();
        iConstForEach(Hash, i, &d->bookmarks) {
            const iBookmark *bm = (const iBookmark *) i.value;
            iRegExpMatch m;
            init_RegExpMatch(&m);
            if (matchString_RegExp(remotePattern, &bm->tags, &m)) {
                /* Remote bookmarks are not saved. */
                continue;
            }
            format_String(str,
                          "%08x %.0lf %s\n%s\n%s\n",
                          bm->icon,
                          seconds_Time(&bm->when),
                          cstr_String(&bm->url),
                          cstr_String(&bm->title),
                          cstr_String(&bm->tags));
            writeData_File(f, cstr_String(str), size_String(str));
        }
    }
    iRelease(f);
    unlock_Mutex(d->mtx);
}

uint32_t add_Bookmarks(iBookmarks *d, const iString *url, const iString *title, const iString *tags,
                       iChar icon) {
    lock_Mutex(d->mtx);
    iBookmark *bm = new_Bookmark();
    set_String(&bm->url, url);
    set_String(&bm->title, title);
    if (tags) {
        set_String(&bm->tags, tags);
    }
    bm->icon = icon;
    initCurrent_Time(&bm->when);
    insert_Bookmarks_(d, bm);
    unlock_Mutex(d->mtx);
    return id_Bookmark(bm);
}

iBool remove_Bookmarks(iBookmarks *d, uint32_t id) {
    lock_Mutex(d->mtx);
    iBookmark *bm = (iBookmark *) remove_Hash(&d->bookmarks, id);
    if (bm) {
        /* If this is a remote source, make sure all the remote bookmarks are
           removed as well. */
        if (hasTag_Bookmark(bm, remoteSource_BookmarkTag)) {
            iForEach(Hash, i, &d->bookmarks) {
                iBookmark *j = (iBookmark *) i.value;
                if (j->sourceId == id_Bookmark(bm)) {
                    remove_HashIterator(&i);
                    delete_Bookmark(j);
                }
            }
        }
        delete_Bookmark(bm);
    }
    unlock_Mutex(d->mtx);
    return bm != NULL;
}

iBool updateBookmarkIcon_Bookmarks(iBookmarks *d, const iString *url, iChar icon) {
    iBool changed = iFalse;
    lock_Mutex(d->mtx);
    const uint32_t id = findUrl_Bookmarks(d, url);
    if (id) {
        iBookmark *bm = get_Bookmarks(d, id);
        if (!hasTag_Bookmark(bm, remote_BookmarkTag) && !hasTag_Bookmark(bm, userIcon_BookmarkTag)) {
            if (icon != bm->icon) {
                bm->icon = icon;
                changed = iTrue;
            }
        }
    }
    unlock_Mutex(d->mtx);
    return changed;
}

iChar siteIcon_Bookmarks(const iBookmarks *d, const iString *url) {
    if (isEmpty_String(url)) {
        return 0;
    }
    static iRegExp *tagPattern_;
    if (!tagPattern_) {
        tagPattern_ = new_RegExp("\\b" userIcon_BookmarkTag "\\b", caseSensitive_RegExpOption);
    }
    const iRangecc urlRoot      = urlRoot_String(url);
    size_t         matchingSize = iInvalidSize; /* we'll pick the shortest matching */
    iChar          icon         = 0;
    lock_Mutex(d->mtx);
    iConstForEach(Hash, i, &d->bookmarks) {
        const iBookmark *bm = (const iBookmark *) i.value;
        iRegExpMatch m;
        init_RegExpMatch(&m);
        if (bm->icon && matchString_RegExp(tagPattern_, &bm->tags, &m)) {
            const iRangecc bmRoot = urlRoot_String(&bm->url);
            if (equalRangeCase_Rangecc(urlRoot, bmRoot)) {
                const size_t n = size_String(&bm->url);
                if (n < matchingSize) {
                    matchingSize = n;
                    icon = bm->icon;
                }
            }
        }
    }
    unlock_Mutex(d->mtx);
    return icon;
}

iBookmark *get_Bookmarks(iBookmarks *d, uint32_t id) {
    return (iBookmark *) value_Hash(&d->bookmarks, id);
}

iBool filterTagsRegExp_Bookmarks(void *regExp, const iBookmark *bm) {
    iRegExpMatch m;
    init_RegExpMatch(&m);
    return matchString_RegExp(regExp, &bm->tags, &m);
}

static iBool matchUrl_(void *url, const iBookmark *bm) {
    return equalCase_String(url, &bm->url);
}

uint32_t findUrl_Bookmarks(const iBookmarks *d, const iString *url) {
    /* TODO: O(n), boo */
    const iPtrArray *found = list_Bookmarks(d, NULL, matchUrl_, (void *) url);
    if (isEmpty_PtrArray(found)) return 0;
    return id_Bookmark(constFront_PtrArray(found));
}

const iPtrArray *list_Bookmarks(const iBookmarks *d, iBookmarksCompareFunc cmp,
                                iBookmarksFilterFunc filter, void *context) {
    lock_Mutex(d->mtx);
    iPtrArray *list = collectNew_PtrArray();
    iConstForEach(Hash, i, &d->bookmarks) {
        const iBookmark *bm = (const iBookmark *) i.value;
        if (!filter || filter(context, bm)) {
            pushBack_PtrArray(list, bm);
        }
    }
    unlock_Mutex(d->mtx);
    if (!cmp) cmp = cmpTimeDescending_Bookmark_;
    sort_Array(list, (int (*)(const void *, const void *)) cmp);
    return list;
}

const iString *bookmarkListPage_Bookmarks(const iBookmarks *d, enum iBookmarkListType listType) {
    iString *str = collectNew_String();
    lock_Mutex(d->mtx);
    format_String(str,
                  "# %s\n\n",
                  listType == listByFolder_BookmarkListType ? "Bookmarks"
                  : listType == listByTag_BookmarkListType  ? "Bookmark tags"
                                                            : "Created bookmarks");
    if (listType == listByFolder_BookmarkListType) {
        appendFormat_String(str,
                            "You have %d bookmark%s.\n\n"
                            "Save this page to export them, or you can copy them to "
                            "the clipboard.\n\n",
                            size_Hash(&d->bookmarks),
                            size_Hash(&d->bookmarks) != 1 ? "s" : "");
    }
    else if (listType == listByTag_BookmarkListType) {
        appendFormat_String(str, "In this list each heading represents a bookmark tag. "
                                 "Only tagged bookmarks are listed. "
                                 "Bookmarks with multiple tags are repeated under each tag.\n\n");
    }
    iStringSet *tags = new_StringSet();
    const iPtrArray *bmList = list_Bookmarks(d,
                                             listType == listByCreationTime_BookmarkListType
                                                 ? cmpTimeDescending_Bookmark_
                                                 : cmpTitleAscending_Bookmark_,
                                             NULL,
                                             NULL);
    iConstForEach(PtrArray, i, bmList) {
        const iBookmark *bm = i.ptr;
        if (listType == listByFolder_BookmarkListType) {
            appendFormat_String(str, "=> %s %s\n", cstr_String(&bm->url), cstr_String(&bm->title));
        }
        else if (listType == listByCreationTime_BookmarkListType) {
            appendFormat_String(str, "=> %s %s - %s\n", cstr_String(&bm->url),
                                cstrCollect_String(format_Time(&bm->when, "%Y-%m-%d")),
                                cstr_String(&bm->title));
        }
        iRangecc tag = iNullRange;
        while (nextSplit_Rangecc(range_String(&bm->tags), " ", &tag)) {
            if (!isEmpty_Range(&tag)) {
                iString t;
                initRange_String(&t, tag);
                insert_StringSet(tags, &t);
                deinit_String(&t);
            }
        }
    }
    if (listType == listByTag_BookmarkListType) {
        iConstForEach(StringSet, t, tags) {
            const iString *tag = t.value;
            appendFormat_String(str, "\n## %s\n", cstr_String(tag));
            iConstForEach(PtrArray, i, bmList) {
                const iBookmark *bm = i.ptr;
                iRangecc bmTag = iNullRange;
                iBool isTagged = iFalse;
                while (nextSplit_Rangecc(range_String(&bm->tags), " ", &bmTag)) {
                    if (equal_Rangecc(bmTag, cstr_String(tag))) {
                        isTagged = iTrue;
                        break;
                    }
                }
                if (isTagged) {
                    appendFormat_String(
                        str, "=> %s %s\n", cstr_String(&bm->url), cstr_String(&bm->title));
                }
            }
        }
    }
    iRelease(tags);
    unlock_Mutex(d->mtx);
    if (listType == listByCreationTime_BookmarkListType) {
        appendCStr_String(str, "\nThis page is formatted according to the "
                               "\"Subscribing to Gemini pages\" companion specification.\n");
    }
    else {
        appendFormat_String(str,
                            "\nEach link represents a bookmark. "
                            "%s"
                            "Bullet lines and quotes are reserved for additional information about "
                            "the preceding bookmark. Text lines and preformatted text are considered "
                            "comments and should be ignored.\n",
                            listType == listByFolder_BookmarkListType
                                ? "Folder structure is defined by level 2/3 headings. "
                            : listType == listByTag_BookmarkListType
                                ? "Tags are defined by level 2 headings. "
                                : "");
    }
    return str;
}

static iBool isRemoteSource_Bookmark_(void *context, const iBookmark *d) {
    iUnused(context);
    return hasTag_Bookmark(d, remoteSource_BookmarkTag);
}

void remoteRequestFinished_Bookmarks_(iBookmarks *d, iGmRequest *req) {
    iUnused(d);
    postCommandf_App("bookmarks.request.finished req:%p", req);
}

void requestFinished_Bookmarks(iBookmarks *d, iGmRequest *req) {
    iBool found = iFalse;
    iForEach(PtrArray, i, &d->remoteRequests) {
        if (i.ptr == req) {
            remove_PtrArrayIterator(&i);
            found = iTrue;
            break;
        }
    }
    iAssert(found);
    /* Parse all links in the result. */
    if (isSuccess_GmStatusCode(status_GmRequest(req))) {
        iTime now;
        initCurrent_Time(&now);
        iRegExp *linkPattern = new_RegExp("^=>\\s*([^\\s]+)(\\s+(.*))?", 0);
        iString src;
        const iString *remoteTag = collectNewCStr_String("remote");
        initBlock_String(&src, body_GmRequest(req));
        iRangecc srcLine = iNullRange;
        while (nextSplit_Rangecc(range_String(&src), "\n", &srcLine)) {
            iRangecc line = srcLine;
            trimEnd_Rangecc(&line);
            iRegExpMatch m;
            init_RegExpMatch(&m);
            if (matchRange_RegExp(linkPattern, line, &m)) {
                const iRangecc url    = capturedRange_RegExpMatch(&m, 1);
                const iRangecc title  = capturedRange_RegExpMatch(&m, 3);
                iString *      urlStr = newRange_String(url);
                const iString *absUrl = absoluteUrl_String(url_GmRequest(req), urlStr);
                if (!findUrl_Bookmarks(d, absUrl)) {
                    iString *titleStr = newRange_String(title);
                    if (isEmpty_String(titleStr)) {
                        setRange_String(titleStr, urlHost_String(urlStr));
                    }
                    const uint32_t bmId = add_Bookmarks(d, absUrl, titleStr, remoteTag, 0x2913);
                    iBookmark *bm = get_Bookmarks(d, bmId);
                    bm->sourceId = *(uint32_t *) userData_Object(req);
                    delete_String(titleStr);
                }
                delete_String(urlStr);
            }
        }
        deinit_String(&src);
        iRelease(linkPattern);
    }
    else {
        /* TODO: Show error? */
    }
    free(userData_Object(req));
    iRelease(req);
    if (isEmpty_PtrArray(&d->remoteRequests)) {
        postCommand_App("bookmarks.changed");
    }
}

void fetchRemote_Bookmarks(iBookmarks *d) {
    if (!isEmpty_PtrArray(&d->remoteRequests)) {
        return; /* Already ongoing. */
    }
    lock_Mutex(d->mtx);
    /* Remove all current remote bookmarks. */ {
        size_t numRemoved = 0;
        iForEach(Hash, i, &d->bookmarks) {
            iBookmark *bm = (iBookmark *) i.value;
            if (hasTag_Bookmark(bm, remote_BookmarkTag)) {
                remove_HashIterator(&i);
                delete_Bookmark(bm);
                numRemoved++;
            }
        }
        if (numRemoved) {
            postCommand_App("bookmarks.changed");
        }
    }
    iConstForEach(PtrArray, i, list_Bookmarks(d, NULL, isRemoteSource_Bookmark_, NULL)) {
        const iBookmark *bm   = i.ptr;
        iGmRequest *     req  = new_GmRequest(certs_App());
        uint32_t *       bmId = malloc(4);
        *bmId                 = id_Bookmark(bm);
        setUserData_Object(req, bmId);
        pushBack_PtrArray(&d->remoteRequests, req);
        setUrl_GmRequest(req, &bm->url);
        iConnect(GmRequest, req, finished, req, remoteRequestFinished_Bookmarks_);
        submit_GmRequest(req);
    }
    unlock_Mutex(d->mtx);
}
